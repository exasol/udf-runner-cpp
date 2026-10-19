#pragma once

#include <sys/socket.h>
#include <sys/uio.h>

#include <unistd.h>

namespace exasol::udf::v2::posix
{

/** Provides an injectable façade for the POSIX APIs used by Unix sockets. */
class Posix
{
public:
    virtual ~Posix() = default;

    Posix()                        = default;
    Posix(const Posix&)            = default;
    Posix& operator=(const Posix&) = default;
    Posix(Posix&&)                 = default;
    Posix& operator=(Posix&&)      = default;

    virtual int socket(int domain, int type, int protocol);
    virtual int connect(int sockfd, const sockaddr* address, socklen_t address_length);
    virtual int bind(int sockfd, const sockaddr* address, socklen_t address_length);
    virtual int listen(int sockfd, int backlog);
    virtual int accept4(int sockfd, sockaddr* address, socklen_t* address_length, int flags);
    virtual int getsockopt(
        int sockfd,
        int level,
        int option_name,
        void* option_value, // NOSONAR: required by the POSIX getsockopt interface
        socklen_t* option_length);
    virtual int shutdown(int sockfd, int how);
    virtual ssize_t readv(int file_descriptor, const iovec* buffers, int buffer_count);
    virtual ssize_t sendmsg(int sockfd, const msghdr* message, int flags);
    virtual int unlink(const char* path);
    virtual int close(int file_descriptor);
};

} // namespace exasol::udf::v2::posix
