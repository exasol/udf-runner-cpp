#include <exasol/udf/v2/internal/context_impl.hpp>

#include <exasol/udf/v2/internal/call_impl.hpp>

#include <utility>

namespace exasol::udf::v2::internal
{

ContextImpl::ContextImpl(const EndpointRole endpoint_role,
                         std::unique_ptr<BackgroundThreadInterface> background_thread)
    : state_(std::make_shared<ContextState>(endpoint_role, std::move(background_thread))),
      control_stream_(state_)
{
}

ContextImpl::~ContextImpl()
{
    state_->shutdown();
}

interface::MessageStatus ContextImpl::receive_status() const
{
    return state_->receive_status();
}

bool ContextImpl::is_new_inbound_call_available() const
{
    return state_->is_new_inbound_call_available();
}

interface::MessageStatus ContextImpl::wait_for_message()
{
    return state_->wait_for_message(std::nullopt);
}

interface::MessageStatus ContextImpl::wait_for_message(const interface::Timeout timeout)
{
    return state_->wait_for_message(timeout);
}

interface::Result<std::unique_ptr<interface::Call>> ContextImpl::accept_call()
{
    auto result = state_->accept_call();
    if (!result)
    {
        return interface::Result<std::unique_ptr<interface::Call>>::failure(result.status,
                                                                            result.error);
    }
    return interface::Result<std::unique_ptr<interface::Call>>::success(
        std::make_unique<CallImpl>(state_, *result.value));
}

std::unique_ptr<interface::OpeningCallMessageBuilder>
ContextImpl::create_opening_call_message_builder()
{
    return state_->create_opening_call_message_builder();
}

interface::Result<std::unique_ptr<interface::Call>> ContextImpl::open_call(
    std::unique_ptr<interface::OpeningCallMessageBuilder>&& opening_message)
{
    auto result = state_->open_call(std::move(opening_message));
    if (!result)
    {
        return interface::Result<std::unique_ptr<interface::Call>>::failure(result.status,
                                                                            result.error);
    }
    return interface::Result<std::unique_ptr<interface::Call>>::success(
        std::make_unique<CallImpl>(state_, *result.value));
}

interface::ControlStream& ContextImpl::control_stream()
{
    return control_stream_;
}

} // namespace exasol::udf::v2::internal
