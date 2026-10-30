#include <exasol/udf/v2/internal/call_message_builder_impl.hpp>

#include <cstdlib>
#include <string>
#include <utility>

namespace exasol::udf::v2::internal
{

CallMessageBuilderImplBase::CallMessageBuilderImplBase(const std::uint64_t stream_id)
    : stream_id_(stream_id)
{
}

CallMessageBuilderImpl::CallMessageBuilderImpl(const std::uint64_t stream_id)
    : CallMessageBuilderImplBase(stream_id)
{
}

OpeningCallMessageBuilderImpl::OpeningCallMessageBuilderImpl(const std::uint64_t stream_id)
    : CallMessageBuilderImplBase(stream_id)
{
}

interface::OpeningCallMessageBuilder& OpeningCallMessageBuilderImpl::open_call(
    std::string_view name)
{
    open_call_ = interface::OpenCall{std::string(name)};
    return *this;
}

// LCOV_EXCL_START: payload encoding will be implemented with the protocol codec.
interface::CallMessageBuilderBase& CallMessageBuilderImplBase::add_payload( // NOSONAR
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

interface::CallMessageBuilderBase& CallMessageBuilderImplBase::data_schema(
    interface::ArrowSchemaPtr schema, bool has_group_id, bool has_row_id)
{
    data_schema_ = interface::DataSchema{std::move(schema), has_group_id, has_row_id};
    return *this;
}

interface::CallMessageBuilderBase& CallMessageBuilderImplBase::next(std::uint32_t byte_budget,
                                                                    bool reset,
                                                                    std::uint64_t row)
{
    outbound_next_ = interface::OutboundNext{byte_budget, reset, row};
    return *this;
}

interface::CallMessageBuilderBase& CallMessageBuilderImplBase::record_batch(
    interface::ArrowArrayPtr array, bool is_end_of_group)
{
    record_batch_ = interface::RecordBatch{std::move(array), is_end_of_group};
    return *this;
}

interface::CallMessageBuilderBase& CallMessageBuilderImplBase::error(std::string_view code,
                                                                     std::string_view text)
{
    error_.emplace(std::string(code), std::string(text));
    return *this;
}

interface::CallMessageBuilderBase& CallMessageBuilderImplBase::close_call()
{
    close_call_ = true;
    return *this;
}

CallMessageEncodingView CallMessageBuilderImplBase::encoding_view() const noexcept
{
    return {stream_id_,    open_call_, data_schema_, outbound_next_,
            record_batch_, error_,     close_call_};
}

} // namespace exasol::udf::v2::internal
