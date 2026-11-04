#include <exasol/udf/v2/internal/call_message_view_impl.hpp>
#include <exasol/udf/v2/interface/message_view/control_message_view.hpp>

#include <exasol/udf/v2/assert.hpp>

#include <functional>
#include <optional>
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

    template <typename T>
    std::optional<std::reference_wrapper<const T>> optional_reference(const T* value) noexcept
    {
        return value != nullptr ? std::optional{std::cref(*value)} : std::nullopt;
    }

} // namespace

std::optional<std::reference_wrapper<const protocol::OpenCall>> CallMessageViewImpl::open_call()
    const noexcept
{
    const auto* control = control_message(*decoded_frame_);
    return optional_reference(control != nullptr ? control->value_as_OpenCall() : nullptr);
}
std::optional<std::reference_wrapper<const protocol::Payloads>> CallMessageViewImpl::payloads()
    const noexcept
{
    const auto* control = control_message(*decoded_frame_);
    return optional_reference(control != nullptr ? control->payloads() : nullptr);
}
std::unique_ptr<DataSchemaView> CallMessageViewImpl::data_schema() noexcept
{
    return nullptr;
}
std::optional<std::reference_wrapper<const protocol::Reset>> CallMessageViewImpl::reset()
    const noexcept
{
    const auto* control = control_message(*decoded_frame_);
    return optional_reference(control != nullptr ? control->reset() : nullptr);
}
std::unique_ptr<RecordBatchView> CallMessageViewImpl::record_batch() noexcept
{
    return nullptr;
}
std::optional<std::reference_wrapper<const protocol::Error>> CallMessageViewImpl::error()
    const noexcept
{
    const auto* control = control_message(*decoded_frame_);
    return optional_reference(control != nullptr ? control->error() : nullptr);
}
CallMessageViewImpl::CallMessageViewImpl(std::shared_ptr<const protocol::DecodedFrame> frame)
    : decoded_frame_(std::move(frame))
{
    EXASOL_UDF_ASSERT(decoded_frame_ != nullptr);
}

std::optional<std::reference_wrapper<const protocol::CloseCall>> CallMessageViewImpl::close_call()
    const noexcept
{
    const auto* control = control_message(*decoded_frame_);
    const auto* close   = control != nullptr ? control->value_as_CloseControlMessage() : nullptr;
    return optional_reference(close != nullptr ? close->close_call() : nullptr);
}

std::uint64_t CallMessageViewImpl::stream_id() const noexcept
{
    return decoded_frame_->frame().stream_id();
}

} // namespace exasol::udf::v2::internal
