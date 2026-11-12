#pragma once

#include <memory>

#include <exasol/udf/v2/socket/socket.hpp>

namespace exasol::udf::v2::internal
{

/** Identifies the transport that established a connection. */
enum class ConnectionType
{
    UnixDomainSocket,
    Tcp
};

/** Common ownership boundary for an accepted runner connection. */
class Connection
{
public:
    virtual ~Connection() = default;

    Connection(const Connection&)            = delete;
    Connection& operator=(const Connection&) = delete;
    Connection(Connection&&)                 = delete;
    Connection& operator=(Connection&&)      = delete;

    [[nodiscard]] virtual ConnectionType get_type() const noexcept = 0;

protected:
    Connection() = default;
};

/** A connection backed by an owning transport-neutral socket. */
class SocketConnection final : public Connection
{
public:
    SocketConnection(ConnectionType type, std::unique_ptr<socket::Socket> socket);

    [[nodiscard]] ConnectionType get_type() const noexcept override;

    /** Transfers ownership of the socket to the caller. */
    [[nodiscard]] std::unique_ptr<socket::Socket> take_socket() noexcept;

private:
    ConnectionType type;
    std::unique_ptr<socket::Socket> socket;
};

} // namespace exasol::udf::v2::internal
