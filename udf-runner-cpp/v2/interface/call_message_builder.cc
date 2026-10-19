#include <exasol/udf/v2/interface/message_builder/call_message_builder.hpp>

#include <cstdlib>
#include <string>
#include <utility>

namespace exasol::udf::v2::interface
{

CallMessageBuilder& CallMessageBuilder::open_call(std::string_view name)
{
    message_.open_call = OpenCall{std::string(name)};
    return *this;
}

// LCOV_EXCL_START: payload encoding will be implemented with the protocol codec.
CallMessageBuilder& CallMessageBuilder::add_payload( // NOSONAR
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

CallMessageBuilder& CallMessageBuilder::data_schema(ArrowSchemaPtr schema,
                                                    bool has_group_id,
                                                    bool has_row_id)
{
    message_.data_schema = DataSchema{std::move(schema), has_group_id, has_row_id};
    return *this;
}

CallMessageBuilder& CallMessageBuilder::next(std::uint32_t byte_budget,
                                             bool reset,
                                             std::uint64_t row)
{
    message_.outbound_next = OutboundNext{byte_budget, reset, row};
    return *this;
}

CallMessageBuilder& CallMessageBuilder::record_batch(ArrowArrayPtr array, bool is_end_of_group)
{
    message_.record_batch = RecordBatch{std::move(array), is_end_of_group};
    return *this;
}

CallMessageBuilder& CallMessageBuilder::error(std::string_view code, std::string_view text)
{
    message_.error.emplace(std::string(code), std::string(text));
    return *this;
}

CallMessageBuilder& CallMessageBuilder::close_call()
{
    message_.close_call = true;
    return *this;
}

const CallMessage& CallMessageBuilder::message() const& noexcept
{
    return message_;
}

CallMessage CallMessageBuilder::take() &&
{
    return std::move(message_);
}

} // namespace exasol::udf::v2::interface
