#include <exasol/udf/v2/socket/owned_file_descriptor.hpp>

#include <stdexcept>
#include <utility>

namespace exasol::udf::v2::socket
{

OwnedFileDescriptor::OwnedFileDescriptor() noexcept = default;

OwnedFileDescriptor::~OwnedFileDescriptor() noexcept
{
    this->close();
}

OwnedFileDescriptor OwnedFileDescriptor::adopt_native_handle(const int owned_fd)
{
    return adopt_native_handle(owned_fd, posix::default_posix());
}

OwnedFileDescriptor OwnedFileDescriptor::adopt_native_handle(const int owned_fd,
                                                             posix::Posix& posix)
{
    if (owned_fd < 0)
    {
        throw std::invalid_argument("adopt invalid file descriptor");
    }
    return OwnedFileDescriptor(owned_fd, posix);
}

OwnedFileDescriptor::OwnedFileDescriptor(OwnedFileDescriptor&& other) noexcept
    : file_descriptor(std::exchange(other.file_descriptor, -1)),
      posix_pointer(std::exchange(other.posix_pointer, &posix::default_posix()))
{
}

OwnedFileDescriptor& OwnedFileDescriptor::operator=(OwnedFileDescriptor&& other) noexcept
{
    if (this != &other)
    {
        this->close();
        this->file_descriptor = std::exchange(other.file_descriptor, -1);
        this->posix_pointer   = std::exchange(other.posix_pointer, &posix::default_posix());
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
        static_cast<void>(this->posix_pointer->close(closed_fd));
    }
}

int OwnedFileDescriptor::release_native_handle() noexcept
{
    return std::exchange(this->file_descriptor, -1);
}

OwnedFileDescriptor::OwnedFileDescriptor(const int owned_fd, posix::Posix& posix) noexcept
    : file_descriptor(owned_fd), posix_pointer(&posix)
{
}

posix::Posix& OwnedFileDescriptor::posix_facade() const noexcept
{
    return *this->posix_pointer;
}

} // namespace exasol::udf::v2::socket
