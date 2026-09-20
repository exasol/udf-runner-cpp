#pragma once

#if !defined(__linux__)
#error "exasol::udf::v2::WaitableQueue requires Linux eventfd"
#endif

#include <cstdint>
#include <iterator>
#include <utility>

#include <exasol/udf/v2/mpmc_queue.hpp>
#include <exasol/udf/v2/spsc_queue.hpp>
#include <exasol/udf/v2/waitable_queue_notification.hpp>

namespace exasol::udf::v2
{

// Adds an epoll-compatible readiness descriptor to a queue. The descriptor
// signals that one or more queue elements may be available; it is not a
// one-to-one mapping between eventfd counter values and queue elements.
template <typename Queue>
class WaitableQueue
{
public:
    using queue_type = Queue;

    WaitableQueue() = default;

    explicit WaitableQueue(Queue queue) : queue_(std::move(queue)) {}

    ~WaitableQueue() = default;

    WaitableQueue(const WaitableQueue&)            = delete;
    WaitableQueue& operator=(const WaitableQueue&) = delete;

    WaitableQueue(WaitableQueue&&) noexcept = default;
    WaitableQueue& operator=(WaitableQueue&&) noexcept = default;

    [[nodiscard]] int native_handle() const noexcept
    {
        return notification_.native_handle();
    }

    template <typename T>
    [[nodiscard]] bool enqueue(T&& value)
    {
        if (!queue_.enqueue(std::forward<T>(value)))
        {
            return false;
        }
        notification_.notify();
        return true;
    }

    template <typename InputIt>
    std::size_t enqueue_batch(InputIt first, InputIt last)
    {
        std::size_t enqueued = 0;
        for (; first != last; ++first)
        {
            if (!queue_.enqueue(*first))
            {
                break;
            }
            ++enqueued;
        }
        if (enqueued != 0)
        {
            notification_.notify();
        }
        return enqueued;
    }

    template <typename Output>
    [[nodiscard]] bool try_dequeue(Output& value)
    {
        return queue_.try_dequeue(value);
    }

    std::uint64_t drain_notifications()
    {
        return notification_.drain();
    }

    Queue& queue() noexcept
    {
        return queue_;
    }
    const Queue& queue() const noexcept
    {
        return queue_;
    }

private:
    Queue queue_;
    WaitableQueueNotification notification_;
};

template <typename T>
using WaitableSpscQueue = WaitableQueue<SpscQueue<T>>;

template <typename T>
using WaitableMpmcQueue = WaitableQueue<MpmcQueue<T>>;

} // namespace exasol::udf::v2
