#pragma once

#include <sys/socket.h>
#include <sys/uio.h>

#include <cstddef>
#include <unistd.h>

namespace exasol::udf::v2::posix
{

/** Provides an injectable interface for the POSIX APIs used by v2. */
class Posix
{
public:
    Posix()                        = default;
    Posix(const Posix&)            = delete;
    Posix& operator=(const Posix&) = delete;
    Posix(Posix&&)                 = delete;
    Posix& operator=(Posix&&)      = delete;
    virtual ~Posix()               = default;

    /** Creates a socket. */
    virtual int socket(int domain, int type, int protocol) = 0;
    /** Creates an event file descriptor. */
    virtual int eventfd(unsigned int initial_value, int flags) = 0;
    /** Connects a socket to a peer address. */
    virtual int connect(int sockfd, const sockaddr* address, socklen_t address_length) = 0;
    /** Binds a socket to a local address. */
    virtual int bind(int sockfd, const sockaddr* address, socklen_t address_length) = 0;
    /** Marks a socket as accepting incoming connections. */
    virtual int listen(int sockfd, int backlog) = 0;
    /** Accepts an incoming connection. */
    virtual int accept4(int sockfd, sockaddr* address, socklen_t* address_length, int flags) = 0;
    /** Retrieves a socket option. */
    virtual int getsockopt(
        int sockfd,
        int level,
        int option_name,
        void* option_value, // NOSONAR: required by the POSIX getsockopt interface
        socklen_t* option_length) = 0;
    /** Shuts down part or all of a socket connection. */
    virtual int shutdown(int sockfd, int how) = 0;
    /** Reads data into multiple buffers. */
    virtual ssize_t readv(int file_descriptor, const iovec* buffers, int buffer_count) = 0;
    /** Reads data into a buffer. */
    virtual ssize_t read(int file_descriptor,
                         void* buffer, // NOSONAR: required by the POSIX read interface
                         size_t count) = 0;
    /** Writes data from a buffer. */
    virtual ssize_t write(int file_descriptor,
                          const void* buffer, // NOSONAR: required by the POSIX write interface
                          size_t count) = 0;
    /** Sends a message on a socket. */
    virtual ssize_t sendmsg(int sockfd, const msghdr* message, int flags) = 0;
    /** Removes a filesystem path. */
    virtual int unlink(const char* path) = 0;
    /** Closes a file descriptor. */
    virtual int close(int file_descriptor) = 0;
};

/** Implements the POSIX interface with the native Linux/POSIX APIs. */
class Native final : public Posix
{
public:
    int socket(int domain, int type, int protocol) override;
    int eventfd(unsigned int initial_value, int flags) override;
    int connect(int sockfd, const sockaddr* address, socklen_t address_length) override;
    int bind(int sockfd, const sockaddr* address, socklen_t address_length) override;
    int listen(int sockfd, int backlog) override;
    int accept4(int sockfd, sockaddr* address, socklen_t* address_length, int flags) override;
    int getsockopt(int sockfd,
                   int level,
                   int option_name,
                   void* option_value, // NOSONAR: required by the POSIX getsockopt interface
                   socklen_t* option_length) override;
    int shutdown(int sockfd, int how) override;
    ssize_t readv(int file_descriptor, const iovec* buffers, int buffer_count) override;
    ssize_t read(int file_descriptor,
                 void* buffer, // NOSONAR: required by the POSIX read interface
                 size_t count) override;
    ssize_t write(int file_descriptor,
                  const void* buffer, // NOSONAR: required by the POSIX write interface
                  size_t count) override;
    ssize_t sendmsg(int sockfd, const msghdr* message, int flags) override;
    int unlink(const char* path) override;
    int close(int file_descriptor) override;
};

} // namespace exasol::udf::v2::posix
