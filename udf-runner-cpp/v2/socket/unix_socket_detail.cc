#include "unix_socket_detail.hpp"

#include <unistd.h>

#include <bit>
#include <cerrno>
#include <cstddef>
#include <cstring>
#include <stdexcept>
#include <string>
#include <system_error>

namespace exasol::udf::v2::socket::detail
{

sockaddr_un make_socket_address(const std::filesystem::path& path, socklen_t& length)
{
    const std::string path_string = path.string();
    if (path_string.empty() || path_string.contains('\0') ||
        path_string.size() >= sizeof(sockaddr_un::sun_path))
    {
        throw std::invalid_argument("invalid Unix socket path: " + path_string);
    }

    sockaddr_un address{};
    address.sun_family = AF_UNIX;
    std::memcpy(address.sun_path, path_string.data(), path_string.size());
    length = static_cast<socklen_t>(offsetof(sockaddr_un, sun_path) + path_string.size() + 1);
    return address;
}

OwnedFileDescriptor create_socket()
{
    const int socket_fd = ::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (socket_fd == -1)
    {
        throw std::system_error(errno, std::generic_category(), "create Unix socket");
    }
    return OwnedFileDescriptor::adopt_native_handle(socket_fd);
}

const sockaddr* as_socket_address(const sockaddr_un& address) noexcept
{
    return std::bit_cast<const sockaddr*>(&address);
}

} // namespace exasol::udf::v2::socket::detail
