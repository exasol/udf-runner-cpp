#pragma once

#include <exasol/udf/v2/internal/context_state_interface.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>

#include <gmock/gmock.h>

namespace exasol::udf::v2::internal::test_support
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

    [[nodiscard]] std::unique_ptr<interface::ControlMessageBuilder> create_control_message_builder()
        const override
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
    receive_call_message(const std::uint64_t stream_id, const interface::Timeout timeout) override
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
    NiceMockFunction<interface::Result<void>(std::unique_ptr<interface::ControlMessageBuilder>&&)>
        send_control_message_mock_;
    std::size_t shutdown_call_count_{};
};

} // namespace exasol::udf::v2::internal::test_support
