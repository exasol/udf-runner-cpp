#include <exasol/udf/v2/socket.hpp>

#include <array>
#include <stdexcept>

namespace exasol::udf::v2::socket
{

OwnedFileDescriptor::OwnedFileDescriptor() noexcept = default;

OwnedFileDescriptor::~OwnedFileDescriptor() = default;

OwnedFileDescriptor OwnedFileDescriptor::adopt_native_handle(const int)
{
    throw std::logic_error("socket library is not implemented yet");
}

OwnedFileDescriptor::OwnedFileDescriptor(OwnedFileDescriptor&& other) noexcept = default;

OwnedFileDescriptor& OwnedFileDescriptor::operator=(OwnedFileDescriptor&& other) noexcept = default;

int OwnedFileDescriptor::native_handle() const noexcept
{
    return file_descriptor;
}

bool OwnedFileDescriptor::is_open() const noexcept
{
    return false;
}

void OwnedFileDescriptor::close() noexcept
{
}

int OwnedFileDescriptor::release_native_handle() noexcept
{
    return -1;
}

OwnedFileDescriptor::OwnedFileDescriptor(const int owned_fd) noexcept : file_descriptor(owned_fd)
{
}

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
