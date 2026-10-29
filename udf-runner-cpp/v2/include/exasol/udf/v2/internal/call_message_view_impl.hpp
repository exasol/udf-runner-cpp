#pragma once

#include <exasol/udf/v2/interface/content_types.hpp>
#include <exasol/udf/v2/interface/message_view/call_message_view.hpp>
#include <exasol/udf/v2/interface/message_view/data_schema_view.hpp>
#include <exasol/udf/v2/interface/message_view/record_batch_view.hpp>

#include <memory>
#include <optional>

namespace exasol::udf::v2::internal
{
class CallMessageViewImpl final : public interface::CallMessageView
{
public:
    explicit CallMessageViewImpl(std::shared_ptr<const protocol::DecodedFrame> frame);
    [[nodiscard]] bool has_open_call() const noexcept override;
    [[nodiscard]] std::optional<std::reference_wrapper<const protocol::OpenCall>> open_call()
        const noexcept override;
    [[nodiscard]] bool has_payloads() const noexcept override;
    [[nodiscard]] std::optional<std::reference_wrapper<const protocol::Payloads>> payloads()
        const noexcept override;
    [[nodiscard]] bool has_data_schema() const noexcept override;
    [[nodiscard]] interface::DataSchemaView data_schema() noexcept override;
    [[nodiscard]] bool has_next() const noexcept override;
    [[nodiscard]] std::optional<std::reference_wrapper<const interface::Next>> next()
        const noexcept override;
    [[nodiscard]] bool has_record_batch() const noexcept override;
    [[nodiscard]] interface::RecordBatchView record_batch() noexcept override;
    [[nodiscard]] bool has_error() const noexcept override;
    [[nodiscard]] std::optional<std::reference_wrapper<const protocol::Error>> error()
        const noexcept override;
    [[nodiscard]] bool has_close_call() const noexcept override;

private:
    std::optional<interface::DataSchema> data_schema_;
    std::optional<interface::Next> next_;
    std::optional<interface::RecordBatch> record_batch_;
    bool close_call_{false};
    std::shared_ptr<const protocol::DecodedFrame> decoded_frame_;
};
} // namespace exasol::udf::v2::internal
