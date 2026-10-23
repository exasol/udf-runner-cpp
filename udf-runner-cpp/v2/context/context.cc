#include <exasol/udf/v2/context/low_level_context.hpp>

#include <stdexcept>
#include <utility>

namespace exasol::udf::v2::context
{

LowLevelContext::LowLevelContext(std::unique_ptr<BackgroundIoThreadInterface> background_thread)
    : background_thread_(std::move(background_thread))
{
    if (background_thread_ == nullptr)
    {
        throw std::invalid_argument("background I/O thread must not be null");
    }
    background_thread_->start();
}

LowLevelContext::~LowLevelContext()
{
    background_thread_->request_stop();
    background_thread_->join();
}

interface::MessageStatus LowLevelContext::receive_status() const
{
    return background_thread_->receive_status();
}

bool LowLevelContext::is_new_inbound_call_available() const
{
    return background_thread_->is_new_inbound_call_available();
}

interface::MessageStatus LowLevelContext::wait_for_message(interface::Timeout timeout)
{
    return background_thread_->wait_for_message(timeout);
}

interface::Result<std::unique_ptr<interface::Call>> LowLevelContext::accept_call()
{
    return background_thread_->accept_call();
}

interface::Result<std::unique_ptr<interface::Call>> LowLevelContext::open_call(
    interface::CallMessageBuilder opening_message)
{
    return background_thread_->open_call(std::move(opening_message));
}

interface::ControlStream& LowLevelContext::control_stream()
{
    return background_thread_->control_stream();
}

} // namespace exasol::udf::v2::context
