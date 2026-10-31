#include <exasol/udf/v2/internal/call_message_builder_impl.hpp>

#include <cstdlib>
#include <string>
#include <utility>

namespace exasol::udf::v2::internal
{

CallMessage::CallMessage(const std::uint64_t stream_id)
    : stream_id_(stream_id)
{
}

// LCOV_EXCL_START: payload encoding will be implemented with the protocol codec.
void CallMessage::add_payload( // NOSONAR
    std::string_view name,
    std::string_view value,
    bool binary)
{
    static_cast<void>(name);
    static_cast<void>(value);
    static_cast<void>(binary);
    std::abort();
}
// LCOV_EXCL_STOP

void CallMessage::data_schema(interface::ArrowSchemaPtr schema,
                              bool has_group_id,
                              bool has_row_id)
{
    data_schema_ = interface::DataSchema{std::move(schema), has_group_id, has_row_id};
}

void CallMessage::reset(std::uint64_t row_id)
{
    reset_row_id_ = row_id;
}

void CallMessage::record_batch(interface::ArrowArrayPtr array, bool is_end_of_group)
{
    record_batch_ = interface::RecordBatch{std::move(array), is_end_of_group};
}

void CallMessage::error(std::string_view code, std::string_view text)
{
    error_.emplace(std::string(code), std::string(text));
}

void CallMessage::close_call()
{
    close_call_ = true;
}

void CallMessage::set_open_call(std::string_view name)
{
    open_call_ = interface::OpenCall{std::string(name)};
}

std::uint64_t CallMessage::stream_id() const noexcept
{
    return stream_id_;
}

const std::optional<interface::OpenCall>& CallMessage::open_call() const noexcept
{
    return open_call_;
}

const std::optional<interface::DataSchema>& CallMessage::data_schema() const noexcept
{
    return data_schema_;
}

const std::optional<std::uint64_t>& CallMessage::reset_row_id() const noexcept
{
    return reset_row_id_;
}

const std::optional<interface::RecordBatch>& CallMessage::record_batch() const noexcept
{
    return record_batch_;
}

const std::optional<interface::ErrorInfo>& CallMessage::error() const noexcept
{
    return error_;
}

bool CallMessage::is_close_call() const noexcept
{
    return close_call_;
}

CallMessageBuilderImpl::CallMessageBuilderImpl(const std::uint64_t stream_id) : message_(stream_id)
{
}

interface::CallMessageBuilder& CallMessageBuilderImpl::add_payload(std::string_view name,
                                                                   std::string_view value,
                                                                   bool binary)
{
    message_.add_payload(name, value, binary);
    return *this;
}

interface::CallMessageBuilder& CallMessageBuilderImpl::data_schema(interface::ArrowSchemaPtr schema,
                                                                   bool has_group_id,
                                                                   bool has_row_id)
{
    message_.data_schema(std::move(schema), has_group_id, has_row_id);
    return *this;
}

interface::CallMessageBuilder& CallMessageBuilderImpl::reset(std::uint64_t row_id)
{
    message_.reset(row_id);
    return *this;
}

interface::CallMessageBuilder& CallMessageBuilderImpl::record_batch(interface::ArrowArrayPtr array,
                                                                    bool is_end_of_group)
{
    message_.record_batch(std::move(array), is_end_of_group);
    return *this;
}

interface::CallMessageBuilder& CallMessageBuilderImpl::error(std::string_view code,
                                                             std::string_view text)
{
    message_.error(code, text);
    return *this;
}

interface::CallMessageBuilder& CallMessageBuilderImpl::close_call()
{
    message_.close_call();
    return *this;
}

const CallMessage& CallMessageBuilderImpl::message() const noexcept
{
    return message_;
}

OpeningCallMessageBuilderImpl::OpeningCallMessageBuilderImpl(const std::uint64_t stream_id)
    : message_(stream_id)
{
}

interface::OpeningCallMessageBuilder& OpeningCallMessageBuilderImpl::open_call(
    std::string_view name)
{
    message_.set_open_call(name);
    return *this;
}

interface::OpeningCallMessageBuilder& OpeningCallMessageBuilderImpl::add_payload(
    std::string_view name, std::string_view value, bool binary)
{
    message_.add_payload(name, value, binary);
    return *this;
}

interface::OpeningCallMessageBuilder& OpeningCallMessageBuilderImpl::data_schema(
    interface::ArrowSchemaPtr schema, bool has_group_id, bool has_row_id)
{
    message_.data_schema(std::move(schema), has_group_id, has_row_id);
    return *this;
}

interface::OpeningCallMessageBuilder& OpeningCallMessageBuilderImpl::reset(std::uint64_t row_id)
{
    message_.reset(row_id);
    return *this;
}

interface::OpeningCallMessageBuilder& OpeningCallMessageBuilderImpl::record_batch(
    interface::ArrowArrayPtr array, bool is_end_of_group)
{
    message_.record_batch(std::move(array), is_end_of_group);
    return *this;
}

interface::OpeningCallMessageBuilder& OpeningCallMessageBuilderImpl::error(std::string_view code,
                                                                           std::string_view text)
{
    message_.error(code, text);
    return *this;
}

interface::OpeningCallMessageBuilder& OpeningCallMessageBuilderImpl::close_call()
{
    message_.close_call();
    return *this;
}

const CallMessage& OpeningCallMessageBuilderImpl::message() const noexcept
{
    return message_;
}

} // namespace exasol::udf::v2::internal
