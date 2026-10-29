#include <exasol/udf/v2/internal/control_message_view_impl.hpp>

#include <functional>
#include <utility>

namespace exasol::udf::v2::internal
{
namespace
{

    const protocol::ControlMessage* control_message(
        const protocol::DecodedFrame* decoded_frame) noexcept
    {
        return decoded_frame != nullptr ? decoded_frame->frame().control_message() : nullptr;
    }

    template <typename T>
    std::optional<std::reference_wrapper<const T>> optional_reference(const T* value) noexcept
    {
        return value != nullptr ? std::optional{std::cref(*value)} : std::nullopt;
    }

    interface::ControlMessage decode_control_message(const protocol::DecodedFrame* decoded_frame)
    {
        interface::ControlMessage message;
        if (const auto* control = control_message(decoded_frame); control != nullptr)
        {
            message.keep_alive = control->value_as_KeepAlive() != nullptr;
            if (const auto* error = control->error(); error != nullptr)
            {
                message.error.emplace(error->code() != nullptr ? error->code()->str() : "",
                                      error->message() != nullptr ? error->message()->str() : "");
            }
            if (const auto* close = control->value_as_CloseControlMessage(); close != nullptr)
            {
                message.close_connection = close->close_connection() != nullptr;
            }
        }
        return message;
    }

} // namespace

ControlMessageViewImpl::ControlMessageViewImpl(std::shared_ptr<const protocol::DecodedFrame> frame)
    : control_message_(decode_control_message(frame.get())), decoded_frame_(std::move(frame))
{
}

bool ControlMessageViewImpl::has_server_capabilities() const noexcept
{
    return server_capabilities().has_value();
}

std::optional<std::reference_wrapper<const protocol::ServerCapabilities>>
ControlMessageViewImpl::server_capabilities() const noexcept
{
    const auto* control = control_message(decoded_frame_.get());
    return optional_reference(control != nullptr ? control->value_as_ServerCapabilities()
                                                 : nullptr);
}

bool ControlMessageViewImpl::has_keep_alive() const noexcept
{
    return control_message_.has_value() && control_message_->keep_alive;
}

bool ControlMessageViewImpl::has_payloads() const noexcept
{
    return payloads().has_value();
}

std::optional<std::reference_wrapper<const protocol::Payloads>> ControlMessageViewImpl::payloads()
    const noexcept
{
    const auto* control = control_message(decoded_frame_.get());
    return optional_reference(control != nullptr ? control->payloads() : nullptr);
}

bool ControlMessageViewImpl::has_error() const noexcept
{
    return error().has_value();
}

std::optional<std::reference_wrapper<const protocol::Error>> ControlMessageViewImpl::error()
    const noexcept
{
    const auto* control = control_message(decoded_frame_.get());
    return optional_reference(control != nullptr ? control->error() : nullptr);
}

bool ControlMessageViewImpl::has_close_connection() const noexcept
{
    return control_message_.has_value() && control_message_->close_connection;
}

} // namespace exasol::udf::v2::internal
