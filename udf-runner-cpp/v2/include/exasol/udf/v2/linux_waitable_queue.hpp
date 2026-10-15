#pragma once

#include <memory>
#include <span>
#include <utility>

#include <exasol/udf/v2/event_fd_factory.hpp>
#include <exasol/udf/v2/mpmc_queue.hpp>
#include <exasol/udf/v2/spsc_queue.hpp>
#include <exasol/udf/v2/waitable_queue.hpp>

namespace exasol::udf::v2
{

template <typename T>
class SpscQueueAdapter final : public QueueInterface<T>
{
public:
    explicit SpscQueueAdapter(SpscQueue<T> queue) : queue(std::move(queue))
    {
    }

    bool enqueue(const T& value) override
    {
        return queue.enqueue(value);
    }

    bool enqueue(T&& value) override
    {
        return queue.enqueue(std::move(value));
    }

    std::size_t enqueue_batch(std::span<const T> values) override
    {
        std::size_t enqueued = 0;
        for (const T& value : values)
        {
            if (!queue.enqueue(value))
            {
                break;
            }
            ++enqueued;
        }
        return enqueued;
    }

    bool try_dequeue(T& value) override
    {
        return queue.try_dequeue(value);
    }

private:
    SpscQueue<T> queue;
};

template <typename T>
class MpmcQueueAdapter final : public QueueInterface<T>
{
public:
    explicit MpmcQueueAdapter(MpmcQueue<T> queue) : queue(std::move(queue))
    {
    }

    bool enqueue(const T& value) override
    {
        return queue.enqueue(value);
    }

    bool enqueue(T&& value) override
    {
        return queue.enqueue(std::move(value));
    }

    std::size_t enqueue_batch(std::span<const T> values) override
    {
        std::size_t enqueued = 0;
        for (const T& value : values)
        {
            if (!queue.enqueue(value))
            {
                break;
            }
            ++enqueued;
        }
        return enqueued;
    }

    bool try_dequeue(T& value) override
    {
        return queue.try_dequeue(value);
    }

private:
    MpmcQueue<T> queue;
};

template <typename T>
class WaitableSpscQueue final : public WaitableQueue<T>
{
public:
    WaitableSpscQueue(SpscQueue<T> queue, std::unique_ptr<EventFd> event_fd)
        : WaitableQueue<T>(std::make_unique<SpscQueueAdapter<T>>(std::move(queue)),
                           std::move(event_fd))
    {
    }
};

template <typename T>
class WaitableMpmcQueue final : public WaitableQueue<T>
{
public:
    WaitableMpmcQueue(MpmcQueue<T> queue, std::unique_ptr<EventFd> event_fd)
        : WaitableQueue<T>(std::make_unique<MpmcQueueAdapter<T>>(std::move(queue)),
                           std::move(event_fd))
    {
    }
};

template <typename T>
[[nodiscard]] WaitableSpscQueue<T> make_waitable_spsc_queue()
{
    return WaitableSpscQueue<T>(SpscQueue<T>{}, make_linux_event_fd());
}

template <typename T>
[[nodiscard]] WaitableSpscQueue<T> make_waitable_spsc_queue(SpscQueue<T> queue)
{
    return WaitableSpscQueue<T>(std::move(queue), make_linux_event_fd());
}

template <typename T>
[[nodiscard]] WaitableMpmcQueue<T> make_waitable_mpmc_queue()
{
    return WaitableMpmcQueue<T>(MpmcQueue<T>{}, make_linux_event_fd());
}

template <typename T>
[[nodiscard]] WaitableMpmcQueue<T> make_waitable_mpmc_queue(MpmcQueue<T> queue)
{
    return WaitableMpmcQueue<T>(std::move(queue), make_linux_event_fd());
}

} // namespace exasol::udf::v2
