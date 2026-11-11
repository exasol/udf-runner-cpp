#pragma once

#include <exasol/udf/v2/interface/message_view/call_message_view.hpp>
#include <exasol/udf/v2/interface/message_view/data_schema_view.hpp>
#include <exasol/udf/v2/interface/message_view/record_batch_view.hpp>
#include <exasol/udf/v2/udf_protocol.hpp>

#include <memory>
#include <optional>

namespace exasol::udf::v2::internal
{
class CallMessageViewImpl final : public interface::CallMessageView
{
public:
    explicit CallMessageViewImpl(std::shared_ptr<const protocol::DecodedFrame> frame);
    [[nodiscard]] std::uint64_t stream_id() const noexcept override;
    [[nodiscard]] std::unique_ptr<interface::OpenCallView> open_call() const override;
    [[nodiscard]] std::unique_ptr<interface::PayloadsView> payloads() const override;
    [[nodiscard]] std::unique_ptr<interface::DataSchemaView> data_schema() noexcept override;
    [[nodiscard]] std::unique_ptr<interface::ResetView> reset() const override;
    [[nodiscard]] std::unique_ptr<interface::RecordBatchView> record_batch() noexcept override;
    [[nodiscard]] std::unique_ptr<interface::ErrorView> error() const override;
    [[nodiscard]] std::unique_ptr<interface::CloseCallView> close_call() const override;

private:
    std::shared_ptr<const protocol::DecodedFrame> decoded_frame_;
};
} // namespace exasol::udf::v2::internal
