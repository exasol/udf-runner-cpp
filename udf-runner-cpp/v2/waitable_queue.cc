#include <exasol/udf/v2/waitable_queue.hpp>

#include <system_error>

namespace exasol::udf::v2
{

WaitableQueueNotifier::WaitableQueueNotifier(std::unique_ptr<EventFd> event_fd)
    : notification_fd(std::move(event_fd))
{
    if (notification_fd == nullptr)
    {
        throw std::invalid_argument("eventfd implementation must not be null");
    }
}

WaitableQueueNotifier::~WaitableQueueNotifier() noexcept = default;

WaitableQueueNotifier::WaitableQueueNotifier(WaitableQueueNotifier&&) noexcept = default;

WaitableQueueNotifier& WaitableQueueNotifier::operator=(WaitableQueueNotifier&&) noexcept = default;

int WaitableQueueNotifier::native_handle() const noexcept
{
    return notification_fd == nullptr ? -1 : notification_fd->native_handle();
}

std::uint64_t WaitableQueueNotifier::drain_notifications()
{
    std::uint64_t total = 0;
    for (;;)
    {
        try
        {
            total += notification_fd->read_notification();
        }
        catch (const std::system_error& error)
        {
            if (error.code() == std::errc::interrupted)
            {
                continue;
            }
            if (error.code() == std::errc::resource_unavailable_try_again)
            {
                return total;
            }
            throw;
        }
    }
}

void WaitableQueueNotifier::notify()
{
    for (;;)
    {
        try
        {
            notification_fd->write_notification();
            return;
        }
        catch (const std::system_error& error)
        {
            if (error.code() == std::errc::interrupted)
            {
                continue;
            }
            // A saturated eventfd is already readable. The queue item remains
            // available, so no additional notification is needed.
            if (error.code() == std::errc::resource_unavailable_try_again)
            {
                return;
            }
            throw;
        }
    }
}

} // namespace exasol::udf::v2
