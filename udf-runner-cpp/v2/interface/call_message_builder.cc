#include <exasol/udf/v2/internal/call_message_builder_impl.hpp>

#include <cstdlib>
#include <string>
#include <utility>

namespace exasol::udf::v2::internal
{

CallMessageBuilderState::CallMessageBuilderState(const std::uint64_t stream_id)
    : stream_id_(stream_id)
{
}

// LCOV_EXCL_START: payload encoding will be implemented with the protocol codec.
void CallMessageBuilderState::add_payload( // NOSONAR
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

void CallMessageBuilderState::data_schema(interface::ArrowSchemaPtr schema,
                                          bool has_group_id,
                                          bool has_row_id)
{
    data_schema_ = interface::DataSchema{std::move(schema), has_group_id, has_row_id};
}

void CallMessageBuilderState::next(std::uint32_t byte_budget, bool reset, std::uint64_t row_id)
{
    outbound_next_ = interface::OutboundNext{byte_budget, reset, row_id};
}

void CallMessageBuilderState::record_batch(interface::ArrowArrayPtr array, bool is_end_of_group)
{
    record_batch_ = interface::RecordBatch{std::move(array), is_end_of_group};
}

void CallMessageBuilderState::error(std::string_view code, std::string_view text)
{
    error_.emplace(std::string(code), std::string(text));
}

void CallMessageBuilderState::close_call()
{
    close_call_ = true;
}

void CallMessageBuilderState::set_open_call(std::string_view name)
{
    open_call_ = interface::OpenCall{std::string(name)};
}

CallMessageEncodingView CallMessageBuilderState::encoding_view() const noexcept
{
    return {stream_id_,    open_call_, data_schema_, outbound_next_,
            record_batch_, error_,     close_call_};
}

CallMessageBuilderImpl::CallMessageBuilderImpl(const std::uint64_t stream_id) : state_(stream_id)
{
}

interface::CallMessageBuilder& CallMessageBuilderImpl::add_payload(std::string_view name,
                                                                   std::string_view value,
                                                                   bool binary)
{
    state_.add_payload(name, value, binary);
    return *this;
}

interface::CallMessageBuilder& CallMessageBuilderImpl::data_schema(interface::ArrowSchemaPtr schema,
                                                                   bool has_group_id,
                                                                   bool has_row_id)
{
    state_.data_schema(std::move(schema), has_group_id, has_row_id);
    return *this;
}

interface::CallMessageBuilder& CallMessageBuilderImpl::next(std::uint32_t byte_budget,
                                                            bool reset,
                                                            std::uint64_t row_id)
{
    state_.next(byte_budget, reset, row_id);
    return *this;
}

interface::CallMessageBuilder& CallMessageBuilderImpl::record_batch(interface::ArrowArrayPtr array,
                                                                    bool is_end_of_group)
{
    state_.record_batch(std::move(array), is_end_of_group);
    return *this;
}

interface::CallMessageBuilder& CallMessageBuilderImpl::error(std::string_view code,
                                                             std::string_view text)
{
    state_.error(code, text);
    return *this;
}

interface::CallMessageBuilder& CallMessageBuilderImpl::close_call()
{
    state_.close_call();
    return *this;
}

CallMessageEncodingView CallMessageBuilderImpl::encoding_view() const noexcept
{
    return state_.encoding_view();
}

OpeningCallMessageBuilderImpl::OpeningCallMessageBuilderImpl(const std::uint64_t stream_id)
    : state_(stream_id)
{
}

interface::OpeningCallMessageBuilder& OpeningCallMessageBuilderImpl::open_call(
    std::string_view name)
{
    state_.set_open_call(name);
    return *this;
}

interface::OpeningCallMessageBuilder& OpeningCallMessageBuilderImpl::add_payload(
    std::string_view name, std::string_view value, bool binary)
{
    state_.add_payload(name, value, binary);
    return *this;
}

interface::OpeningCallMessageBuilder& OpeningCallMessageBuilderImpl::data_schema(
    interface::ArrowSchemaPtr schema, bool has_group_id, bool has_row_id)
{
    state_.data_schema(std::move(schema), has_group_id, has_row_id);
    return *this;
}

interface::OpeningCallMessageBuilder& OpeningCallMessageBuilderImpl::next(std::uint32_t byte_budget,
                                                                          bool reset,
                                                                          std::uint64_t row_id)
{
    state_.next(byte_budget, reset, row_id);
    return *this;
}

interface::OpeningCallMessageBuilder& OpeningCallMessageBuilderImpl::record_batch(
    interface::ArrowArrayPtr array, bool is_end_of_group)
{
    state_.record_batch(std::move(array), is_end_of_group);
    return *this;
}

interface::OpeningCallMessageBuilder& OpeningCallMessageBuilderImpl::error(std::string_view code,
                                                                           std::string_view text)
{
    state_.error(code, text);
    return *this;
}

interface::OpeningCallMessageBuilder& OpeningCallMessageBuilderImpl::close_call()
{
    state_.close_call();
    return *this;
}

CallMessageEncodingView OpeningCallMessageBuilderImpl::encoding_view() const noexcept
{
    return state_.encoding_view();
}

} // namespace exasol::udf::v2::internal
