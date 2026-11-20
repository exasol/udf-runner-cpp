#include <exasol/udf/v2/internal/context_impl.hpp>
#include <exasol/udf/v2/internal/context_state_interface.hpp>

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

    template <typename Signature>
    using NiceMockFunction = testing::NiceMock<testing::MockFunction<Signature>>;

    class MockContextState : public ContextStateInterface
    {
    public:
        [[nodiscard]] std::unique_ptr<interface::OpeningCallMessageBuilder>
        create_opening_call_message_builder() override
        {
            return create_opening_call_message_builder_mock_.Call();
        }

        [[nodiscard]] std::unique_ptr<interface::CallMessageBuilder> create_call_message_builder(
            const std::uint64_t stream_id) const override
        {
            return create_call_message_builder_mock_.Call(stream_id);
        }

        [[nodiscard]] std::unique_ptr<interface::ControlMessageBuilder>
        create_control_message_builder() const override
        {
            return create_control_message_builder_mock_.Call();
        }

        [[nodiscard]] interface::Result<std::uint64_t> accept_call() override
        {
            return accept_call_mock_.Call();
        }

        [[nodiscard]] interface::Result<std::uint64_t> open_call(
            std::unique_ptr<interface::OpeningCallMessageBuilder>&& opening_message) override
        {
            return open_call_mock_.Call(std::move(opening_message));
        }

        [[nodiscard]] interface::MessageStatus receive_status() override
        {
            return receive_status_mock_.Call();
        }

        [[nodiscard]] bool is_new_inbound_call_available() override
        {
            return is_new_inbound_call_available_mock_.Call();
        }

        [[nodiscard]] interface::MessageStatus wait_for_message(
            const interface::Timeout timeout) override
        {
            return wait_for_message_mock_.Call(timeout);
        }

        [[nodiscard]] interface::MessageStatus call_receive_status(
            const std::uint64_t stream_id) override
        {
            return call_receive_status_mock_.Call(stream_id);
        }

        [[nodiscard]] interface::MessageStatus control_receive_status() override
        {
            return control_receive_status_mock_.Call();
        }

        [[nodiscard]] interface::Result<std::unique_ptr<interface::CallMessageView>>
        receive_call_message(const std::uint64_t stream_id,
                             const interface::Timeout timeout) override
        {
            return receive_call_message_mock_.Call(stream_id, timeout);
        }

        [[nodiscard]] interface::Result<std::unique_ptr<interface::ControlMessageView>>
        receive_control_message(const interface::Timeout timeout) override
        {
            return receive_control_message_mock_.Call(timeout);
        }

        [[nodiscard]] interface::Result<void> send_call_message(
            const std::uint64_t stream_id,
            std::unique_ptr<interface::CallMessageBuilder>&& message,
            const interface::Timeout timeout) override
        {
            return send_call_message_mock_.Call(stream_id, std::move(message), timeout);
        }

        [[nodiscard]] interface::Result<void> send_control_message(
            std::unique_ptr<interface::ControlMessageBuilder>&& message) override
        {
            return send_control_message_mock_.Call(std::move(message));
        }

        [[nodiscard]] auto& create_opening_call_message_builder_mock() noexcept
        {
            return create_opening_call_message_builder_mock_;
        }

        [[nodiscard]] auto& create_call_message_builder_mock() noexcept
        {
            return create_call_message_builder_mock_;
        }

        [[nodiscard]] auto& create_control_message_builder_mock() noexcept
        {
            return create_control_message_builder_mock_;
        }

        [[nodiscard]] auto& accept_call_mock() noexcept
        {
            return accept_call_mock_;
        }

        [[nodiscard]] auto& open_call_mock() noexcept
        {
            return open_call_mock_;
        }

        [[nodiscard]] auto& receive_status_mock() noexcept
        {
            return receive_status_mock_;
        }

        [[nodiscard]] auto& is_new_inbound_call_available_mock() noexcept
        {
            return is_new_inbound_call_available_mock_;
        }

        [[nodiscard]] auto& wait_for_message_mock() noexcept
        {
            return wait_for_message_mock_;
        }

        [[nodiscard]] auto& call_receive_status_mock() noexcept
        {
            return call_receive_status_mock_;
        }

        [[nodiscard]] auto& control_receive_status_mock() noexcept
        {
            return control_receive_status_mock_;
        }

        [[nodiscard]] auto& receive_call_message_mock() noexcept
        {
            return receive_call_message_mock_;
        }

        [[nodiscard]] auto& receive_control_message_mock() noexcept
        {
            return receive_control_message_mock_;
        }

        [[nodiscard]] auto& send_call_message_mock() noexcept
        {
            return send_call_message_mock_;
        }

        [[nodiscard]] auto& send_control_message_mock() noexcept
        {
            return send_control_message_mock_;
        }

        [[nodiscard]] std::size_t shutdown_call_count() const noexcept
        {
            return shutdown_call_count_;
        }

        void shutdown() noexcept override
        {
            ++shutdown_call_count_;
        }

    private:
        NiceMockFunction<std::unique_ptr<interface::OpeningCallMessageBuilder>()>
            create_opening_call_message_builder_mock_;
        mutable NiceMockFunction<std::unique_ptr<interface::CallMessageBuilder>(std::uint64_t)>
            create_call_message_builder_mock_;
        mutable NiceMockFunction<std::unique_ptr<interface::ControlMessageBuilder>()>
            create_control_message_builder_mock_;
        NiceMockFunction<interface::Result<std::uint64_t>()> accept_call_mock_;
        NiceMockFunction<interface::Result<std::uint64_t>(
            std::unique_ptr<interface::OpeningCallMessageBuilder>&&)>
            open_call_mock_;
        NiceMockFunction<interface::MessageStatus()> receive_status_mock_;
        NiceMockFunction<bool()> is_new_inbound_call_available_mock_;
        NiceMockFunction<interface::MessageStatus(interface::Timeout)> wait_for_message_mock_;
        NiceMockFunction<interface::MessageStatus(std::uint64_t)> call_receive_status_mock_;
        NiceMockFunction<interface::MessageStatus()> control_receive_status_mock_;
        NiceMockFunction<interface::Result<std::unique_ptr<interface::CallMessageView>>(
            std::uint64_t, interface::Timeout)>
            receive_call_message_mock_;
        NiceMockFunction<interface::Result<std::unique_ptr<interface::ControlMessageView>>(
            interface::Timeout)>
            receive_control_message_mock_;
        NiceMockFunction<interface::Result<void>(
            std::uint64_t, std::unique_ptr<interface::CallMessageBuilder>&&, interface::Timeout)>
            send_call_message_mock_;
        NiceMockFunction<interface::Result<void>(
            std::unique_ptr<interface::ControlMessageBuilder>&&)>
            send_control_message_mock_;
        std::size_t shutdown_call_count_{};
    };

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
