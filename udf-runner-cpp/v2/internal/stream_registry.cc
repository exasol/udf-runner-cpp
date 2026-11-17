#include <exasol/udf/v2/internal/stream_registry.hpp>

#include <limits>
#include <utility>
#include <variant>

namespace exasol::udf::v2::internal
{

StreamRegistry::StreamRegistry(const EndpointRole endpoint_role)
    : endpoint_role_(endpoint_role),
      next_local_stream_id_(endpoint_role == EndpointRole::Client ? 1 : 2)
{
}

std::optional<std::uint64_t> StreamRegistry::reserve_local_stream_id()
{
    if (!next_local_stream_id_.has_value())
    {
        return std::nullopt;
    }

    const auto reserved_id = *next_local_stream_id_;
    local_high_water_mark_ = reserved_id;
    if (reserved_id > std::numeric_limits<std::uint64_t>::max() - 2)
    {
        next_local_stream_id_.reset();
    }
    else
    {
        *next_local_stream_id_ += 2;
    }
    return reserved_id;
}

bool StreamRegistry::register_local_call(const std::uint64_t stream_id,
                                         const bool local_close_accepted)
{
    if (stream_id == 0 || stream_id > local_high_water_mark_ ||
        !has_local_endpoint_parity(stream_id) || active_calls_.contains(stream_id))
    {
        return false;
    }

    active_calls_.try_emplace(stream_id, CallState{.local_close_accepted = local_close_accepted});
    return true;
}

void StreamRegistry::rollback_local_call(const std::uint64_t stream_id) noexcept
{
    const auto call = active_calls_.find(stream_id);
    if (call != active_calls_.end())
    {
        pending_call_message_count_ -= call->second.pending_messages.size();
        active_calls_.erase(call);
    }
}

DispatchResult StreamRegistry::dispatch(InboundMessage message)
{
    if (auto* control_message =
            std::get_if<std::unique_ptr<interface::ControlMessageView>>(&message))
    {
        return dispatch_control_message(std::move(*control_message));
    }

    if (auto* call_message = std::get_if<std::unique_ptr<interface::CallMessageView>>(&message))
    {
        return dispatch_call_message(std::move(*call_message));
    }

    return DispatchResult::ProtocolError;
}

DispatchResult StreamRegistry::dispatch_control_message(
    std::unique_ptr<interface::ControlMessageView> message)
{
    using enum DispatchResult;

    if (message == nullptr || message->stream_id() != 0)
    {
        return ProtocolError;
    }
    if (message->close_connection() != nullptr)
    {
        control_pending_.push_back(std::move(message));
        return ConnectionShutdown;
    }
    control_pending_.push_back(std::move(message));
    return Routed;
}

DispatchResult StreamRegistry::dispatch_call_message(
    std::unique_ptr<interface::CallMessageView> message)
{
    if (message == nullptr)
    {
        return DispatchResult::ProtocolError;
    }

    const auto stream_id = message->stream_id();
    if (stream_id == 0)
    {
        return DispatchResult::ProtocolError;
    }

    if (const auto active_call = active_calls_.find(stream_id); active_call != active_calls_.end())
    {
        return dispatch_call_message_for_existing_call(active_call->second, std::move(message));
    }
    return dispatch_call_message_for_new_call(std::move(message));
}

DispatchResult StreamRegistry::dispatch_call_message_for_existing_call(
    CallState& call_state, std::unique_ptr<interface::CallMessageView> message)
{
    if (call_state.peer_close_received)
    {
        return DispatchResult::Discarded;
    }
    if (message->open_call() != nullptr)
    {
        return DispatchResult::ProtocolError;
    }

    const bool is_closing = message->close_call() != nullptr;
    call_state.pending_messages.push_back(std::move(message));
    ++pending_call_message_count_;
    if (is_closing)
    {
        call_state.peer_close_received = true;
    }
    return DispatchResult::Routed;
}

DispatchResult StreamRegistry::dispatch_call_message_for_new_call(
    std::unique_ptr<interface::CallMessageView> message)
{
    using enum DispatchResult;

    const auto stream_id  = message->stream_id();
    const bool is_opening = message->open_call() != nullptr;
    const bool is_closing = message->close_call() != nullptr;

    if (has_local_endpoint_parity(stream_id))
    {
        // Either endpoint can close a call, so old local-owned streams can
        // also receive delayed peer traffic.
        if (is_opening || stream_id > local_high_water_mark_)
        {
            return ProtocolError;
        }
        return Discarded;
    }

    if (stream_id <= peer_high_water_mark_)
    {
        return is_opening ? ProtocolError : Discarded;
    }
    if (!is_opening)
    {
        return ProtocolError;
    }

    peer_high_water_mark_ = stream_id;
    CallState state;
    state.peer_close_received = is_closing;
    state.pending_messages.push_back(std::move(message));
    active_calls_.try_emplace(stream_id, std::move(state));
    ++pending_call_message_count_;
    unaccepted_call_ids_.push_back(stream_id);
    return Routed;
}

bool StreamRegistry::has_unaccepted_call() const noexcept
{
    return !unaccepted_call_ids_.empty();
}

std::optional<std::uint64_t> StreamRegistry::take_next_unaccepted_call()
{
    if (unaccepted_call_ids_.empty())
    {
        return std::nullopt;
    }
    const auto stream_id = unaccepted_call_ids_.front();
    unaccepted_call_ids_.pop_front();
    return stream_id;
}

bool StreamRegistry::has_any_pending_message() const noexcept
{
    return !control_pending_.empty() || pending_call_message_count_ != 0;
}

interface::MessageStatus StreamRegistry::call_receive_status(
    const std::uint64_t stream_id) const noexcept
{
    using enum interface::MessageStatus;

    const auto call = active_calls_.find(stream_id);
    if (call == active_calls_.end())
    {
        return PeerClosed;
    }
    return call->second.pending_messages.empty() ? NoMessage : MessageAvailable;
}

interface::MessageStatus StreamRegistry::control_receive_status() const noexcept
{
    return control_pending_.empty() ? interface::MessageStatus::NoMessage
                                    : interface::MessageStatus::MessageAvailable;
}

CallMessageRead StreamRegistry::take_call_message(const std::uint64_t stream_id)
{
    const auto call = active_calls_.find(stream_id);
    if (call == active_calls_.end())
    {
        return {.result = CallMessageResult::PeerClosed};
    }
    if (call->second.pending_messages.empty())
    {
        return {.result = CallMessageResult::Empty};
    }

    auto message = std::move(call->second.pending_messages.front());
    call->second.pending_messages.pop_front();
    --pending_call_message_count_;
    if (const bool is_closing = message->close_call() != nullptr; is_closing)
    {
        active_calls_.erase(call);
    }
    return {.result = CallMessageResult::Message, .message = std::move(message)};
}

std::unique_ptr<interface::ControlMessageView> StreamRegistry::take_control_message()
{
    if (control_pending_.empty())
    {
        return nullptr;
    }
    auto message = std::move(control_pending_.front());
    control_pending_.pop_front();
    return message;
}

interface::OperationStatus StreamRegistry::call_send_status(
    const std::uint64_t stream_id) const noexcept
{
    using enum interface::OperationStatus;

    const auto call = active_calls_.find(stream_id);
    if (call == active_calls_.end() || call->second.peer_close_received)
    {
        return PeerClosed;
    }
    return call->second.local_close_accepted ? ProtocolError : Ok;
}

bool StreamRegistry::mark_local_close_accepted(const std::uint64_t stream_id) noexcept
{
    const auto call = active_calls_.find(stream_id);
    if (call == active_calls_.end() || call->second.local_close_accepted ||
        call->second.peer_close_received)
    {
        return false;
    }
    call->second.local_close_accepted = true;
    return true;
}

void StreamRegistry::discard_call_streams() noexcept
{
    active_calls_.clear();
    unaccepted_call_ids_.clear();
    pending_call_message_count_ = 0;
}

void StreamRegistry::clear() noexcept
{
    discard_call_streams();
    control_pending_.clear();
}

EndpointRole StreamRegistry::endpoint_role() const noexcept
{
    return endpoint_role_;
}

std::uint64_t StreamRegistry::local_high_water_mark() const noexcept
{
    return local_high_water_mark_;
}

std::uint64_t StreamRegistry::peer_high_water_mark() const noexcept
{
    return peer_high_water_mark_;
}

bool StreamRegistry::has_local_endpoint_parity(const std::uint64_t stream_id) const noexcept
{
    return endpoint_role_ == EndpointRole::Client ? stream_id % 2 == 1 : stream_id % 2 == 0;
}

} // namespace exasol::udf::v2::internal
