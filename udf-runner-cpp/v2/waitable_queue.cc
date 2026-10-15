#include <exasol/udf/v2/waitable_queue.hpp>

#include <system_error>

namespace exasol::udf::v2
{

WaitableQueueCore::WaitableQueueCore(std::unique_ptr<QueueInterface> queue,
                                     std::unique_ptr<EventFd> event_fd)
    : queue_storage(std::move(queue)), notifier(std::move(event_fd))
{
    if (queue_storage == nullptr)
    {
        throw std::invalid_argument("queue implementation must not be null");
    }
}

WaitableQueueCore::~WaitableQueueCore() noexcept = default;

WaitableQueueCore::WaitableQueueCore(WaitableQueueCore&&) noexcept = default;

WaitableQueueCore& WaitableQueueCore::operator=(WaitableQueueCore&&) noexcept = default;

int WaitableQueueCore::native_handle() const noexcept
{
    return notifier.native_handle();
}

bool WaitableQueueCore::enqueue_copy(const void* value)
{
    if (value == nullptr)
    {
        throw std::invalid_argument("queue value must not be null");
    }
    if (!queue_storage->enqueue_copy(value))
    {
        return false;
    }
    notifier.notify();
    return true;
}

bool WaitableQueueCore::enqueue_move(void* value)
{
    if (value == nullptr)
    {
        throw std::invalid_argument("queue value must not be null");
    }
    if (!queue_storage->enqueue_move(value))
    {
        return false;
    }
    notifier.notify();
    return true;
}

std::size_t WaitableQueueCore::enqueue_batch(const void* values, const std::size_t count)
{
    if (count != 0 && values == nullptr)
    {
        throw std::invalid_argument("queue batch must not be null");
    }
    const std::size_t enqueued = queue_storage->enqueue_batch(values, count);
    if (enqueued != 0)
    {
        notifier.notify();
    }
    return enqueued;
}

bool WaitableQueueCore::try_dequeue(void* value)
{
    if (value == nullptr)
    {
        throw std::invalid_argument("queue output must not be null");
    }
    return queue_storage->try_dequeue(value);
}

std::uint64_t WaitableQueueCore::drain_notifications()
{
    return notifier.drain_notifications();
}

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
