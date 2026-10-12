#pragma once

#include <cstddef>
#include <span>

#include <exasol/udf/v2/socket/owned_file_descriptor.hpp>

namespace exasol::udf::v2::socket
{

/** Direction used when shutting down part or all of a socket connection. */
enum class Shutdown
{
    Receive,
    Send,
    Both,
};

/** Common interface for owned stream sockets. */
class Socket
{
public:
    /** Destroys the socket and releases its native descriptor. */
    virtual ~Socket() = default;

    Socket(const Socket&)            = delete;
    Socket& operator=(const Socket&) = delete;

    /** Returns the native socket descriptor, or -1 when closed. */
    [[nodiscard]] virtual int native_handle() const noexcept = 0;

    /** Returns whether the socket owns an open native descriptor. */
    [[nodiscard]] virtual bool is_open() const noexcept      = 0;

    /** Closes the socket. This operation is idempotent. */
    virtual void close() noexcept                                              = 0;

    /** Releases the native descriptor without closing it. */
    [[nodiscard]] virtual OwnedFileDescriptor release_native_handle() noexcept = 0;

    /** Shuts down the selected direction of the connection.
     *
     * @throws std::system_error if the operating-system shutdown fails.
     */
    virtual void shutdown(Shutdown how)                                        = 0;

    /** Reads up to the capacity described by the buffers.
     *
     * A successful call may read fewer bytes than requested. A return value of
     * zero indicates end-of-file.
     */
    virtual std::size_t read_some(std::span<const std::span<std::byte>> buffers)        = 0;

    /** Writes up to the size described by the buffers.
     *
     * A successful call may write fewer bytes than requested.
     */
    virtual std::size_t write_some(std::span<const std::span<const std::byte>> buffers) = 0;

    /** Reads into one contiguous buffer. */
    std::size_t read_some(std::span<std::byte> buffer);

    /** Writes from one contiguous buffer. */
    std::size_t write_some(std::span<const std::byte> buffer);

protected:
    Socket()                    = default;
    Socket(Socket&&)            = default;
    Socket& operator=(Socket&&) = default;
};

} // namespace exasol::udf::v2::socket
