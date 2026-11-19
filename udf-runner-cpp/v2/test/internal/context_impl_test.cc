#include <exasol/udf/v2/internal/call_impl.hpp>
#include <exasol/udf/v2/internal/context_impl.hpp>
#include <exasol/udf/v2/internal/context_state.hpp>

#include "stream_management_test_utils.hpp"

#include <chrono>
#include <memory>
#include <utility>

#include <gtest/gtest.h>

namespace exasol::udf::v2::internal
{
namespace
{

    using test_support::FakeBackgroundThread;
    using test_support::make_call_message_view;
    using test_support::make_control_message_view;

    TEST(ContextImplTest, DispatchesInboundCallAndKeepsCallStateAfterAccept)
    {
        auto background = std::make_unique<FakeBackgroundThread>();
        background->push_inbound(make_call_message_view(2, true));
        background->push_inbound(make_call_message_view(2, false, true));
        ContextImpl context{
            std::make_shared<ContextState>(EndpointRole::Client, std::move(background))};

        auto result = context.accept_call();
        ASSERT_TRUE(result);
        auto& call = **result.value;
        EXPECT_EQ(call.receive_status(), interface::MessageStatus::MessageAvailable);
        auto opening = call.receive();
        ASSERT_TRUE(opening);
        EXPECT_NE((*opening.value)->open_call(), nullptr);
        auto close = call.receive();
        ASSERT_TRUE(close);
        EXPECT_NE((*close.value)->close_call(), nullptr);
        EXPECT_EQ(call.receive().status, interface::OperationStatus::PeerClosed);
    }

    TEST(ContextImplTest, SendOverloadsAcceptMessagesAndZeroTimeoutReceiveReturnsTimedOut)
    {
        auto background            = std::make_unique<FakeBackgroundThread>();
        const auto* background_ptr = background.get();
        ContextImpl context{
            std::make_shared<ContextState>(EndpointRole::Client, std::move(background))};
        auto opening = context.create_opening_call_message_builder();
        ASSERT_NE(opening, nullptr);
        opening->add_payload("opening", "value")
            .data_schema(nullptr, false, false)
            .reset(1)
            .record_batch(nullptr, false)
            .error("OPEN", "opening error")
            .data_schema(interface::ArrowSchemaPtr{new ArrowSchema{}}, true, true)
            .reset(2)
            .record_batch(interface::ArrowArrayPtr{new ArrowArray{}}, true)
            .error("OPEN_REPLACED", "replacement opening error");
        opening->open_call("test").add_payload("", "", true);
        auto opened = context.open_call(std::move(opening));
        ASSERT_TRUE(opened);
        const auto outbound_before_sends = background_ptr->outbound_size();

        auto no_wait_builder = (*opened.value)->create_message_builder();
        ASSERT_NE(no_wait_builder, nullptr);
        EXPECT_TRUE((*opened.value)->send(std::move(no_wait_builder)));
        EXPECT_EQ(background_ptr->outbound_size(), outbound_before_sends + 1);

        auto zero_timeout_builder = (*opened.value)->create_message_builder();
        ASSERT_NE(zero_timeout_builder, nullptr);
        EXPECT_TRUE((*opened.value)
                        ->send(std::move(zero_timeout_builder),
                               interface::Timeout{std::chrono::steady_clock::duration::zero()}));
        EXPECT_EQ(background_ptr->outbound_size(), outbound_before_sends + 2);

        EXPECT_EQ((*opened.value)
                      ->receive(interface::Timeout{std::chrono::steady_clock::duration::zero()})
                      .status,
                  interface::OperationStatus::TimedOut);
        EXPECT_EQ((*opened.value)->receive_status(), interface::MessageStatus::NoMessage);
        auto populated_builder = (*opened.value)->create_message_builder();
        ASSERT_NE(populated_builder, nullptr);
        populated_builder->add_payload("call", "value")
            .data_schema(nullptr, false, false)
            .reset(2)
            .record_batch(nullptr, false)
            .error("CALL", "call error")
            .data_schema(interface::ArrowSchemaPtr{new ArrowSchema{}}, true, true)
            .reset(3)
            .record_batch(interface::ArrowArrayPtr{new ArrowArray{}}, true)
            .add_payload("", "", true)
            .error("CALL_REPLACED", "replacement call error");
        EXPECT_TRUE((*opened.value)->send(std::move(populated_builder)));
        EXPECT_EQ((*opened.value)
                      ->receive(interface::Timeout{std::chrono::steady_clock::duration::zero()})
                      .status,
                  interface::OperationStatus::TimedOut);
        EXPECT_EQ(context.control_stream().receive().status, interface::OperationStatus::TimedOut);

        auto control_builder = context.control_stream().create_message_builder();
        ASSERT_NE(control_builder, nullptr);
        control_builder->server_capabilities(1, 0, std::endian::native, 1)
            .keep_alive()
            .add_payload("control", "value")
            .error("CONTROL", "control error")
            .server_capabilities(2, 1, std::endian::native, 2)
            .add_payload("", "", true)
            .error("CONTROL_REPLACED", "replacement control error");
        EXPECT_TRUE(context.control_stream().send(std::move(control_builder)));
    }

