#include <exasol/udf/v2/unix_socket.hpp>

#include <stdexcept>
#include <utility>

namespace exasol::udf::v2::socket
{

namespace
{

[[noreturn]] void not_implemented()
{
    throw std::logic_error("Unix socket library is not implemented yet");
}

} // namespace

UnixSocket::UnixSocket() noexcept = default;

UnixSocket::~UnixSocket() = default;

UnixSocket::UnixSocket(UnixSocket&& other) noexcept = default;

UnixSocket& UnixSocket::operator=(UnixSocket&& other) noexcept = default;

UnixSocket UnixSocket::connect(const std::filesystem::path&)
{
    not_implemented();
}

UnixSocket UnixSocket::adopt_native_handle(OwnedFileDescriptor)
{
    not_implemented();
}

int UnixSocket::native_handle() const noexcept
{
    return file_descriptor.native_handle();
}

bool UnixSocket::is_open() const noexcept
{
    return file_descriptor.is_open();
}

void UnixSocket::close() noexcept
{
    file_descriptor.close();
}

OwnedFileDescriptor UnixSocket::release_native_handle() noexcept
{
    const int released_fd = file_descriptor.release_native_handle();
    if (released_fd == -1)
    {
        return {};
    }
    return OwnedFileDescriptor::adopt_native_handle(released_fd);
}

void UnixSocket::shutdown(Shutdown)
{
    not_implemented();
}

std::size_t UnixSocket::read_some(std::span<const std::span<std::byte>>)
{
    not_implemented();
}

std::size_t UnixSocket::write_some(std::span<const std::span<const std::byte>>)
{
    not_implemented();
}

UnixSocket::UnixSocket(OwnedFileDescriptor owned_fd) noexcept : file_descriptor(std::move(owned_fd))
{
}

UnixSocketListener::UnixSocketListener() noexcept = default;

UnixSocketListener::~UnixSocketListener() = default;

UnixSocketListener::UnixSocketListener(UnixSocketListener&& other) noexcept = default;

UnixSocketListener& UnixSocketListener::operator=(UnixSocketListener&& other) noexcept = default;

UnixSocketListener UnixSocketListener::bind(const std::filesystem::path&, const int)
{
    not_implemented();
}

int UnixSocketListener::native_handle() const noexcept
{
    return file_descriptor.native_handle();
}

bool UnixSocketListener::is_open() const noexcept
{
    return file_descriptor.is_open();
}

void UnixSocketListener::close() noexcept
{
    file_descriptor.close();
}

OwnedFileDescriptor UnixSocketListener::release_native_handle() noexcept
{
    const int released_fd = file_descriptor.release_native_handle();
    if (released_fd == -1)
    {
        return {};
    }
    return OwnedFileDescriptor::adopt_native_handle(released_fd);
}

UnixSocket UnixSocketListener::accept()
{
    not_implemented();
}

void UnixSocketListener::unlink_path()
{
    not_implemented();
}

UnixSocketListener::UnixSocketListener(OwnedFileDescriptor owned_fd,
                                       std::filesystem::path path) noexcept
    : file_descriptor(std::move(owned_fd)),
      bound_path(std::move(path))
{
}

} // namespace exasol::udf::v2::socket
