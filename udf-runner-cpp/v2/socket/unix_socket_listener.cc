#include <exasol/udf/v2/socket/unix_socket_listener.hpp>

#include "unix_socket_detail.hpp"

#include <cerrno>
#include <system_error>
#include <utility>

namespace exasol::udf::v2::socket
{

namespace
{

    void unlink_socket_path(std::filesystem::path& path, posix::Posix& posix) noexcept
    {
        std::filesystem::path path_to_unlink = std::exchange(path, {});
        if (!path_to_unlink.empty())
        {
            static_cast<void>(posix.unlink(path_to_unlink.c_str()));
        }
    }

    class SocketPathCleanup final
    {
    public:
        SocketPathCleanup(std::filesystem::path& path, posix::Posix& posix) noexcept
            : m_path(path), m_posix(posix)
        {
        }

        SocketPathCleanup(const SocketPathCleanup&)            = delete;
        SocketPathCleanup& operator=(const SocketPathCleanup&) = delete;
        SocketPathCleanup(SocketPathCleanup&&)                 = delete;
        SocketPathCleanup& operator=(SocketPathCleanup&&)      = delete;

        ~SocketPathCleanup()
        {
            if (m_cleanup)
            {
                unlink_socket_path(m_path, m_posix);
            }
        }

        void dont_clean() noexcept
        {
            m_cleanup = false;
        }

    private:
        std::filesystem::path& m_path;
        posix::Posix& m_posix;
        bool m_cleanup = true;
    };

} // namespace

UnixSocketListener::~UnixSocketListener() noexcept
{
    this->close();
}

UnixSocketListener::UnixSocketListener(UnixSocketListener&& other) noexcept
    : m_file_descriptor(std::move(other.m_file_descriptor)),
      m_bound_path(std::exchange(other.m_bound_path, {}))
{
}

UnixSocketListener& UnixSocketListener::operator=(UnixSocketListener&& other) noexcept
{
    if (this != &other)
    {
        this->close();
        this->m_file_descriptor = std::move(other.m_file_descriptor);
        this->m_bound_path      = std::exchange(other.m_bound_path, {});
    }
    return *this;
}

UnixSocketListener UnixSocketListener::bind(const std::filesystem::path& path,
                                            posix::Posix& posix,
                                            const int backlog)
{
    socklen_t address_length      = 0;
    const sockaddr_un address     = detail::make_socket_address(path, address_length);
    OwnedFileDescriptor socket_fd = detail::create_socket(posix);
    std::filesystem::path bound_path(path);
    if (posix.bind(socket_fd.native_handle(), detail::as_socket_address(address), address_length) ==
        -1)
    {
        const int error = errno;
        throw std::system_error(error, std::generic_category(),
                                "bind Unix socket listener '" + path.string() + "'");
    }

    SocketPathCleanup cleanup_path(bound_path, posix);
    if (posix.listen(socket_fd.native_handle(), backlog) == -1)
    {
        throw std::system_error(errno, std::generic_category(), "listen on Unix socket");
    }
    UnixSocketListener listener(std::move(socket_fd), std::move(bound_path));
    cleanup_path.dont_clean();
    return listener;
}

int UnixSocketListener::native_handle() const noexcept
{
    return m_file_descriptor.native_handle();
}

bool UnixSocketListener::is_open() const noexcept
{
    return m_file_descriptor.is_open();
}

void UnixSocketListener::close() noexcept
{
    m_file_descriptor.close();
    unlink_path();
}

OwnedFileDescriptor UnixSocketListener::release_native_handle() noexcept
{
    const int released_fd = m_file_descriptor.release_native_handle();
    this->unlink_path();
    if (released_fd == -1)
    {
        return OwnedFileDescriptor(m_file_descriptor.posix_facade());
    }
    return OwnedFileDescriptor::adopt_native_handle(released_fd, m_file_descriptor.posix_facade());
}

UnixSocket UnixSocketListener::accept(const int flags) const
{
    int accepted_fd = -1;
    do
    {
        accepted_fd = m_file_descriptor.posix_facade().accept4(m_file_descriptor.native_handle(),
                                                               nullptr, nullptr, flags);
    } while (accepted_fd == -1 && errno == EINTR);

    if (accepted_fd == -1)
    {
        throw std::system_error(errno, std::generic_category(), "accept Unix socket connection");
    }
    return UnixSocket::adopt_native_handle(
        OwnedFileDescriptor::adopt_native_handle(accepted_fd, m_file_descriptor.posix_facade()));
}

void UnixSocketListener::unlink_path() noexcept
{
    unlink_socket_path(m_bound_path, m_file_descriptor.posix_facade());
}

UnixSocketListener::UnixSocketListener(OwnedFileDescriptor owned_fd,
                                       std::filesystem::path path) noexcept
    : m_file_descriptor(std::move(owned_fd)), m_bound_path(std::move(path))
{
}

} // namespace exasol::udf::v2::socket
