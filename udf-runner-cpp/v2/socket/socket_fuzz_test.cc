#include <exasol/udf/v2/socket/unix_socket.hpp>

#include <sys/socket.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

/**
 * Exercises vectored Unix-socket writes and matching peer reads using arbitrary fuzzer input.
 *
 * Inputs larger than the fixed 4 KiB receive buffer are ignored. The socket API permits partial
 * writes, so the peer reads exactly the byte count reported by write_some(), rather than the
 * entire fuzzer input. A socketpair creation failure is an infrastructure failure and aborts the
 * fuzzing run.
 */
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, const std::size_t size)
{
    if (size > 4096)
    {
        return 0;
    }
    std::array<int, 2> descriptors{};
    if (::socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, descriptors.data()) != 0)
    {
        __builtin_trap();
    }
    auto writer = exasol::udf::v2::socket::UnixSocket::adopt_native_handle(
        exasol::udf::v2::socket::OwnedFileDescriptor::adopt_native_handle(descriptors[0]));
    auto reader = exasol::udf::v2::socket::UnixSocket::adopt_native_handle(
        exasol::udf::v2::socket::OwnedFileDescriptor::adopt_native_handle(descriptors[1]));
    const std::span bytes(reinterpret_cast<const std::byte*>(data), size);
    const std::size_t written =
        writer.write_some(std::array{bytes.first(size / 2), bytes.subspan(size / 2)});
    if (std::array<std::byte, 4096> received{};
        reader.read_some(std::span(received).first(written)) != written)
    {
        __builtin_trap();
    }
    return 0;
}
