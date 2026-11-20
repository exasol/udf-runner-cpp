#include <exasol/udf/v2/internal/context_impl.hpp>
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

    using test_support::MockBackgroundThread;

    TEST(ContextHandleIntegrationTest, CallSendOverloadsAcceptMessagesAndZeroTimeoutReceivePolls)
    {
        auto background      = std::make_unique<testing::NiceMock<MockBackgroundThread>>();
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
        auto background      = std::make_unique<testing::NiceMock<MockBackgroundThread>>();
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

    TEST(CallImplIntegrationTest, NoTimeoutReceiveWaitsWithoutADeadline)
    {
        auto background      = std::make_unique<testing::NiceMock<MockBackgroundThread>>();
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

} // namespace
} // namespace exasol::udf::v2::internal
