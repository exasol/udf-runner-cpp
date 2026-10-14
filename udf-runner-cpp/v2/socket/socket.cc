#include <exasol/udf/v2/socket/socket.hpp>

#include <array>

namespace exasol::udf::v2::socket
{

std::size_t Socket::read_some(const std::span<std::byte> buffer)
{
    const std::array buffers{buffer};
    return read_some(buffers);
}

std::size_t Socket::write_some(const std::span<const std::byte> buffer)
{
    const std::array buffers{buffer};
    return write_some(buffers);
}

} // namespace exasol::udf::v2::socket
