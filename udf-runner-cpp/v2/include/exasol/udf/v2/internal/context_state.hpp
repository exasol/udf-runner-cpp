#pragma once

#include <exasol/udf/v2/interface/context.hpp>
#include <exasol/udf/v2/interface/error_info.hpp>
#include <exasol/udf/v2/internal/background_thread_interface.hpp>
#include <exasol/udf/v2/internal/stream_registry.hpp>

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace exasol::udf::v2::internal
{

/// Context-owned state shared by handles and accessed only by the worker thread.
class ContextState final
{
public:
    ContextState(EndpointRole endpoint_role,
                 std::unique_ptr<BackgroundThreadInterface> background_thread);
    ~ContextState();
    ContextState(const ContextState&)            = delete;
    ContextState& operator=(const ContextState&) = delete;
    ContextState(ContextState&&)                 = delete;
    ContextState& operator=(ContextState&&)      = delete;

    [[nodiscard]] std::unique_ptr<interface::OpeningCallMessageBuilder>
    create_opening_call_message_builder();
    [[nodiscard]] std::unique_ptr<interface::CallMessageBuilder> create_call_message_builder(
        std::uint64_t stream_id) const;
    [[nodiscard]] std::unique_ptr<interface::ControlMessageBuilder> create_control_message_builder()
        const;

    [[nodiscard]] interface::Result<std::uint64_t> accept_call();
    [[nodiscard]] interface::Result<std::uint64_t> open_call(
        std::unique_ptr<interface::OpeningCallMessageBuilder> opening_message);

    [[nodiscard]] interface::MessageStatus receive_status();
    [[nodiscard]] bool is_new_inbound_call_available();
    [[nodiscard]] interface::MessageStatus wait_for_message(interface::Timeout timeout);
    [[nodiscard]] interface::MessageStatus call_receive_status(std::uint64_t stream_id);
    [[nodiscard]] interface::MessageStatus control_receive_status();
    [[nodiscard]] interface::Result<std::unique_ptr<interface::CallMessageView>>
    receive_call_message(std::uint64_t stream_id, interface::Timeout timeout);
    [[nodiscard]] interface::Result<std::unique_ptr<interface::ControlMessageView>>
    receive_control_message(interface::Timeout timeout);

    [[nodiscard]] interface::Result<void> send_call_message(
        std::uint64_t stream_id,
        std::unique_ptr<interface::CallMessageBuilder>&& message,
        interface::Timeout timeout);
    [[nodiscard]] interface::Result<void> send_control_message(
        std::unique_ptr<interface::ControlMessageBuilder>&& message);

    void shutdown() noexcept;
    [[nodiscard]] bool is_closed() const noexcept;

private:
    using Clock = std::chrono::steady_clock;

    [[nodiscard]] std::optional<Clock::time_point> make_deadline(interface::Timeout timeout) const;
    [[nodiscard]] bool dispatch_available_inbound_messages();
    [[nodiscard]] std::optional<BackgroundThreadResult> wait_for_background_event(
        std::optional<Clock::time_point> deadline);
    void mark_terminal(interface::OperationStatus status,
                       std::optional<interface::ErrorInfo> error = {},
                       bool request_background_shutdown          = true) noexcept;
    [[nodiscard]] interface::MessageStatus message_status_for_terminal() const noexcept;
    [[nodiscard]] interface::OperationStatus operation_status_for_terminal() const noexcept;
    [[nodiscard]] interface::ErrorInfo protocol_error(std::string message) const;

    std::unique_ptr<BackgroundThreadInterface> background_thread_;
    std::shared_ptr<const std::uint8_t> builder_owner_token_{
        std::make_shared<const std::uint8_t>(0)};
    StreamRegistry stream_registry_;
    std::optional<interface::OperationStatus> terminal_status_;
    std::optional<interface::ErrorInfo> terminal_error_;
    bool worker_close_accepted_{};
};

} // namespace exasol::udf::v2::internal
