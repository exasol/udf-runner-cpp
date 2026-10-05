#pragma once

#include <sys/socket.h>

#include <filesystem>

#include <exasol/udf/v2/socket/socket.hpp>

namespace exasol::udf::v2::socket
{

class UnixSocket final : public Socket
{
public:
    using Socket::read_some;
    using Socket::write_some;

    UnixSocket() noexcept;
    ~UnixSocket() override;

    UnixSocket(const UnixSocket&)            = delete;
    UnixSocket& operator=(const UnixSocket&) = delete;
    UnixSocket(UnixSocket&& other) noexcept;
    UnixSocket& operator=(UnixSocket&& other) noexcept;

    [[nodiscard]] static UnixSocket connect(const std::filesystem::path& path);
    [[nodiscard]] static UnixSocket adopt_native_handle(OwnedFileDescriptor owned_fd);

    [[nodiscard]] int native_handle() const noexcept override;
    [[nodiscard]] bool is_open() const noexcept override;
    void close() noexcept override;
    [[nodiscard]] OwnedFileDescriptor release_native_handle() noexcept override;
    void shutdown(Shutdown how) override;
    std::size_t read_some(std::span<const std::span<std::byte>> buffers) override;
    std::size_t write_some(std::span<const std::span<const std::byte>> buffers) override;

private:
    explicit UnixSocket(OwnedFileDescriptor owned_fd) noexcept;

    OwnedFileDescriptor file_descriptor;
};

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
    [[nodiscard]] UnixSocket accept();
    void unlink_path() const;

private:
    UnixSocketListener(OwnedFileDescriptor owned_fd, std::filesystem::path path) noexcept;

    OwnedFileDescriptor file_descriptor;
    std::filesystem::path bound_path;
};

} // namespace exasol::udf::v2::socket
