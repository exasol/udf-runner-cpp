#include <exasol/udf/v2/socket/unix_socket.hpp>

#include "unix_socket_detail.hpp"

#include <sys/uio.h>
#include <cerrno>
#include <climits>
#include <iterator>
#include <limits>
#include <stdexcept>
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

    void check_unix_stream_socket(const int file_descriptor, posix::Posix& posix)
    {
        int domain = 0;
        if (socklen_t domain_length = sizeof(domain);
            posix.getsockopt(file_descriptor, SOL_SOCKET, SO_DOMAIN, &domain, &domain_length) == -1)
        {
            throw std::system_error(errno, std::generic_category(), "inspect socket domain");
        }

        int type = 0;
        if (socklen_t type_length = sizeof(type);
            posix.getsockopt(file_descriptor, SOL_SOCKET, SO_TYPE, &type, &type_length) == -1)
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
        if (std::size(buffers) > IOV_MAX)
        {
            throw std::system_error(EINVAL, std::generic_category(), "too many I/O buffers");
        }

        std::size_t total_size = 0;
        std::vector<iovec> iovecs;
        iovecs.reserve(std::size(buffers));
        for (const Buffer buffer : std::span<const Buffer>(buffers))
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

} // namespace

UnixSocket::~UnixSocket()
{
    close();
}

UnixSocket::UnixSocket(UnixSocket&& other) noexcept = default;

UnixSocket& UnixSocket::operator=(UnixSocket&& other) noexcept = default;

UnixSocket UnixSocket::connect(const std::filesystem::path& path, posix::Posix& posix)
{
    socklen_t address_length      = 0;
    const sockaddr_un address     = detail::make_socket_address(path, address_length);
    OwnedFileDescriptor socket_fd = detail::create_socket(posix);
    while (posix.connect(socket_fd.native_handle(), detail::as_socket_address(address),
                         address_length) == -1)
    {
        if (errno != EINTR)
        {
            const int error = errno;
            throw std::system_error(error, std::generic_category(),
                                    "connect Unix socket '" + path.string() + "'");
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
    check_unix_stream_socket(owned_fd.native_handle(), owned_fd.posix_facade());
    return UnixSocket(std::move(owned_fd));
}

int UnixSocket::native_handle() const noexcept
{
    return m_file_descriptor.native_handle();
}

bool UnixSocket::is_open() const noexcept
{
    return m_file_descriptor.is_open();
}

void UnixSocket::close() noexcept
{
    m_file_descriptor.close();
}

OwnedFileDescriptor UnixSocket::release_native_handle() noexcept
{
    const int released_fd = m_file_descriptor.release_native_handle();
    if (released_fd == -1)
    {
        return OwnedFileDescriptor(m_file_descriptor.posix_facade());
    }
    return OwnedFileDescriptor::adopt_native_handle(released_fd, m_file_descriptor.posix_facade());
}

void UnixSocket::shutdown(const Shutdown how)
{
    if (m_file_descriptor.posix_facade().shutdown(m_file_descriptor.native_handle(),
                                                  shutdown_direction(how)) == -1)
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
        result = m_file_descriptor.posix_facade().readv(
            m_file_descriptor.native_handle(), iovecs.data(), static_cast<int>(iovecs.size()));
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
        result = m_file_descriptor.posix_facade().sendmsg(m_file_descriptor.native_handle(),
                                                          &message, MSG_NOSIGNAL);
    } while (result == -1 && errno == EINTR);

    if (result == -1)
    {
        throw std::system_error(errno, std::generic_category(), "write to Unix socket");
    }
    return static_cast<std::size_t>(result);
}

UnixSocket::UnixSocket(OwnedFileDescriptor owned_fd) noexcept
    : m_file_descriptor(std::move(owned_fd))
{
}

} // namespace exasol::udf::v2::socket
