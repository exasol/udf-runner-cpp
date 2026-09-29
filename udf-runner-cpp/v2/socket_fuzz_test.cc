#include <exasol/udf/v2/unix_socket.hpp>

#include <sys/socket.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, const std::size_t size)
{
    if (size > 4096)
    {
        return 0;
    }
    std::array<int, 2> descriptors{};
    if (::socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, descriptors.data()) != 0)
    {
        return 0;
    }
    auto writer = exasol::udf::v2::socket::UnixSocket::adopt_native_handle(
        exasol::udf::v2::socket::OwnedFileDescriptor::adopt_native_handle(descriptors[0]));
    auto reader = exasol::udf::v2::socket::UnixSocket::adopt_native_handle(
        exasol::udf::v2::socket::OwnedFileDescriptor::adopt_native_handle(descriptors[1]));
    const std::span bytes(reinterpret_cast<const std::byte*>(data), size);
    const std::array buffers{bytes.first(size / 2), bytes.subspan(size / 2)};
    if (writer.write_some(buffers) != size)
    {
        return 0;
    }
    std::array<std::byte, 4096> received{};
    if (reader.read_some(std::span(received).first(size)) != size)
    {
        __builtin_trap();
    }
    return 0;
}