    TEST(ContextImplTest, ForwardsFailedOperationsAndWaitOverloads)
    {
        auto background = std::make_unique<FakeBackgroundThread>();
        background->set_next_wait_result({.event = Event::DeadlineExpired});
        ContextImpl context{
            std::make_shared<ContextState>(EndpointRole::Client, std::move(background))};

        EXPECT_FALSE(context.is_new_inbound_call_available());
        EXPECT_EQ(context.receive_status(), interface::MessageStatus::NoMessage);
        const auto rejected_open = context.open_call(nullptr);
        EXPECT_EQ(rejected_open.status, interface::OperationStatus::ProtocolError);
        EXPECT_EQ(context.wait_for_message(interface::Timeout{std::chrono::seconds{1}}),
                  interface::MessageStatus::TimedOut);
        EXPECT_EQ(context.wait_for_message(), interface::MessageStatus::Cancelled);

        EXPECT_EQ(context.accept_call().status, interface::OperationStatus::Cancelled);
    }

    TEST(CallImplTest, ReportsContextClosedAfterSharedStateExpires)
    {
        std::unique_ptr<CallImpl> call;
        {
            auto state = std::make_shared<ContextState>(EndpointRole::Client,
                                                        std::make_unique<FakeBackgroundThread>());
            call       = std::make_unique<CallImpl>(state, 1);
        }

        EXPECT_EQ(call->receive_status(), interface::MessageStatus::ContextClosed);
        EXPECT_EQ(call->receive().status, interface::OperationStatus::ContextClosed);
        EXPECT_EQ(call->receive(interface::Timeout{std::chrono::seconds{1}}).status,
                  interface::OperationStatus::ContextClosed);
        EXPECT_EQ(call->create_message_builder(), nullptr);
        EXPECT_EQ(call->send(nullptr).status, interface::OperationStatus::ContextClosed);
    }

    TEST(CallImplTest, NoTimeoutReceiveWaitsIndefinitely)
    {
        auto background            = std::make_unique<FakeBackgroundThread>();
        const auto* background_ptr = background.get();
        ContextImpl context{
            std::make_shared<ContextState>(EndpointRole::Client, std::move(background))};
        auto opening = context.create_opening_call_message_builder();
        ASSERT_NE(opening, nullptr);
        opening->open_call("test");
        auto opened = context.open_call(std::move(opening));
        ASSERT_TRUE(opened);

        EXPECT_EQ((*opened.value)->receive().status, interface::OperationStatus::Cancelled);
        EXPECT_FALSE(background_ptr->last_deadline().has_value());
    }

