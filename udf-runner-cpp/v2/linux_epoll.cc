#include <exasol/udf/v2/linux_epoll.hpp>

#include "linux_epoll_registration.hpp"
#include "linux_epoll_state.hpp"

#include <cerrno>
#include <chrono>
#include <stdexcept>
#include <system_error>
#include <utility>

namespace exasol::udf::v2
{

namespace
{

    class EpollWaitResultError final : public std::runtime_error
    {
    public:
        using std::runtime_error::runtime_error;
    };

    [[nodiscard]] int wait_timeout(const std::optional<std::chrono::milliseconds> timeout)
    {
        if (!timeout.has_value())
        {
            return -1;
        }
        if (timeout.value() < std::chrono::milliseconds::zero() ||
            !std::in_range<int>(timeout.value().count()))
        {
            throw std::invalid_argument("invalid epoll timeout");
        }
        return static_cast<int>(timeout.value().count());
    }

    [[nodiscard]] int remaining_wait_timeout(
        const std::optional<std::chrono::steady_clock::time_point> deadline)
    {
        if (!deadline.has_value())
        {
            return -1;
        }
        const auto remaining = deadline.value() - std::chrono::steady_clock::now();
        if (remaining <= std::chrono::steady_clock::duration::zero())
        {
            return 0;
        }
        return wait_timeout(std::optional<std::chrono::milliseconds>(
            std::chrono::ceil<std::chrono::milliseconds>(remaining)));
    }

} // namespace

NativeLinuxEpoll::NativeLinuxEpoll() : NativeLinuxEpoll(std::make_shared<posix::Native>())
{
}

NativeLinuxEpoll::NativeLinuxEpoll(std::shared_ptr<posix::Posix> posix_facade)
    : state(std::make_shared<detail::NativeEpollState>(std::move(posix_facade)))
{
}

NativeLinuxEpoll::~NativeLinuxEpoll()
{
    close();
}

std::unique_ptr<EpollRegistration> NativeLinuxEpoll::add(const int file_descriptor,
                                                         const EpollInterest interest,
                                                         const std::uint64_t token)
{
    if (!state || !state->is_open())
    {
        throw std::system_error(EBADF, std::generic_category(), "add to closed epoll");
    }
    return std::make_unique<detail::NativeEpollRegistration>(state, file_descriptor, interest,
                                                             token);
}

void NativeLinuxEpoll::wait(std::vector<EpollReadyEvent>& events,
                            const std::optional<std::chrono::milliseconds> timeout)
{
    if (events.empty())
    {
        throw std::invalid_argument("epoll event buffer must not be empty");
    }
    if (!std::in_range<int>(events.size()))
    {
        throw std::invalid_argument("epoll event buffer is too large");
    }
    static_cast<void>(wait_timeout(timeout));
    if (!state || !state->is_open())
    {
        throw std::system_error(EBADF, std::generic_category(), "wait on closed epoll");
    }

    native_events.resize(events.size());
    const auto deadline = timeout.has_value()
                              ? std::optional(std::chrono::steady_clock::now() + timeout.value())
                              : std::nullopt;
    int result          = -1;
    do
    {
        result = state->posix_facade()->epoll_wait(state->native_handle(), native_events.data(),
                                                   static_cast<int>(native_events.size()),
                                                   remaining_wait_timeout(deadline));
    } while (result == -1 && errno == EINTR &&
             (!deadline.has_value() || std::chrono::steady_clock::now() < deadline.value()));

    if (result == -1 && errno == EINTR && deadline.has_value())
    {
        events.clear();
        return;
    }
    if (result == -1)
    {
        throw std::system_error(errno, std::generic_category(), "wait for epoll events");
    }
    if (result > static_cast<int>(native_events.size()))
    {
        throw EpollWaitResultError("epoll returned more events than requested");
    }

    events.clear();
    for (int index = 0; index < result; ++index)
    {
        const epoll_event& native_event = native_events[static_cast<std::size_t>(index)];
        events.push_back({
            .token       = native_event.data.u64,
            .readable    = (native_event.events & EPOLLIN) != 0U,
            .writable    = (native_event.events & EPOLLOUT) != 0U,
            .error       = (native_event.events & EPOLLERR) != 0U,
            .hangup      = (native_event.events & EPOLLHUP) != 0U,
            .peer_closed = (native_event.events & EPOLLRDHUP) != 0U,
        });
    }
}

int NativeLinuxEpoll::native_handle() const noexcept
{
    return state ? state->native_handle() : -1;
}

bool NativeLinuxEpoll::is_open() const noexcept
{
    return state && state->is_open();
}

void NativeLinuxEpoll::close() noexcept
{
    if (state)
    {
        state->close();
    }
}

} // namespace exasol::udf::v2
