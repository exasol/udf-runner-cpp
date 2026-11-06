#include <exasol/udf/v2/interface/message_view/control_message_view.hpp>
#include <exasol/udf/v2/internal/call_message_flatbuffer_view_impl.hpp>
#include <exasol/udf/v2/internal/call_message_view_impl.hpp>
#include <exasol/udf/v2/internal/common_flatbuffer_view_impl.hpp>

#include <exasol/udf/v2/assert.hpp>

#include <utility>

namespace exasol::udf::v2::internal
{

using interface::DataSchemaView;
using interface::RecordBatchView;

namespace
{

    const protocol::ControlMessage* control_message(
        const protocol::DecodedFrame& decoded_frame) noexcept
    {
        return decoded_frame.frame().control_message();
    }

} // namespace

std::unique_ptr<interface::OpenCallView> CallMessageViewImpl::open_call() const
{
    const auto* control = control_message(*decoded_frame_);
    return make_open_call_view(decoded_frame_,
                               control != nullptr ? control->value_as_OpenCall() : nullptr);
}
std::unique_ptr<interface::PayloadsView> CallMessageViewImpl::payloads() const
{
    const auto* control = control_message(*decoded_frame_);
    return make_payloads_view(decoded_frame_, control != nullptr ? control->payloads() : nullptr);
}
std::unique_ptr<DataSchemaView> CallMessageViewImpl::data_schema() noexcept
{
    return nullptr;
}
std::unique_ptr<interface::ResetView> CallMessageViewImpl::reset() const
{
    const auto* control = control_message(*decoded_frame_);
    return make_reset_view(decoded_frame_, control != nullptr ? control->reset() : nullptr);
}
std::unique_ptr<RecordBatchView> CallMessageViewImpl::record_batch() noexcept
{
    return nullptr;
}
std::unique_ptr<interface::ErrorView> CallMessageViewImpl::error() const
{
    const auto* control = control_message(*decoded_frame_);
    return make_error_view(decoded_frame_, control != nullptr ? control->error() : nullptr);
}
CallMessageViewImpl::CallMessageViewImpl(std::shared_ptr<const protocol::DecodedFrame> frame)
    : decoded_frame_(std::move(frame))
{
    EXASOL_UDF_ASSERT(decoded_frame_ != nullptr);
}

std::unique_ptr<interface::CloseCallView> CallMessageViewImpl::close_call() const
{
    const auto* control = control_message(*decoded_frame_);
    const auto* close   = control != nullptr ? control->value_as_CloseControlMessage() : nullptr;
    return make_close_call_view(decoded_frame_, close != nullptr ? close->close_call() : nullptr);
}

std::uint64_t CallMessageViewImpl::stream_id() const noexcept
{
    return decoded_frame_->frame().stream_id();
}

} // namespace exasol::udf::v2::internal