    TEST(ControlStreamImplTest, ReportsContextClosedAfterStateExpires)
    {
        ControlStreamImpl stream{std::weak_ptr<ContextStateInterface>{}};

        EXPECT_EQ(stream.receive_status(), interface::MessageStatus::ContextClosed);
        EXPECT_EQ(stream.create_message_builder(), nullptr);
        EXPECT_EQ(stream.receive().status, interface::OperationStatus::ContextClosed);
        EXPECT_EQ(stream.receive(interface::Timeout{std::chrono::seconds{1}}).status,
                  interface::OperationStatus::ContextClosed);
        EXPECT_EQ(stream.send(nullptr).status, interface::OperationStatus::ContextClosed);
    }

    TEST(ContextImplTest, RetainsBuilderOwnershipWhenBackgroundRejectsCallMessage)
    {
        auto background      = std::make_unique<FakeBackgroundThread>();
        auto* background_ptr = background.get();
        ContextImpl context{
            std::make_shared<ContextState>(EndpointRole::Client, std::move(background))};
        auto opening = context.create_opening_call_message_builder();
        ASSERT_NE(opening, nullptr);
        opening->open_call("test");
        auto opened = context.open_call(std::move(opening));
        ASSERT_TRUE(opened);

        auto builder = (*opened.value)->create_message_builder();
        ASSERT_NE(builder, nullptr);
        background_ptr->set_reject_outbound(true);
        auto result = (*opened.value)->send(std::move(builder));

        EXPECT_EQ(result.status, interface::OperationStatus::TransportError);
        EXPECT_NE(builder, nullptr);
    }

    TEST(ContextImplTest, RetainsOpeningBuilderWhenBackgroundRejectsIt)
    {
        auto background = std::make_unique<FakeBackgroundThread>();
        background->set_reject_outbound(true);
        ContextImpl context{
            std::make_shared<ContextState>(EndpointRole::Client, std::move(background))};
        auto opening = context.create_opening_call_message_builder();
        ASSERT_NE(opening, nullptr);
        opening->open_call("test");
        const auto result = context.open_call(std::move(opening));

        EXPECT_EQ(result.status, interface::OperationStatus::TransportError);
        EXPECT_NE(opening, nullptr);
    }

    TEST(ContextImplTest, DeliversPeerConnectionCloseOnceBeforeBecomingTerminal)
    {
        auto background = std::make_unique<FakeBackgroundThread>();
        background->push_inbound(make_control_message_view(0, true));
        const auto* background_ptr = background.get();
        ContextImpl context{
            std::make_shared<ContextState>(EndpointRole::Client, std::move(background))};

        EXPECT_EQ(context.receive_status(), interface::MessageStatus::MessageAvailable);
        EXPECT_EQ(background_ptr->outbound_size(), 1);
        auto& control_stream = context.control_stream();
        EXPECT_EQ(control_stream.receive_status(), interface::MessageStatus::MessageAvailable);
        auto close_message = control_stream.receive();
        ASSERT_TRUE(close_message);
        EXPECT_NE((*close_message.value)->close_connection(), nullptr);
        EXPECT_EQ(control_stream.receive().status, interface::OperationStatus::ContextClosed);
    }

    TEST(ContextImplTest, RejectsBuildersCreatedByAnotherContextWithTheSameStreamId)
    {
        ContextImpl first{std::make_shared<ContextState>(EndpointRole::Client,
                                                         std::make_unique<FakeBackgroundThread>())};
        ContextImpl second{std::make_shared<ContextState>(
            EndpointRole::Client, std::make_unique<FakeBackgroundThread>())};
        auto first_opening = first.create_opening_call_message_builder();
        first_opening->open_call("first");
        auto first_call     = first.open_call(std::move(first_opening));
        auto second_opening = second.create_opening_call_message_builder();
        second_opening->open_call("second");
        auto second_call = second.open_call(std::move(second_opening));
        ASSERT_TRUE(first_call);
        ASSERT_TRUE(second_call);

        auto foreign_builder = (*first_call.value)->create_message_builder();
        auto result          = (*second_call.value)->send(std::move(foreign_builder));

        EXPECT_EQ(result.status, interface::OperationStatus::ProtocolError);
        EXPECT_NE(foreign_builder, nullptr);
    }

} // namespace
} // namespace exasol::udf::v2::internal
