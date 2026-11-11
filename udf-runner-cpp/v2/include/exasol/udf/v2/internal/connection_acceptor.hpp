#pragma once

#include <chrono>
#include <memory>
#include <optional>

namespace exasol::udf::v2::internal
{

class Connection;

/** Optional duration used while waiting for an accepted connection. */
using ConnectionTimeout = std::optional<std::chrono::steady_clock::duration>;

/** Supplies accepted connections to the runner. */
class ConnectionAcceptor
{
public:
    virtual ~ConnectionAcceptor() = default;

    ConnectionAcceptor(const ConnectionAcceptor&)            = delete;
    ConnectionAcceptor& operator=(const ConnectionAcceptor&) = delete;
    ConnectionAcceptor(ConnectionAcceptor&&)                 = delete;
    ConnectionAcceptor& operator=(ConnectionAcceptor&&)      = delete;

    /**
     * Waits for one connection.
     *
     * @returns An owning connection, or nullptr when @p timeout expires or
     *          after shutdown.
     */
    [[nodiscard]] virtual std::unique_ptr<Connection> get_next_connection(
        ConnectionTimeout timeout) = 0;

    /** Stops acceptance and wakes any pending get_next_connection call. */
    virtual void shutdown() = 0;

protected:
    ConnectionAcceptor() = default;
};

} // namespace exasol::udf::v2::internal
