#include "linux_epoll_state.hpp"

#include <sys/epoll.h>

#include <cerrno>
#include <system_error>
#include <utility>

namespace exasol::udf::v2::detail
{

NativeEpollState::NativeEpollState(std::shared_ptr<posix::Posix> posix_facade)
    : posix(std::move(posix_facade))
{
    file_descriptor = posix->epoll_create1(EPOLL_CLOEXEC);
    if (file_descriptor == -1)
    {
        throw std::system_error(errno, std::generic_category(), "create epoll");
    }
}

NativeEpollState::~NativeEpollState()
{
    close();
}

int NativeEpollState::native_handle() const noexcept
{
    return file_descriptor;
}

bool NativeEpollState::is_open() const noexcept
{
    return file_descriptor != -1;
}

std::shared_ptr<posix::Posix> NativeEpollState::posix_facade() const noexcept
{
    return posix;
}

void NativeEpollState::close() noexcept
{
    const int closed_fd = std::exchange(file_descriptor, -1);
    if (closed_fd != -1)
    {
        static_cast<void>(posix->close(closed_fd));
    }
}

} // namespace exasol::udf::v2::detail
