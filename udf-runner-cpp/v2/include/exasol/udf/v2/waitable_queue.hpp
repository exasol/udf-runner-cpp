#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <stdexcept>
#include <utility>

#include <exasol/udf/v2/event_fd.hpp>

namespace exasol::udf::v2
{

template <typename T>
class WaitableQueue;

class WaitableQueueNotifier
{
public:
    explicit WaitableQueueNotifier(std::unique_ptr<EventFd> event_fd);
    ~WaitableQueueNotifier() noexcept;

    WaitableQueueNotifier(const WaitableQueueNotifier&)            = delete;
    WaitableQueueNotifier& operator=(const WaitableQueueNotifier&) = delete;
    WaitableQueueNotifier(WaitableQueueNotifier&&) noexcept;
    WaitableQueueNotifier& operator=(WaitableQueueNotifier&&) noexcept;

    [[nodiscard]] int native_handle() const noexcept;
    std::uint64_t drain_notifications();

private:
    void notify();

    template <typename T>
    friend class WaitableQueue;

    std::unique_ptr<EventFd> notification_fd;
};

template <typename T>
class QueueInterface
{
public:
    QueueInterface()          = default;
    virtual ~QueueInterface() = default;

    QueueInterface(const QueueInterface&)            = delete;
    QueueInterface& operator=(const QueueInterface&) = delete;
    QueueInterface(QueueInterface&&)                 = default;
    QueueInterface& operator=(QueueInterface&&)      = default;

    virtual bool enqueue(const T& value)                         = 0;
    virtual bool enqueue(T&& value)                              = 0;
    virtual std::size_t enqueue_batch(std::span<const T> values) = 0;
    virtual bool try_dequeue(T& value)                           = 0;
};

template <typename T>
class WaitableQueue
{
public:
    WaitableQueue(std::unique_ptr<QueueInterface<T>> queue, std::unique_ptr<EventFd> event_fd)
        : queue_storage(std::move(queue)), notifier(std::move(event_fd))
    {
        if (queue_storage == nullptr)
        {
            throw std::invalid_argument("queue implementation must not be null");
        }
    }

    ~WaitableQueue() noexcept = default;

    WaitableQueue(const WaitableQueue&)                = delete;
    WaitableQueue& operator=(const WaitableQueue&)     = delete;
    WaitableQueue(WaitableQueue&&) noexcept            = default;
    WaitableQueue& operator=(WaitableQueue&&) noexcept = default;

    [[nodiscard]] int native_handle() const noexcept
    {
        return notifier.native_handle();
    }

    [[nodiscard]] bool enqueue(const T& value)
    {
        if (!queue_storage->enqueue(value))
        {
            return false;
        }
        notifier.notify();
        return true;
    }

    [[nodiscard]] bool enqueue(T&& value)
    {
        if (!queue_storage->enqueue(std::move(value)))
        {
            return false;
        }
        notifier.notify();
        return true;
    }

    std::size_t enqueue_batch(std::span<const T> values)
    {
        const std::size_t enqueued = queue_storage->enqueue_batch(values);
        if (enqueued != 0)
        {
            notifier.notify();
        }
        return enqueued;
    }

    [[nodiscard]] bool try_dequeue(T& value)
    {
        return queue_storage->try_dequeue(value);
    }

    std::uint64_t drain_notifications()
    {
        return notifier.drain_notifications();
    }

private:
    std::unique_ptr<QueueInterface<T>> queue_storage;
    WaitableQueueNotifier notifier;
};

} // namespace exasol::udf::v2
