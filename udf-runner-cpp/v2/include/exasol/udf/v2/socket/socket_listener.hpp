#pragma once

#include <memory>

namespace exasol::udf::v2::socket
{

class Socket;

/** Accepts connected sockets from one listening endpoint. */
class SocketListener
{
public:
    virtual ~SocketListener() = default;

    SocketListener(const SocketListener&)            = delete;
    SocketListener& operator=(const SocketListener&) = delete;

    /** Returns the borrowed descriptor that becomes readable for a pending
     * connection. */
    [[nodiscard]] virtual int native_handle() const noexcept = 0;

    /** Accepts one socket after the listener descriptor has become readable. */
    [[nodiscard]] virtual std::unique_ptr<Socket> accept_socket() = 0;

protected:
    SocketListener()                            = default;
    SocketListener(SocketListener&&)            = default;
    SocketListener& operator=(SocketListener&&) = default;
};

} // namespace exasol::udf::v2::socket
