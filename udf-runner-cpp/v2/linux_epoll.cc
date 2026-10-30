#include <exasol/udf/v2/linux_epoll.hpp>

#include <cerrno>
#include <chrono>
#include <limits>
#include <stdexcept>
#include <system_error>
#include <utility>

namespace exasol::udf::v2
{

namespace detail
{

    class NativeEpollState final
    {
    public:
        explicit NativeEpollState(std::shared_ptr<posix::Posix> posix_facade)
            : posix(std::move(posix_facade))
        {
            file_descriptor = this->posix->epoll_create1(EPOLL_CLOEXEC);
            if (file_descriptor == -1)
            {
                throw std::system_error(errno, std::generic_category(), "create epoll");
            }
        }

        ~NativeEpollState()
        {
            close();
        }

        NativeEpollState(const NativeEpollState&)            = delete;
        NativeEpollState& operator=(const NativeEpollState&) = delete;
        NativeEpollState(NativeEpollState&&)                 = delete;
        NativeEpollState& operator=(NativeEpollState&&)      = delete;

        [[nodiscard]] int native_handle() const noexcept
        {
            return file_descriptor;
        }

        [[nodiscard]] bool is_open() const noexcept
        {
            return file_descriptor != -1;
        }

        [[nodiscard]] std::shared_ptr<posix::Posix> posix_facade() const noexcept
        {
            return posix;
        }

        void close() noexcept
        {
            const int closed_fd = std::exchange(file_descriptor, -1);
            if (closed_fd != -1)
            {
                static_cast<void>(posix->close(closed_fd));
            }
        }

    private:
        int file_descriptor = -1;
        std::shared_ptr<posix::Posix> posix;
    };

} // namespace detail

namespace
{

    [[nodiscard]] epoll_event make_native_event(const EpollInterest interest,
                                                const std::uint64_t token)
    {
        if (!interest.readable && !interest.writable)
        {
            throw std::invalid_argument("epoll registration must have an interest");
        }

        epoll_event event{};
        if (interest.readable)
        {
            event.events |= EPOLLIN;
        }
        if (interest.writable)
        {
            event.events |= EPOLLOUT;
        }
        event.data.u64 = token;
        return event;
    }

    [[nodiscard]] int wait_timeout(const std::optional<std::chrono::milliseconds> timeout)
    {
        if (!timeout.has_value())
        {
            return -1;
        }
        if (timeout.value() < std::chrono::milliseconds::zero() ||
            timeout.value().count() > std::numeric_limits<int>::max())
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

    class NativeEpollRegistration final : public EpollRegistration
    {
    public:
        NativeEpollRegistration(std::weak_ptr<detail::NativeEpollState> state,
                                const int file_descriptor) noexcept
            : state(std::move(state)), file_descriptor(file_descriptor)
        {
        }

        ~NativeEpollRegistration() override
        {
            remove();
        }

        NativeEpollRegistration(const NativeEpollRegistration&)            = delete;
        NativeEpollRegistration& operator=(const NativeEpollRegistration&) = delete;
        NativeEpollRegistration(NativeEpollRegistration&&)                 = delete;
        NativeEpollRegistration& operator=(NativeEpollRegistration&&)      = delete;

        void activate() noexcept
        {
            registered = true;
        }

        void modify(const EpollInterest interest, const std::uint64_t token) override
        {
            if (!registered)
            {
                throw std::system_error(EBADF, std::generic_category(),
                                        "modify inactive epoll registration");
            }
            const std::shared_ptr<detail::NativeEpollState> epoll_state = state.lock();
            if (!epoll_state || !epoll_state->is_open())
            {
                throw std::system_error(EBADF, std::generic_category(), "modify closed epoll");
            }

            epoll_event event = make_native_event(interest, token);
            if (epoll_state->posix_facade()->epoll_ctl(epoll_state->native_handle(), EPOLL_CTL_MOD,
                                                       file_descriptor, &event) == -1)
            {
                throw std::system_error(errno, std::generic_category(),
                                        "modify epoll registration");
            }
        }

        void remove() noexcept override
        {
            if (!std::exchange(registered, false))
            {
                return;
            }
            const std::shared_ptr<detail::NativeEpollState> epoll_state = state.lock();
            if (epoll_state && epoll_state->is_open())
            {
                static_cast<void>(epoll_state->posix_facade()->epoll_ctl(
                    epoll_state->native_handle(), EPOLL_CTL_DEL, file_descriptor, nullptr));
            }
        }

        [[nodiscard]] bool is_registered() const noexcept override
        {
            const std::shared_ptr<detail::NativeEpollState> epoll_state = state.lock();
            return registered && epoll_state && epoll_state->is_open();
        }

    private:
        std::weak_ptr<detail::NativeEpollState> state;
        int file_descriptor = -1;
        bool registered     = false;
    };

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

NativeLinuxEpoll::NativeLinuxEpoll(NativeLinuxEpoll&& other) noexcept
    : state(std::move(other.state)), native_events(std::move(other.native_events))
{
}

NativeLinuxEpoll& NativeLinuxEpoll::operator=(NativeLinuxEpoll&& other) noexcept
{
    if (this != &other)
    {
        close();
        state         = std::move(other.state);
        native_events = std::move(other.native_events);
    }
    return *this;
}

std::unique_ptr<EpollRegistration> NativeLinuxEpoll::add(const int file_descriptor,
                                                         const EpollInterest interest,
                                                         const std::uint64_t token)
{
    if (!state || !state->is_open())
    {
        throw std::system_error(EBADF, std::generic_category(), "add to closed epoll");
    }

    auto registration = std::make_unique<NativeEpollRegistration>(state, file_descriptor);
    epoll_event event = make_native_event(interest, token);
    if (state->posix_facade()->epoll_ctl(state->native_handle(), EPOLL_CTL_ADD, file_descriptor,
                                         &event) == -1)
    {
        throw std::system_error(errno, std::generic_category(), "add epoll registration");
    }
    registration->activate();
    return registration;
}

std::size_t NativeLinuxEpoll::wait(const std::span<EpollReadyEvent> events,
                                   const std::optional<std::chrono::milliseconds> timeout)
{
    if (events.empty())
    {
        throw std::invalid_argument("epoll event buffer must not be empty");
    }
    if (events.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
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
        return 0;
    }

    if (result == -1)
    {
        throw std::system_error(errno, std::generic_category(), "wait for epoll events");
    }

    for (int index = 0; index < result; ++index)
    {
        const epoll_event& native_event         = native_events[static_cast<std::size_t>(index)];
        events[static_cast<std::size_t>(index)] = {
            .token       = native_event.data.u64,
            .readable    = (native_event.events & EPOLLIN) != 0U,
            .writable    = (native_event.events & EPOLLOUT) != 0U,
            .error       = (native_event.events & EPOLLERR) != 0U,
            .hangup      = (native_event.events & EPOLLHUP) != 0U,
            .peer_closed = (native_event.events & EPOLLRDHUP) != 0U,
        };
    }
    return static_cast<std::size_t>(result);
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
