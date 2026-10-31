#pragma once

#include <exasol/udf/v2/interface/error_info.hpp>
#include <exasol/udf/v2/interface/arrow_c_data.hpp>
#include <exasol/udf/v2/interface/message_builder/call_message_builder.hpp>
#include <exasol/udf/v2/internal/payload.hpp>

#include <optional>
#include <string>
#include <vector>

namespace exasol::udf::v2::internal
{

class DataSchema
{
public:
    DataSchema(interface::ArrowSchemaPtr schema, bool has_group_id, bool has_row_id)
        : schema_(std::move(schema)), has_group_id_(has_group_id), has_row_id_(has_row_id)
    {
    }
    [[nodiscard]] const interface::ArrowSchemaPtr& schema() const noexcept
    {
        return schema_;
    }
    [[nodiscard]] bool has_group_id() const noexcept
    {
        return has_group_id_;
    }
    [[nodiscard]] bool has_row_id() const noexcept
    {
        return has_row_id_;
    }

private:
    interface::ArrowSchemaPtr schema_;
    bool has_group_id_;
    bool has_row_id_;
};

class RecordBatch
{
public:
    RecordBatch(interface::ArrowArrayPtr array, bool is_end_of_group)
        : array_(std::move(array)), is_end_of_group_(is_end_of_group)
    {
    }
    [[nodiscard]] const interface::ArrowArrayPtr& array() const noexcept
    {
        return array_;
    }
    [[nodiscard]] bool is_end_of_group() const noexcept
    {
        return is_end_of_group_;
    }

private:
    interface::ArrowArrayPtr array_;
    bool is_end_of_group_;
};

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
    [[nodiscard]] const std::optional<std::string>& open_call() const noexcept;
    [[nodiscard]] const std::vector<Payload>& payloads() const noexcept;
    [[nodiscard]] const std::optional<DataSchema>& data_schema() const noexcept;
    [[nodiscard]] const std::optional<std::uint64_t>& reset_row_id() const noexcept;
    [[nodiscard]] const std::optional<RecordBatch>& record_batch() const noexcept;
    [[nodiscard]] const std::optional<interface::ErrorInfo>& error() const noexcept;
    [[nodiscard]] bool is_close_call() const noexcept;

private:
    std::uint64_t stream_id_;
    std::optional<std::string> open_call_;
    std::vector<Payload> payloads_;
    std::optional<DataSchema> data_schema_;
    std::optional<std::uint64_t> reset_row_id_;
    std::optional<RecordBatch> record_batch_;
    std::optional<interface::ErrorInfo> error_;
    bool close_call_{false};
};

class CallMessageBuilderImpl final : public interface::CallMessageBuilder
{
public:
    using interface::CallMessageBuilderBase::add_payload;
    using interface::CallMessageBuilderBase::data_schema;
    using interface::CallMessageBuilderBase::record_batch;
    using interface::CallMessageBuilderBase::reset;

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
    using interface::CallMessageBuilderBase::record_batch;
    using interface::CallMessageBuilderBase::reset;

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
