#include <exasol/udf/v2/message_view/call_message_view.hpp>
#include <exasol/udf/v2/message_view/control_message_view.hpp>

namespace exasol::udf::v2
{

bool CallMessageView::has_open_call() const noexcept
{
    return call_message_.has_value() && call_message_.value().get().open_call.has_value();
}
std::optional<std::reference_wrapper<const OpenCall>> CallMessageView::open_call() const noexcept
{
    if (!call_message_.has_value() || !call_message_.value().get().open_call.has_value())
    {
        return std::nullopt;
    }
    return std::cref(call_message_.value().get().open_call.value());
}
bool CallMessageView::has_payloads() const noexcept
{
    return call_message_.has_value() && call_message_.value().get().payloads.has_value();
}
std::optional<std::reference_wrapper<const Payloads>> CallMessageView::payloads() const noexcept
{
    if (!call_message_.has_value() || !call_message_.value().get().payloads.has_value())
    {
        return std::nullopt;
    }
    return std::cref(call_message_.value().get().payloads.value());
}
bool CallMessageView::has_data_schema() const noexcept
{
    return call_message_.has_value() && call_message_.value().get().data_schema.has_value();
}
DataSchemaView CallMessageView::data_schema() const noexcept
{
    if (!call_message_.has_value() || !call_message_.value().get().data_schema.has_value())
    {
        return DataSchemaView{};
    }
    return DataSchemaView(call_message_.value().get().data_schema.value());
}
bool CallMessageView::has_next() const noexcept
{
    return call_message_.has_value() && call_message_.value().get().next.has_value();
}
std::optional<std::reference_wrapper<const Next>> CallMessageView::next() const noexcept
{
    if (!call_message_.has_value() || !call_message_.value().get().next.has_value())
    {
        return std::nullopt;
    }
    return std::cref(call_message_.value().get().next.value());
}
bool CallMessageView::has_record_batch() const noexcept
{
    return call_message_.has_value() && call_message_.value().get().record_batch.has_value();
}
RecordBatchView CallMessageView::record_batch() const noexcept
{
    if (!call_message_.has_value() || !call_message_.value().get().record_batch.has_value())
    {
        return RecordBatchView{};
    }
    return RecordBatchView(call_message_.value().get().record_batch.value());
}
bool CallMessageView::has_error() const noexcept
{
    return call_message_.has_value() && call_message_.value().get().error.has_value();
}
std::optional<std::reference_wrapper<const ErrorInfo>> CallMessageView::error() const noexcept
{
    if (!call_message_.has_value() || !call_message_.value().get().error.has_value())
    {
        return std::nullopt;
    }
    return std::cref(call_message_.value().get().error.value());
}
bool CallMessageView::close_call() const noexcept
{
    return call_message_.has_value() && call_message_.value().get().close_call;
}

bool ControlMessageView::has_server_capabilities() const noexcept
{
    return control_message_.has_value() &&
           control_message_.value().get().server_capabilities.has_value();
}
std::optional<std::reference_wrapper<const ServerCapabilities>>
ControlMessageView::server_capabilities() const noexcept
{
    if (!control_message_.has_value() ||
        !control_message_.value().get().server_capabilities.has_value())
    {
        return std::nullopt;
    }
    return std::cref(control_message_.value().get().server_capabilities.value());
}
bool ControlMessageView::keep_alive() const noexcept
{
    return control_message_.has_value() && control_message_.value().get().keep_alive;
}
bool ControlMessageView::has_payloads() const noexcept
{
    return control_message_.has_value() && control_message_.value().get().payloads.has_value();
}
std::optional<std::reference_wrapper<const Payloads>> ControlMessageView::payloads() const noexcept
{
    if (!control_message_.has_value() || !control_message_.value().get().payloads.has_value())
    {
        return std::nullopt;
    }
    return std::cref(control_message_.value().get().payloads.value());
}
bool ControlMessageView::has_error() const noexcept
{
    return control_message_.has_value() && control_message_.value().get().error.has_value();
}
std::optional<std::reference_wrapper<const ErrorInfo>> ControlMessageView::error() const noexcept
{
    if (!control_message_.has_value() || !control_message_.value().get().error.has_value())
    {
        return std::nullopt;
    }
    return std::cref(control_message_.value().get().error.value());
}
bool ControlMessageView::close_connection() const noexcept
{
    return control_message_.has_value() && control_message_.value().get().close_connection;
}

} // namespace exasol::udf::v2
