#pragma once

#include <exasol/udf/v2/interface/context.hpp>

#include <cstdint>
#include <memory>

namespace exasol::udf::v2::internal
{

/// Injectable boundary between Context/Call/ControlStream handles and their shared state.
class ContextStateInterface
{
public:
    virtual ~ContextStateInterface()                               = default;
    ContextStateInterface(const ContextStateInterface&)            = delete;
    ContextStateInterface& operator=(const ContextStateInterface&) = delete;
    ContextStateInterface(ContextStateInterface&&)                 = delete;
    ContextStateInterface& operator=(ContextStateInterface&&)      = delete;

    [[nodiscard]] virtual std::unique_ptr<interface::OpeningCallMessageBuilder>
    create_opening_call_message_builder() = 0;
    [[nodiscard]] virtual std::unique_ptr<interface::CallMessageBuilder>
    create_call_message_builder(std::uint64_t stream_id) const = 0;
    [[nodiscard]] virtual std::unique_ptr<interface::ControlMessageBuilder>
    create_control_message_builder() const = 0;

    [[nodiscard]] virtual interface::Result<std::uint64_t> accept_call() = 0;
    [[nodiscard]] virtual interface::Result<std::uint64_t> open_call(
        std::unique_ptr<interface::OpeningCallMessageBuilder>&& opening_message) = 0;

    [[nodiscard]] virtual interface::MessageStatus receive_status()                             = 0;
    [[nodiscard]] virtual bool is_new_inbound_call_available()                                  = 0;
    [[nodiscard]] virtual interface::MessageStatus wait_for_message(interface::Timeout timeout) = 0;
    [[nodiscard]] virtual interface::MessageStatus call_receive_status(std::uint64_t stream_id) = 0;
    [[nodiscard]] virtual interface::MessageStatus control_receive_status()                     = 0;
    [[nodiscard]] virtual interface::Result<std::unique_ptr<interface::CallMessageView>>
    receive_call_message(std::uint64_t stream_id, interface::Timeout timeout) = 0;
    [[nodiscard]] virtual interface::Result<std::unique_ptr<interface::ControlMessageView>>
    receive_control_message(interface::Timeout timeout) = 0;

    [[nodiscard]] virtual interface::Result<void> send_call_message(
        std::uint64_t stream_id,
        std::unique_ptr<interface::CallMessageBuilder>&& message,
        interface::Timeout timeout) = 0;
    [[nodiscard]] virtual interface::Result<void> send_control_message(
        std::unique_ptr<interface::ControlMessageBuilder>&& message) = 0;

    virtual void shutdown() noexcept = 0;

protected:
    ContextStateInterface() = default;
};

} // namespace exasol::udf::v2::internal
