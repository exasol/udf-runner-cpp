#include <exasol/udf/v2/message_view/call_message_view.hpp>
#include <exasol/udf/v2/message_view/control_message_view.hpp>

#include <utility>

namespace exasol::udf::v2
{

bool CallMessageView::has_open_call() const noexcept
{
    return call_message_.has_value() && call_message_->get().open_call.has_value();
}
std::optional<std::reference_wrapper<const OpenCall>> CallMessageView::open_call() const noexcept
{
    if (!call_message_.has_value() || !call_message_->get().open_call.has_value())
    {
        return std::nullopt;
    }
    return std::cref(*call_message_->get().open_call);
}
bool CallMessageView::has_payloads() const noexcept
{
    return call_message_.has_value() && call_message_->get().payloads.has_value();
}
std::optional<std::reference_wrapper<const Payloads>> CallMessageView::payloads() const noexcept
{
    if (!call_message_.has_value() || !call_message_->get().payloads.has_value())
    {
        return std::nullopt;
    }
    return std::cref(*call_message_->get().payloads);
}
std::optional<std::reference_wrapper<const protocol::Payloads>>
CallMessageView::flatbuffer_payloads() const noexcept
{
    const auto* control  = decoded_frame_ ? decoded_frame_->frame().control_message() : nullptr;
    const auto* payloads = control != nullptr ? control->payloads() : nullptr;
    return payloads != nullptr ? std::optional{std::cref(*payloads)} : std::nullopt;
}
bool CallMessageView::has_data_schema() const noexcept
{
    return call_message_.has_value() && call_message_->get().data_schema.has_value();
}
DataSchemaView CallMessageView::data_schema() const noexcept
{
    if (!call_message_.has_value() || !call_message_->get().data_schema.has_value())
    {
        return DataSchemaView{};
    }
    return DataSchemaView(*call_message_->get().data_schema);
}
bool CallMessageView::has_next() const noexcept
{
    return call_message_.has_value() && call_message_->get().next.has_value();
}
std::optional<std::reference_wrapper<const Next>> CallMessageView::next() const noexcept
{
    if (!call_message_.has_value() || !call_message_->get().next.has_value())
    {
        return std::nullopt;
    }
    return std::cref(*call_message_->get().next);
}
bool CallMessageView::has_record_batch() const noexcept
{
    return call_message_.has_value() && call_message_->get().record_batch.has_value();
}
RecordBatchView CallMessageView::record_batch() const noexcept
{
    if (!call_message_.has_value() || !call_message_->get().record_batch.has_value())
    {
        return RecordBatchView{};
    }
    return RecordBatchView(*call_message_->get().record_batch);
}
bool CallMessageView::has_error() const noexcept
{
    return call_message_.has_value() && call_message_->get().error.has_value();
}
std::optional<std::reference_wrapper<const ErrorInfo>> CallMessageView::error() const noexcept
{
    if (!call_message_.has_value() || !call_message_->get().error.has_value())
    {
        return std::nullopt;
    }
    return std::cref(*call_message_->get().error);
}
std::optional<std::reference_wrapper<const protocol::Error>> CallMessageView::flatbuffer_error()
    const noexcept
{
    const auto* control = decoded_frame_ ? decoded_frame_->frame().control_message() : nullptr;
    const auto* error   = control != nullptr ? control->error() : nullptr;
    return error != nullptr ? std::optional{std::cref(*error)} : std::nullopt;
}

CallMessageView::CallMessageView(std::shared_ptr<const protocol::DecodedFrame> frame)
    : decoded_frame_(std::move(frame))
{
}

bool CallMessageView::close_call() const noexcept
{
    return call_message_.has_value() && call_message_->get().close_call;
}

bool ControlMessageView::has_server_capabilities() const noexcept
{
    return control_message_.has_value() && control_message_->get().server_capabilities.has_value();
}
std::optional<std::reference_wrapper<const ServerCapabilities>>
ControlMessageView::server_capabilities() const noexcept
{
    if (!control_message_.has_value() || !control_message_->get().server_capabilities.has_value())
    {
        return std::nullopt;
    }
    return std::cref(*control_message_->get().server_capabilities);
}
std::optional<std::reference_wrapper<const protocol::ServerCapabilities>>
ControlMessageView::flatbuffer_server_capabilities() const noexcept
{
    const auto* control = decoded_frame_ ? decoded_frame_->frame().control_message() : nullptr;
    const auto* capabilities =
        control != nullptr ? control->value_as_ServerCapabilities() : nullptr;
    return capabilities != nullptr ? std::optional{std::cref(*capabilities)} : std::nullopt;
}
std::optional<std::reference_wrapper<const protocol::KeepAlive>> ControlMessageView::keep_alive()
    const noexcept
{
    const auto* control    = decoded_frame_ ? decoded_frame_->frame().control_message() : nullptr;
    const auto* keep_alive = control != nullptr ? control->value_as_KeepAlive() : nullptr;
    return keep_alive != nullptr ? std::optional{std::cref(*keep_alive)} : std::nullopt;
}
bool ControlMessageView::has_payloads() const noexcept
{
    return control_message_.has_value() && control_message_->get().payloads.has_value();
}
std::optional<std::reference_wrapper<const Payloads>> ControlMessageView::payloads() const noexcept
{
    if (!control_message_.has_value() || !control_message_->get().payloads.has_value())
    {
        return std::nullopt;
    }
    return std::cref(*control_message_->get().payloads);
}
std::optional<std::reference_wrapper<const protocol::Payloads>>
ControlMessageView::flatbuffer_payloads() const noexcept
{
    const auto* control  = decoded_frame_ ? decoded_frame_->frame().control_message() : nullptr;
    const auto* payloads = control != nullptr ? control->payloads() : nullptr;
    return payloads != nullptr ? std::optional{std::cref(*payloads)} : std::nullopt;
}
bool ControlMessageView::has_error() const noexcept
{
    return control_message_.has_value() && control_message_->get().error.has_value();
}
std::optional<std::reference_wrapper<const ErrorInfo>> ControlMessageView::error() const noexcept
{
    if (!control_message_.has_value() || !control_message_->get().error.has_value())
    {
        return std::nullopt;
    }
    return std::cref(*control_message_->get().error);
}
std::optional<std::reference_wrapper<const protocol::Error>> ControlMessageView::flatbuffer_error()
    const noexcept
{
    const auto* control = decoded_frame_ ? decoded_frame_->frame().control_message() : nullptr;
    const auto* error   = control != nullptr ? control->error() : nullptr;
    return error != nullptr ? std::optional{std::cref(*error)} : std::nullopt;
}

ControlMessageView::ControlMessageView(std::shared_ptr<const protocol::DecodedFrame> frame)
    : decoded_frame_(std::move(frame))
{
}

bool ControlMessageView::close_connection() const noexcept
{
    return control_message_.has_value() && control_message_->get().close_connection;
}

} // namespace exasol::udf::v2
