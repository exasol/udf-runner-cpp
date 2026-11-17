#include <exasol/udf/v2/internal/context_state.hpp>

#include <exasol/udf/v2/assert.hpp>
#include <exasol/udf/v2/internal/call_message_builder_impl.hpp>
#include <exasol/udf/v2/internal/control_message_builder_impl.hpp>

#include <memory>
#include <string>
#include <utility>
#include <variant>

namespace exasol::udf::v2::internal
{

ContextState::ContextState(const EndpointRole endpoint_role,
                           std::unique_ptr<BackgroundThreadInterface> background_thread,
                           NowFunction now)
    : background_thread_(std::move(background_thread)),
      now_(std::move(now)),
      stream_registry_(endpoint_role)
{
    EXASOL_UDF_ASSERT(background_thread_ != nullptr);
    EXASOL_UDF_ASSERT(now_);
}

ContextState::~ContextState()
{
    shutdown();
}

std::unique_ptr<interface::OpeningCallMessageBuilder>
ContextState::create_opening_call_message_builder()
{
    if (is_closed())
    {
        return nullptr;
    }
    const auto stream_id = stream_registry_.reserve_local_stream_id();
    if (!stream_id.has_value())
    {
        return nullptr;
    }
    return std::make_unique<OpeningCallMessageBuilderImpl>(*stream_id, builder_owner_token_);
}

std::unique_ptr<interface::CallMessageBuilder> ContextState::create_call_message_builder(
    const std::uint64_t stream_id) const
{
    if (is_closed() ||
        stream_registry_.call_send_status(stream_id) != interface::OperationStatus::Ok)
    {
        return nullptr;
    }
    return std::make_unique<CallMessageBuilderImpl>(stream_id, builder_owner_token_);
}

std::unique_ptr<interface::ControlMessageBuilder> ContextState::create_control_message_builder()
    const
{
    if (is_closed() || worker_close_accepted_)
    {
        return nullptr;
    }
    return std::make_unique<ControlMessageBuilderImpl>(0, builder_owner_token_);
}

interface::Result<std::uint64_t> ContextState::accept_call()
{
    while (!is_closed())
    {
        if (const auto stream_id = stream_registry_.take_next_unaccepted_call();
            stream_id.has_value())
        {
            return interface::Result<std::uint64_t>::success(*stream_id);
        }
        if (!dispatch_available_inbound_messages())
        {
            break;
        }
        if (const auto stream_id = stream_registry_.take_next_unaccepted_call();
            stream_id.has_value())
        {
            return interface::Result<std::uint64_t>::success(*stream_id);
        }
        static_cast<void>(wait_for_background_event(std::nullopt));
    }
    return interface::Result<std::uint64_t>::failure(operation_status_for_terminal(),
                                                     terminal_error_);
}

interface::Result<std::uint64_t> ContextState::open_call(
    std::unique_ptr<interface::OpeningCallMessageBuilder>&& opening_message)
{
    if (is_closed())
    {
        return interface::Result<std::uint64_t>::failure(operation_status_for_terminal(),
                                                         terminal_error_);
    }
    if (opening_message == nullptr)
    {
        return interface::Result<std::uint64_t>::failure(
            interface::OperationStatus::ProtocolError,
            protocol_error("Cannot open a call with a null opening message"));
    }

    auto* implementation = dynamic_cast<OpeningCallMessageBuilderImpl*>(opening_message.get());
    if (implementation == nullptr || !implementation->belongs_to(builder_owner_token_))
    {
        return interface::Result<std::uint64_t>::failure(
            interface::OperationStatus::ProtocolError,
            protocol_error("Opening message was not created by this context"));
    }

    const auto& built_message = implementation->message();
    const auto stream_id      = built_message.stream_id();
    if (!built_message.open_call().has_value())
    {
        return interface::Result<std::uint64_t>::failure(
            interface::OperationStatus::ProtocolError,
            protocol_error("An outbound opening message must contain OpenCall"));
    }
    if (!stream_registry_.register_local_call(stream_id, built_message.is_close_call()))
    {
        return interface::Result<std::uint64_t>::failure(
            interface::OperationStatus::ProtocolError,
            protocol_error("Opening message does not use an available reserved local stream ID"));
    }

    std::unique_ptr<interface::CallMessageBuilderBase> base_builder(std::move(opening_message));
    OutboundMessage outbound{std::move(base_builder)};
    if (!background_thread_->enqueue_outbound_message(outbound))
    {
        auto rejected =
            std::move(std::get<std::unique_ptr<interface::CallMessageBuilderBase>>(outbound));
        opening_message.reset(
            dynamic_cast<interface::OpeningCallMessageBuilder*>(rejected.release()));
        stream_registry_.rollback_local_call(stream_id);
        auto error = interface::ErrorInfo("OUTBOUND_REJECTED",
                                          "Background thread rejected the opening message");
        mark_terminal(interface::OperationStatus::TransportError, error);
        return interface::Result<std::uint64_t>::failure(interface::OperationStatus::TransportError,
                                                         error);
    }
    return interface::Result<std::uint64_t>::success(stream_id);
}

interface::MessageStatus ContextState::receive_status()
{
    if (!is_closed())
    {
        static_cast<void>(dispatch_available_inbound_messages());
    }
    if (is_closed())
    {
        return message_status_for_terminal();
    }
    return stream_registry_.has_any_pending_message() ? interface::MessageStatus::MessageAvailable
                                                      : interface::MessageStatus::NoMessage;
}

bool ContextState::is_new_inbound_call_available()
{
    if (!is_closed())
    {
        static_cast<void>(dispatch_available_inbound_messages());
    }
    return !is_closed() && stream_registry_.has_unaccepted_call();
}

interface::MessageStatus ContextState::wait_for_message(const interface::Timeout timeout)
{
    const auto deadline = make_deadline(timeout);
    while (!is_closed())
    {
        if (!dispatch_available_inbound_messages())
        {
            return message_status_for_terminal();
        }
        if (stream_registry_.has_any_pending_message())
        {
            return interface::MessageStatus::MessageAvailable;
        }
        const auto event = wait_for_background_event(deadline);
        if (!event)
        {
            return message_status_for_terminal();
        }
        if (event->event == Event::DeadlineExpired)
        {
            return interface::MessageStatus::TimedOut;
        }
    }
    return message_status_for_terminal();
}

interface::MessageStatus ContextState::call_receive_status(const std::uint64_t stream_id)
{
    if (peer_close_pending_delivery_)
    {
        return interface::MessageStatus::ContextClosed;
    }
    if (!is_closed())
    {
        static_cast<void>(dispatch_available_inbound_messages());
    }
    return is_closed() ? message_status_for_terminal()
                       : stream_registry_.call_receive_status(stream_id);
}

interface::MessageStatus ContextState::control_receive_status()
{
    if (!is_closed())
    {
        static_cast<void>(dispatch_available_inbound_messages());
    }
    return is_closed() ? message_status_for_terminal() : stream_registry_.control_receive_status();
}

interface::Result<std::unique_ptr<interface::CallMessageView>> ContextState::receive_call_message(
    const std::uint64_t stream_id, const interface::Timeout timeout)
{
    const auto deadline = make_deadline(timeout);
    while (!is_closed())
    {
        if (auto message = take_call_message_or_dispatch(stream_id))
        {
            return std::move(*message);
        }
        if (is_closed())
        {
            return interface::Result<std::unique_ptr<interface::CallMessageView>>::failure(
                operation_status_for_terminal(), terminal_error_);
        }
        const auto event = wait_for_background_event(deadline);
        if (!event)
        {
            return interface::Result<std::unique_ptr<interface::CallMessageView>>::failure(
                operation_status_for_terminal(), terminal_error_);
        }
        if (event->event == Event::DeadlineExpired)
        {
            return interface::Result<std::unique_ptr<interface::CallMessageView>>::failure(
                interface::OperationStatus::TimedOut);
        }
    }
    return interface::Result<std::unique_ptr<interface::CallMessageView>>::failure(
        operation_status_for_terminal(), terminal_error_);
}

interface::Result<std::unique_ptr<interface::ControlMessageView>>
ContextState::receive_control_message(const interface::Timeout timeout)
{
    const auto deadline             = make_deadline(timeout);
    const auto take_control_message = [this]()
        -> std::optional<interface::Result<std::unique_ptr<interface::ControlMessageView>>> {
        auto message = stream_registry_.take_control_message();
        if (!message)
        {
            return std::nullopt;
        }
        if (message->close_connection() != nullptr)
        {
            peer_close_pending_delivery_ = false;
            mark_terminal(interface::OperationStatus::ContextClosed, {}, false);
        }
        return interface::Result<std::unique_ptr<interface::ControlMessageView>>::success(
            std::move(message));
    };

    while (true)
    {
        if (auto message = take_control_message())
        {
            return std::move(*message);
        }
        if (is_closed())
        {
            return interface::Result<std::unique_ptr<interface::ControlMessageView>>::failure(
                operation_status_for_terminal(), terminal_error_);
        }
        if (!dispatch_available_inbound_messages())
        {
            if (auto message = take_control_message())
            {
                return std::move(*message);
            }
            return interface::Result<std::unique_ptr<interface::ControlMessageView>>::failure(
                operation_status_for_terminal(), terminal_error_);
        }
        if (auto message = take_control_message())
        {
            return std::move(*message);
        }
        const auto event = wait_for_background_event(deadline);
        if (!event)
        {
            return interface::Result<std::unique_ptr<interface::ControlMessageView>>::failure(
                operation_status_for_terminal(), terminal_error_);
        }
        if (event->event == Event::DeadlineExpired)
        {
            return interface::Result<std::unique_ptr<interface::ControlMessageView>>::failure(
                interface::OperationStatus::TimedOut);
        }
    }
    return interface::Result<std::unique_ptr<interface::ControlMessageView>>::failure(
        operation_status_for_terminal(), terminal_error_);
}

interface::Result<void> ContextState::send_call_message(
    const std::uint64_t stream_id,
    std::unique_ptr<interface::CallMessageBuilder>&& message,
    const interface::Timeout timeout)
{
    // BackgroundThreadInterface does not yet expose per-call send credit. Until that boundary is
    // extended, the send timeout cannot be applied to flow-control waiting.
    static_cast<void>(timeout);
    if (is_closed())
    {
        return interface::Result<void>::failure(operation_status_for_terminal(), terminal_error_);
    }
    if (!dispatch_available_inbound_messages())
    {
        return interface::Result<void>::failure(operation_status_for_terminal(), terminal_error_);
    }
    if (message == nullptr)
    {
        return interface::Result<void>::failure(interface::OperationStatus::ProtocolError,
                                                protocol_error("Cannot send a null call message"));
    }
    const auto send_status = stream_registry_.call_send_status(stream_id);
    if (send_status != interface::OperationStatus::Ok)
    {
        return interface::Result<void>::failure(send_status);
    }

    auto* implementation = dynamic_cast<CallMessageBuilderImpl*>(message.get());
    if (implementation == nullptr || !implementation->belongs_to(builder_owner_token_) ||
        implementation->message().stream_id() != stream_id ||
        implementation->message().open_call().has_value())
    {
        return interface::Result<void>::failure(
            interface::OperationStatus::ProtocolError,
            protocol_error("Call message builder is invalid for this stream"));
    }
    const bool closes_call = implementation->message().is_close_call();

    std::unique_ptr<interface::CallMessageBuilderBase> base_builder(std::move(message));
    OutboundMessage outbound{std::move(base_builder)};
    if (!background_thread_->enqueue_outbound_message(outbound))
    {
        auto rejected =
            std::move(std::get<std::unique_ptr<interface::CallMessageBuilderBase>>(outbound));
        message.reset(dynamic_cast<interface::CallMessageBuilder*>(rejected.release()));
        auto error = interface::ErrorInfo("OUTBOUND_REJECTED",
                                          "Background thread rejected the call message");
        mark_terminal(interface::OperationStatus::TransportError, error);
        return interface::Result<void>::failure(interface::OperationStatus::TransportError, error);
    }

    if (closes_call)
    {
        EXASOL_UDF_ASSERT(stream_registry_.mark_local_close_accepted(stream_id));
    }
    return interface::Result<void>::success();
}

interface::Result<void> ContextState::send_control_message(
    std::unique_ptr<interface::ControlMessageBuilder>&& message)
{
    if (is_closed() || worker_close_accepted_)
    {
        return interface::Result<void>::failure(operation_status_for_terminal(), terminal_error_);
    }
    if (message == nullptr)
    {
        return interface::Result<void>::failure(
            interface::OperationStatus::ProtocolError,
            protocol_error("Cannot send a null control message"));
    }
    const auto* implementation = dynamic_cast<ControlMessageBuilderImpl*>(message.get());
    if (implementation == nullptr || !implementation->belongs_to(builder_owner_token_) ||
        implementation->message().stream_id() != 0)
    {
        return interface::Result<void>::failure(
            interface::OperationStatus::ProtocolError,
            protocol_error("Control message builder is not bound to stream zero"));
    }
    const bool closes_connection = implementation->message().is_close_connection();
    if (OutboundMessage outbound{std::move(message)};
        !background_thread_->enqueue_outbound_message(outbound))
    {
        auto rejected =
            std::move(std::get<std::unique_ptr<interface::ControlMessageBuilder>>(outbound));
        message    = std::move(rejected);
        auto error = interface::ErrorInfo("OUTBOUND_REJECTED",
                                          "Background thread rejected the control message");
        mark_terminal(interface::OperationStatus::TransportError, error);
        return interface::Result<void>::failure(interface::OperationStatus::TransportError, error);
    }
    if (closes_connection)
    {
        worker_close_accepted_ = true;
        mark_terminal(interface::OperationStatus::ContextClosed, {}, false);
    }
    return interface::Result<void>::success();
}

void ContextState::shutdown() noexcept
{
    if (!is_closed())
    {
        mark_terminal(interface::OperationStatus::ContextClosed);
    }
}

bool ContextState::is_closed() const noexcept
{
    return terminal_status_.has_value() || peer_close_pending_delivery_;
}

std::optional<ContextState::Clock::time_point> ContextState::make_deadline(
    const interface::Timeout timeout) const
{
    if (!timeout)
    {
        return std::nullopt;
    }
    return now_() + *timeout;
}

bool ContextState::dispatch_available_inbound_messages()
{
    if (is_closed())
    {
        return false;
    }
    auto messages = background_thread_->dequeue_inbound_messages();
    for (auto& message : messages)
    {
        switch (stream_registry_.dispatch(std::move(message)))
        {
            case DispatchResult::Routed:
            case DispatchResult::Discarded:
                break;
            case DispatchResult::ProtocolError:
                mark_terminal(interface::OperationStatus::ProtocolError,
                              protocol_error("Inbound message violates stream management rules"));
                return false;
            case DispatchResult::ConnectionShutdown:
            {
                peer_close_pending_delivery_ = true;
                stream_registry_.discard_call_streams();
                auto response = std::make_unique<ControlMessageBuilderImpl>(0);
                response->close_connection();
                OutboundMessage reply{std::move(response)};
                static_cast<void>(background_thread_->enqueue_outbound_message(reply));
                return false;
            }
        }
    }
    return !is_closed();
}

std::optional<interface::Result<std::unique_ptr<interface::CallMessageView>>>
ContextState::take_call_message_or_dispatch(const std::uint64_t stream_id)
{
    auto read = stream_registry_.take_call_message(stream_id);
    if (read.result == CallMessageResult::Empty)
    {
        if (!dispatch_available_inbound_messages())
        {
            return std::nullopt;
        }
        read = stream_registry_.take_call_message(stream_id);
    }
    switch (read.result)
    {
        case CallMessageResult::Message:
            return interface::Result<std::unique_ptr<interface::CallMessageView>>::success(
                std::move(read.message));
        case CallMessageResult::PeerClosed:
            return interface::Result<std::unique_ptr<interface::CallMessageView>>::failure(
                interface::OperationStatus::PeerClosed);
        case CallMessageResult::Empty:
            return std::nullopt;
    }
    return std::nullopt;
}

std::optional<BackgroundThreadResult> ContextState::wait_for_background_event(
    const std::optional<Clock::time_point> deadline)
{
    if (is_closed())
    {
        return std::nullopt;
    }
    auto result = background_thread_->wait_until(deadline);
    if (result.event == Event::ShutdownRequested)
    {
        const auto status = result.error ? interface::OperationStatus::TransportError
                                         : interface::OperationStatus::Cancelled;
        mark_terminal(status, std::move(result.error), false);
    }
    return result;
}

void ContextState::mark_terminal(const interface::OperationStatus status,
                                 std::optional<interface::ErrorInfo> error,
                                 const bool request_background_shutdown) noexcept
{
    if (terminal_status_)
    {
        return;
    }
    terminal_status_.emplace(status);
    peer_close_pending_delivery_ = false;
    if (error)
    {
        terminal_error_.emplace(std::move(*error));
    }
    stream_registry_.clear();
    if (request_background_shutdown)
    {
        background_thread_->request_shutdown();
    }
}

interface::MessageStatus ContextState::message_status_for_terminal() const noexcept
{
    if (peer_close_pending_delivery_)
    {
        return stream_registry_.control_receive_status();
    }
    if (!terminal_status_)
    {
        return interface::MessageStatus::NoMessage;
    }
    switch (*terminal_status_)
    {
        case interface::OperationStatus::TimedOut:
            return interface::MessageStatus::TimedOut;
        case interface::OperationStatus::Cancelled:
            return interface::MessageStatus::Cancelled;
        case interface::OperationStatus::PeerClosed:
            return interface::MessageStatus::PeerClosed;
        case interface::OperationStatus::ProtocolError:
        case interface::OperationStatus::TransportError:
            return interface::MessageStatus::ConnectionError;
        case interface::OperationStatus::ContextClosed:
            return interface::MessageStatus::ContextClosed;
        case interface::OperationStatus::Ok:
            return interface::MessageStatus::NoMessage;
    }
    return interface::MessageStatus::ConnectionError;
}

interface::OperationStatus ContextState::operation_status_for_terminal() const noexcept
{
    return terminal_status_.value_or(interface::OperationStatus::ContextClosed);
}

interface::ErrorInfo ContextState::protocol_error(std::string message) const
{
    return interface::ErrorInfo("PROTOCOL_ERROR", std::move(message));
}

} // namespace exasol::udf::v2::internal
