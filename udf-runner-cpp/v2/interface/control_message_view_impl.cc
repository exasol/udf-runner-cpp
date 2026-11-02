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

} // namespace

ControlMessageViewImpl::ControlMessageViewImpl(std::shared_ptr<const protocol::DecodedFrame> frame)
    : decoded_frame_(std::move(frame))
{
}

std::optional<std::uint64_t> ControlMessageViewImpl::stream_id() const noexcept
{
    if (decoded_frame_ == nullptr) {
        return std::nullopt;
    }
    return decoded_frame_->frame().stream_id();
}

std::optional<std::reference_wrapper<const protocol::ServerCapabilities>>
ControlMessageViewImpl::server_capabilities() const noexcept
{
    const auto* control = control_message(decoded_frame_.get());
    return optional_reference(control != nullptr ? control->value_as_ServerCapabilities()
                                                 : nullptr);
}

std::optional<std::reference_wrapper<const protocol::KeepAlive>>
ControlMessageViewImpl::keep_alive() const noexcept
{
    const auto* control = control_message(decoded_frame_.get());
    return optional_reference(control != nullptr ? control->value_as_KeepAlive() : nullptr);
}

std::optional<std::reference_wrapper<const protocol::Payloads>> ControlMessageViewImpl::payloads()
    const noexcept
{
    const auto* control = control_message(decoded_frame_.get());
    return optional_reference(control != nullptr ? control->payloads() : nullptr);
}

std::optional<std::reference_wrapper<const protocol::Error>> ControlMessageViewImpl::error()
    const noexcept
{
    const auto* control = control_message(decoded_frame_.get());
    return optional_reference(control != nullptr ? control->error() : nullptr);
}

std::optional<std::reference_wrapper<const protocol::CloseConnection>>
ControlMessageViewImpl::close_connection() const noexcept
{
    const auto* control = control_message(decoded_frame_.get());
    const auto* close   = control != nullptr ? control->value_as_CloseControlMessage() : nullptr;
    return optional_reference(close != nullptr ? close->close_connection() : nullptr);
}

} // namespace exasol::udf::v2::internal
