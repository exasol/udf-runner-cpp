#include <exasol/udf/v2/posix/posix.hpp>

namespace exasol::udf::v2::posix
{

int Posix::socket(const int domain, const int type, const int protocol)
{
    return ::socket(domain, type, protocol);
}

int Posix::connect(const int sockfd, const sockaddr* address, const socklen_t address_length)
{
    return ::connect(sockfd, address, address_length);
}

int Posix::bind(const int sockfd, const sockaddr* address, const socklen_t address_length)
{
    return ::bind(sockfd, address, address_length);
}

int Posix::listen(const int sockfd, const int backlog)
{
    return ::listen(sockfd, backlog);
}

int Posix::accept4(const int sockfd, sockaddr* address, socklen_t* address_length, const int flags)
{
    return ::accept4(sockfd, address, address_length, flags);
}

int Posix::getsockopt(const int sockfd,
                      const int level,
                      const int option_name,
                      void* option_value, // NOSONAR: required by the POSIX getsockopt interface
                      socklen_t* option_length)
{
    return ::getsockopt(sockfd, level, option_name, option_value, option_length);
}

int Posix::shutdown(const int sockfd, const int how)
{
    return ::shutdown(sockfd, how);
}

ssize_t Posix::readv(const int file_descriptor, const iovec* buffers, const int buffer_count)
{
    return ::readv(file_descriptor, buffers, buffer_count);
}

ssize_t Posix::sendmsg(const int sockfd, const msghdr* message, const int flags)
{
    return ::sendmsg(sockfd, message, flags);
}

int Posix::unlink(const char* path)
{
    return ::unlink(path);
}

int Posix::close(const int file_descriptor)
{
    return ::close(file_descriptor);
}

} // namespace exasol::udf::v2::posix
