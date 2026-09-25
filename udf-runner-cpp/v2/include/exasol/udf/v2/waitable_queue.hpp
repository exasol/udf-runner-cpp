#pragma once

#if !defined(__linux__)
#error "exasol::udf::v2::WaitableQueue requires Linux eventfd"
#endif

#include <cstdint>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <system_error>
#include <utility>

#include <exasol/udf/v2/event_fd.hpp>
#include <exasol/udf/v2/mpmc_queue.hpp>
#include <exasol/udf/v2/spsc_queue.hpp>

namespace exasol::udf::v2
{

template <typename Queue>
class WaitableQueue
{
public:
    using queue_type = Queue;

    WaitableQueue() : WaitableQueue(Queue{}, std::make_unique<LinuxEventFd>())
    {
    }

    explicit WaitableQueue(Queue queue)
        : WaitableQueue(std::move(queue), std::make_unique<LinuxEventFd>())
    {
    }

    WaitableQueue(Queue queue, std::unique_ptr<EventFd> event_fd)
        : queue_storage(std::move(queue)), notification_fd(std::move(event_fd))
    {
        if (notification_fd == nullptr)
        {
            throw std::invalid_argument("eventfd implementation must not be null");
        }
    }

    ~WaitableQueue() noexcept
    {
        notification_fd.reset();
    }
    WaitableQueue(const WaitableQueue&)            = delete;
    WaitableQueue& operator=(const WaitableQueue&) = delete;

    WaitableQueue(WaitableQueue&& other) noexcept
        : queue_storage(std::move(other.queue_storage)),
          notification_fd(std::move(other.notification_fd))
    {
    }

    WaitableQueue& operator=(WaitableQueue&& other) noexcept
    {
        if (this != &other)
        {
            queue_storage   = std::move(other.queue_storage);
            notification_fd = std::move(other.notification_fd);
        }
        return *this;
    }

    [[nodiscard]] int native_handle() const noexcept
    {
        return notification_fd == nullptr ? -1 : notification_fd->native_handle();
    }

    template <typename T>
    [[nodiscard]] bool enqueue(T&& value)
    {
        if (!queue_storage.enqueue(std::forward<T>(value)))
        {
            return false;
        }
        notify();
        return true;
    }

    template <typename InputIt>
    std::size_t enqueue_batch(InputIt first, InputIt last)
    {
        std::size_t enqueued = 0;
        for (; first != last; ++first)
        {
            if (!queue_storage.enqueue(*first))
            {
                break;
            }
            ++enqueued;
        }
        if (enqueued != 0)
        {
            notify();
        }
        return enqueued;
    }

    template <typename Output>
    [[nodiscard]] bool try_dequeue(Output& value)
    {
        return queue_storage.try_dequeue(value);
    }

    // Drains all eventfd notifications and returns their accumulated count.
    // Callers should then dequeue until the queue is empty and recheck it
    // before going back to epoll_wait().
    std::uint64_t drain_notifications()
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

    Queue& queue() noexcept
    {
        return queue_storage;
    }
    const Queue& queue() const noexcept
    {
        return queue_storage;
    }

private:
    void notify()
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

    Queue queue_storage;
    std::unique_ptr<EventFd> notification_fd;
};

template <typename T>
using WaitableSpscQueue = WaitableQueue<SpscQueue<T>>;

template <typename T>
using WaitableMpmcQueue = WaitableQueue<MpmcQueue<T>>;

} // namespace exasol::udf::v2
