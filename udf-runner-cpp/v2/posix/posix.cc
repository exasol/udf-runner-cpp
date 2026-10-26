#include <exasol/udf/v2/posix/posix.hpp>

#include <sys/eventfd.h>

namespace exasol::udf::v2::posix
{

int Native::socket(const int domain, const int type, const int protocol)
{
    return ::socket(domain, type, protocol);
}

int Native::eventfd(const unsigned int initial_value, const int flags)
{
    return ::eventfd(initial_value, flags);
}

int Native::connect(const int sockfd, const sockaddr* address, const socklen_t address_length)
{
    return ::connect(sockfd, address, address_length);
}

int Native::bind(const int sockfd, const sockaddr* address, const socklen_t address_length)
{
    return ::bind(sockfd, address, address_length);
}

int Native::listen(const int sockfd, const int backlog)
{
    return ::listen(sockfd, backlog);
}

int Native::accept4(const int sockfd, sockaddr* address, socklen_t* address_length, const int flags)
{
    return ::accept4(sockfd, address, address_length, flags);
}

int Native::getsockopt(const int sockfd,
                       const int level,
                       const int option_name,
                       void* option_value, // NOSONAR: required by the POSIX getsockopt interface
                       socklen_t* option_length)
{
    return ::getsockopt(sockfd, level, option_name, option_value, option_length);
}

int Native::shutdown(const int sockfd, const int how)
{
    return ::shutdown(sockfd, how);
}

ssize_t Native::readv(const int file_descriptor, const iovec* buffers, const int buffer_count)
{
    return ::readv(file_descriptor, buffers, buffer_count);
}

ssize_t Native::read(const int file_descriptor,
                     void* buffer, // NOSONAR: required by the POSIX read interface
                     const size_t count)
{
    return ::read(file_descriptor, buffer, count);
}

ssize_t Native::write(const int file_descriptor,
                      const void* buffer, // NOSONAR: required by the POSIX write interface
                      const size_t count)
{
    return ::write(file_descriptor, buffer, count);
}

ssize_t Native::sendmsg(const int sockfd, const msghdr* message, const int flags)
{
    return ::sendmsg(sockfd, message, flags);
}

int Native::unlink(const char* path)
{
    return ::unlink(path);
}

int Native::close(const int file_descriptor)
{
    return ::close(file_descriptor);
}

Posix& default_posix() noexcept
{
    static Native instance;
    return instance;
}

} // namespace exasol::udf::v2::posix
