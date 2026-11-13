#include <exasol/udf/v2/internal/call_impl.hpp>

#include <exasol/udf/v2/internal/context_state.hpp>

#include <utility>

namespace exasol::udf::v2::internal
{

CallImpl::CallImpl(std::weak_ptr<ContextState> state, const std::uint64_t stream_id)
    : state_(std::move(state)), stream_id_(stream_id)
{
}

interface::MessageStatus CallImpl::receive_status() const
{
    const auto state = state_.lock();
    return state ? state->call_receive_status(stream_id_) : interface::MessageStatus::ContextClosed;
}

std::unique_ptr<interface::CallMessageBuilder> CallImpl::create_message_builder() const
{
    const auto state = state_.lock();
    return state ? state->create_call_message_builder(stream_id_) : nullptr;
}

interface::Result<std::unique_ptr<interface::CallMessageView>> CallImpl::receive()
{
    return receive(std::chrono::steady_clock::duration::zero());
}

interface::Result<std::unique_ptr<interface::CallMessageView>> CallImpl::receive(
    const interface::Timeout timeout)
{
    const auto state = state_.lock();
    if (!state)
    {
        return interface::Result<std::unique_ptr<interface::CallMessageView>>::failure(
            interface::OperationStatus::ContextClosed);
    }
    return state->receive_call_message(stream_id_, timeout);
}

interface::Result<void> CallImpl::send(std::unique_ptr<interface::CallMessageBuilder>&& message)
{
    return send(std::move(message), std::chrono::steady_clock::duration::zero());
}

interface::Result<void> CallImpl::send(std::unique_ptr<interface::CallMessageBuilder>&& message,
                                       const interface::Timeout timeout)
{
    const auto state = state_.lock();
    if (!state)
    {
        return interface::Result<void>::failure(interface::OperationStatus::ContextClosed);
    }
    return state->send_call_message(stream_id_, std::move(message), timeout);
}

} // namespace exasol::udf::v2::internal
