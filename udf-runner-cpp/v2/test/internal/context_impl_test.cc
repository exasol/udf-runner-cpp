#include <exasol/udf/v2/internal/call_impl.hpp>
#include <exasol/udf/v2/internal/context_impl.hpp>
#include <exasol/udf/v2/internal/context_state.hpp>

#include "stream_management_test_utils.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace exasol::udf::v2::internal
{
namespace
{

    using test_support::k_first_client_stream_id;
    using test_support::MockBackgroundThread;

    class MockContextQueries
    {
    public:
        MOCK_METHOD((std::unique_ptr<interface::OpeningCallMessageBuilder>),
                    create_opening_call_message_builder,
                    (),
                    ());
        MOCK_METHOD((std::unique_ptr<interface::CallMessageBuilder>),
                    create_call_message_builder,
                    (std::uint64_t stream_id),
                    (const));
        MOCK_METHOD((std::unique_ptr<interface::ControlMessageBuilder>),
                    create_control_message_builder,
                    (),
                    (const));
        MOCK_METHOD(interface::MessageStatus, receive_status, (), ());
        MOCK_METHOD(bool, is_new_inbound_call_available, (), ());
        MOCK_METHOD(interface::MessageStatus, wait_for_message, (interface::Timeout timeout), ());
        MOCK_METHOD(interface::MessageStatus, call_receive_status, (std::uint64_t stream_id), ());
        MOCK_METHOD(interface::MessageStatus, control_receive_status, (), ());
    };

    class MockContextCallOperations
    {
    public:
        MOCK_METHOD((interface::Result<std::uint64_t>), accept_call, (), ());
        MOCK_METHOD((interface::Result<std::uint64_t>),
                    open_call,
                    (std::unique_ptr<interface::OpeningCallMessageBuilder> && opening_message),
                    ());
        MOCK_METHOD((interface::Result<std::unique_ptr<interface::CallMessageView>>),
                    receive_call_message,
                    (std::uint64_t stream_id, interface::Timeout timeout),
                    ());
        MOCK_METHOD((interface::Result<void>),
                    send_call_message,
                    (std::uint64_t stream_id,
                     std::unique_ptr<interface::CallMessageBuilder>&& message,
                     interface::Timeout timeout),
                    ());
    };

    class MockContextControlOperations
    {
    public:
        MOCK_METHOD((interface::Result<std::unique_ptr<interface::ControlMessageView>>),
                    receive_control_message,
                    (interface::Timeout timeout),
                    ());
        MOCK_METHOD((interface::Result<void>),
                    send_control_message,
                    (std::unique_ptr<interface::ControlMessageBuilder> && message),
                    ());
    };

    class MockContextState final : public ContextStateInterface
    {
    public:
        [[nodiscard]] auto& queries() noexcept
        {
            return queries_;
        }

        [[nodiscard]] auto& call_operations() noexcept
        {
            return call_operations_;
        }

        [[nodiscard]] auto& control_operations() noexcept
        {
            return control_operations_;
        }

        std::unique_ptr<interface::OpeningCallMessageBuilder> create_opening_call_message_builder()
            override
        {
            return queries_.create_opening_call_message_builder();
        }
        std::unique_ptr<interface::CallMessageBuilder> create_call_message_builder(
            const std::uint64_t stream_id) const override
        {
            return queries_.create_call_message_builder(stream_id);
        }
        std::unique_ptr<interface::ControlMessageBuilder> create_control_message_builder()
            const override
        {
            return queries_.create_control_message_builder();
        }
        interface::Result<std::uint64_t> accept_call() override
        {
            return call_operations_.accept_call();
        }
        interface::Result<std::uint64_t> open_call(
            std::unique_ptr<interface::OpeningCallMessageBuilder>&& opening_message) override
        {
            return call_operations_.open_call(std::move(opening_message));
        }
        interface::MessageStatus receive_status() override
        {
            return queries_.receive_status();
        }
        bool is_new_inbound_call_available() override
        {
            return queries_.is_new_inbound_call_available();
        }
        interface::MessageStatus wait_for_message(const interface::Timeout timeout) override
        {
            return queries_.wait_for_message(timeout);
        }
        interface::MessageStatus call_receive_status(const std::uint64_t stream_id) override
        {
            return queries_.call_receive_status(stream_id);
        }
        interface::MessageStatus control_receive_status() override
        {
            return queries_.control_receive_status();
        }
        interface::Result<std::unique_ptr<interface::CallMessageView>> receive_call_message(
            const std::uint64_t stream_id, const interface::Timeout timeout) override
        {
            return call_operations_.receive_call_message(stream_id, timeout);
        }
        interface::Result<std::unique_ptr<interface::ControlMessageView>> receive_control_message(
            const interface::Timeout timeout) override
        {
            return control_operations_.receive_control_message(timeout);
        }
        interface::Result<void> send_call_message(
            const std::uint64_t stream_id,
            std::unique_ptr<interface::CallMessageBuilder>&& message,
            const interface::Timeout timeout) override
        {
            return call_operations_.send_call_message(stream_id, std::move(message), timeout);
        }
        interface::Result<void> send_control_message(
            std::unique_ptr<interface::ControlMessageBuilder>&& message) override
        {
            return control_operations_.send_control_message(std::move(message));
        }
        void shutdown() noexcept override
        {
            ++shutdown_call_count_;
        }

        [[nodiscard]] std::size_t shutdown_call_count() const noexcept
        {
            return shutdown_call_count_;
        }

    private:
        testing::StrictMock<MockContextQueries> queries_;
        testing::StrictMock<MockContextCallOperations> call_operations_;
        testing::StrictMock<MockContextControlOperations> control_operations_;
        std::size_t shutdown_call_count_{};
    };

    TEST(ContextImplTest, DelegatesQueriesWaitsAndBuilderCreation)
    {
        auto state = std::make_shared<MockContextState>();
        const interface::Timeout timeout{std::chrono::seconds{1}};
        EXPECT_CALL(state->queries(), receive_status())
            .WillOnce(testing::Return(interface::MessageStatus::NoMessage));
        EXPECT_CALL(state->queries(), is_new_inbound_call_available())
            .WillOnce(testing::Return(true));
        EXPECT_CALL(state->queries(), wait_for_message(interface::Timeout{}))
            .WillOnce(testing::Return(interface::MessageStatus::Cancelled));
        EXPECT_CALL(state->queries(), wait_for_message(timeout))
            .WillOnce(testing::Return(interface::MessageStatus::TimedOut));
        EXPECT_CALL(state->queries(), create_opening_call_message_builder()).WillOnce([] {
            return std::unique_ptr<interface::OpeningCallMessageBuilder>{};
        });

        ContextImpl context{state};
        EXPECT_EQ(context.receive_status(), interface::MessageStatus::NoMessage);
        EXPECT_TRUE(context.is_new_inbound_call_available());
        EXPECT_EQ(context.wait_for_message(), interface::MessageStatus::Cancelled);
        EXPECT_EQ(context.wait_for_message(timeout), interface::MessageStatus::TimedOut);
        EXPECT_EQ(context.create_opening_call_message_builder(), nullptr);
    }

    TEST(ContextImplTest, AcceptCallCreatesHandleBoundToAcceptedStream)
    {
        auto state = std::make_shared<MockContextState>();
        EXPECT_CALL(state->call_operations(), accept_call()).WillOnce([] {
            return interface::Result<std::uint64_t>::success(
                test_support::k_first_server_stream_id);
        });
        EXPECT_CALL(state->queries(), call_receive_status(test_support::k_first_server_stream_id))
            .WillOnce(testing::Return(interface::MessageStatus::MessageAvailable));
        EXPECT_CALL(state->queries(),
                    create_call_message_builder(test_support::k_first_server_stream_id))
            .WillOnce([] { return std::unique_ptr<interface::CallMessageBuilder>{}; });

        ContextImpl context{state};
        auto result = context.accept_call();
        ASSERT_TRUE(result);
        EXPECT_EQ((*result.value)->receive_status(), interface::MessageStatus::MessageAvailable);
        EXPECT_EQ((*result.value)->create_message_builder(), nullptr);
    }

    TEST(ContextImplTest, PropagatesFailedAcceptAndOpenResults)
    {
        auto state = std::make_shared<MockContextState>();
        EXPECT_CALL(state->call_operations(), accept_call()).WillOnce([] {
            return interface::Result<std::uint64_t>::failure(interface::OperationStatus::Cancelled);
        });
        EXPECT_CALL(state->call_operations(), open_call(testing::_))
            .WillOnce([](std::unique_ptr<interface::OpeningCallMessageBuilder> opening_message) {
                EXPECT_EQ(opening_message, nullptr);
                return interface::Result<std::uint64_t>::failure(
                    interface::OperationStatus::ProtocolError);
            });

        ContextImpl context{state};
        EXPECT_EQ(context.accept_call().status, interface::OperationStatus::Cancelled);
        EXPECT_EQ(context.open_call(nullptr).status, interface::OperationStatus::ProtocolError);
    }

    TEST(ContextImplTest, OpenCallCreatesHandleBoundToStreamReturnedByState)
    {
        auto state = std::make_shared<MockContextState>();
        EXPECT_CALL(state->call_operations(), open_call(testing::_))
            .WillOnce([](std::unique_ptr<interface::OpeningCallMessageBuilder> opening_message) {
                EXPECT_EQ(opening_message, nullptr);
                return interface::Result<std::uint64_t>::success(
                    test_support::k_first_client_stream_id);
            });
        EXPECT_CALL(state->queries(), call_receive_status(test_support::k_first_client_stream_id))
            .WillOnce(testing::Return(interface::MessageStatus::NoMessage));

        ContextImpl context{state};
        auto result = context.open_call(nullptr);
        ASSERT_TRUE(result);
        EXPECT_EQ((*result.value)->receive_status(), interface::MessageStatus::NoMessage);
    }

    TEST(ContextImplTest, ControlStreamUsesSharedState)
    {
        auto state = std::make_shared<MockContextState>();
        EXPECT_CALL(state->queries(), control_receive_status())
            .WillOnce(testing::Return(interface::MessageStatus::MessageAvailable));
        EXPECT_CALL(state->queries(), create_control_message_builder()).WillOnce([] {
            return std::unique_ptr<interface::ControlMessageBuilder>{};
        });

        ContextImpl context{state};
        auto& control_stream = context.control_stream();
        EXPECT_EQ(&context.control_stream(), &control_stream);
        EXPECT_EQ(control_stream.receive_status(), interface::MessageStatus::MessageAvailable);
        EXPECT_EQ(control_stream.create_message_builder(), nullptr);
    }

    TEST(ContextImplTest, DestructorShutsDownSharedState)
    {
        auto state = std::make_shared<MockContextState>();
        {
            ContextImpl context{state};
        }
        EXPECT_EQ(state->shutdown_call_count(), 1);
    }

    TEST(ContextHandleIntegrationTest, CallSendOverloadsAcceptMessagesAndZeroTimeoutReceivePolls)
    {
        auto background      = std::make_unique<testing::StrictMock<MockBackgroundThread>>();
        auto* background_ptr = background.get();
        EXPECT_CALL(*background_ptr, enqueue_outbound_message(testing::_))
            .Times(4)
            .WillRepeatedly([background_ptr](OutboundMessage& message) {
                return background_ptr->accept_outbound_message(message);
            });
        EXPECT_CALL(*background_ptr, dequeue_inbound_messages()).Times(6).WillRepeatedly([] {
            return InboundBatch{};
        });
        EXPECT_CALL(*background_ptr, wait_until(testing::_))
            .Times(2)
            .WillRepeatedly([](const auto deadline) {
                EXPECT_TRUE(deadline.has_value());
                return BackgroundThreadResult{.event = Event::DeadlineExpired};
            });
        ContextImpl context{
            std::make_shared<ContextState>(EndpointRole::Client, std::move(background))};
        auto opening = context.create_opening_call_message_builder();
        ASSERT_NE(opening, nullptr);
        opening->open_call("test");
        auto opened = context.open_call(std::move(opening));
        ASSERT_TRUE(opened);

        auto no_wait_builder = (*opened.value)->create_message_builder();
        ASSERT_NE(no_wait_builder, nullptr);
        EXPECT_TRUE((*opened.value)->send(std::move(no_wait_builder)));

        auto zero_timeout_builder = (*opened.value)->create_message_builder();
        ASSERT_NE(zero_timeout_builder, nullptr);
        EXPECT_TRUE((*opened.value)
                        ->send(std::move(zero_timeout_builder),
                               interface::Timeout{std::chrono::steady_clock::duration::zero()}));

        EXPECT_EQ((*opened.value)
                      ->receive(interface::Timeout{std::chrono::steady_clock::duration::zero()})
                      .status,
                  interface::OperationStatus::TimedOut);
        EXPECT_EQ((*opened.value)->receive_status(), interface::MessageStatus::NoMessage);
        auto populated_builder = (*opened.value)->create_message_builder();
        ASSERT_NE(populated_builder, nullptr);
        populated_builder->add_payload("call", "value");
        EXPECT_TRUE((*opened.value)->send(std::move(populated_builder)));
        EXPECT_EQ((*opened.value)
                      ->receive(interface::Timeout{std::chrono::steady_clock::duration::zero()})
                      .status,
                  interface::OperationStatus::TimedOut);
    }

    TEST(ContextHandleIntegrationTest, ControlStreamSendsAndTimesOutWhenNoMessageIsAvailable)
    {
        auto background      = std::make_unique<testing::StrictMock<MockBackgroundThread>>();
        auto* background_ptr = background.get();
        EXPECT_CALL(*background_ptr, dequeue_inbound_messages()).WillOnce([] {
            return InboundBatch{};
        });
        EXPECT_CALL(*background_ptr, wait_until(testing::_)).WillOnce([](const auto deadline) {
            EXPECT_TRUE(deadline.has_value());
            return BackgroundThreadResult{.event = Event::DeadlineExpired};
        });
        EXPECT_CALL(*background_ptr, enqueue_outbound_message(testing::_))
            .WillOnce([background_ptr](OutboundMessage& message) {
                return background_ptr->accept_outbound_message(message);
            });
        ContextImpl context{
            std::make_shared<ContextState>(EndpointRole::Client, std::move(background))};

        EXPECT_EQ(context.control_stream().receive().status, interface::OperationStatus::TimedOut);
        auto control_builder = context.control_stream().create_message_builder();
        ASSERT_NE(control_builder, nullptr);
        control_builder->keep_alive();
        EXPECT_TRUE(context.control_stream().send(std::move(control_builder)));
    }

    TEST(CallImplTest, ReportsContextClosedAfterSharedStateExpires)
    {
        std::unique_ptr<CallImpl> call;
        {
            auto state = std::make_shared<ContextState>(
                EndpointRole::Client, std::make_unique<testing::NiceMock<MockBackgroundThread>>());
            call = std::make_unique<CallImpl>(state, k_first_client_stream_id);
        }

        EXPECT_EQ(call->receive_status(), interface::MessageStatus::ContextClosed);
        EXPECT_EQ(call->receive().status, interface::OperationStatus::ContextClosed);
        EXPECT_EQ(call->receive(interface::Timeout{std::chrono::seconds{1}}).status,
                  interface::OperationStatus::ContextClosed);
        EXPECT_EQ(call->create_message_builder(), nullptr);
        EXPECT_EQ(call->send(nullptr).status, interface::OperationStatus::ContextClosed);
    }

    TEST(CallImplTest, NoTimeoutReceivePassesNoDeadlineToBackgroundThread)
    {
        auto background      = std::make_unique<testing::StrictMock<MockBackgroundThread>>();
        auto* background_ptr = background.get();
        EXPECT_CALL(*background_ptr, enqueue_outbound_message(testing::_))
            .WillOnce([background_ptr](OutboundMessage& message) {
                return background_ptr->accept_outbound_message(message);
            });
        EXPECT_CALL(*background_ptr, dequeue_inbound_messages()).WillOnce([] {
            return InboundBatch{};
        });
        EXPECT_CALL(*background_ptr,
                    wait_until(testing::Eq(std::optional<std::chrono::steady_clock::time_point>{})))
            .WillOnce([] { return BackgroundThreadResult{.event = Event::ShutdownRequested}; });
        ContextImpl context{
            std::make_shared<ContextState>(EndpointRole::Client, std::move(background))};
        auto opening = context.create_opening_call_message_builder();
        ASSERT_NE(opening, nullptr);
        opening->open_call("test");
        auto opened = context.open_call(std::move(opening));
        ASSERT_TRUE(opened);

        EXPECT_EQ((*opened.value)->receive().status, interface::OperationStatus::Cancelled);
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

} // namespace
} // namespace exasol::udf::v2::internal
