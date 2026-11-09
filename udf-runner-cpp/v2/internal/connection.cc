#include <exasol/udf/v2/internal/connection.hpp>

#include <stdexcept>
#include <utility>

namespace exasol::udf::v2::internal
{

SocketConnection::SocketConnection(const ConnectionType type,
                                   std::unique_ptr<socket::Socket> socket)
    : type(type), socket(std::move(socket))
{
    if (!this->socket)
    {
        throw std::invalid_argument("SocketConnection requires an open socket object");
    }
}

ConnectionType SocketConnection::get_type() const noexcept
{
    return this->type;
}

std::unique_ptr<socket::Socket> SocketConnection::take_socket() noexcept
{
    return std::move(this->socket);
}

} // namespace exasol::udf::v2::internal
