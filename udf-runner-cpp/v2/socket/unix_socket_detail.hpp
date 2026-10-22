#pragma once

#include <sys/socket.h>
#include <sys/un.h>

#include <filesystem>

#include <exasol/udf/v2/socket/owned_file_descriptor.hpp>

namespace exasol::udf::v2::socket::detail
{

[[nodiscard]] sockaddr_un make_socket_address(const std::filesystem::path& path, socklen_t& length);
[[nodiscard]] OwnedFileDescriptor create_socket(posix::Posix& posix);
[[nodiscard]] const sockaddr* as_socket_address(const sockaddr_un& address) noexcept;

} // namespace exasol::udf::v2::socket::detail
