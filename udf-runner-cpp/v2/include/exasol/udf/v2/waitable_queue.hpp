#pragma once

#if !defined(__linux__)
#error "exasol::udf::v2::WaitableQueue requires Linux eventfd"
#endif

#include <sys/eventfd.h>
#include <unistd.h>

#include <cerrno>
#include <cstdint>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <system_error>
#include <utility>

#include <exasol/udf/v2/mpmc_queue.hpp>
#include <exasol/udf/v2/spsc_queue.hpp>

namespace exasol::udf::v2
{

// Adds an epoll-compatible readiness descriptor to a queue. The descriptor
// signals that one or more queue elements may be available; it is not a
// one-to-one mapping between eventfd counter values and queue elements.
class EventFd
{
public:
    virtual ~EventFd() = default;

    [[nodiscard]] virtual int native_handle() const noexcept = 0;
    virtual std::uint64_t read_notification()                = 0;
    virtual void write_notification()                        = 0;
};

class LinuxEventFd final : public EventFd
{
public:
    LinuxEventFd() : file_descriptor(::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC))
    {
        if (file_descriptor == -1)
        {
            throw std::system_error(errno, std::generic_category(), "eventfd");
        }
    }

    ~LinuxEventFd() override
    {
        if (file_descriptor != -1)
        {
            ::close(file_descriptor);
        }
    }

    LinuxEventFd(const LinuxEventFd&)            = delete;
    LinuxEventFd& operator=(const LinuxEventFd&) = delete;

    [[nodiscard]] int native_handle() const noexcept override
    {
        return file_descriptor;
    }

    std::uint64_t read_notification() override
    {
        std::uint64_t value  = 0;
        const ssize_t result = ::read(file_descriptor, &value, sizeof(value));
        if (result == sizeof(value))
        {
            return value;
        }
        if (result == -1)
        {
            throw std::system_error(errno, std::generic_category(), "read eventfd");
        }
        throw std::system_error(EIO, std::generic_category(), "short read from eventfd");
    }

    void write_notification() override
    {
        constexpr std::uint64_t signal = 1;
        const ssize_t result           = ::write(file_descriptor, &signal, sizeof(signal));
        if (result == sizeof(signal))
        {
            return;
        }
        if (result == -1)
        {
            throw std::system_error(errno, std::generic_category(), "write eventfd");
        }
        throw std::system_error(EIO, std::generic_category(), "short write to eventfd");
    }

private:
    int file_descriptor;
};

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

    ~WaitableQueue() = default;

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
