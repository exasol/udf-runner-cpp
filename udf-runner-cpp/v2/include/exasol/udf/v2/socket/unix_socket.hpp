#pragma once

#include <filesystem>

#include <exasol/udf/v2/socket/socket.hpp>

namespace exasol::udf::v2::socket
{

/** Owns one connected Unix-domain stream socket. */
class UnixSocket final : public Socket
{
public:
    using Socket::read_some;
    using Socket::write_some;

    ~UnixSocket() override;

    UnixSocket(const UnixSocket&)            = delete;
    UnixSocket& operator=(const UnixSocket&) = delete;
    UnixSocket(UnixSocket&& other) noexcept;
    UnixSocket& operator=(UnixSocket&& other) noexcept;

    /** Connects using the supplied POSIX facade. */
    [[nodiscard]] static UnixSocket connect(const std::filesystem::path& path, posix::Posix& posix);

    /** Takes ownership of an open Unix stream socket descriptor.
     *
     * @throws std::system_error if @p owned_fd is closed or is not a Unix stream socket.
     */
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

    OwnedFileDescriptor m_file_descriptor;
};

} // namespace exasol::udf::v2::socket
