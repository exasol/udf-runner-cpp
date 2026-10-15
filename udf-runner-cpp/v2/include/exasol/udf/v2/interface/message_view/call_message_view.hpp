#pragma once

#include <exasol/udf/v2/interface/message_view/data_schema_view.hpp>
#include <exasol/udf/v2/interface/message_view/record_batch_view.hpp>
#include <exasol/udf/v2/interface/content_types.hpp>
#include <exasol/udf/v2/udf_protocol.hpp>

#include <functional>
#include <memory>
#include <optional>

namespace exasol::udf::v2
{

/// Non-owning view of one received composite call message.
class CallMessageView
{
public:
    CallMessageView() = default;
    /// Returns whether call-opening metadata is present.
    [[nodiscard]] bool has_open_call() const noexcept;
    /// Returns the wire-level FlatBuffer call-opening table, or an empty optional when absent.
    [[nodiscard]] std::optional<std::reference_wrapper<const protocol::OpenCall>> open_call()
        const noexcept;
    /// Returns whether wire-level payloads are present.
    [[nodiscard]] bool has_payloads() const noexcept;
    /// Returns the wire-level FlatBuffer payload table, or an empty optional when absent.
    [[nodiscard]] std::optional<std::reference_wrapper<const protocol::Payloads>> payloads()
        const noexcept;
    /// Returns whether a data schema is present.
    [[nodiscard]] bool has_data_schema() const noexcept;
    /// Returns the data-schema view.
    [[nodiscard]] DataSchemaView data_schema() noexcept;
    /// Returns whether flow-control information is present.
    [[nodiscard]] bool has_next() const noexcept;
    /// Returns flow-control information, or an empty optional when absent.
    [[nodiscard]] std::optional<std::reference_wrapper<const Next>> next() const noexcept;
    /// Returns whether a record batch is present.
    [[nodiscard]] bool has_record_batch() const noexcept;
    /// Returns the record-batch view.
    [[nodiscard]] RecordBatchView record_batch() noexcept;
    /// Returns whether an error is present.
    [[nodiscard]] bool has_error() const noexcept;
    /// Returns the wire-level FlatBuffer error table, or an empty optional when absent.
    [[nodiscard]] std::optional<std::reference_wrapper<const protocol::Error>> error()
        const noexcept;
    /// Returns whether this message closes the call.
    [[nodiscard]] bool has_close_call() const noexcept;

    explicit CallMessageView(std::shared_ptr<const protocol::DecodedFrame> frame);

private:
    std::optional<CallMessage> call_message_;
    std::shared_ptr<const protocol::DecodedFrame> decoded_frame_;
};

} // namespace exasol::udf::v2
