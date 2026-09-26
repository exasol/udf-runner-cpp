#include <exasol/udf/v2/message_view.hpp>

namespace exasol::udf::v2
{

bool CallMessageView::has_open_call() const noexcept
{
    return value_ != nullptr && value_->open_call.has_value();
}
const OpenCall* CallMessageView::open_call() const noexcept
{
    return value_ == nullptr || !value_->open_call ? nullptr : &*value_->open_call;
}
bool CallMessageView::has_payloads() const noexcept
{
    return value_ != nullptr && value_->payloads.has_value();
}
const Payloads* CallMessageView::payloads() const noexcept
{
    return value_ == nullptr || !value_->payloads ? nullptr : &*value_->payloads;
}
bool CallMessageView::has_data_schema() const noexcept
{
    return value_ != nullptr && value_->data_schema.has_value();
}
DataSchemaView CallMessageView::data_schema() const noexcept
{
    return DataSchemaView(value_ == nullptr ? nullptr : &*value_->data_schema);
}
bool CallMessageView::has_next() const noexcept
{
    return value_ != nullptr && value_->next.has_value();
}
const Next* CallMessageView::next() const noexcept
{
    return value_ == nullptr || !value_->next ? nullptr : &*value_->next;
}
bool CallMessageView::has_record_batch() const noexcept
{
    return value_ != nullptr && value_->record_batch.has_value();
}
RecordBatchView CallMessageView::record_batch() const noexcept
{
    return RecordBatchView(value_ == nullptr ? nullptr : &*value_->record_batch);
}
bool CallMessageView::has_error() const noexcept
{
    return value_ != nullptr && value_->error.has_value();
}
const ErrorInfo* CallMessageView::error() const noexcept
{
    return value_ == nullptr || !value_->error ? nullptr : &*value_->error;
}
bool CallMessageView::has_close_call() const noexcept
{
    return value_ != nullptr && value_->close_call;
}

bool ControlMessageView::has_server_capabilities() const noexcept
{
    return value_ != nullptr && value_->server_capabilities.has_value();
}
const ServerCapabilities* ControlMessageView::server_capabilities() const noexcept
{
    return value_ == nullptr || !value_->server_capabilities ? nullptr
                                                             : &*value_->server_capabilities;
}
bool ControlMessageView::has_keep_alive() const noexcept
{
    return value_ != nullptr && value_->keep_alive;
}
bool ControlMessageView::has_payloads() const noexcept
{
    return value_ != nullptr && value_->payloads.has_value();
}
const Payloads* ControlMessageView::payloads() const noexcept
{
    return value_ == nullptr || !value_->payloads ? nullptr : &*value_->payloads;
}
bool ControlMessageView::has_error() const noexcept
{
    return value_ != nullptr && value_->error.has_value();
}
const ErrorInfo* ControlMessageView::error() const noexcept
{
    return value_ == nullptr || !value_->error ? nullptr : &*value_->error;
}
bool ControlMessageView::has_close_connection() const noexcept
{
    return value_ != nullptr && value_->close_connection;
}

} // namespace exasol::udf::v2
