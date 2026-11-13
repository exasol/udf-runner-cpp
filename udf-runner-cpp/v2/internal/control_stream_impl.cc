#include <exasol/udf/v2/internal/control_stream_impl.hpp>

#include <exasol/udf/v2/internal/context_state.hpp>

#include <chrono>
#include <utility>

namespace exasol::udf::v2::internal
{

ControlStreamImpl::ControlStreamImpl(std::weak_ptr<ContextState> state) : state_(std::move(state))
{
}

interface::MessageStatus ControlStreamImpl::receive_status() const
{
    const auto state = state_.lock();
    return state ? state->control_receive_status() : interface::MessageStatus::ContextClosed;
}

std::unique_ptr<interface::ControlMessageBuilder> ControlStreamImpl::create_message_builder() const
{
    const auto state = state_.lock();
    return state ? state->create_control_message_builder() : nullptr;
}

interface::Result<std::unique_ptr<interface::ControlMessageView>> ControlStreamImpl::receive()
{
    return receive(std::chrono::steady_clock::duration::zero());
}

interface::Result<std::unique_ptr<interface::ControlMessageView>> ControlStreamImpl::receive(
    const interface::Timeout timeout)
{
    const auto state = state_.lock();
    if (!state)
    {
        return interface::Result<std::unique_ptr<interface::ControlMessageView>>::failure(
            interface::OperationStatus::ContextClosed);
    }
    return state->receive_control_message(timeout);
}

interface::Result<void> ControlStreamImpl::send(
    std::unique_ptr<interface::ControlMessageBuilder>&& message)
{
    const auto state = state_.lock();
    if (!state)
    {
        return interface::Result<void>::failure(interface::OperationStatus::ContextClosed);
    }
    return state->send_control_message(std::move(message));
}

} // namespace exasol::udf::v2::internal
