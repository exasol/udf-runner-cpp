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
    if (local_id_space_exhausted_)
    {
        return std::nullopt;
    }

    const auto reserved_id = next_local_stream_id_;
    local_high_water_mark_ = reserved_id;
    if (reserved_id > std::numeric_limits<std::uint64_t>::max() - 2)
    {
        local_id_space_exhausted_ = true;
    }
    else
    {
        next_local_stream_id_ += 2;
    }
    return reserved_id;
}

bool StreamRegistry::register_local_call(const std::uint64_t stream_id,
                                         const bool local_close_accepted)
{
    if (stream_id == 0 || stream_id > local_high_water_mark_ ||
        !has_local_endpoint_parity(stream_id) || active_calls_.contains(stream_id) ||
        !opened_local_stream_ids_.insert(stream_id).second)
    {
        return false;
    }

    active_calls_.emplace(stream_id, CallState{.local_close_accepted = local_close_accepted});
    return true;
}

void StreamRegistry::rollback_local_call(const std::uint64_t stream_id) noexcept
{
    active_calls_.erase(stream_id);
}

DispatchResult StreamRegistry::dispatch(InboundMessage&& message)
{
    if (auto* control_message =
            std::get_if<std::unique_ptr<interface::ControlMessageView>>(&message))
    {
        if (*control_message == nullptr || (*control_message)->stream_id() != 0)
        {
            return DispatchResult::ProtocolError;
        }
        if ((*control_message)->close_connection() != nullptr)
        {
            return DispatchResult::ConnectionShutdown;
        }
        control_pending_.push_back(std::move(*control_message));
        return DispatchResult::Routed;
    }

    auto* call_message = std::get_if<std::unique_ptr<interface::CallMessageView>>(&message);
    if (call_message == nullptr || *call_message == nullptr)
    {
        return DispatchResult::ProtocolError;
    }

    const auto stream_id = (*call_message)->stream_id();
    if (stream_id == 0)
    {
        return DispatchResult::ProtocolError;
    }

    const bool is_opening  = (*call_message)->open_call() != nullptr;
    const bool is_closing  = (*call_message)->close_call() != nullptr;
    const auto active_call = active_calls_.find(stream_id);
    if (active_call != active_calls_.end())
    {
        if (active_call->second.peer_close_received)
        {
            return DispatchResult::Discarded;
        }
        if (is_opening)
        {
            return DispatchResult::ProtocolError;
        }
        active_call->second.pending_messages.push_back(std::move(*call_message));
        if (is_closing)
        {
            active_call->second.peer_close_received = true;
        }
        return DispatchResult::Routed;
    }

    if (has_local_endpoint_parity(stream_id))
    {
        // Inbound stream IDs must have peer-owned parity. Delayed traffic is
        // only possible on a previously used peer-owned stream.
        return DispatchResult::ProtocolError;
    }

    if (stream_id <= peer_high_water_mark_)
    {
        return is_opening ? DispatchResult::ProtocolError : DispatchResult::Discarded;
    }
    if (!is_opening)
    {
        return DispatchResult::ProtocolError;
    }

    peer_high_water_mark_ = stream_id;
    CallState state;
    state.peer_close_received = is_closing;
    state.pending_messages.push_back(std::move(*call_message));
    active_calls_.emplace(stream_id, std::move(state));
    unaccepted_call_ids_.push_back(stream_id);
    return DispatchResult::Routed;
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
    if (!control_pending_.empty())
    {
        return true;
    }
    for (const auto& [stream_id, call_state] : active_calls_)
    {
        static_cast<void>(stream_id);
        if (!call_state.pending_messages.empty())
        {
            return true;
        }
    }
    return false;
}

interface::MessageStatus StreamRegistry::call_receive_status(
    const std::uint64_t stream_id) const noexcept
{
    const auto call = active_calls_.find(stream_id);
    if (call == active_calls_.end())
    {
        return interface::MessageStatus::PeerClosed;
    }
    return call->second.pending_messages.empty() ? interface::MessageStatus::NoMessage
                                                 : interface::MessageStatus::MessageAvailable;
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
    const bool is_closing = message->close_call() != nullptr;
    if (is_closing)
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
    const auto call = active_calls_.find(stream_id);
    if (call == active_calls_.end() || call->second.peer_close_received)
    {
        return interface::OperationStatus::PeerClosed;
    }
    return call->second.local_close_accepted ? interface::OperationStatus::ProtocolError
                                             : interface::OperationStatus::Ok;
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

void StreamRegistry::clear() noexcept
{
    active_calls_.clear();
    unaccepted_call_ids_.clear();
    control_pending_.clear();
    opened_local_stream_ids_.clear();
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
