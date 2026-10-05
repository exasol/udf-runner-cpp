#include <exasol/udf/v2/socket/unix_socket.hpp>

#include <sys/un.h>
#include <sys/uio.h>
#include <unistd.h>

#include <bit>
#include <cerrno>
#include <cstddef>
#include <cstring>
#include <climits>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace exasol::udf::v2::socket
{

namespace
{

    class InvalidShutdownDirection final : public std::logic_error
    {
    public:
        using std::logic_error::logic_error;
    };

    class ListenerStillOpen final : public std::logic_error
    {
    public:
        using std::logic_error::logic_error;
    };

    sockaddr_un make_socket_address(const std::filesystem::path& path, socklen_t& length)
    {
        const std::string path_string = path.string();
        if (path_string.empty() || path_string.contains('\0') ||
            path_string.size() >= sizeof(sockaddr_un::sun_path))
        {
            throw std::system_error(ENAMETOOLONG, std::generic_category(),
                                    "invalid Unix socket path");
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
        int domain = 0;
        if (socklen_t domain_length = sizeof(domain);
            ::getsockopt(file_descriptor, SOL_SOCKET, SO_DOMAIN, &domain, &domain_length) == -1)
        {
            throw std::system_error(errno, std::generic_category(), "inspect socket domain");
        }

        int type = 0;
        if (socklen_t type_length = sizeof(type);
            ::getsockopt(file_descriptor, SOL_SOCKET, SO_TYPE, &type, &type_length) == -1)
        {
            throw std::system_error(errno, std::generic_category(), "inspect socket type");
        }
        if (domain != AF_UNIX || type != SOCK_STREAM)
        {
            throw std::system_error(EPROTOTYPE, std::generic_category(),
                                    "expected Unix stream socket");
        }
    }

    template <typename Buffer>
    std::vector<iovec> make_iovecs(const std::span<const Buffer> buffers)
    {
        if (buffers.size() > IOV_MAX)
        {
            throw std::system_error(EINVAL, std::generic_category(), "too many I/O buffers");
        }

        std::size_t total_size = 0;
        std::vector<iovec> iovecs;
        iovecs.reserve(buffers.size());
        for (const Buffer buffer : buffers)
        {
            if (std::size(buffer) >
                static_cast<std::size_t>(std::numeric_limits<ssize_t>::max()) - total_size)
            {
                throw std::system_error(EINVAL, std::generic_category(),
                                        "I/O buffer size is too large");
            }
            total_size += std::size(buffer);
            // POSIX declares iovec::iov_base as void* even for sendmsg(), which does not mutate it.
            iovecs.push_back(
                {const_cast<std::byte*>(std::data(buffer)), std::size(buffer)}); // NOSONAR
        }
        return iovecs;
    }

    int shutdown_direction(const Shutdown how)
    {
        using enum Shutdown;
        switch (how)
        {
            case Receive:
                return SHUT_RD;
            case Send:
                return SHUT_WR;
            case Both:
                return SHUT_RDWR;
        }
        throw InvalidShutdownDirection("invalid socket shutdown direction");
    }

    [[nodiscard]] const sockaddr* as_socket_address(const sockaddr_un& address) noexcept
    {
        return std::bit_cast<const sockaddr*>(&address);
    }

} // namespace

UnixSocket::UnixSocket() noexcept = default;

UnixSocket::~UnixSocket()
{
    close();
}

UnixSocket::UnixSocket(UnixSocket&& other) noexcept = default;

UnixSocket& UnixSocket::operator=(UnixSocket&& other) noexcept = default;

UnixSocket UnixSocket::connect(const std::filesystem::path& path)
{
    socklen_t address_length      = 0;
    const sockaddr_un address     = make_socket_address(path, address_length);
    OwnedFileDescriptor socket_fd = create_socket();
    while (::connect(socket_fd.native_handle(), as_socket_address(address), address_length) == -1)
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

void UnixSocket::shutdown(const Shutdown how)
{
    if (::shutdown(file_descriptor.native_handle(), shutdown_direction(how)) == -1)
    {
        throw std::system_error(errno, std::generic_category(), "shutdown Unix socket");
    }
}

std::size_t UnixSocket::read_some(const std::span<const std::span<std::byte>> buffers)
{
    std::vector<iovec> iovecs = make_iovecs(buffers);
    if (iovecs.empty())
    {
        return 0;
    }

    ssize_t result = -1;
    do
    {
        result = ::readv(file_descriptor.native_handle(), iovecs.data(),
                         static_cast<int>(iovecs.size()));
    } while (result == -1 && errno == EINTR);

    if (result == -1)
    {
        throw std::system_error(errno, std::generic_category(), "read from Unix socket");
    }
    return static_cast<std::size_t>(result);
}

std::size_t UnixSocket::write_some(const std::span<const std::span<const std::byte>> buffers)
{
    std::vector<iovec> iovecs = make_iovecs(buffers);
    if (iovecs.empty())
    {
        return 0;
    }

    msghdr message{};
    message.msg_iov    = iovecs.data();
    message.msg_iovlen = iovecs.size();
    ssize_t result     = -1;
    do
    {
        result = ::sendmsg(file_descriptor.native_handle(), &message, MSG_NOSIGNAL);
    } while (result == -1 && errno == EINTR);

    if (result == -1)
    {
        throw std::system_error(errno, std::generic_category(), "write to Unix socket");
    }
    return static_cast<std::size_t>(result);
}

UnixSocket::UnixSocket(OwnedFileDescriptor owned_fd) noexcept : file_descriptor(std::move(owned_fd))
{
}

UnixSocketListener::UnixSocketListener() noexcept = default;

UnixSocketListener::~UnixSocketListener()
{
    close();
}

UnixSocketListener::UnixSocketListener(UnixSocketListener&& other) noexcept = default;

UnixSocketListener& UnixSocketListener::operator=(UnixSocketListener&& other) noexcept = default;

UnixSocketListener UnixSocketListener::bind(const std::filesystem::path& path, const int backlog)
{
    socklen_t address_length      = 0;
    const sockaddr_un address     = make_socket_address(path, address_length);
    OwnedFileDescriptor socket_fd = create_socket();
    if (::bind(socket_fd.native_handle(), as_socket_address(address), address_length) == -1)
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

void UnixSocketListener::unlink_path() const
{
    if (is_open())
    {
        throw ListenerStillOpen("close Unix socket listener before unlinking its path");
    }
    if (bound_path.empty())
    {
        return;
    }
    if (::unlink(bound_path.c_str()) == -1)
    {
        throw std::system_error(errno, std::generic_category(), "unlink Unix socket path");
    }
}

UnixSocketListener::UnixSocketListener(OwnedFileDescriptor owned_fd,
                                       std::filesystem::path path) noexcept
    : file_descriptor(std::move(owned_fd)), bound_path(std::move(path))
{
}

} // namespace exasol::udf::v2::socket
