#include <exasol/udf/v2/internal/context_state.hpp>

#include "stream_management_test_utils.hpp"

#include <chrono>
#include <memory>
#include <optional>
#include <utility>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace exasol::udf::v2::internal
{
namespace
{

    using test_support::ForeignCallMessageBuilder;
    using test_support::ForeignControlMessageBuilder;
    using test_support::k_control_stream_id;
    using test_support::k_first_client_stream_id;
    using test_support::k_first_server_stream_id;
    using test_support::k_second_client_stream_id;
    using test_support::k_unknown_stream_id;
    using test_support::make_call_message_view;
    using test_support::make_control_message_view;
    using test_support::MockBackgroundThread;
    using test_support::StatefulBackgroundThreadFake;

    TEST(ContextStateTest, UsesInjectedClockToCreateDeadlineWithoutSleeping)
    {
        using Clock          = std::chrono::steady_clock;
        const auto now       = Clock::time_point{std::chrono::seconds{123}};
        const auto timeout   = std::chrono::seconds{5};
        auto background      = std::make_unique<testing::NiceMock<MockBackgroundThread>>();
        auto* background_ptr = background.get();
        EXPECT_CALL(*background_ptr, dequeue_inbound_messages()).WillOnce([] {
            return InboundBatch{};
        });
        EXPECT_CALL(*background_ptr, wait_until(std::optional<Clock::time_point>{now + timeout}))
            .WillOnce([] { return BackgroundThreadResult{.event = Event::DeadlineExpired}; });
        ContextState context_state{EndpointRole::Client, std::move(background),
                                   [now] { return now; }};

        EXPECT_EQ(context_state.wait_for_message(interface::Timeout{timeout}),
                  interface::MessageStatus::TimedOut);
    }

    TEST(ContextStateTest, CreatesAndSendsControlMessagesUntilConnectionClose)
    {
        auto background      = std::make_unique<testing::NiceMock<MockBackgroundThread>>();
        auto* background_ptr = background.get();
        EXPECT_CALL(*background_ptr, enqueue_outbound_message(testing::_))
            .Times(2)
            .WillRepeatedly([background_ptr](OutboundMessage& message) {
                return background_ptr->accept_outbound_message(message);
            });
        ContextState state{EndpointRole::Client, std::move(background)};

        auto message = state.create_control_message_builder();
        ASSERT_NE(message, nullptr);
        message->keep_alive();
        EXPECT_TRUE(state.send_control_message(std::move(message)));

        auto send_after_close = state.create_control_message_builder();
        ASSERT_NE(send_after_close, nullptr);
        auto close = state.create_control_message_builder();
        ASSERT_NE(close, nullptr);
        close->close_connection();
        EXPECT_TRUE(state.send_control_message(std::move(close)));
        EXPECT_EQ(state.create_control_message_builder(), nullptr);
        EXPECT_EQ(state.send_control_message(std::move(send_after_close)).status,
                  interface::OperationStatus::ContextClosed);
        EXPECT_NE(send_after_close, nullptr);
        EXPECT_EQ(state.control_receive_status(), interface::MessageStatus::ContextClosed);
    }

    TEST(ContextStateTest, RejectsNullControlBuilder)
    {
        std::unique_ptr<interface::ControlMessageBuilder> null_message;
        ContextState state{EndpointRole::Client,
                           std::make_unique<testing::NiceMock<MockBackgroundThread>>()};
        EXPECT_EQ(state.send_control_message(std::move(null_message)).status,
                  interface::OperationStatus::ProtocolError);
    }

    TEST(ContextStateTest, RejectsForeignControlBuilderAndRetainsOwnership)
    {
        ContextState state{EndpointRole::Client,
                           std::make_unique<testing::NiceMock<MockBackgroundThread>>()};
        ContextState other{EndpointRole::Client,
                           std::make_unique<testing::NiceMock<MockBackgroundThread>>()};
        auto foreign = other.create_control_message_builder();
        ASSERT_NE(foreign, nullptr);
        EXPECT_EQ(state.send_control_message(std::move(foreign)).status,
                  interface::OperationStatus::ProtocolError);
        EXPECT_NE(foreign, nullptr);
    }

    TEST(ContextStateTest, RejectsNonImplementationControlBuilderAndRetainsOwnership)
    {
        ContextState state{EndpointRole::Client,
                           std::make_unique<testing::NiceMock<MockBackgroundThread>>()};
        std::unique_ptr<interface::ControlMessageBuilder> wrong_implementation =
            std::make_unique<testing::StrictMock<ForeignControlMessageBuilder>>();
        EXPECT_EQ(state.send_control_message(std::move(wrong_implementation)).status,
                  interface::OperationStatus::ProtocolError);
        EXPECT_NE(wrong_implementation, nullptr);
    }

    TEST(ContextStateTest, RetainsControlBuilderWhenBackgroundRejectsIt)
    {
        auto background      = std::make_unique<testing::NiceMock<MockBackgroundThread>>();
        auto* background_ptr = background.get();
        EXPECT_CALL(*background_ptr, enqueue_outbound_message(testing::_))
            .WillOnce(testing::Return(false));
        ContextState state{EndpointRole::Client, std::move(background)};
        auto rejected = state.create_control_message_builder();
        ASSERT_NE(rejected, nullptr);
        EXPECT_EQ(state.send_control_message(std::move(rejected)).status,
                  interface::OperationStatus::TransportError);
        EXPECT_NE(rejected, nullptr);
        EXPECT_EQ(state.receive_status(), interface::MessageStatus::ConnectionError);
    }

    TEST(ContextStateTest, RejectsControlBuilderFromExpiredOwnerAndRetainsOwnership)
    {
        std::unique_ptr<interface::ControlMessageBuilder> expired_owner_builder;
        {
            ContextState source{EndpointRole::Client,
                                std::make_unique<testing::NiceMock<MockBackgroundThread>>()};
            expired_owner_builder = source.create_control_message_builder();
        }
        ContextState fresh_state{EndpointRole::Client,
                                 std::make_unique<testing::NiceMock<MockBackgroundThread>>()};
        EXPECT_EQ(fresh_state.send_control_message(std::move(expired_owner_builder)).status,
                  interface::OperationStatus::ProtocolError);
        EXPECT_NE(expired_owner_builder, nullptr);
    }

    TEST(ContextStateTest, RejectsNullOpeningBuilder)
    {
        std::unique_ptr<interface::OpeningCallMessageBuilder> null_message;
        ContextState state{EndpointRole::Client,
                           std::make_unique<testing::NiceMock<MockBackgroundThread>>()};
        EXPECT_EQ(state.open_call(std::move(null_message)).status,
                  interface::OperationStatus::ProtocolError);
    }

    TEST(ContextStateTest, RejectsOpeningBuilderWithoutOpenCallAndRetainsOwnership)
    {
        ContextState state{EndpointRole::Client,
                           std::make_unique<testing::NiceMock<MockBackgroundThread>>()};
        auto missing_open = state.create_opening_call_message_builder();
        ASSERT_NE(missing_open, nullptr);
        EXPECT_EQ(state.open_call(std::move(missing_open)).status,
                  interface::OperationStatus::ProtocolError);
        EXPECT_NE(missing_open, nullptr);
    }

    TEST(ContextStateTest, RejectsForeignOpeningBuilderAndRetainsOwnership)
    {
        ContextState state{EndpointRole::Client,
                           std::make_unique<testing::NiceMock<MockBackgroundThread>>()};
        ContextState other{EndpointRole::Client,
                           std::make_unique<testing::NiceMock<MockBackgroundThread>>()};
        auto foreign = other.create_opening_call_message_builder();
        ASSERT_NE(foreign, nullptr);
        foreign->open_call("foreign");
        EXPECT_EQ(state.open_call(std::move(foreign)).status,
                  interface::OperationStatus::ProtocolError);
        EXPECT_NE(foreign, nullptr);
    }

    TEST(ContextStateTest, RejectsNullCallBuilder)
    {
        ContextState state{EndpointRole::Client,
                           std::make_unique<testing::NiceMock<MockBackgroundThread>>()};
        auto opening = state.create_opening_call_message_builder();
        ASSERT_NE(opening, nullptr);
        opening->open_call("call");
        const auto opened = state.open_call(std::move(opening));
        ASSERT_TRUE(opened);
        const auto stream_id = *opened.value;
        ASSERT_EQ(stream_id, k_first_client_stream_id);

        std::unique_ptr<interface::CallMessageBuilder> null_message;
        EXPECT_EQ(state.send_call_message(stream_id, std::move(null_message), std::nullopt).status,
                  interface::OperationStatus::ProtocolError);
    }

    TEST(ContextStateTest, ReturnsPeerClosedForUnknownCallStream)
    {
        ContextState state{EndpointRole::Client,
                           std::make_unique<testing::NiceMock<MockBackgroundThread>>()};
        auto opening = state.create_opening_call_message_builder();
        opening->open_call("call");
        const auto opened = state.open_call(std::move(opening));
        ASSERT_TRUE(opened);

        EXPECT_EQ(
            state
                .send_call_message(k_second_client_stream_id,
                                   state.create_call_message_builder(*opened.value), std::nullopt)
                .status,
            interface::OperationStatus::PeerClosed);
    }

    TEST(ContextStateTest, RejectsForeignCallBuilderAndRetainsOwnership)
    {
        ContextState state{EndpointRole::Client,
                           std::make_unique<testing::NiceMock<MockBackgroundThread>>()};
        auto opening = state.create_opening_call_message_builder();
        opening->open_call("call");
        const auto opened = state.open_call(std::move(opening));
        ASSERT_TRUE(opened);

        ContextState other{EndpointRole::Client,
                           std::make_unique<testing::NiceMock<MockBackgroundThread>>()};
        auto other_opening = other.create_opening_call_message_builder();
        ASSERT_NE(other_opening, nullptr);
        other_opening->open_call("other");
        ASSERT_TRUE(other.open_call(std::move(other_opening)));
        auto foreign = other.create_call_message_builder(*opened.value);
        ASSERT_NE(foreign, nullptr);
        EXPECT_EQ(state.send_call_message(*opened.value, std::move(foreign), std::nullopt).status,
                  interface::OperationStatus::ProtocolError);
        EXPECT_NE(foreign, nullptr);
    }

    TEST(ContextStateTest, RejectsNonImplementationCallBuilderAndRetainsOwnership)
    {
        ContextState state{EndpointRole::Client,
                           std::make_unique<testing::NiceMock<MockBackgroundThread>>()};
        auto opening = state.create_opening_call_message_builder();
        opening->open_call("call");
        const auto opened = state.open_call(std::move(opening));
        ASSERT_TRUE(opened);
        std::unique_ptr<interface::CallMessageBuilder> wrong_implementation =
            std::make_unique<testing::StrictMock<ForeignCallMessageBuilder>>();
        EXPECT_EQ(
            state.send_call_message(*opened.value, std::move(wrong_implementation), std::nullopt)
                .status,
            interface::OperationStatus::ProtocolError);
        EXPECT_NE(wrong_implementation, nullptr);
    }

    TEST(ContextStateTest, AcceptedCallClosePreventsFurtherSends)
    {
        ContextState state{EndpointRole::Client,
                           std::make_unique<testing::NiceMock<MockBackgroundThread>>()};
        auto opening = state.create_opening_call_message_builder();
        opening->open_call("call");
        const auto opened = state.open_call(std::move(opening));
        ASSERT_TRUE(opened);

        auto close = state.create_call_message_builder(*opened.value);
        ASSERT_NE(close, nullptr);
        close->close_call();
        EXPECT_TRUE(state.send_call_message(*opened.value, std::move(close), std::nullopt));
        EXPECT_EQ(state.call_receive_status(*opened.value), interface::MessageStatus::NoMessage);
        EXPECT_EQ(
            state
                .send_call_message(*opened.value, state.create_call_message_builder(*opened.value),
                                   std::nullopt)
                .status,
            interface::OperationStatus::ProtocolError);
    }

    TEST(ContextStateTest, RejectsCallBuilderFromExpiredOwnerAndRetainsOwnership)
    {
        std::unique_ptr<interface::CallMessageBuilder> expired_owner_builder;
        {
            ContextState source{EndpointRole::Client,
                                std::make_unique<testing::NiceMock<MockBackgroundThread>>()};
            auto source_opening = source.create_opening_call_message_builder();
            source_opening->open_call("source");
            ASSERT_TRUE(source.open_call(std::move(source_opening)));
            expired_owner_builder = source.create_call_message_builder(k_first_client_stream_id);
        }
        ContextState state{EndpointRole::Client,
                           std::make_unique<testing::NiceMock<MockBackgroundThread>>()};
        auto destination_opening = state.create_opening_call_message_builder();
        destination_opening->open_call("destination");
        const auto destination = state.open_call(std::move(destination_opening));
        ASSERT_TRUE(destination);
        EXPECT_EQ(state
                      .send_call_message(*destination.value, std::move(expired_owner_builder),
                                         std::nullopt)
                      .status,
                  interface::OperationStatus::ProtocolError);
        EXPECT_NE(expired_owner_builder, nullptr);
    }

    TEST(ContextStateTest, RetainsCallMessageBuilderWhenBackgroundRejectsIt)
    {
        auto background      = std::make_unique<testing::NiceMock<MockBackgroundThread>>();
        auto* background_ptr = background.get();
        EXPECT_CALL(*background_ptr, dequeue_inbound_messages())
            .Times(testing::AnyNumber())
            .WillRepeatedly([] { return InboundBatch{}; });
        EXPECT_CALL(*background_ptr, enqueue_outbound_message(testing::_))
            .WillOnce([background_ptr](OutboundMessage& message) {
                return background_ptr->accept_outbound_message(message);
            })
            .WillOnce(testing::Return(false));
        ContextState state{EndpointRole::Client, std::move(background)};
        auto opening = state.create_opening_call_message_builder();
        ASSERT_NE(opening, nullptr);
        opening->open_call("test");
        auto opened = state.open_call(std::move(opening));
        ASSERT_TRUE(opened);

        auto message = state.create_call_message_builder(*opened.value);
        ASSERT_NE(message, nullptr);
        const auto result =
            state.send_call_message(*opened.value, std::move(message), interface::Timeout{});

        EXPECT_EQ(result.status, interface::OperationStatus::TransportError);
        EXPECT_NE(message, nullptr);
    }

    TEST(ContextStateTest, RetainsOpeningBuilderWhenBackgroundRejectsIt)
    {
        auto background = std::make_unique<testing::NiceMock<MockBackgroundThread>>();
        EXPECT_CALL(*background, enqueue_outbound_message(testing::_))
            .WillOnce(testing::Return(false));
        ContextState state{EndpointRole::Client, std::move(background)};
        auto opening = state.create_opening_call_message_builder();
        ASSERT_NE(opening, nullptr);
        opening->open_call("test");

        const auto result = state.open_call(std::move(opening));

        EXPECT_EQ(result.status, interface::OperationStatus::TransportError);
        EXPECT_NE(opening, nullptr);
    }

    TEST(ContextStateTest, ClosedStateRejectsNewBuildersAndOperations)
    {
        ContextState state{EndpointRole::Client,
                           std::make_unique<testing::NiceMock<MockBackgroundThread>>()};
        state.shutdown();

        EXPECT_EQ(state.create_opening_call_message_builder(), nullptr);
        EXPECT_EQ(state.create_call_message_builder(k_first_client_stream_id), nullptr);
        EXPECT_EQ(state.create_control_message_builder(), nullptr);
        EXPECT_EQ(state.accept_call().status, interface::OperationStatus::ContextClosed);
        EXPECT_EQ(state.open_call(nullptr).status, interface::OperationStatus::ContextClosed);
        EXPECT_EQ(state.wait_for_message(std::nullopt), interface::MessageStatus::ContextClosed);
        EXPECT_EQ(state.send_call_message(k_first_client_stream_id, nullptr, std::nullopt).status,
                  interface::OperationStatus::ContextClosed);
        EXPECT_EQ(state.send_control_message(nullptr).status,
                  interface::OperationStatus::ContextClosed);
    }

    TEST(ContextStateTest, MapsBackgroundShutdownToCancelled)
    {
        auto background = std::make_unique<StatefulBackgroundThreadFake>();
        background->set_next_wait_result({.event = Event::ShutdownRequested});
        ContextState cancelled{EndpointRole::Client, std::move(background)};
        EXPECT_EQ(cancelled.wait_for_message(std::nullopt), interface::MessageStatus::Cancelled);
        EXPECT_EQ(cancelled.accept_call().status, interface::OperationStatus::Cancelled);
    }

    TEST(ContextStateTest, MapsBackgroundErrorToConnectionFailure)
    {
        auto failed_background = std::make_unique<StatefulBackgroundThreadFake>();
        failed_background->set_next_wait_result(
            {.event = Event::ShutdownRequested,
             .error = interface::ErrorInfo("IO", "background failed")});
        ContextState failed{EndpointRole::Client, std::move(failed_background)};
        EXPECT_EQ(failed.wait_for_message(std::nullopt), interface::MessageStatus::ConnectionError);
        EXPECT_EQ(failed.receive_control_message(std::nullopt).status,
                  interface::OperationStatus::TransportError);
    }

    TEST(ContextStateTest, MarksContextFailedAfterInvalidInboundCall)
    {
        auto invalid_background = std::make_unique<StatefulBackgroundThreadFake>();
        invalid_background->push_inbound(make_call_message_view(k_control_stream_id));
        ContextState invalid{EndpointRole::Client, std::move(invalid_background)};
        EXPECT_EQ(invalid.receive_status(), interface::MessageStatus::ConnectionError);
        EXPECT_EQ(invalid.is_new_inbound_call_available(), false);
        EXPECT_EQ(invalid.receive_call_message(k_first_client_stream_id, std::nullopt).status,
                  interface::OperationStatus::ProtocolError);
    }

    TEST(ContextStateTest, RejectsMalformedInboundTrafficAcrossOperations)
    {
        auto wait_background = std::make_unique<StatefulBackgroundThreadFake>();
        wait_background->push_inbound(make_control_message_view(k_first_client_stream_id));
        ContextState wait_state{EndpointRole::Client, std::move(wait_background)};
        EXPECT_EQ(wait_state.wait_for_message(std::nullopt),
                  interface::MessageStatus::ConnectionError);

        auto accept_background = std::make_unique<StatefulBackgroundThreadFake>();
        accept_background->push_inbound(make_control_message_view(k_first_client_stream_id));
        ContextState accept_state{EndpointRole::Client, std::move(accept_background)};
        EXPECT_EQ(accept_state.accept_call().status, interface::OperationStatus::ProtocolError);

        auto control_background = std::make_unique<StatefulBackgroundThreadFake>();
        control_background->push_inbound(make_control_message_view(k_first_client_stream_id));
        ContextState control_state{EndpointRole::Client, std::move(control_background)};
        EXPECT_EQ(control_state.receive_control_message(std::nullopt).status,
                  interface::OperationStatus::ProtocolError);

        auto send_background      = std::make_unique<StatefulBackgroundThreadFake>();
        auto* send_background_ptr = send_background.get();
        ContextState send_state{EndpointRole::Client, std::move(send_background)};
        auto opening = send_state.create_opening_call_message_builder();
        opening->open_call("call");
        const auto call = send_state.open_call(std::move(opening));
        ASSERT_TRUE(call);
        send_background_ptr->push_inbound(make_call_message_view(k_control_stream_id));
        auto message = send_state.create_call_message_builder(*call.value);
        EXPECT_EQ(
            send_state.send_call_message(*call.value, std::move(message), std::nullopt).status,
            interface::OperationStatus::ProtocolError);
        EXPECT_NE(message, nullptr);
    }

    TEST(ContextStateTest, AcceptsInboundCallAndDeliversItsOpeningMessage)
    {
        auto background = std::make_unique<StatefulBackgroundThreadFake>();
        background->push_inbound(make_call_message_view(k_first_server_stream_id, true));
        ContextState state{EndpointRole::Client, std::move(background)};
        EXPECT_TRUE(state.is_new_inbound_call_available());
        const auto accepted = state.accept_call();
        ASSERT_TRUE(accepted);
        EXPECT_EQ(*accepted.value, k_first_server_stream_id);
        EXPECT_EQ(state.call_receive_status(k_first_server_stream_id),
                  interface::MessageStatus::MessageAvailable);
        EXPECT_TRUE(state.receive_call_message(k_first_server_stream_id, std::nullopt));
    }

    TEST(ContextStateTest, ReceivesMessageFromBackgroundDequeuedBatch)
    {
        auto background      = std::make_unique<testing::NiceMock<MockBackgroundThread>>();
        auto* background_ptr = background.get();
        InboundBatch batch;
        batch.emplace_back(make_control_message_view(k_control_stream_id));
        EXPECT_CALL(*background_ptr, dequeue_inbound_messages())
            .WillOnce([batch = std::move(batch)]() mutable { return std::move(batch); });
        ContextState state{EndpointRole::Client, std::move(background)};

        const auto received = state.receive_control_message(
            interface::Timeout{std::chrono::steady_clock::duration::zero()});

        ASSERT_TRUE(received);
        EXPECT_EQ((*received.value)->stream_id(), k_control_stream_id);
    }

    TEST(ContextStateTest, CallReceiveReportsCancelledAfterBackgroundShutdown)
    {
        auto background = std::make_unique<StatefulBackgroundThreadFake>();
        background->push_inbound(make_call_message_view(k_first_server_stream_id, true));
        ContextState state{EndpointRole::Client, std::move(background)};
        const auto accepted = state.accept_call();
        ASSERT_TRUE(accepted);
        EXPECT_TRUE(state.receive_call_message(k_first_server_stream_id, std::nullopt));
        EXPECT_EQ(state.receive_call_message(k_first_server_stream_id, std::nullopt).status,
                  interface::OperationStatus::Cancelled);
    }

    TEST(ContextStateTest, DiscardsCallMessagesFollowingPeerCloseInTheSameBatch)
    {
        auto background = std::make_unique<StatefulBackgroundThreadFake>();
        background->push_inbound(make_call_message_view(k_first_server_stream_id, true));
        background->push_inbound(make_call_message_view(k_first_server_stream_id));
        background->push_inbound(make_call_message_view(k_first_server_stream_id, false, true));
        background->push_inbound(make_call_message_view(k_first_server_stream_id));
        ContextState state{EndpointRole::Client, std::move(background)};

        const auto accepted = state.accept_call();
        ASSERT_TRUE(accepted);
        EXPECT_EQ(*accepted.value, k_first_server_stream_id);
        auto opening = state.receive_call_message(k_first_server_stream_id, std::nullopt);
        ASSERT_TRUE(opening);
        EXPECT_NE((*opening.value)->open_call(), nullptr);
        auto message_before_close =
            state.receive_call_message(k_first_server_stream_id, std::nullopt);
        ASSERT_TRUE(message_before_close);
        EXPECT_EQ((*message_before_close.value)->close_call(), nullptr);
        auto close = state.receive_call_message(k_first_server_stream_id, std::nullopt);
        ASSERT_TRUE(close);
        EXPECT_NE((*close.value)->close_call(), nullptr);

        EXPECT_EQ(state.receive_status(), interface::MessageStatus::NoMessage);
        EXPECT_EQ(state.receive_call_message(k_first_server_stream_id, std::nullopt).status,
                  interface::OperationStatus::PeerClosed);
    }

    TEST(ContextStateTest, ReturnsTimedOutWhenWaitDeadlineExpires)
    {
        auto background = std::make_unique<StatefulBackgroundThreadFake>();
        ContextState state{EndpointRole::Client, std::move(background)};

        EXPECT_EQ(state.wait_for_message(interface::Timeout{std::chrono::seconds{1}}),
                  interface::MessageStatus::TimedOut);
    }

    TEST(ContextStateTest, ReceivesControlMessageThenTimesOutWhenQueueIsEmpty)
    {
        auto background = std::make_unique<StatefulBackgroundThreadFake>();
        background->push_inbound(make_control_message_view(k_control_stream_id));
        ContextState state{EndpointRole::Client, std::move(background)};

        EXPECT_TRUE(state.receive_control_message(std::nullopt));
        EXPECT_EQ(state.receive_control_message(interface::Timeout{std::chrono::seconds{0}}).status,
                  interface::OperationStatus::TimedOut);
    }

    TEST(ContextStateTest, UnknownCallsReportPeerClosed)
    {
        ContextState state{EndpointRole::Client,
                           std::make_unique<testing::NiceMock<MockBackgroundThread>>()};
        EXPECT_EQ(state.call_receive_status(k_unknown_stream_id),
                  interface::MessageStatus::PeerClosed);
        EXPECT_EQ(state.receive_call_message(k_unknown_stream_id, std::nullopt).status,
                  interface::OperationStatus::PeerClosed);
    }

    TEST(ContextStateTest, RejectsBuilderBoundToAnotherActiveCall)
    {
        ContextState state{EndpointRole::Client,
                           std::make_unique<testing::NiceMock<MockBackgroundThread>>()};
        auto first_opening = state.create_opening_call_message_builder();
        ASSERT_NE(first_opening, nullptr);
        first_opening->open_call("first");
        const auto first_call = state.open_call(std::move(first_opening));
        ASSERT_TRUE(first_call);
        const auto second_call = [&state] {
            auto opening = state.create_opening_call_message_builder();
            opening->open_call("second");
            return state.open_call(std::move(opening));
        }();
        ASSERT_TRUE(second_call);

        auto wrong_stream_builder = state.create_call_message_builder(*first_call.value);
        ASSERT_NE(wrong_stream_builder, nullptr);
        EXPECT_EQ(state
                      .send_call_message(*second_call.value, std::move(wrong_stream_builder),
                                         std::nullopt)
                      .status,
                  interface::OperationStatus::ProtocolError);
        EXPECT_NE(wrong_stream_builder, nullptr);
    }

    TEST(ContextStateTest, TimesOutWhenActiveCallHasNoMessage)
    {
        ContextState state{EndpointRole::Client,
                           std::make_unique<testing::NiceMock<MockBackgroundThread>>()};
        auto opening = state.create_opening_call_message_builder();
        ASSERT_NE(opening, nullptr);
        opening->open_call("active");
        const auto call = state.open_call(std::move(opening));
        ASSERT_TRUE(call);
        EXPECT_EQ(
            state.receive_call_message(*call.value, interface::Timeout{std::chrono::seconds{0}})
                .status,
            interface::OperationStatus::TimedOut);
    }

    TEST(ContextStateTest, RejectsSendOnCallClosedByItsOpeningMessage)
    {
        ContextState state{EndpointRole::Client,
                           std::make_unique<testing::NiceMock<MockBackgroundThread>>()};
        auto opening_close = state.create_opening_call_message_builder();
        ASSERT_NE(opening_close, nullptr);
        opening_close->open_call("closed at open").close_call();
        const auto locally_closed = state.open_call(std::move(opening_close));
        ASSERT_TRUE(locally_closed);
        EXPECT_EQ(state
                      .send_call_message(*locally_closed.value,
                                         state.create_call_message_builder(*locally_closed.value),
                                         std::nullopt)
                      .status,
                  interface::OperationStatus::ProtocolError);
    }

    TEST(ContextStateTest, PeerConnectionCloseStopsCallReceiveAndRemainsAvailableOnControlStream)
    {
        auto background      = std::make_unique<StatefulBackgroundThreadFake>();
        auto* background_ptr = background.get();
        ContextState state{EndpointRole::Client, std::move(background)};
        auto opening = state.create_opening_call_message_builder();
        opening->open_call("active");
        const auto opened = state.open_call(std::move(opening));
        ASSERT_TRUE(opened);

        background_ptr->push_inbound(make_control_message_view(k_control_stream_id, true));
        EXPECT_EQ(state.receive_call_message(*opened.value, std::nullopt).status,
                  interface::OperationStatus::ContextClosed);
        EXPECT_EQ(state.control_receive_status(), interface::MessageStatus::MessageAvailable);
        EXPECT_EQ(state.call_receive_status(*opened.value),
                  interface::MessageStatus::ContextClosed);
        auto close = state.receive_control_message(std::nullopt);
        ASSERT_TRUE(close);
        EXPECT_NE((*close.value)->close_connection(), nullptr);
    }

    TEST(ContextStateTest, AcceptsOpeningMessageThatAlsoClosesTheCall)
    {
        auto background = std::make_unique<StatefulBackgroundThreadFake>();
        background->push_inbound(make_call_message_view(k_first_server_stream_id, true, true));
        ContextState state{EndpointRole::Client, std::move(background)};

        const auto accepted = state.accept_call();
        ASSERT_TRUE(accepted);
        EXPECT_EQ(*accepted.value, k_first_server_stream_id);
        auto opening_and_close = state.receive_call_message(k_first_server_stream_id, std::nullopt);
        ASSERT_TRUE(opening_and_close);
        EXPECT_NE((*opening_and_close.value)->open_call(), nullptr);
        EXPECT_NE((*opening_and_close.value)->close_call(), nullptr);
        EXPECT_EQ(state.receive_call_message(k_first_server_stream_id, std::nullopt).status,
                  interface::OperationStatus::PeerClosed);
    }

    TEST(ContextStateTest, IgnoresSpuriousInboundNotificationWhileAccepting)
    {
        auto background = std::make_unique<StatefulBackgroundThreadFake>();
        background->set_next_wait_result({.event = Event::InboundAvailable});
        ContextState state{EndpointRole::Client, std::move(background)};

        EXPECT_EQ(state.accept_call().status, interface::OperationStatus::Cancelled);
    }

    TEST(ContextStateTest, DeliversPeerConnectionCloseDispatchedDuringControlReceive)
    {
        auto background = std::make_unique<StatefulBackgroundThreadFake>();
        background->push_inbound(make_control_message_view(k_control_stream_id, true));
        ContextState state{EndpointRole::Client, std::move(background)};

        auto result = state.receive_control_message(std::nullopt);
        ASSERT_TRUE(result);
        EXPECT_NE((*result.value)->close_connection(), nullptr);
    }

} // namespace
} // namespace exasol::udf::v2::internal
