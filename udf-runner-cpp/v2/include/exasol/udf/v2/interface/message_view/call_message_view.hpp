#pragma once

#include <exasol/udf/v2/interface/message_view/call_message_views.hpp>
#include <exasol/udf/v2/interface/message_view/common_views.hpp>
#include <exasol/udf/v2/interface/message_view/data_schema_view.hpp>
#include <exasol/udf/v2/interface/message_view/record_batch_view.hpp>

#include <cstdint>
#include <memory>

namespace exasol::udf::v2::interface
{

/// View of one received composite call message.
///
/// Typed child views retain the decoded-frame storage they need and remain
/// valid for their own lifetime. Arrow data is owned by the call view until
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
    /// Returns the stream ID of this view's verified frame.
    [[nodiscard]] virtual std::uint64_t stream_id() const noexcept = 0;
    /// Returns the call-opening view, or nullptr when absent.
    [[nodiscard]] virtual std::unique_ptr<OpenCallView> open_call() const = 0;
    /// Returns the payloads view, or nullptr when absent.
    [[nodiscard]] virtual std::unique_ptr<PayloadsView> payloads() const = 0;
    /// Returns the data-schema view, or nullptr when absent.
    [[nodiscard]] virtual std::unique_ptr<DataSchemaView> data_schema() noexcept = 0;
    /// Returns the reset view, or nullptr when absent.
    [[nodiscard]] virtual std::unique_ptr<ResetView> reset() const = 0;
    /// Returns the record-batch view, or nullptr when absent.
    [[nodiscard]] virtual std::unique_ptr<RecordBatchView> record_batch() noexcept = 0;
    /// Returns the error view, or nullptr when absent.
    [[nodiscard]] virtual std::unique_ptr<ErrorView> error() const = 0;
    /// Returns the close-call view, or nullptr when absent.
    [[nodiscard]] virtual std::unique_ptr<CloseCallView> close_call() const = 0;

protected:
    CallMessageView() = default;
};

} // namespace exasol::udf::v2::interface
