#include <exasol/udf/v2/internal/call_message_view_impl.hpp>
#include <exasol/udf/v2/interface/message_view/control_message_view.hpp>

#include <functional>
#include <optional>
#include <utility>

namespace exasol::udf::v2::internal
{

using interface::DataSchemaView;
using interface::Next;
using interface::RecordBatchView;

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

bool CallMessageViewImpl::has_open_call() const noexcept
{
    return open_call().has_value();
}
std::optional<std::reference_wrapper<const protocol::OpenCall>> CallMessageViewImpl::open_call()
    const noexcept
{
    const auto* control = control_message(decoded_frame_.get());
    return optional_reference(control != nullptr ? control->value_as_OpenCall() : nullptr);
}
bool CallMessageViewImpl::has_payloads() const noexcept
{
    return payloads().has_value();
}
std::optional<std::reference_wrapper<const protocol::Payloads>> CallMessageViewImpl::payloads()
    const noexcept
{
    const auto* control = control_message(decoded_frame_.get());
    return optional_reference(control != nullptr ? control->payloads() : nullptr);
}
bool CallMessageViewImpl::has_data_schema() const noexcept
{
    return data_schema_.has_value();
}
DataSchemaView CallMessageViewImpl::data_schema() noexcept
{
    if (!data_schema_.has_value())
    {
        return DataSchemaView{};
    }
    DataSchemaView view(std::move(*data_schema_));
    data_schema_.reset();
    return view;
}
bool CallMessageViewImpl::has_next() const noexcept
{
    return next_.has_value();
}
std::optional<std::reference_wrapper<const Next>> CallMessageViewImpl::next() const noexcept
{
    if (!next_.has_value())
    {
        return std::nullopt;
    }
    return std::cref(*next_);
}
bool CallMessageViewImpl::has_record_batch() const noexcept
{
    return record_batch_.has_value();
}
RecordBatchView CallMessageViewImpl::record_batch() noexcept
{
    if (!record_batch_.has_value())
    {
        return RecordBatchView{};
    }
    RecordBatchView view(std::move(*record_batch_));
    record_batch_.reset();
    return view;
}
bool CallMessageViewImpl::has_error() const noexcept
{
    return error().has_value();
}
std::optional<std::reference_wrapper<const protocol::Error>> CallMessageViewImpl::error()
    const noexcept
{
    const auto* control = control_message(decoded_frame_.get());
    return optional_reference(control != nullptr ? control->error() : nullptr);
}
CallMessageViewImpl::CallMessageViewImpl(std::shared_ptr<const protocol::DecodedFrame> frame)
    : decoded_frame_(std::move(frame))
{
    if (const auto* control = control_message(decoded_frame_.get()); control != nullptr)
    {
        if (const auto* next = control->next(); next != nullptr)
        {
            next_ = Next{next->reset(), next->row_id()};
        }
        if (const auto* close = control->value_as_CloseControlMessage(); close != nullptr)
        {
            close_call_ = close->close_call() != nullptr;
        }
    }
}

bool CallMessageViewImpl::has_close_call() const noexcept
{
    return close_call_;
}

} // namespace exasol::udf::v2::internal
