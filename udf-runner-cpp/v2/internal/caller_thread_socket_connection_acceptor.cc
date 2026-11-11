#include <exasol/udf/v2/internal/caller_thread_socket_connection_acceptor.hpp>

#include <exasol/udf/v2/assert.hpp>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <system_error>
#include <utility>
#include <vector>

namespace exasol::udf::v2::internal
{

namespace
{

    constexpr std::uint64_t listener_token = 0;
    constexpr std::uint64_t shutdown_token = 1;

    class UnexpectedEpollEventError final : public std::runtime_error
    {
    public:
        using std::runtime_error::runtime_error;
    };

    [[nodiscard]] std::optional<std::chrono::milliseconds> to_milliseconds(
        const ConnectionTimeout timeout)
    {
        if (!timeout.has_value())
        {
            return std::nullopt;
        }
        if (timeout.value() < std::chrono::steady_clock::duration::zero())
        {
            throw std::invalid_argument("connection accept timeout must not be negative");
        }
        const auto milliseconds = std::chrono::ceil<std::chrono::milliseconds>(timeout.value());
        if (!std::in_range<int>(milliseconds.count()))
        {
            throw std::invalid_argument("connection accept timeout is too large");
        }
        return milliseconds;
    }

} // namespace

CallerThreadSocketConnectionAcceptor::CallerThreadSocketConnectionAcceptor(
    const ConnectionType connection_type,
    std::unique_ptr<socket::SocketListener> listener,
    std::unique_ptr<LinuxEpoll> epoll,
    std::unique_ptr<EventFd> shutdown_event)
    : connection_type(connection_type),
      listener(std::move(listener)),
      epoll(std::move(epoll)),
      shutdown_event(std::move(shutdown_event))
{
    EXASOL_UDF_ASSERT(this->listener);
    EXASOL_UDF_ASSERT(this->epoll);
    EXASOL_UDF_ASSERT(this->shutdown_event);
    this->listener_registration = this->epoll->add(this->listener->native_handle(),
                                                   EpollInterest{.readable = true}, listener_token);
    this->shutdown_registration = this->epoll->add(this->shutdown_event->native_handle(),
                                                   EpollInterest{.readable = true}, shutdown_token);
}

std::unique_ptr<Connection> CallerThreadSocketConnectionAcceptor::get_next_connection(
    const ConnectionTimeout timeout)
{
    std::lock_guard acceptance_lock(this->acceptance_mutex);
    {
        std::lock_guard shutdown_lock(this->shutdown_mutex);
        if (this->shutdown_requested)
        {
            return {};
        }
    }

    std::vector<EpollReadyEvent> events(2);
    this->epoll->wait(events, to_milliseconds(timeout));

    for (const EpollReadyEvent& event : events)
    {
        if (event.token == shutdown_token)
        {
            static_cast<void>(this->shutdown_event->read_notification());
            return {};
        }
    }
    if (events.empty())
    {
        return {};
    }

    const auto listener_event = std::ranges::find_if(
        events, [](const EpollReadyEvent& event) { return event.token == listener_token; });
    if (listener_event == events.end())
    {
        throw UnexpectedEpollEventError("epoll returned an unknown connection acceptor event");
    }
    if (!listener_event->readable)
    {
        throw std::system_error(EIO, std::generic_category(), "listen socket became unusable");
    }

    std::unique_ptr<socket::Socket> socket = this->listener->accept_socket();
    {
        std::lock_guard shutdown_lock(this->shutdown_mutex);
        if (this->shutdown_requested)
        {
            return {};
        }
    }
    return std::make_unique<SocketConnection>(this->connection_type, std::move(socket));
}

void CallerThreadSocketConnectionAcceptor::shutdown()
{
    {
        std::lock_guard shutdown_lock(this->shutdown_mutex);
        if (this->shutdown_requested)
        {
            return;
        }
        this->shutdown_requested = true;
    }
    try
    {
        this->shutdown_event->write_notification();
    }
    catch (const std::system_error& error)
    {
        if (error.code() != std::errc::resource_unavailable_try_again)
        {
            throw;
        }
        // A saturated eventfd is already readable, so a shutdown wake-up is pending.
    }
}

} // namespace exasol::udf::v2::internal
