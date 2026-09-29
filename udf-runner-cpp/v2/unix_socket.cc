#include <exasol/udf/v2/unix_socket.hpp>

#include <sys/un.h>
#include <unistd.h>

#include <cerrno>
#include <cstddef>
#include <cstring>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>

namespace exasol::udf::v2::socket
{

namespace
{

[[noreturn]] void not_implemented()
{
    throw std::logic_error("Unix socket library is not implemented yet");
}

sockaddr_un make_socket_address(const std::filesystem::path& path, socklen_t& length)
{
    const std::string path_string = path.string();
    if (path_string.empty() || path_string.find('\0') != std::string::npos ||
        path_string.size() >= sizeof(sockaddr_un::sun_path))
    {
        throw std::system_error(ENAMETOOLONG, std::generic_category(), "invalid Unix socket path");
    }

    sockaddr_un address{};
    address.sun_family = AF_UNIX;
    std::memcpy(address.sun_path, path_string.data(), path_string.size());
    length = static_cast<socklen_t>(offsetof(sockaddr_un, sun_path) + path_string.size() + 1);
    return address;
}

[[nodiscard]] OwnedFileDescriptor create_socket()
{
    const int socket_fd = ::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (socket_fd == -1)
    {
        throw std::system_error(errno, std::generic_category(), "create Unix socket");
    }
    return OwnedFileDescriptor::adopt_native_handle(socket_fd);
}

void check_unix_stream_socket(const int file_descriptor)
{
    int domain              = 0;
    socklen_t domain_length = sizeof(domain);
    if (::getsockopt(file_descriptor, SOL_SOCKET, SO_DOMAIN, &domain, &domain_length) == -1)
    {
        throw std::system_error(errno, std::generic_category(), "inspect socket domain");
    }

    int type              = 0;
    socklen_t type_length = sizeof(type);
    if (::getsockopt(file_descriptor, SOL_SOCKET, SO_TYPE, &type, &type_length) == -1)
    {
        throw std::system_error(errno, std::generic_category(), "inspect socket type");
    }
    if (domain != AF_UNIX || type != SOCK_STREAM)
    {
        throw std::system_error(EPROTOTYPE, std::generic_category(), "expected Unix stream socket");
    }
}

} // namespace

UnixSocket::UnixSocket() noexcept = default;

UnixSocket::~UnixSocket() = default;

UnixSocket::UnixSocket(UnixSocket&& other) noexcept = default;

UnixSocket& UnixSocket::operator=(UnixSocket&& other) noexcept = default;

UnixSocket UnixSocket::connect(const std::filesystem::path& path)
{
    socklen_t address_length      = 0;
    const sockaddr_un address     = make_socket_address(path, address_length);
    OwnedFileDescriptor socket_fd = create_socket();
    while (::connect(socket_fd.native_handle(), reinterpret_cast<const sockaddr*>(&address), address_length) ==
           -1)
    {
        if (errno != EINTR)
        {
            throw std::system_error(errno, std::generic_category(), "connect Unix socket");
        }
    }
    return UnixSocket(std::move(socket_fd));
}

UnixSocket UnixSocket::adopt_native_handle(OwnedFileDescriptor owned_fd)
{
    if (!owned_fd.is_open())
    {
        throw std::system_error(EBADF, std::generic_category(), "adopt closed socket");
    }
    check_unix_stream_socket(owned_fd.native_handle());
    return UnixSocket(std::move(owned_fd));
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

UnixSocketListener UnixSocketListener::bind(const std::filesystem::path& path, const int backlog)
{
    socklen_t address_length      = 0;
    const sockaddr_un address     = make_socket_address(path, address_length);
    OwnedFileDescriptor socket_fd = create_socket();
    if (::bind(socket_fd.native_handle(), reinterpret_cast<const sockaddr*>(&address), address_length) == -1)
    {
        throw std::system_error(errno, std::generic_category(), "bind Unix socket listener");
    }
    if (::listen(socket_fd.native_handle(), backlog) == -1)
    {
        throw std::system_error(errno, std::generic_category(), "listen on Unix socket");
    }
    return UnixSocketListener(std::move(socket_fd), path);
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
    int accepted_fd = -1;
    do
    {
        accepted_fd = ::accept4(file_descriptor.native_handle(), nullptr, nullptr, SOCK_CLOEXEC);
    } while (accepted_fd == -1 && errno == EINTR);

    if (accepted_fd == -1)
    {
        throw std::system_error(errno, std::generic_category(), "accept Unix socket connection");
    }
    return UnixSocket::adopt_native_handle(OwnedFileDescriptor::adopt_native_handle(accepted_fd));
}

void UnixSocketListener::unlink_path()
{
    if (is_open())
    {
        throw std::logic_error("close Unix socket listener before unlinking its path");
    }
    if (bound_path.empty())
    {
        return;
    }
    if (::unlink(bound_path.c_str()) == -1)
    {
        throw std::system_error(errno, std::generic_category(), "unlink Unix socket path");
    }
    bound_path.clear();
}

UnixSocketListener::UnixSocketListener(OwnedFileDescriptor owned_fd,
                                       std::filesystem::path path) noexcept
    : file_descriptor(std::move(owned_fd)),
      bound_path(std::move(path))
{
}

} // namespace exasol::udf::v2::socket
