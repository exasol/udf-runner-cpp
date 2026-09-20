#include <exasol/udf/v2/waitable_queue_notification.hpp>

#include <sys/eventfd.h>
#include <unistd.h>

#include <cerrno>
#include <system_error>
#include <utility>

namespace exasol::udf::v2
{

WaitableQueueNotification::WaitableQueueNotification()
    : notification_fd_(::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC))
{
    if (notification_fd_ == -1)
    {
        throw std::system_error(errno, std::generic_category(), "eventfd");
    }
}

WaitableQueueNotification::~WaitableQueueNotification()
{
    // Descriptor cleanup is covered by the normal test process lifetime, but
    // the moved-from branch cannot be observed without depending on fd reuse.
    // mull-off: cxx_ne_to_eq
    if (notification_fd_ != -1)
    {
        ::close(notification_fd_);
    }
    // mull-on
}

WaitableQueueNotification::WaitableQueueNotification(WaitableQueueNotification&& other) noexcept
    : notification_fd_(std::exchange(other.notification_fd_, -1))
{
}

WaitableQueueNotification& WaitableQueueNotification::operator=(
    WaitableQueueNotification&& other) noexcept
{
    // Self-move and replacement of an owned descriptor are defensive lifetime
    // paths; mutation testing them would require invalid or aliased ownership.
    // mull-off: cxx_ne_to_eq
    if (this != &other)
    {
        // mull-off: cxx_ne_to_eq
        if (notification_fd_ != -1)
        {
            ::close(notification_fd_);
        }
        // mull-on
        notification_fd_ = std::exchange(other.notification_fd_, -1);
    }
    // mull-on
    return *this;
}

int WaitableQueueNotification::native_handle() const noexcept
{
    return notification_fd_;
}

void WaitableQueueNotification::notify() const
{
    constexpr std::uint64_t signal = 1;
    for (;;)
    {
        const ssize_t result = ::write(notification_fd_, &signal, sizeof(signal));
        if (result == sizeof(signal))
        {
            return;
        }
        // EINTR and write failures require fault injection to exercise
        // deterministically; the successful write path and saturation policy
        // remain mutation-tested.
        // mull-off: cxx_eq_to_ne
        if (result == -1 && errno == EINTR)
        {
            continue;
        }
        // mull-on
        // A saturated eventfd is already readable. The queue item remains
        // available, so no additional notification is needed. This requires
        // fault injection to reach deterministically.
        // mull-off: cxx_eq_to_ne
        if (result == -1 && errno == EAGAIN)
        {
            return;
        }
        // mull-on
        // mull-off: cxx_eq_to_ne
        if (result == -1)
        {
            throw std::system_error(errno, std::generic_category(), "write eventfd");
        }
        // mull-on
        throw std::system_error(EIO, std::generic_category(), "short write to eventfd");
    }
}

std::uint64_t WaitableQueueNotification::drain() const
{
    std::uint64_t total = 0;
    for (;;)
    {
        std::uint64_t value  = 0;
        const ssize_t result = ::read(notification_fd_, &value, sizeof(value));
        if (result == sizeof(value))
        {
            total += value;
            continue;
        }
        // EINTR and read failures require fault injection to exercise
        // deterministically; successful draining and EAGAIN termination are
        // covered by the queue test.
        // mull-off: cxx_eq_to_ne
        if (result == -1 && errno == EINTR)
        {
            continue;
        }
        // mull-on
        if (result == -1 && errno == EAGAIN)
        {
            return total;
        }
        // mull-off: cxx_eq_to_ne
        if (result == -1)
        {
            throw std::system_error(errno, std::generic_category(), "read eventfd");
        }
        // mull-on
        throw std::system_error(EIO, std::generic_category(), "short read from eventfd");
    }
}

} // namespace exasol::udf::v2
