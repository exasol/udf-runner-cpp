#pragma once

#include <cstddef>
#include <span>

#include <exasol/udf/v2/socket/owned_file_descriptor.hpp>

namespace exasol::udf::v2::socket
{

enum class Shutdown
{
    Receive,
    Send,
    Both,
};

class Socket
{
public:
    virtual ~Socket() = default;

    Socket(const Socket&)            = delete;
    Socket& operator=(const Socket&) = delete;

    [[nodiscard]] virtual int native_handle() const noexcept = 0;
    [[nodiscard]] virtual bool is_open() const noexcept      = 0;

    virtual void close() noexcept                                              = 0;
    [[nodiscard]] virtual OwnedFileDescriptor release_native_handle() noexcept = 0;
    virtual void shutdown(Shutdown how)                                        = 0;

    virtual std::size_t read_some(std::span<const std::span<std::byte>> buffers)        = 0;
    virtual std::size_t write_some(std::span<const std::span<const std::byte>> buffers) = 0;

    std::size_t read_some(std::span<std::byte> buffer);
    std::size_t write_some(std::span<const std::byte> buffer);

protected:
    Socket()                    = default;
    Socket(Socket&&)            = default;
    Socket& operator=(Socket&&) = default;
};

} // namespace exasol::udf::v2::socket
