#pragma once

#include <exasol/udf/v2/message_view/data_schema_view.hpp>
#include <exasol/udf/v2/message_view/record_batch_view.hpp>
#include <exasol/udf/v2/types.hpp>

#include <functional>
#include <optional>

namespace exasol::udf::v2
{

/// Non-owning view of one received composite call message.
class CallMessageView
{
public:
    CallMessageView() = default;
    explicit CallMessageView(const CallMessage& value) : value_(value)
    {
    }
    /// Returns whether call-opening metadata is present.
    [[nodiscard]] bool has_open_call() const noexcept;
    /// Returns call-opening metadata, or an empty optional when absent.
    [[nodiscard]] std::optional<std::reference_wrapper<const OpenCall>> open_call() const noexcept;
    /// Returns whether payloads are present.
    [[nodiscard]] bool has_payloads() const noexcept;
    /// Returns payloads, or an empty optional when absent.
    [[nodiscard]] std::optional<std::reference_wrapper<const Payloads>> payloads() const noexcept;
    /// Returns whether a data schema is present.
    [[nodiscard]] bool has_data_schema() const noexcept;
    /// Returns the data-schema view.
    [[nodiscard]] DataSchemaView data_schema() const noexcept;
    /// Returns whether flow-control information is present.
    [[nodiscard]] bool has_next() const noexcept;
    /// Returns flow-control information, or an empty optional when absent.
    [[nodiscard]] std::optional<std::reference_wrapper<const Next>> next() const noexcept;
    /// Returns whether a record batch is present.
    [[nodiscard]] bool has_record_batch() const noexcept;
    /// Returns the record-batch view.
    [[nodiscard]] RecordBatchView record_batch() const noexcept;
    /// Returns whether an error is present.
    [[nodiscard]] bool has_error() const noexcept;
    /// Returns the error, or an empty optional when absent.
    [[nodiscard]] std::optional<std::reference_wrapper<const ErrorInfo>> error() const noexcept;
    /// Returns whether this message closes the call.
    [[nodiscard]] bool has_close_call() const noexcept;
    /// Returns whether this message closes the call.
    [[nodiscard]] bool close_call() const noexcept;

private:
    std::optional<std::reference_wrapper<const CallMessage>> value_;
};

} // namespace exasol::udf::v2
