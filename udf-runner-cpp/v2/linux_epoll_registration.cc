#include "linux_epoll_registration.hpp"

#include <sys/epoll.h>

#include <cerrno>
#include <stdexcept>
#include <system_error>
#include <utility>

namespace exasol::udf::v2::detail
{

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

} // namespace

NativeEpollRegistration::NativeEpollRegistration(const std::shared_ptr<NativeEpollState> state,
                                                 const int file_descriptor,
                                                 const EpollInterest interest,
                                                 const std::uint64_t token)
    : state(state), file_descriptor(file_descriptor)
{
    if (!state->is_open())
    {
        throw std::system_error(EBADF, std::generic_category(), "add to closed epoll");
    }

    if (epoll_event event = make_native_event(interest, token);
        state->posix_facade()->epoll_ctl(state->native_handle(), EPOLL_CTL_ADD, file_descriptor,
                                         &event) == -1)
    {
        throw std::system_error(errno, std::generic_category(), "add epoll registration");
    }
}

NativeEpollRegistration::~NativeEpollRegistration()
{
    if (const std::shared_ptr<NativeEpollState> epoll_state = state.lock();
        epoll_state && epoll_state->is_open())
    {
        static_cast<void>(epoll_state->posix_facade()->epoll_ctl(
            epoll_state->native_handle(), EPOLL_CTL_DEL, file_descriptor, nullptr));
    }
}

void NativeEpollRegistration::modify(const EpollInterest interest, const std::uint64_t token)
{
    const std::shared_ptr<NativeEpollState> epoll_state = state.lock();
    if (!epoll_state || !epoll_state->is_open())
    {
        throw std::system_error(EBADF, std::generic_category(), "modify closed epoll");
    }

    if (epoll_event event = make_native_event(interest, token);
        epoll_state->posix_facade()->epoll_ctl(epoll_state->native_handle(), EPOLL_CTL_MOD,
                                               file_descriptor, &event) == -1)
    {
        throw std::system_error(errno, std::generic_category(), "modify epoll registration");
    }
}

} // namespace exasol::udf::v2::detail
