#pragma once

#include <exasol/udf/v2/types.hpp>

namespace exasol::udf::v2
{

/// Non-owning view of a received Arrow data schema and its correlation flags.
class DataSchemaView
{
public:
    explicit DataSchemaView(const DataSchema* value = nullptr) : value_(value)
    {
    }
    /// Returns the received Arrow schema pointer, or null when absent.
    [[nodiscard]] ArrowSchema* schema() const noexcept
    {
        return value_ == nullptr ? nullptr : value_->schema;
    }
    /// Returns whether the schema reserves a group-id field.
    [[nodiscard]] bool has_group_id() const noexcept
    {
        return value_ != nullptr && value_->has_group_id;
    }
    /// Returns whether the schema reserves a row-id field.
    [[nodiscard]] bool has_row_id() const noexcept
    {
        return value_ != nullptr && value_->has_row_id;
    }

private:
    const DataSchema* value_;
};

/// Non-owning view of a received Arrow record batch.
class RecordBatchView
{
public:
    explicit RecordBatchView(const RecordBatch* value = nullptr) : value_(value)
    {
    }
    /// Returns the received Arrow array pointer, or null when absent.
    [[nodiscard]] ArrowArray* array() const noexcept
    {
        return value_ == nullptr ? nullptr : value_->array;
    }
    /// Returns whether the final group in this batch ends here.
    [[nodiscard]] bool is_end_of_group() const noexcept
    {
        return value_ != nullptr && value_->is_end_of_group;
    }

private:
    const RecordBatch* value_;
};

/// Non-owning view of one received composite call message.
class CallMessageView
{
public:
    explicit CallMessageView(const CallMessage* value = nullptr) : value_(value)
    {
    }
    /// Returns whether call-opening metadata is present.
    [[nodiscard]] bool has_open_call() const noexcept;
    /// Returns call-opening metadata, or null when absent.
    [[nodiscard]] const OpenCall* open_call() const noexcept;
    /// Returns whether payloads are present.
    [[nodiscard]] bool has_payloads() const noexcept;
    /// Returns payloads, or null when absent.
    [[nodiscard]] const Payloads* payloads() const noexcept;
    /// Returns whether a data schema is present.
    [[nodiscard]] bool has_data_schema() const noexcept;
    /// Returns the data-schema view.
    [[nodiscard]] DataSchemaView data_schema() const noexcept;
    /// Returns whether flow-control information is present.
    [[nodiscard]] bool has_next() const noexcept;
    /// Returns flow-control information, or null when absent.
    [[nodiscard]] const Next* next() const noexcept;
    /// Returns whether a record batch is present.
    [[nodiscard]] bool has_record_batch() const noexcept;
    /// Returns the record-batch view.
    [[nodiscard]] RecordBatchView record_batch() const noexcept;
    /// Returns whether an error is present.
    [[nodiscard]] bool has_error() const noexcept;
    /// Returns the error, or null when absent.
    [[nodiscard]] const ErrorInfo* error() const noexcept;
    /// Returns whether this message closes the call.
    [[nodiscard]] bool has_close_call() const noexcept;

private:
    const CallMessage* value_;
};

/// Non-owning view of one received connection-level message.
class ControlMessageView
{
public:
    explicit ControlMessageView(const ControlMessage* value = nullptr) : value_(value)
    {
    }
    /// Returns whether server capabilities are present.
    [[nodiscard]] bool has_server_capabilities() const noexcept;
    /// Returns server capabilities, or null when absent.
    [[nodiscard]] const ServerCapabilities* server_capabilities() const noexcept;
    /// Returns whether a keep-alive is present.
    [[nodiscard]] bool has_keep_alive() const noexcept;
    /// Returns whether payloads are present.
    [[nodiscard]] bool has_payloads() const noexcept;
    /// Returns payloads, or null when absent.
    [[nodiscard]] const Payloads* payloads() const noexcept;
    /// Returns whether an error is present.
    [[nodiscard]] bool has_error() const noexcept;
    /// Returns the error, or null when absent.
    [[nodiscard]] const ErrorInfo* error() const noexcept;
    /// Returns whether connection shutdown is being started or acknowledged.
    [[nodiscard]] bool has_close_connection() const noexcept;

private:
    const ControlMessage* value_;
};

} // namespace exasol::udf::v2
