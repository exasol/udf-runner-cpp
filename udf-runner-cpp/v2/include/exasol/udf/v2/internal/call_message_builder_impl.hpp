#pragma once

#include <exasol/udf/v2/interface/error_info.hpp>
#include <exasol/udf/v2/interface/message_builder/call_message_builder.hpp>

#include <optional>

namespace exasol::udf::v2::internal
{

/// Internal mutable representation consumed by the protocol encoder.
class CallMessage
{
public:
    explicit CallMessage(std::uint64_t stream_id);

    void add_payload(std::string_view name, std::string_view value, bool binary);
    void data_schema(interface::ArrowSchemaPtr schema, bool has_group_id, bool has_row_id);
    void reset(std::uint64_t row_id);
    void record_batch(interface::ArrowArrayPtr array, bool is_end_of_group);
    void error(std::string_view code, std::string_view text);
    void close_call();
    void set_open_call(std::string_view name);
    [[nodiscard]] std::uint64_t stream_id() const noexcept;
    [[nodiscard]] const std::optional<interface::OpenCall>& open_call() const noexcept;
    [[nodiscard]] const std::optional<interface::DataSchema>& data_schema() const noexcept;
    [[nodiscard]] const std::optional<std::uint64_t>& reset_row_id() const noexcept;
    [[nodiscard]] const std::optional<interface::RecordBatch>& record_batch() const noexcept;
    [[nodiscard]] const std::optional<interface::ErrorInfo>& error() const noexcept;
    [[nodiscard]] bool is_close_call() const noexcept;

private:
    std::uint64_t stream_id_;
    std::optional<interface::OpenCall> open_call_;
    std::optional<interface::DataSchema> data_schema_;
    std::optional<std::uint64_t> reset_row_id_;
    std::optional<interface::RecordBatch> record_batch_;
    std::optional<interface::ErrorInfo> error_;
    bool close_call_{false};
};

class CallMessageBuilderImpl final : public interface::CallMessageBuilder
{
public:
    using interface::CallMessageBuilderBase::add_payload;
    using interface::CallMessageBuilderBase::data_schema;
    using interface::CallMessageBuilderBase::reset;
    using interface::CallMessageBuilderBase::record_batch;

    explicit CallMessageBuilderImpl(std::uint64_t stream_id);

    interface::CallMessageBuilder& add_payload(std::string_view name,
                                               std::string_view value,
                                               bool binary) override;
    interface::CallMessageBuilder& data_schema(interface::ArrowSchemaPtr schema,
                                               bool has_group_id,
                                               bool has_row_id) override;
    interface::CallMessageBuilder& reset(std::uint64_t row_id) override;
    interface::CallMessageBuilder& record_batch(interface::ArrowArrayPtr array,
                                                bool is_end_of_group) override;
    interface::CallMessageBuilder& error(std::string_view code, std::string_view text) override;
    interface::CallMessageBuilder& close_call() override;
    [[nodiscard]] const CallMessage& message() const noexcept;

private:
    CallMessage message_;
};

class OpeningCallMessageBuilderImpl final : public interface::OpeningCallMessageBuilder
{
public:
    using interface::CallMessageBuilderBase::add_payload;
    using interface::CallMessageBuilderBase::data_schema;
    using interface::CallMessageBuilderBase::reset;
    using interface::CallMessageBuilderBase::record_batch;

    explicit OpeningCallMessageBuilderImpl(std::uint64_t stream_id);

    interface::OpeningCallMessageBuilder& open_call(std::string_view name) override;
    interface::OpeningCallMessageBuilder& add_payload(std::string_view name,
                                                      std::string_view value,
                                                      bool binary) override;
    interface::OpeningCallMessageBuilder& data_schema(interface::ArrowSchemaPtr schema,
                                                      bool has_group_id,
                                                      bool has_row_id) override;
    interface::OpeningCallMessageBuilder& reset(std::uint64_t row_id) override;
    interface::OpeningCallMessageBuilder& record_batch(interface::ArrowArrayPtr array,
                                                       bool is_end_of_group) override;
    interface::OpeningCallMessageBuilder& error(std::string_view code,
                                                std::string_view text) override;
    interface::OpeningCallMessageBuilder& close_call() override;
    [[nodiscard]] const CallMessage& message() const noexcept;

private:
    CallMessage message_;
};

} // namespace exasol::udf::v2::internal
