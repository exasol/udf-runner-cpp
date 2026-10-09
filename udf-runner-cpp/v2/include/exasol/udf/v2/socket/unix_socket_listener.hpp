#pragma once

#include <sys/socket.h>

#include <filesystem>

#include <exasol/udf/v2/socket/unix_socket.hpp>

namespace exasol::udf::v2::socket
{

class UnixSocketListener
{
public:
    UnixSocketListener() noexcept;
    ~UnixSocketListener();

    UnixSocketListener(const UnixSocketListener&)            = delete;
    UnixSocketListener& operator=(const UnixSocketListener&) = delete;
    UnixSocketListener(UnixSocketListener&& other) noexcept;
    UnixSocketListener& operator=(UnixSocketListener&& other) noexcept;

    [[nodiscard]] static UnixSocketListener bind(const std::filesystem::path& path,
                                                 int backlog = SOMAXCONN);

    [[nodiscard]] int native_handle() const noexcept;
    [[nodiscard]] bool is_open() const noexcept;
    void close() noexcept;
    [[nodiscard]] OwnedFileDescriptor release_native_handle() noexcept;
    [[nodiscard]] UnixSocket accept() const;
    void unlink_path() const;

private:
    UnixSocketListener(OwnedFileDescriptor owned_fd, std::filesystem::path path) noexcept;

    OwnedFileDescriptor file_descriptor;
    std::filesystem::path bound_path;
};

} // namespace exasol::udf::v2::socket
