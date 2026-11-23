#include <exasol/udf/v2/internal/context_impl.hpp>

#include "context_state_mock.hpp"
#include "stream_management_test_utils.hpp"

#include <chrono>
#include <cstdint>
#include <memory>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace exasol::udf::v2::internal
{
namespace
{

    using test_support::MockContextState;

    TEST(ContextImplTest, DelegatesQueriesWaitsAndBuilderCreation)
    {
        auto state = std::make_shared<MockContextState>();
        const interface::Timeout timeout{std::chrono::seconds{1}};
        EXPECT_CALL(state->receive_status_mock(), Call())
            .WillOnce(testing::Return(interface::MessageStatus::NoMessage));
        EXPECT_CALL(state->is_new_inbound_call_available_mock(), Call())
            .WillOnce(testing::Return(true));
        EXPECT_CALL(state->wait_for_message_mock(), Call(interface::Timeout{}))
            .WillOnce(testing::Return(interface::MessageStatus::Cancelled));
        EXPECT_CALL(state->wait_for_message_mock(), Call(timeout))
            .WillOnce(testing::Return(interface::MessageStatus::TimedOut));
        EXPECT_CALL(state->create_opening_call_message_builder_mock(), Call()).WillOnce([] {
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
        EXPECT_CALL(state->accept_call_mock(), Call()).WillOnce([] {
            return interface::Result<std::uint64_t>::success(
                test_support::k_first_server_stream_id);
        });
        EXPECT_CALL(state->call_receive_status_mock(), Call(test_support::k_first_server_stream_id))
            .WillOnce(testing::Return(interface::MessageStatus::MessageAvailable));
        EXPECT_CALL(state->create_call_message_builder_mock(),
                    Call(test_support::k_first_server_stream_id))
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
        EXPECT_CALL(state->accept_call_mock(), Call()).WillOnce([] {
            return interface::Result<std::uint64_t>::failure(interface::OperationStatus::Cancelled);
        });
        EXPECT_CALL(state->open_call_mock(), Call(testing::_))
            .WillOnce([](std::unique_ptr<interface::OpeningCallMessageBuilder> opening_message) {
                EXPECT_EQ(opening_message, nullptr);
                return interface::Result<std::uint64_t>::failure(
                    interface::OperationStatus::InvalidArgument);
            });

        ContextImpl context{state};
        EXPECT_EQ(context.accept_call().status, interface::OperationStatus::Cancelled);
        EXPECT_EQ(context.open_call(nullptr).status, interface::OperationStatus::InvalidArgument);
    }

    TEST(ContextImplTest, OpenCallCreatesHandleBoundToStreamReturnedByState)
    {
        auto state = std::make_shared<MockContextState>();
        EXPECT_CALL(state->open_call_mock(), Call(testing::_))
            .WillOnce([](std::unique_ptr<interface::OpeningCallMessageBuilder> opening_message) {
                EXPECT_EQ(opening_message, nullptr);
                return interface::Result<std::uint64_t>::success(
                    test_support::k_first_client_stream_id);
            });
        EXPECT_CALL(state->call_receive_status_mock(), Call(test_support::k_first_client_stream_id))
            .WillOnce(testing::Return(interface::MessageStatus::NoMessage));

        ContextImpl context{state};
        auto result = context.open_call(nullptr);
        ASSERT_TRUE(result);
        EXPECT_EQ((*result.value)->receive_status(), interface::MessageStatus::NoMessage);
    }

    TEST(ContextImplTest, ControlStreamUsesSharedState)
    {
        auto state = std::make_shared<MockContextState>();
        EXPECT_CALL(state->control_receive_status_mock(), Call())
            .WillOnce(testing::Return(interface::MessageStatus::MessageAvailable));
        EXPECT_CALL(state->create_control_message_builder_mock(), Call()).WillOnce([] {
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

} // namespace
} // namespace exasol::udf::v2::internal
