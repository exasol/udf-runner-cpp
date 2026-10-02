#include <exasol/udf/v2/message_builder.hpp>

namespace exasol::udf::v2
{

CallMessageBuilder& CallMessageBuilder::open_call(std::string_view name)
{
    message_.open_call = OpenCall{std::string(name)};
    return *this;
}
CallMessageBuilder& CallMessageBuilder::add_payload(std::string_view name,
                                                    std::string_view value,
                                                    bool binary)
{
    if (!message_.payloads)
    {
        message_.payloads.emplace();
    }
    message_.payloads->push_back(Payload{std::string(name), std::string(value), binary});
    return *this;
}
CallMessageBuilder& CallMessageBuilder::data_schema(ArrowSchema* schema,
                                                    bool has_group_id,
                                                    bool has_row_id)
{
    message_.data_schema = DataSchema{schema, has_group_id, has_row_id};
    return *this;
}
CallMessageBuilder& CallMessageBuilder::next(bool reset, std::uint64_t row)
{
    message_.next = Next{reset, row};
    return *this;
}
CallMessageBuilder& CallMessageBuilder::record_batch(ArrowArray* array, bool is_end_of_group)
{
    message_.record_batch = RecordBatch{array, is_end_of_group};
    return *this;
}
CallMessageBuilder& CallMessageBuilder::error(std::string_view code, std::string_view text)
{
    message_.error = ErrorInfo{std::string(code), std::string(text)};
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

ControlMessageBuilder& ControlMessageBuilder::server_capabilities(
    std::uint32_t major,
    std::uint32_t minor,
    std::endian endianness,
    std::uint32_t number_of_supported_workers)
{
    message_.server_capabilities =
        ServerCapabilities{major, minor, endianness, number_of_supported_workers};
    return *this;
}
ControlMessageBuilder& ControlMessageBuilder::keep_alive()
{
    message_.keep_alive = true;
    return *this;
}
ControlMessageBuilder& ControlMessageBuilder::add_payload(std::string_view name,
                                                          std::string_view value,
                                                          bool binary)
{
    if (!message_.payloads)
    {
        message_.payloads.emplace();
    }
    message_.payloads->push_back(Payload{std::string(name), std::string(value), binary});
    return *this;
}
ControlMessageBuilder& ControlMessageBuilder::error(std::string_view code, std::string_view text)
{
    message_.error = ErrorInfo{std::string(code), std::string(text)};
    return *this;
}
ControlMessageBuilder& ControlMessageBuilder::close_connection()
{
    message_.close_connection = true;
    return *this;
}
const ControlMessage& ControlMessageBuilder::message() const& noexcept
{
    return message_;
}
ControlMessage ControlMessageBuilder::take() &&
{
    return std::move(message_);
}

} // namespace exasol::udf::v2
