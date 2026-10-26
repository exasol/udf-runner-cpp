#include <exasol/udf/v2/socket/owned_file_descriptor.hpp>

#include <stdexcept>
#include <utility>

namespace exasol::udf::v2::socket
{

OwnedFileDescriptor::OwnedFileDescriptor(posix::Posix& posix) noexcept : m_posix_pointer(&posix)
{
}

OwnedFileDescriptor::~OwnedFileDescriptor() noexcept
{
    this->close();
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
    : m_file_descriptor(std::exchange(other.m_file_descriptor, -1)),
      m_posix_pointer(std::exchange(other.m_posix_pointer, nullptr))
{
}

OwnedFileDescriptor& OwnedFileDescriptor::operator=(OwnedFileDescriptor&& other) noexcept
{
    if (this != &other)
    {
        this->close();
        this->m_file_descriptor = std::exchange(other.m_file_descriptor, -1);
        this->m_posix_pointer   = std::exchange(other.m_posix_pointer, nullptr);
    }
    return *this;
}

int OwnedFileDescriptor::native_handle() const noexcept
{
    return m_file_descriptor;
}

bool OwnedFileDescriptor::is_open() const noexcept
{
    return m_file_descriptor != -1;
}

void OwnedFileDescriptor::close() noexcept
{
    const int closed_fd = std::exchange(m_file_descriptor, -1);
    if (closed_fd != -1)
    {
        static_cast<void>(m_posix_pointer->close(closed_fd));
    }
}

int OwnedFileDescriptor::release_native_handle() noexcept
{
    return std::exchange(m_file_descriptor, -1);
}

OwnedFileDescriptor::OwnedFileDescriptor(const int owned_fd, posix::Posix& posix) noexcept
    : m_file_descriptor(owned_fd), m_posix_pointer(&posix)
{
}

posix::Posix& OwnedFileDescriptor::posix_facade() const noexcept
{
    return *m_posix_pointer;
}

} // namespace exasol::udf::v2::socket
