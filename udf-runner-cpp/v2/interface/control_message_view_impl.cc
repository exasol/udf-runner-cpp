#include <exasol/udf/v2/internal/common_flatbuffer_view_impl.hpp>
#include <exasol/udf/v2/internal/control_message_flatbuffer_view_impl.hpp>
#include <exasol/udf/v2/internal/control_message_view_impl.hpp>

#include <exasol/udf/v2/assert.hpp>

#include <utility>

namespace exasol::udf::v2::internal
{
namespace
{

    const protocol::ControlMessage* control_message(
        const protocol::DecodedFrame& decoded_frame) noexcept
    {
        return decoded_frame.frame().control_message();
    }

} // namespace

ControlMessageViewImpl::ControlMessageViewImpl(std::shared_ptr<const protocol::DecodedFrame> frame)
    : decoded_frame_(std::move(frame))
{
    EXASOL_UDF_ASSERT(decoded_frame_ != nullptr);
}

std::uint64_t ControlMessageViewImpl::stream_id() const noexcept
{
    return decoded_frame_->frame().stream_id();
}

std::unique_ptr<interface::ServerCapabilitiesView> ControlMessageViewImpl::server_capabilities()
    const
{
    const auto* control = control_message(*decoded_frame_);
    return make_server_capabilities_view(
        decoded_frame_, control != nullptr ? control->value_as_ServerCapabilities() : nullptr);
}

std::unique_ptr<interface::KeepAliveView> ControlMessageViewImpl::keep_alive() const
{
    const auto* control = control_message(*decoded_frame_);
    return make_keep_alive_view(decoded_frame_,
                                control != nullptr ? control->value_as_KeepAlive() : nullptr);
}

std::unique_ptr<interface::PayloadsView> ControlMessageViewImpl::payloads() const
{
    const auto* control = control_message(*decoded_frame_);
    return make_payloads_view(decoded_frame_, control != nullptr ? control->payloads() : nullptr);
}

std::unique_ptr<interface::ErrorView> ControlMessageViewImpl::error() const
{
    const auto* control = control_message(*decoded_frame_);
    return make_error_view(decoded_frame_, control != nullptr ? control->error() : nullptr);
}

std::unique_ptr<interface::CloseConnectionView> ControlMessageViewImpl::close_connection() const
{
    const auto* control = control_message(*decoded_frame_);
    const auto* close   = control != nullptr ? control->value_as_CloseControlMessage() : nullptr;
    return make_close_connection_view(decoded_frame_,
                                      close != nullptr ? close->close_connection() : nullptr);
}

} // namespace exasol::udf::v2::internal
