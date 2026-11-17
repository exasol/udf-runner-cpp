#pragma once

#include <exasol/udf/v2/interface/context.hpp>
#include <exasol/udf/v2/interface/error_info.hpp>
#include <exasol/udf/v2/internal/background_thread_interface.hpp>
#include <exasol/udf/v2/internal/builder_owner_token.hpp>
#include <exasol/udf/v2/internal/context_state_interface.hpp>
#include <exasol/udf/v2/internal/stream_registry.hpp>

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace exasol::udf::v2::internal
{

/// Context-owned state shared by handles and accessed only by the worker thread.
class ContextState final : public ContextStateInterface
{
public:
    using NowFunction = std::function<std::chrono::steady_clock::time_point()>;

    ContextState(
        EndpointRole endpoint_role,
        std::unique_ptr<BackgroundThreadInterface> background_thread,
        NowFunction now = [] { return std::chrono::steady_clock::now(); });
    ~ContextState();
    ContextState(const ContextState&)            = delete;
    ContextState& operator=(const ContextState&) = delete;
    ContextState(ContextState&&)                 = delete;
    ContextState& operator=(ContextState&&)      = delete;

    [[nodiscard]] std::unique_ptr<interface::OpeningCallMessageBuilder>
    create_opening_call_message_builder() override;
    [[nodiscard]] std::unique_ptr<interface::CallMessageBuilder> create_call_message_builder(
        std::uint64_t stream_id) const override;
    [[nodiscard]] std::unique_ptr<interface::ControlMessageBuilder> create_control_message_builder()
        const override;

    [[nodiscard]] interface::Result<std::uint64_t> accept_call() override;
    [[nodiscard]] interface::Result<std::uint64_t> open_call(
        std::unique_ptr<interface::OpeningCallMessageBuilder>&& opening_message) override;

    [[nodiscard]] interface::MessageStatus receive_status() override;
    [[nodiscard]] bool is_new_inbound_call_available() override;
    [[nodiscard]] interface::MessageStatus wait_for_message(interface::Timeout timeout) override;
    [[nodiscard]] interface::MessageStatus call_receive_status(std::uint64_t stream_id) override;
    [[nodiscard]] interface::MessageStatus control_receive_status() override;
    [[nodiscard]] interface::Result<std::unique_ptr<interface::CallMessageView>>
    receive_call_message(std::uint64_t stream_id, interface::Timeout timeout) override;
    [[nodiscard]] interface::Result<std::unique_ptr<interface::ControlMessageView>>
    receive_control_message(interface::Timeout timeout) override;

    [[nodiscard]] interface::Result<void> send_call_message(
        std::uint64_t stream_id,
        std::unique_ptr<interface::CallMessageBuilder>&& message,
        interface::Timeout timeout) override;
    [[nodiscard]] interface::Result<void> send_control_message(
        std::unique_ptr<interface::ControlMessageBuilder>&& message) override;

    void shutdown() noexcept override;
    [[nodiscard]] bool is_closed() const noexcept;

private:
    using Clock = std::chrono::steady_clock;

    [[nodiscard]] std::optional<Clock::time_point> make_deadline(interface::Timeout timeout) const;
    [[nodiscard]] bool dispatch_available_inbound_messages();
    [[nodiscard]] std::optional<interface::Result<std::unique_ptr<interface::CallMessageView>>>
    take_call_message_or_dispatch(std::uint64_t stream_id);
    [[nodiscard]] std::optional<BackgroundThreadResult> wait_for_background_event(
        std::optional<Clock::time_point> deadline);
    void mark_terminal(interface::OperationStatus status,
                       std::optional<interface::ErrorInfo> error = {},
                       bool request_background_shutdown          = true) noexcept;
    [[nodiscard]] interface::MessageStatus message_status_for_terminal() const noexcept;
    [[nodiscard]] interface::OperationStatus operation_status_for_terminal() const noexcept;
    [[nodiscard]] interface::ErrorInfo protocol_error(std::string message) const;

    std::unique_ptr<BackgroundThreadInterface> background_thread_;
    NowFunction now_;
    std::shared_ptr<const BuilderOwnerToken> builder_owner_token_{
        std::make_shared<const BuilderOwnerToken>()};
    StreamRegistry stream_registry_;
    std::optional<interface::OperationStatus> terminal_status_;
    std::optional<interface::ErrorInfo> terminal_error_;
    bool worker_close_accepted_{};
    bool peer_close_pending_delivery_{};
};

} // namespace exasol::udf::v2::internal
