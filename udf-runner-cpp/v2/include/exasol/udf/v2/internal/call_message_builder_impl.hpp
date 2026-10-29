#pragma once

#include <exasol/udf/v2/interface/content_types.hpp>
#include <exasol/udf/v2/interface/message_builder/call_message_builder.hpp>

#include <optional>

namespace exasol::udf::v2::internal
{

/// Internal, non-owning representation consumed by the protocol encoder.
struct CallMessageEncodingView
{
    std::uint64_t stream_id;
    const std::optional<interface::OpenCall>& open_call;
    const std::optional<interface::DataSchema>& data_schema;
    const std::optional<interface::OutboundNext>& next;
    const std::optional<interface::RecordBatch>& record_batch;
    const std::optional<interface::ErrorInfo>& error;
    bool close_call;
};

class CallMessageBuilderImpl final : public interface::CallMessageBuilder
{
public:
    explicit CallMessageBuilderImpl(std::uint64_t stream_id);

    interface::CallMessageBuilder& open_call(std::string_view name) override;
    interface::CallMessageBuilder& add_payload(std::string_view name,
                                               std::string_view value,
                                               bool binary) override;
    interface::CallMessageBuilder& data_schema(interface::ArrowSchemaPtr schema,
                                               bool has_group_id,
                                               bool has_row_id) override;
    interface::CallMessageBuilder& next(std::uint32_t byte_budget,
                                        bool reset,
                                        std::uint64_t row_id) override;
    interface::CallMessageBuilder& record_batch(interface::ArrowArrayPtr array,
                                                bool is_end_of_group) override;
    interface::CallMessageBuilder& error(std::string_view code, std::string_view text) override;
    interface::CallMessageBuilder& close_call() override;

    [[nodiscard]] CallMessageEncodingView encoding_view() const noexcept;

private:
    std::uint64_t stream_id_;
    std::optional<interface::OpenCall> open_call_;
    std::optional<interface::DataSchema> data_schema_;
    std::optional<interface::OutboundNext> outbound_next_;
    std::optional<interface::RecordBatch> record_batch_;
    std::optional<interface::ErrorInfo> error_;
    bool close_call_{false};
};

} // namespace exasol::udf::v2::internal
