#pragma once

#include <sys/socket.h>

#include <filesystem>

#include <exasol/udf/v2/socket/unix_socket.hpp>

namespace exasol::udf::v2::socket
{

/** Owns a Unix-domain stream listener. */
class UnixSocketListener
{
public:
    ~UnixSocketListener() noexcept;

    UnixSocketListener(const UnixSocketListener&)            = delete;
    UnixSocketListener& operator=(const UnixSocketListener&) = delete;
    UnixSocketListener(UnixSocketListener&& other) noexcept;
    UnixSocketListener& operator=(UnixSocketListener&& other) noexcept;

    /** Creates a listener using the supplied POSIX facade. */
    [[nodiscard]] static UnixSocketListener bind(const std::filesystem::path& path,
                                                 posix::Posix& posix,
                                                 int backlog = SOMAXCONN);

    /** Returns the native listener descriptor, or -1 when closed. */
    [[nodiscard]] int native_handle() const noexcept;

    /** Returns whether the listener owns an open native descriptor. */
    [[nodiscard]] bool is_open() const noexcept;

    /** Closes the listener and removes its filesystem path. */
    void close() noexcept;

    /** Releases the native descriptor and removes the listener's filesystem path. */
    [[nodiscard]] OwnedFileDescriptor release_native_handle() noexcept;

    /** Accepts one connection from the listener.
     *
     * @param flags Zero or a bitwise combination of SOCK_NONBLOCK and SOCK_CLOEXEC.
     */
    [[nodiscard]] UnixSocket accept(int flags = SOCK_CLOEXEC) const;

private:
    UnixSocketListener(OwnedFileDescriptor owned_fd, std::filesystem::path path) noexcept;
    void unlink_path() noexcept;

    OwnedFileDescriptor file_descriptor;
    std::filesystem::path bound_path;
};

} // namespace exasol::udf::v2::socket
