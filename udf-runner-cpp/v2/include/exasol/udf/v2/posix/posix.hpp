#pragma once

#include <sys/socket.h>
#include <sys/uio.h>

#include <cstddef>
#include <unistd.h>

namespace exasol::udf::v2::posix
{

/** Provides an injectable facade for the POSIX APIs used by Unix sockets. */
class Posix
{
public:
    virtual ~Posix() = default;

    Posix()                        = default;
    Posix(const Posix&)            = default;
    Posix& operator=(const Posix&) = default;
    Posix(Posix&&)                 = default;
    Posix& operator=(Posix&&)      = default;

    /** Creates a socket. */
    virtual int socket(int domain, int type, int protocol);
    /** Creates an event file descriptor. */
    virtual int eventfd(unsigned int initial_value, int flags);
    /** Connects a socket to a peer address. */
    virtual int connect(int sockfd, const sockaddr* address, socklen_t address_length);
    /** Binds a socket to a local address. */
    virtual int bind(int sockfd, const sockaddr* address, socklen_t address_length);
    /** Marks a socket as accepting incoming connections. */
    virtual int listen(int sockfd, int backlog);
    /** Accepts an incoming connection. */
    virtual int accept4(int sockfd, sockaddr* address, socklen_t* address_length, int flags);
    /** Retrieves a socket option. */
    virtual int getsockopt(
        int sockfd,
        int level,
        int option_name,
        void* option_value, // NOSONAR: required by the POSIX getsockopt interface
        socklen_t* option_length);
    /** Shuts down part or all of a socket connection. */
    virtual int shutdown(int sockfd, int how);
    /** Reads data into multiple buffers. */
    virtual ssize_t readv(int file_descriptor, const iovec* buffers, int buffer_count);
    /** Reads data into a buffer. */
    virtual ssize_t read(int file_descriptor,
                         void* buffer, // NOSONAR: required by the POSIX read interface
                         size_t count);
    /** Writes data from a buffer. */
    virtual ssize_t write(int file_descriptor,
                          const void* buffer, // NOSONAR: required by the POSIX write interface
                          size_t count);
    /** Sends a message on a socket. */
    virtual ssize_t sendmsg(int sockfd, const msghdr* message, int flags);
    /** Removes a filesystem path. */
    virtual int unlink(const char* path);
    /** Closes a file descriptor. */
    virtual int close(int file_descriptor);
};

/** Returns the default POSIX facade used by non-injecting APIs. */
Posix& default_posix() noexcept;

} // namespace exasol::udf::v2::posix
