#pragma once

#include <memory>
#include <mutex>

#include <exasol/udf/v2/event_fd.hpp>
#include <exasol/udf/v2/internal/connection.hpp>
#include <exasol/udf/v2/internal/connection_acceptor.hpp>
#include <exasol/udf/v2/linux_epoll.hpp>
#include <exasol/udf/v2/provider/logging_provider.h>
#include <exasol/udf/v2/socket/socket_listener.hpp>

namespace exasol::udf::v2::internal
{

/** Accepts socket connections on the thread calling get_next_connection().
 * The injected logging provider must outlive this acceptor.
 */
class CallerThreadSocketConnectionAcceptor final : public ConnectionAcceptor
{
public:
    CallerThreadSocketConnectionAcceptor(ConnectionType connection_type,
                                         std::unique_ptr<socket::SocketListener> listener,
                                         std::unique_ptr<LinuxEpoll> epoll,
                                         std::unique_ptr<EventFd> shutdown_event,
                                         provider::logging::LoggingProviderIf& logging_provider);

    [[nodiscard]] std::unique_ptr<Connection> get_next_connection(
        ConnectionTimeout timeout) override;
    void shutdown() override;

private:
    ConnectionType connection_type;
    std::unique_ptr<socket::SocketListener> listener;
    std::unique_ptr<LinuxEpoll> epoll;
    std::unique_ptr<EventFd> shutdown_event;
    provider::logging::LoggingProviderIf& logging_provider;
    std::unique_ptr<EpollRegistration> listener_registration;
    std::unique_ptr<EpollRegistration> shutdown_registration;
    std::mutex acceptance_mutex;
    std::mutex shutdown_mutex;
    bool shutdown_requested = false;
};

} // namespace exasol::udf::v2::internal
