#include <exasol/udf/v2/socket/socket.hpp>

#include <array>
#include <stdexcept>
#include <unistd.h>
#include <utility>

namespace exasol::udf::v2::socket
{

OwnedFileDescriptor::OwnedFileDescriptor() noexcept = default;

OwnedFileDescriptor::~OwnedFileDescriptor()
{
    close();
}

OwnedFileDescriptor OwnedFileDescriptor::adopt_native_handle(const int owned_fd)
{
    if (owned_fd < 0)
    {
        throw std::invalid_argument("adopt invalid file descriptor");
    }
    return OwnedFileDescriptor(owned_fd);
}

OwnedFileDescriptor::OwnedFileDescriptor(OwnedFileDescriptor&& other) noexcept
    : file_descriptor(std::exchange(other.file_descriptor, -1))
{
}

OwnedFileDescriptor& OwnedFileDescriptor::operator=(OwnedFileDescriptor&& other) noexcept
{
    if (this != &other)
    {
        this->close();
        this->file_descriptor = std::exchange(other.file_descriptor, -1);
    }
    return *this;
}

int OwnedFileDescriptor::native_handle() const noexcept
{
    return this->file_descriptor;
}

bool OwnedFileDescriptor::is_open() const noexcept
{
    return this->file_descriptor != -1;
}

void OwnedFileDescriptor::close() noexcept
{
    const int closed_fd = std::exchange(this->file_descriptor, -1);
    if (closed_fd != -1)
    {
        ::close(closed_fd);
    }
}

int OwnedFileDescriptor::release_native_handle() noexcept
{
    return std::exchange(this->file_descriptor, -1);
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
