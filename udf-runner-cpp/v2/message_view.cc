#include <exasol/udf/v2/message_view/call_message_view.hpp>
#include <exasol/udf/v2/message_view/control_message_view.hpp>

namespace exasol::udf::v2
{

bool CallMessageView::has_open_call() const noexcept
{
    return value_ && value_->get().open_call.has_value();
}
std::optional<std::reference_wrapper<const OpenCall>> CallMessageView::open_call() const noexcept
{
    return value_ && value_->get().open_call
               ? std::optional<std::reference_wrapper<const OpenCall>>{*value_->get().open_call}
               : std::nullopt;
}
bool CallMessageView::has_payloads() const noexcept
{
    return value_ && value_->get().payloads.has_value();
}
std::optional<std::reference_wrapper<const Payloads>> CallMessageView::payloads() const noexcept
{
    return value_ && value_->get().payloads
               ? std::optional<std::reference_wrapper<const Payloads>>{*value_->get().payloads}
               : std::nullopt;
}
bool CallMessageView::has_data_schema() const noexcept
{
    return value_ && value_->get().data_schema.has_value();
}
DataSchemaView CallMessageView::data_schema() const noexcept
{
    return value_ && value_->get().data_schema ? DataSchemaView(*value_->get().data_schema)
                                               : DataSchemaView{};
}
bool CallMessageView::has_next() const noexcept
{
    return value_ && value_->get().next.has_value();
}
std::optional<std::reference_wrapper<const Next>> CallMessageView::next() const noexcept
{
    return value_ && value_->get().next
               ? std::optional<std::reference_wrapper<const Next>>{*value_->get().next}
               : std::nullopt;
}
bool CallMessageView::has_record_batch() const noexcept
{
    return value_ && value_->get().record_batch.has_value();
}
RecordBatchView CallMessageView::record_batch() const noexcept
{
    return value_ && value_->get().record_batch ? RecordBatchView(*value_->get().record_batch)
                                                : RecordBatchView{};
}
bool CallMessageView::has_error() const noexcept
{
    return value_ && value_->get().error.has_value();
}
std::optional<std::reference_wrapper<const ErrorInfo>> CallMessageView::error() const noexcept
{
    return value_ && value_->get().error
               ? std::optional<std::reference_wrapper<const ErrorInfo>>{*value_->get().error}
               : std::nullopt;
}
bool CallMessageView::has_close_call() const noexcept
{
    return value_ && value_->get().close_call;
}
bool CallMessageView::close_call() const noexcept
{
    return has_close_call();
}

bool ControlMessageView::has_server_capabilities() const noexcept
{
    return value_ && value_->get().server_capabilities.has_value();
}
std::optional<std::reference_wrapper<const ServerCapabilities>>
ControlMessageView::server_capabilities() const noexcept
{
    return value_ && value_->get().server_capabilities
               ? std::optional<
                     std::reference_wrapper<const ServerCapabilities>>{*value_->get()
                                                                            .server_capabilities}
               : std::nullopt;
}
bool ControlMessageView::has_keep_alive() const noexcept
{
    return value_ && value_->get().keep_alive;
}
bool ControlMessageView::keep_alive() const noexcept
{
    return has_keep_alive();
}
bool ControlMessageView::has_payloads() const noexcept
{
    return value_ && value_->get().payloads.has_value();
}
std::optional<std::reference_wrapper<const Payloads>> ControlMessageView::payloads() const noexcept
{
    return value_ && value_->get().payloads
               ? std::optional<std::reference_wrapper<const Payloads>>{*value_->get().payloads}
               : std::nullopt;
}
bool ControlMessageView::has_error() const noexcept
{
    return value_ && value_->get().error.has_value();
}
std::optional<std::reference_wrapper<const ErrorInfo>> ControlMessageView::error() const noexcept
{
    return value_ && value_->get().error
               ? std::optional<std::reference_wrapper<const ErrorInfo>>{*value_->get().error}
               : std::nullopt;
}
bool ControlMessageView::has_close_connection() const noexcept
{
    return value_ && value_->get().close_connection;
}
bool ControlMessageView::close_connection() const noexcept
{
    return has_close_connection();
}

} // namespace exasol::udf::v2
