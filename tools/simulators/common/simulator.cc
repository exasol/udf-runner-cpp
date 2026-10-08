#include "common/simulator.hpp"

#include "udf_protocol.hpp"

#include <exasol/udf/v2/socket/unix_socket.hpp>

#include <sys/socket.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace exasol::udf::simulator
{
namespace
{
namespace protocol = exasol::udf::protocol;
namespace fb = exasol::udf::v2::third_party::flatbuffers;
namespace transport = exasol::udf::v2::socket;

constexpr std::size_t max_frame = 16 * 1024 * 1024;
constexpr std::size_t max_buffer = 64 * 1024 * 1024;
constexpr std::size_t max_batch = 128 * 1024 * 1024;
constexpr std::uint64_t max_rows = 1'000'000;

struct Field
{
    std::string name;
    std::string type;
    bool nullable;

    bool operator==(const Field&) const = default;
};

struct Batch
{
    fb::Offset<protocol::DataRecordBatchMetadata> metadata;
    std::vector<std::vector<std::uint8_t>> buffers;
    std::size_t bytes = 0;
};

[[noreturn]] void fail(const std::string& message)
{
    throw std::runtime_error(message);
}

std::vector<Field> fields_from_columns(const Json& columns)
{
    if (!columns.is_array() || columns.empty())
    {
        fail("columns must be a nonempty array");
    }
    std::vector<Field> fields;
    for (const auto& column : columns)
    {
        fields.push_back({column.at("name").get<std::string>(),
                          column.at("type").get<std::string>(),
                          column.value("nullable", true)});
    }
    return fields;
}

int width(const std::string& type)
{
    if (type == "int8" || type == "uint8") return 1;
    if (type == "int16" || type == "uint16") return 2;
    if (type == "int32" || type == "uint32") return 4;
    if (type == "int64" || type == "uint64" || type == "float64") return 8;
    if (type == "bool") return 0;
    if (type == "utf8" || type == "binary") return -1;
    fail("unsupported Arrow type: " + type);
}

bool is_signed(const std::string& type)
{
    return type.starts_with("int");
}

void append_le(std::vector<std::uint8_t>& bytes, std::uint64_t value, int size)
{
    for (int index = 0; index < size; ++index)
    {
        bytes.push_back(static_cast<std::uint8_t>(value >> (8 * index)));
    }
}

std::uint64_t read_le(const std::vector<std::uint8_t>& bytes, std::size_t offset, int size)
{
    if (offset > bytes.size() || static_cast<std::size_t>(size) > bytes.size() - offset)
    {
        fail("truncated Arrow value buffer");
    }
    std::uint64_t result = 0;
    for (int index = 0; index < size; ++index)
    {
        result |= static_cast<std::uint64_t>(bytes[offset + index]) << (8 * index);
    }
    return result;
}

void set_bit(std::vector<std::uint8_t>& bytes, std::size_t index)
{
    bytes[index / 8] |= static_cast<std::uint8_t>(1U << (index % 8));
}

bool bit(const std::vector<std::uint8_t>& bytes, std::size_t index)
{
    if (index / 8 >= bytes.size()) fail("truncated Arrow bitmap");
    return (bytes[index / 8] & (1U << (index % 8))) != 0;
}

constexpr std::string_view base64_alphabet =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::vector<std::uint8_t> decode_base64(const std::string& text)
{
    if (text.size() % 4 != 0) fail("invalid Base64 binary value");
    std::vector<std::uint8_t> result;
    for (std::size_t index = 0; index < text.size(); index += 4)
    {
        std::array<int, 4> digits{};
        for (int part = 0; part < 4; ++part)
        {
            if (text[index + part] == '=')
            {
                if (index + 4 != text.size() || part < 2) fail("invalid Base64 padding");
                digits[part] = 0;
            }
            else
            {
                const auto pos = base64_alphabet.find(text[index + part]);
                if (pos == std::string_view::npos) fail("invalid Base64 binary value");
                digits[part] = static_cast<int>(pos);
            }
        }
        const auto word = static_cast<std::uint32_t>((digits[0] << 18) | (digits[1] << 12) |
                                                      (digits[2] << 6) | digits[3]);
        result.push_back(static_cast<std::uint8_t>(word >> 16));
        if (text[index + 2] != '=') result.push_back(static_cast<std::uint8_t>(word >> 8));
        if (text[index + 3] != '=') result.push_back(static_cast<std::uint8_t>(word));
    }
    return result;
}

std::string encode_base64(const std::vector<std::uint8_t>& bytes)
{
    std::string result;
    for (std::size_t index = 0; index < bytes.size(); index += 3)
    {
        const std::uint32_t word = (static_cast<std::uint32_t>(bytes[index]) << 16) |
                                   (index + 1 < bytes.size() ? bytes[index + 1] << 8 : 0) |
                                   (index + 2 < bytes.size() ? bytes[index + 2] : 0);
        result.push_back(base64_alphabet[(word >> 18) & 63]);
        result.push_back(base64_alphabet[(word >> 12) & 63]);
        result.push_back(index + 1 < bytes.size() ? base64_alphabet[(word >> 6) & 63] : '=');
        result.push_back(index + 2 < bytes.size() ? base64_alphabet[word & 63] : '=');
    }
    return result;
}

fb::Offset<protocol::DataSchema> make_schema(fb::FlatBufferBuilder& builder,
                                              const std::vector<Field>& fields,
                                              bool has_group_id, bool has_row_id)
{
    const std::size_t prefix = static_cast<std::size_t>(has_group_id) +
                               static_cast<std::size_t>(has_row_id);
    if (fields.size() < prefix ||
        std::any_of(fields.begin(), fields.begin() + static_cast<std::ptrdiff_t>(prefix),
                    [](const Field& field) { return field.type != "uint64"; }))
    {
        fail("correlation columns must be a uint64 prefix");
    }
    std::vector<fb::Offset<protocol::Field>> offsets;
    for (const auto& field : fields)
    {
        const int bytes = width(field.type);
        protocol::Type type_tag = protocol::Type_NONE;
        fb::Offset<void> type_value;
        if (bytes > 0 && field.type != "float64")
        {
            type_tag = protocol::Type_Int;
            type_value = protocol::CreateInt(builder, bytes * 8, is_signed(field.type)).Union();
        }
        else if (field.type == "float64")
        {
            type_tag = protocol::Type_FloatingPoint;
            type_value = protocol::CreateFloatingPoint(builder, protocol::Precision_Double).Union();
        }
        else if (field.type == "bool")
        {
            type_tag = protocol::Type_Bool;
            type_value = protocol::CreateBool(builder).Union();
        }
        else if (field.type == "utf8")
        {
            type_tag = protocol::Type_Utf8;
            type_value = protocol::CreateUtf8(builder).Union();
        }
        else
        {
            type_tag = protocol::Type_Binary;
            type_value = protocol::CreateBinary(builder).Union();
        }
        offsets.push_back(protocol::CreateField(builder, builder.CreateString(field.name),
                                                field.nullable, type_tag, type_value));
    }
    const auto schema = protocol::CreateSchema(builder, builder.CreateVector(offsets));
    return protocol::CreateDataSchema(builder, schema, has_group_id, has_row_id);
}

std::vector<Field> parse_schema(const protocol::DataSchema* data_schema)
{
    if (data_schema == nullptr || data_schema->schema() == nullptr ||
        data_schema->schema()->fields() == nullptr)
    {
        fail("missing data schema");
    }
    std::vector<Field> fields;
    for (const auto* field : *data_schema->schema()->fields())
    {
        if (field == nullptr || field->name() == nullptr) fail("missing Arrow field name");
        if (field->children() != nullptr && !field->children()->empty())
            fail("nested Arrow fields are unsupported");
        if (field->custom_metadata() != nullptr && !field->custom_metadata()->empty())
            fail("extension Arrow fields are unsupported");
        std::string type;
        switch (field->type_type())
        {
            case protocol::Type_Int:
            {
                const auto* integer = field->type_as_Int();
                if (integer == nullptr) fail("missing integer type");
                type = integer->is_signed() ? "int" : "uint";
                type += std::to_string(integer->bit_width());
                break;
            }
            case protocol::Type_FloatingPoint:
                if (field->type_as_FloatingPoint() == nullptr ||
                    field->type_as_FloatingPoint()->precision() != protocol::Precision_Double)
                    fail("unsupported floating point precision");
                type = "float64";
                break;
            case protocol::Type_Bool: type = "bool"; break;
            case protocol::Type_Utf8: type = "utf8"; break;
            case protocol::Type_Binary: type = "binary"; break;
            default: fail("unsupported Arrow field type");
        }
        width(type);
        fields.push_back({field->name()->str(), type, field->nullable()});
    }
    const std::size_t prefix = static_cast<std::size_t>(data_schema->has_group_id()) +
                               static_cast<std::size_t>(data_schema->has_row_id());
    if (fields.size() < prefix ||
        std::any_of(fields.begin(), fields.begin() + static_cast<std::ptrdiff_t>(prefix),
                    [](const Field& field) { return field.type != "uint64"; }))
        fail("invalid correlation prefix");
    return fields;
}

Batch make_batch(fb::FlatBufferBuilder& builder, const Json& columns, bool is_end_of_group)
{
    const auto fields = fields_from_columns(columns);
    const std::size_t rows = columns.at(0).at("values").size();
    if (rows > max_rows) fail("too many rows");
    Batch batch;
    std::vector<protocol::FieldNode> nodes;
    std::vector<protocol::Buffer> descriptors;
    for (std::size_t column_index = 0; column_index < fields.size(); ++column_index)
    {
        const auto& field = fields[column_index];
        const auto& values = columns.at(column_index).at("values");
        if (!values.is_array() || values.size() != rows) fail("unequal column lengths");
        const int bytes = width(field.type);
        const auto nulls = std::count_if(values.begin(), values.end(),
                                         [](const Json& value) { return value.is_null(); });
        if (nulls > 0 && !field.nullable) fail("null in nonnullable column");
        nodes.emplace_back(static_cast<std::int64_t>(rows), static_cast<std::int64_t>(nulls));
        std::vector<std::uint8_t> validity(nulls ? (rows + 7) / 8 : 0, 0);
        std::vector<std::uint8_t> values_buffer;
        std::vector<std::uint8_t> data_buffer;
        if (bytes < 0) append_le(values_buffer, 0, 4);
        if (bytes == 0) values_buffer.resize((rows + 7) / 8, 0);
        for (std::size_t row = 0; row < rows; ++row)
        {
            const auto& value = values.at(row);
            if (!value.is_null())
            {
                if (nulls) set_bit(validity, row);
                if (bytes > 0 && field.type != "float64")
                {
                    const auto integer = is_signed(field.type) ?
                        static_cast<std::uint64_t>(value.get<std::int64_t>()) :
                        value.get<std::uint64_t>();
                    append_le(values_buffer, integer, bytes);
                }
                else if (field.type == "float64")
                    append_le(values_buffer, std::bit_cast<std::uint64_t>(value.get<double>()), 8);
                else if (field.type == "bool")
                {
                    if (value.get<bool>()) set_bit(values_buffer, row);
                }
                else
                {
                    const auto text = value.get<std::string>();
                    const auto item = field.type == "binary" ?
                        decode_base64(text) :
                        std::vector<std::uint8_t>(text.begin(), text.end());
                    data_buffer.insert(data_buffer.end(), item.begin(), item.end());
                }
            }
            else if (bytes > 0)
                append_le(values_buffer, 0, bytes);
            if (bytes < 0)
            {
                if (data_buffer.size() > std::numeric_limits<std::int32_t>::max())
                    fail("variable-length data buffer too large");
                append_le(values_buffer, data_buffer.size(), 4);
            }
        }
        auto add = [&](std::vector<std::uint8_t>&& buffer) {
            if (buffer.size() > max_buffer || batch.bytes + buffer.size() > max_batch)
                fail("batch buffer limit exceeded");
            batch.bytes += buffer.size();
            descriptors.emplace_back(0, static_cast<std::int64_t>(buffer.size()));
            batch.buffers.push_back(std::move(buffer));
        };
        add(std::move(validity));
        add(std::move(values_buffer));
        if (bytes < 0) add(std::move(data_buffer));
    }
    batch.metadata = protocol::CreateDataRecordBatchMetadataDirect(
        builder, protocol::BufferTransport_Inline, is_end_of_group,
        static_cast<std::int64_t>(rows), &nodes, &descriptors);
    return batch;
}

Json parse_batch(const protocol::DataRecordBatchMetadata* metadata,
                 const std::vector<Field>& fields,
                 const std::vector<std::vector<std::uint8_t>>& buffers)
{
    if (metadata->length() < 0 || static_cast<std::uint64_t>(metadata->length()) > max_rows ||
        metadata->nodes() == nullptr || metadata->nodes()->size() != fields.size())
        fail("invalid record batch node count or row count");
    const auto rows = static_cast<std::size_t>(metadata->length());
    Json columns = Json::array();
    std::size_t buffer_index = 0;
    for (std::size_t column_index = 0; column_index < fields.size(); ++column_index)
    {
        const auto& field = fields[column_index];
        const auto* node = metadata->nodes()->Get(column_index);
        if (node->length() != metadata->length() || node->null_count() < 0 ||
            static_cast<std::uint64_t>(node->null_count()) > rows)
            fail("invalid field node");
        const int bytes = width(field.type);
        const std::size_t needed = bytes < 0 ? 3 : 2;
        if (buffer_index + needed > buffers.size()) fail("missing Arrow buffers");
        const auto& validity = buffers[buffer_index];
        const auto& values = buffers[buffer_index + 1];
        if (node->null_count() && validity.size() < (rows + 7) / 8)
            fail("truncated validity bitmap");
        if (bytes == 0 && values.size() < (rows + 7) / 8)
            fail("truncated boolean values");
        if (bytes > 0 && values.size() < rows * static_cast<std::size_t>(bytes))
            fail("truncated fixed-width values");
        if (bytes < 0 && values.size() < (rows + 1) * 4)
            fail("truncated variable-length offsets");
        Json output_values = Json::array();
        for (std::size_t row = 0; row < rows; ++row)
        {
            if (node->null_count() && !bit(validity, row))
            {
                output_values.push_back(nullptr);
                continue;
            }
            if (bytes > 0 && field.type != "float64")
            {
                std::uint64_t raw = read_le(values, row * bytes, bytes);
                if (is_signed(field.type))
                {
                    if (bytes < 8 && (raw & (1ULL << (bytes * 8 - 1))))
                        raw |= ~((1ULL << (bytes * 8)) - 1);
                    output_values.push_back(std::bit_cast<std::int64_t>(raw));
                }
                else output_values.push_back(raw);
            }
            else if (field.type == "float64")
                output_values.push_back(std::bit_cast<double>(read_le(values, row * 8, 8)));
            else if (field.type == "bool")
                output_values.push_back(bit(values, row));
            else
            {
                const auto start = read_le(values, row * 4, 4);
                const auto end = read_le(values, (row + 1) * 4, 4);
                const auto& data = buffers[buffer_index + 2];
                if (start > end || end > data.size()) fail("invalid variable-length offsets");
                const std::vector<std::uint8_t> item(data.begin() + static_cast<std::ptrdiff_t>(start),
                                                     data.begin() + static_cast<std::ptrdiff_t>(end));
                output_values.push_back(field.type == "binary" ? encode_base64(item) :
                                        std::string(item.begin(), item.end()));
            }
        }
        columns.push_back({{"name", field.name}, {"type", field.type},
                           {"nullable", field.nullable}, {"values", output_values}});
        buffer_index += needed;
    }
    if (buffer_index != buffers.size()) fail("extra Arrow buffers");
    return columns;
}

void write_all(transport::UnixSocket& socket, const std::uint8_t* data, std::size_t size)
{
    std::size_t position = 0;
    while (position < size)
    {
        const auto count = socket.write_some(std::span(
            reinterpret_cast<const std::byte*>(data + position), size - position));
        if (count == 0) fail("socket closed while writing");
        position += count;
    }
}

std::vector<std::uint8_t> read_all(transport::UnixSocket& socket, std::size_t size)
{
    std::vector<std::uint8_t> result(size);
    std::size_t position = 0;
    while (position < size)
    {
        const auto count = socket.read_some(std::span(
            reinterpret_cast<std::byte*>(result.data() + position), size - position));
        if (count == 0) fail("truncated frame or inline buffer");
        position += count;
    }
    return result;
}

class Session
{
public:
    Session(transport::UnixSocket socket, Role role, int timeout_seconds)
        : socket_(std::move(socket)), role_(role)
    {
        if (timeout_seconds <= 0) fail("timeout must be positive");
        const timeval timeout{timeout_seconds, 0};
        if (::setsockopt(socket_.native_handle(), SOL_SOCKET, SO_RCVTIMEO, &timeout,
                         sizeof(timeout)) != 0 ||
            ::setsockopt(socket_.native_handle(), SOL_SOCKET, SO_SNDTIMEO, &timeout,
                         sizeof(timeout)) != 0)
            fail("failed to configure socket timeout");
    }

    void send(const Json& step)
    {
        const auto stream_id = step.at("stream_id").get<std::uint64_t>();
        const auto kind = step.at("kind").get<std::string>();
        validate(stream_id, kind, true);
        fb::FlatBufferBuilder builder;
        const bool has_columns = step.contains("columns");
        const bool schema = step.value("schema", false) || kind == "schema";
        const bool batch = has_columns && !(kind == "schema" && !step.value("batch", true));
        const auto fields = has_columns ? fields_from_columns(step.at("columns")) : std::vector<Field>{};
        if ((kind == "batch" || kind == "schema") && !has_columns)
            fail("batch/schema step requires columns");
        if (schema)
        {
            if (sent_schemas_.contains(stream_id)) fail("schema already sent");
            sent_fields_[stream_id] = fields;
        }
        if (batch)
        {
            if (!schema && !sent_schemas_.contains(stream_id)) fail("batch sent before schema");
            if (!schema && fields != sent_fields_.at(stream_id)) fail("batch schema mismatch");
            if (stream_id == 0) fail("batch requires a call stream");
            if (step.value("is_end_of_group", false) &&
                !(schema ? step.value("has_group_id", false) : sent_has_group_[stream_id]))
                fail("end-of-group requires group ID");
        }
        Batch batch_data;
        if (batch) batch_data = make_batch(builder, step.at("columns"),
                                           step.value("is_end_of_group", false));
        if (batch && first_sent_.contains(stream_id))
        {
            auto& budget = received_budgets_[stream_id];
            if (budget == 0) fail("later batch requires Next");
            budget = batch_data.bytes >= budget ? 0 : budget - batch_data.bytes;
        }
        const auto data_schema = schema ? make_schema(builder, fields,
                            step.value("has_group_id", false), step.value("has_row_id", false)) :
                            fb::Offset<protocol::DataSchema>{};
        const auto control = make_control(builder, step, data_schema);
        const auto frame = protocol::CreateFrame(builder, stream_id, control, batch_data.metadata);
        builder.Finish(frame);
        if (builder.GetSize() == 0 || builder.GetSize() > max_frame) fail("frame size limit exceeded");
        std::vector<std::uint8_t> prefix;
        append_le(prefix, builder.GetSize(), 4);
        write_all(socket_, prefix.data(), prefix.size());
        write_all(socket_, builder.GetBufferPointer(), builder.GetSize());
        for (const auto& buffer : batch_data.buffers)
            write_all(socket_, buffer.data(), buffer.size());
        if (schema)
        {
            sent_schemas_.insert(stream_id);
            sent_has_group_[stream_id] = step.value("has_group_id", false);
        }
        if (batch) first_sent_.insert(stream_id);
        if (step.contains("byte_budget"))
            granted_budgets_[stream_id] = step.at("byte_budget").get<std::uint32_t>();
    }

    Json receive()
    {
        const auto prefix = read_all(socket_, 4);
        const auto frame_size = read_le(prefix, 0, 4);
        if (frame_size == 0 || frame_size > max_frame) fail("invalid frame length");
        const auto frame_bytes = read_all(socket_, frame_size);
        if (!protocol::verify_frame_buffer(frame_bytes.data(), frame_bytes.size()))
            fail("malformed FlatBuffer frame");
        const auto* frame = protocol::GetFrame(frame_bytes.data());
        const auto stream_id = frame->stream_id();
        const auto* control = frame->control_message();
        const auto* metadata = frame->data_record_batch_metadata();
        Json event = parse_control(control);
        if (control != nullptr && control->data_schema() != nullptr)
        {
            if (received_schemas_.contains(stream_id)) fail("duplicate schema");
            received_fields_[stream_id] = parse_schema(control->data_schema());
            received_schemas_.insert(stream_id);
            event["schema"] = true;
            received_has_group_[stream_id] = control->data_schema()->has_group_id();
        }
        if (metadata != nullptr)
        {
            if (!received_schemas_.contains(stream_id)) fail("batch before schema");
            if (metadata->buffer_transport() != protocol::BufferTransport_Inline)
                fail("only inline transport is supported");
            if (metadata->is_end_of_group() && !received_has_group_[stream_id])
                fail("end-of-group requires group ID");
            if (metadata->buffers() == nullptr) fail("missing buffer descriptions");
            std::size_t total = 0;
            std::vector<std::vector<std::uint8_t>> buffers;
            for (const auto* descriptor : *metadata->buffers())
            {
                if (descriptor->length() < 0 ||
                    static_cast<std::uint64_t>(descriptor->length()) > max_buffer ||
                    total + static_cast<std::size_t>(descriptor->length()) > max_batch)
                    fail("invalid inline buffer length");
                total += static_cast<std::size_t>(descriptor->length());
                buffers.push_back(read_all(socket_, static_cast<std::size_t>(descriptor->length())));
            }
            if (first_received_.contains(stream_id))
            {
                auto& budget = granted_budgets_[stream_id];
                if (budget == 0) fail("later batch without Next");
                budget = total >= budget ? 0 : budget - total;
            }
            event["columns"] = parse_batch(metadata, received_fields_.at(stream_id), buffers);
            event["rows"] = metadata->length();
            event["is_end_of_group"] = metadata->is_end_of_group();
            first_received_.insert(stream_id);
            if (!event.contains("kind")) event["kind"] = "batch";
        }
        else event["is_end_of_group"] = false;
        if (!event.contains("kind")) fail("frame has no supported content");
        validate(stream_id, event.at("kind").get<std::string>(), false);
        if (control != nullptr && control->next() != nullptr)
            received_budgets_[stream_id] = control->next()->byte_budget();
        event["stream_id"] = stream_id;
        return event;
    }

private:
    fb::Offset<protocol::ControlMessage> make_control(
        fb::FlatBufferBuilder& builder, const Json& step,
        fb::Offset<protocol::DataSchema> schema)
    {
        const auto kind = step.at("kind").get<std::string>();
        protocol::ControlMessageValue tag = protocol::ControlMessageValue_NONE;
        fb::Offset<void> value;
        if (kind == "capabilities")
        {
            tag = protocol::ControlMessageValue_ServerCapabilities;
            const auto version = protocol::CreateVersion(builder, 2, 0);
            value = protocol::CreateServerCapabilities(builder, version,
                        protocol::Endianness_Little, 1).Union();
        }
        else if (kind == "open")
        {
            tag = protocol::ControlMessageValue_OpenCall;
            value = protocol::CreateOpenCall(builder,
                        builder.CreateString(step.at("call_name").get<std::string>())).Union();
        }
        else if (kind == "keepalive")
        {
            tag = protocol::ControlMessageValue_KeepAlive;
            value = protocol::CreateKeepAlive(builder).Union();
        }
        else if (kind == "close_call" || kind == "close_connection")
        {
            tag = protocol::ControlMessageValue_CloseControlMessage;
            value = kind == "close_call" ?
                protocol::CreateCloseControlMessage(builder,
                    protocol::CreateCloseCall(builder)).Union() :
                protocol::CreateCloseControlMessage(builder, {},
                    protocol::CreateCloseConnection(builder)).Union();
        }
        else if (kind != "next" && kind != "schema" && kind != "batch" && kind != "payloads")
            fail("unsupported control kind: " + kind);
        fb::Offset<protocol::Next> next;
        if (step.contains("byte_budget"))
            next = protocol::CreateNext(builder, step.at("byte_budget").get<std::uint32_t>(),
                    step.value("reset", false), step.value("row_id", 0ULL));
        fb::Offset<protocol::Payloads> payloads;
        if (step.contains("payloads"))
        {
            std::vector<fb::Offset<protocol::Payload>> items;
            for (auto item = step.at("payloads").begin(); item != step.at("payloads").end(); ++item)
            {
                const auto body = protocol::CreateStringPayload(builder,
                                                builder.CreateString(item.value().get<std::string>()));
                items.push_back(protocol::CreatePayload(builder, builder.CreateString(item.key()),
                                 protocol::PayloadValue_StringPayload, body.Union()));
            }
            payloads = protocol::CreatePayloads(builder, builder.CreateVector(items));
        }
        fb::Offset<protocol::Error> error;
        if (step.contains("error"))
            error = protocol::CreateError(builder,
                     builder.CreateString(step.value("error_code", "SIMULATOR")),
                     builder.CreateString(step.at("error").get<std::string>()));
        if (tag == protocol::ControlMessageValue_NONE && !next.o && !payloads.o &&
            !schema.o && !error.o) return {};
        return protocol::CreateControlMessage(builder, payloads, next, schema, error, tag, value);
    }

    Json parse_control(const protocol::ControlMessage* control)
    {
        Json event = Json::object();
        if (control == nullptr) return event;
        switch (control->value_type())
        {
            case protocol::ControlMessageValue_ServerCapabilities:
            {
                const auto* value = control->value_as_ServerCapabilities();
                if (value == nullptr || value->supported_version() == nullptr ||
                    value->supported_version()->major() != 2 ||
                    value->number_of_supported_workers() == 0)
                    fail("incompatible server capabilities");
                event["kind"] = "capabilities";
                break;
            }
            case protocol::ControlMessageValue_OpenCall:
                if (control->value_as_OpenCall() == nullptr ||
                    control->value_as_OpenCall()->call_name() == nullptr)
                    fail("missing call name");
                event["kind"] = "open";
                event["call_name"] = control->value_as_OpenCall()->call_name()->str();
                break;
            case protocol::ControlMessageValue_KeepAlive:
                event["kind"] = "keepalive";
                break;
            case protocol::ControlMessageValue_CloseControlMessage:
                if (control->value_as_CloseControlMessage() == nullptr)
                    fail("missing close message");
                event["kind"] = control->value_as_CloseControlMessage()->close_connection() != nullptr ?
                                "close_connection" : "close_call";
                break;
            case protocol::ControlMessageValue_NONE: break;
            default: fail("unsupported control message");
        }
        if (control->next() != nullptr)
        {
            if (!event.contains("kind")) event["kind"] = "next";
            event["byte_budget"] = control->next()->byte_budget();
            event["reset"] = control->next()->reset();
            event["row_id"] = control->next()->row_id();
        }
        if (control->data_schema() != nullptr && !event.contains("kind")) event["kind"] = "schema";
        if (control->payloads() != nullptr)
        {
            if (!event.contains("kind")) event["kind"] = "payloads";
            Json items = Json::object();
            if (control->payloads()->payloads() != nullptr)
            {
                for (const auto* item : *control->payloads()->payloads())
                {
                    if (item == nullptr || item->name() == nullptr ||
                        item->payload_type() != protocol::PayloadValue_StringPayload ||
                        item->payload_as_StringPayload() == nullptr ||
                        item->payload_as_StringPayload()->value() == nullptr)
                        fail("only string payloads are supported");
                    items[item->name()->str()] = item->payload_as_StringPayload()->value()->str();
                }
            }
            event["payloads"] = std::move(items);
        }
        if (control->error() != nullptr)
        {
            if (control->error()->message() == nullptr || control->error()->code() == nullptr)
                fail("malformed error message");
            event["error"] = control->error()->message()->str();
            event["error_code"] = control->error()->code()->str();
        }
        return event;
    }

    void validate(std::uint64_t stream_id, const std::string& kind, bool sending)
    {
        if (closing_ && kind != "close_connection") fail("traffic after CloseConnection");
        if (kind == "capabilities" || kind == "keepalive" || kind == "close_connection")
        {
            if (stream_id != 0) fail("control message requires stream zero");
            if (kind == "capabilities" && (sending != (role_ == Role::Runner)))
                fail("capabilities must come from the runner");
        }
        else if (stream_id == 0 && kind != "payloads") fail("call traffic requires nonzero stream");
        if (kind == "open")
        {
            const bool local_odd = role_ == Role::Client;
            const bool expected_odd = sending ? local_odd : !local_odd;
            if (static_cast<bool>(stream_id & 1) != expected_odd || calls_.contains(stream_id))
                fail("invalid or reused stream ID");
            calls_.insert(stream_id);
        }
        else if (stream_id != 0 && !calls_.contains(stream_id))
            fail("traffic on unopened stream");
        if (closed_calls_.contains(stream_id) && kind != "close_call")
            fail("traffic on closed call");
        if (kind == "close_call") closed_calls_.insert(stream_id);
        if (kind == "close_connection") closing_ = true;
    }

    transport::UnixSocket socket_;
    Role role_;
    bool closing_ = false;
    std::map<std::uint64_t, std::vector<Field>> sent_fields_;
    std::map<std::uint64_t, std::vector<Field>> received_fields_;
    std::map<std::uint64_t, bool> sent_has_group_;
    std::map<std::uint64_t, bool> received_has_group_;
    std::map<std::uint64_t, std::size_t> received_budgets_;
    std::map<std::uint64_t, std::size_t> granted_budgets_;
    std::set<std::uint64_t> calls_;
    std::set<std::uint64_t> closed_calls_;
    std::set<std::uint64_t> sent_schemas_;
    std::set<std::uint64_t> received_schemas_;
    std::set<std::uint64_t> first_sent_;
    std::set<std::uint64_t> first_received_;
};

void check_expected(const Json& step, const Json& event)
{
    for (auto key : {"kind", "stream_id", "call_name", "rows", "payloads", "byte_budget",
                     "reset", "row_id", "schema", "is_end_of_group", "error", "error_code"})
    {
        if (step.contains(key) && (!event.contains(key) || step.at(key) != event.at(key)))
            fail(std::string("unexpected ") + key + ": " + event.dump());
    }
    if (step.contains("columns") && step.at("columns") != event.value("columns", Json{}))
        fail("unexpected batch columns: " + event.dump());
}

Json column(std::string name, std::string type, bool nullable, Json values)
{
    return {{"name", std::move(name)}, {"type", std::move(type)},
            {"nullable", nullable}, {"values", std::move(values)}};
}

Json columns(const Json& values)
{
    return Json::array({column("group_id", "uint64", false, values.at(0)),
                        column("row_id", "uint64", false, values.at(1)),
                        column("value", "utf8", true, values.at(2))});
}

} // namespace

Json default_steps(Role role)
{
    const auto first = columns(Json::array({Json::array({7, 7}), Json::array({1, 2}),
                                            Json::array({"left", nullptr})}));
    const auto second = columns(Json::array({Json::array({7}), Json::array({3}),
                                             Json::array({"right"})}));
    const auto out_first = columns(Json::array({Json::array({7, 7}), Json::array({1, 2}),
                                                Json::array({"LEFT", nullptr})}));
    const auto out_second = columns(Json::array({Json::array({7}), Json::array({3}),
                                                 Json::array({"RIGHT"})}));
    if (role == Role::Client)
    {
        const Json call_metadata = {
            {"database_name", "EXASOL"}, {"database_version", "8.0"}, {"session_id", "1"},
            {"statement_id", 1}, {"node_count", 1}, {"node_id", 0}, {"vm_id", "1"},
            {"maximal_memory_limit", "1073741824"}, {"script_schema", "SYS"},
            {"input_iter_type", "EXACTLY_ONCE"}, {"output_iter_type", "EXACTLY_ONCE"},
            {"single_call_mode", false}};
        const Json column_metadata = {
            {"input_columns", Json::array({{{"name", "VALUE"}, {"type", "VARCHAR"},
                                             {"type_name", "VARCHAR(100)"}, {"size", 100},
                                             {"character_set", "UTF8"}}})},
            {"output_columns", Json::array({{{"name", "VALUE"}, {"type", "VARCHAR"},
                                              {"type_name", "VARCHAR(100)"}, {"size", 100},
                                              {"character_set", "UTF8"}}})}};
        return Json::array({
            {{"action", "expect"}, {"kind", "capabilities"}, {"stream_id", 0}},
            {{"action", "send"}, {"kind", "open"}, {"call_name", "Run"},
             {"stream_id", 1}, {"columns", first}, {"schema", true},
             {"has_group_id", true}, {"has_row_id", true},
             {"payloads", {{"script_name", "SIMULATOR"}, {"script_source", ""},
                            {"call_metadata", call_metadata.dump()},
                            {"column_metadata", column_metadata.dump()}}}},
            {{"action", "expect"}, {"kind", "next"}, {"stream_id", 1}},
            {{"action", "send"}, {"kind", "batch"}, {"stream_id", 1},
             {"columns", second}, {"is_end_of_group", true}},
            {{"action", "expect"}, {"kind", "schema"}, {"stream_id", 1}, {"rows", 2},
             {"columns", out_first}},
            {{"action", "send"}, {"kind", "next"}, {"stream_id", 1},
             {"byte_budget", 4096}},
            {{"action", "expect"}, {"kind", "batch"}, {"stream_id", 1}, {"rows", 1},
             {"columns", out_second}},
            {{"action", "send"}, {"kind", "close_call"}, {"stream_id", 1}},
            {{"action", "send"}, {"kind", "close_connection"}, {"stream_id", 0}},
            {{"action", "expect"}, {"kind", "close_connection"}, {"stream_id", 0}},
        });
    }
    return Json::array({
        {{"action", "send"}, {"kind", "capabilities"}, {"stream_id", 0},
         {"payloads", {{"high_level_name", "exasol.udf"},
                        {"high_level_version", "2.0-dev"}}}},
        {{"action", "expect"}, {"kind", "open"}, {"call_name", "Run"},
         {"stream_id", 1}, {"rows", 2}, {"columns", first}},
        {{"action", "send"}, {"kind", "next"}, {"stream_id", 1}, {"byte_budget", 4096}},
        {{"action", "expect"}, {"kind", "batch"}, {"stream_id", 1},
         {"rows", 1}, {"columns", second}},
        {{"action", "send"}, {"kind", "schema"}, {"stream_id", 1},
         {"columns", out_first}, {"has_group_id", true}, {"has_row_id", true}},
        {{"action", "expect"}, {"kind", "next"}, {"stream_id", 1}},
        {{"action", "send"}, {"kind", "batch"}, {"stream_id", 1},
         {"columns", out_second}, {"is_end_of_group", true}},
        {{"action", "expect"}, {"kind", "close_call"}, {"stream_id", 1}},
        {{"action", "expect"}, {"kind", "close_connection"}, {"stream_id", 0}},
        {{"action", "send"}, {"kind", "close_connection"}, {"stream_id", 0}},
    });
}

Json load_steps(const std::filesystem::path& path)
{
    std::ifstream input(path);
    if (!input) fail("cannot open scenario file: " + path.string());
    Json steps;
    input >> steps;
    if (!steps.is_array()) fail("scenario must be a JSON array");
    return steps;
}

void run(Role role, const std::filesystem::path& socket_path, const Json& steps,
         int timeout_seconds)
{
    if (!steps.is_array()) fail("scenario must be a JSON array");
    auto execute = [&](transport::UnixSocket socket) {
        Session session(std::move(socket), role, timeout_seconds);
        for (const auto& step : steps)
        {
            const auto action = step.at("action").get<std::string>();
            if (action == "send") session.send(step);
            else if (action == "expect") check_expected(step, session.receive());
            else fail("unknown scenario action: " + action);
        }
    };
    if (role == Role::Client)
    {
        execute(transport::UnixSocket::connect(socket_path));
    }
    else
    {
        auto listener = transport::UnixSocketListener::bind(socket_path);
        try
        {
            execute(listener.accept());
        }
        catch (...)
        {
            listener.close();
            listener.unlink_path();
            throw;
        }
        listener.close();
        listener.unlink_path();
    }
}

} // namespace exasol::udf::simulator
