#pragma once

#include <exasol/udf/v2/interface/content_types.hpp>
#include <exasol/udf/v2/interface/message_view/data_schema_view.hpp>
#include <exasol/udf/v2/interface/message_view/record_batch_view.hpp>
#include <exasol/udf/v2/udf_protocol.hpp>

#include <functional>
#include <optional>

namespace exasol::udf::v2::interface
{

/// View of one received composite call message.
///
/// FlatBuffer-backed accessors borrow data from the decoded frame and remain
/// valid while the view is alive. Arrow data is owned by the call view until
/// transferred into a DataSchemaView or RecordBatchView. Those specialized
/// views retain ownership until take_schema() or take_array() is called.
class CallMessageView
{
public:
    virtual ~CallMessageView()                         = default;
    CallMessageView(const CallMessageView&)            = delete;
    CallMessageView& operator=(const CallMessageView&) = delete;
    CallMessageView(CallMessageView&&)                 = delete;
    CallMessageView& operator=(CallMessageView&&)      = delete;
    /// Returns whether call-opening metadata is present.
    [[nodiscard]] virtual bool has_open_call() const noexcept = 0;
    /// Returns the wire-level FlatBuffer call-opening table, or an empty optional when absent.
    [[nodiscard]] virtual std::optional<std::reference_wrapper<const protocol::OpenCall>>
    open_call() const noexcept = 0;
    /// Returns whether wire-level payloads are present.
    [[nodiscard]] virtual bool has_payloads() const noexcept = 0;
    /// Returns the wire-level FlatBuffer payload table, or an empty optional when absent.
    [[nodiscard]] virtual std::optional<std::reference_wrapper<const protocol::Payloads>> payloads()
        const noexcept = 0;
    /// Returns whether a data schema is present.
    [[nodiscard]] virtual bool has_data_schema() const noexcept = 0;
    /// Returns the data-schema view.
    [[nodiscard]] virtual DataSchemaView data_schema() noexcept = 0;
    /// Returns whether flow-control information is present.
    [[nodiscard]] virtual bool has_next() const noexcept = 0;
    /// Returns flow-control information, or an empty optional when absent.
    [[nodiscard]] virtual std::optional<std::reference_wrapper<const Next>> next()
        const noexcept = 0;
    /// Returns whether a record batch is present.
    [[nodiscard]] virtual bool has_record_batch() const noexcept = 0;
    /// Returns the record-batch view.
    [[nodiscard]] virtual RecordBatchView record_batch() noexcept = 0;
    /// Returns whether an error is present.
    [[nodiscard]] virtual bool has_error() const noexcept = 0;
    /// Returns the wire-level FlatBuffer error table, or an empty optional when absent.
    [[nodiscard]] virtual std::optional<std::reference_wrapper<const protocol::Error>> error()
        const noexcept = 0;
    /// Returns whether this message closes the call.
    [[nodiscard]] virtual bool has_close_call() const noexcept = 0;

protected:
    CallMessageView() = default;
};

} // namespace exasol::udf::v2::interface
