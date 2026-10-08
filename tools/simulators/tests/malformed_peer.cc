#include "udf_protocol.hpp"

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
namespace protocol = exasol::udf::protocol;
namespace fb = exasol::udf::v2::third_party::flatbuffers;

void write_all(int fd, const void* data, std::size_t size)
{
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    while (size != 0)
    {
        const auto count = ::write(fd, bytes, size);
        if (count <= 0) throw std::runtime_error("write failed");
        bytes += count;
        size -= static_cast<std::size_t>(count);
    }
}

void send_frame(int fd, fb::FlatBufferBuilder& builder)
{
    const auto length = static_cast<std::uint32_t>(builder.GetSize());
    const std::uint8_t prefix[4] = {
        static_cast<std::uint8_t>(length), static_cast<std::uint8_t>(length >> 8),
        static_cast<std::uint8_t>(length >> 16), static_cast<std::uint8_t>(length >> 24)};
    write_all(fd, prefix, sizeof(prefix));
    write_all(fd, builder.GetBufferPointer(), builder.GetSize());
}

void send_invalid_stream(int fd)
{
    fb::FlatBufferBuilder builder;
    const auto open = protocol::CreateOpenCall(builder, builder.CreateString("Run"));
    const auto control = protocol::CreateControlMessage(builder, {}, {}, {}, {},
                           protocol::ControlMessageValue_OpenCall, open.Union());
    builder.Finish(protocol::CreateFrame(builder, 2, control));
    send_frame(fd, builder);
}

void send_truncated_buffer(int fd)
{
    fb::FlatBufferBuilder builder;
    const auto integer = protocol::CreateInt(builder, 64, false);
    const auto field = protocol::CreateField(builder, builder.CreateString("row_id"),
                          false, protocol::Type_Int, integer.Union());
    const auto schema = protocol::CreateSchema(builder,
                            builder.CreateVector(std::vector<fb::Offset<protocol::Field>>{field}));
    const auto data_schema = protocol::CreateDataSchema(builder, schema, false, true);
    const auto open = protocol::CreateOpenCall(builder, builder.CreateString("Run"));
    const auto control = protocol::CreateControlMessage(builder, {}, {}, data_schema, {},
                           protocol::ControlMessageValue_OpenCall, open.Union());
    const std::vector<protocol::FieldNode> nodes{{1, 0}};
    const std::vector<protocol::Buffer> buffers{{0, 0}, {0, 8}};
    const auto metadata = protocol::CreateDataRecordBatchMetadataDirect(builder,
                            protocol::BufferTransport_Inline, false, 1, &nodes, &buffers);
    builder.Finish(protocol::CreateFrame(builder, 1, control, metadata));
    send_frame(fd, builder);
    const std::uint8_t only_one_byte = 42;
    write_all(fd, &only_one_byte, 1);
}
} // namespace

int main(int argc, char** argv)
{
    if (argc != 3) return 2;
    const std::string path = argv[1];
    if (path.size() >= sizeof(sockaddr_un::sun_path)) return 2;
    const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return 2;
    sockaddr_un address{};
    address.sun_family = AF_UNIX;
    std::memcpy(address.sun_path, path.c_str(), path.size() + 1);
    if (::connect(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) return 2;
    const std::string mode = argv[2];
    if (mode == "zero_length")
    {
        const std::uint8_t prefix[4]{};
        write_all(fd, prefix, sizeof(prefix));
    }
    else if (mode == "invalid_stream") send_invalid_stream(fd);
    else if (mode == "truncated_buffer") send_truncated_buffer(fd);
    else return 2;
    ::close(fd);
    return 0;
}
