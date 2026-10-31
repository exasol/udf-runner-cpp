#pragma once

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
    [[nodiscard]] std::optional<std::reference_wrapper<const protocol::OpenCall>> open_call()
        const noexcept override;
    [[nodiscard]] std::optional<std::reference_wrapper<const protocol::Payloads>> payloads()
        const noexcept override;
    [[nodiscard]] std::unique_ptr<interface::DataSchemaView> data_schema() noexcept override;
    [[nodiscard]] std::optional<std::reference_wrapper<const protocol::Reset>> reset()
        const noexcept override;
    [[nodiscard]] std::unique_ptr<interface::RecordBatchView> record_batch() noexcept override;
    [[nodiscard]] std::optional<std::reference_wrapper<const protocol::Error>> error()
        const noexcept override;
    [[nodiscard]] std::optional<std::reference_wrapper<const protocol::CloseCall>> close_call()
        const noexcept override;

private:
    std::shared_ptr<const protocol::DecodedFrame> decoded_frame_;
};
} // namespace exasol::udf::v2::internal
