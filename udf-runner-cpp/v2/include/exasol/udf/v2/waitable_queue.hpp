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
    friend class WaitableQueueCore;

    void notify();

    std::unique_ptr<EventFd> notification_fd;
};

class QueueInterface
{
public:
    QueueInterface()          = default;
    virtual ~QueueInterface() = default;

    QueueInterface(const QueueInterface&)            = delete;
    QueueInterface& operator=(const QueueInterface&) = delete;
    QueueInterface(QueueInterface&&)                 = default;
    QueueInterface& operator=(QueueInterface&&)      = default;

    // Pointers are borrowed for the duration of the synchronous call only.
    // Implementations must copy or move values into their queue and must not
    // retain any pointer after returning.
    virtual bool enqueue_copy(const void* value)                             = 0;
    virtual bool enqueue_move(void* value)                                   = 0;
    virtual std::size_t enqueue_batch(const void* values, std::size_t count) = 0;
    virtual bool try_dequeue(void* value)                                    = 0;
};

class WaitableQueueCore
{
public:
    WaitableQueueCore(std::unique_ptr<QueueInterface> queue, std::unique_ptr<EventFd> event_fd);
    ~WaitableQueueCore() noexcept;

    WaitableQueueCore(const WaitableQueueCore&)            = delete;
    WaitableQueueCore& operator=(const WaitableQueueCore&) = delete;
    WaitableQueueCore(WaitableQueueCore&&) noexcept;
    WaitableQueueCore& operator=(WaitableQueueCore&&) noexcept;

    [[nodiscard]] int native_handle() const noexcept;
    [[nodiscard]] bool enqueue_copy(const void* value);
    [[nodiscard]] bool enqueue_move(void* value);
    std::size_t enqueue_batch(const void* values, std::size_t count);
    [[nodiscard]] bool try_dequeue(void* value);
    std::uint64_t drain_notifications();

private:
    std::unique_ptr<QueueInterface> queue_storage;
    WaitableQueueNotifier notifier;
};

template <typename T>
class WaitableQueue
{
public:
    WaitableQueue(std::unique_ptr<QueueInterface> queue, std::unique_ptr<EventFd> event_fd)
        : core(std::move(queue), std::move(event_fd))
    {
    }

    ~WaitableQueue() noexcept = default;

    WaitableQueue(const WaitableQueue&)                = delete;
    WaitableQueue& operator=(const WaitableQueue&)     = delete;
    WaitableQueue(WaitableQueue&&) noexcept            = default;
    WaitableQueue& operator=(WaitableQueue&&) noexcept = default;

    [[nodiscard]] int native_handle() const noexcept
    {
        return core.native_handle();
    }

    [[nodiscard]] bool enqueue(const T& value)
    {
        return core.enqueue_copy(&value);
    }

    [[nodiscard]] bool enqueue(T&& value)
    {
        return core.enqueue_move(&value);
    }

    std::size_t enqueue_batch(std::span<const T> values)
    {
        return core.enqueue_batch(values.data(), values.size());
    }

    [[nodiscard]] bool try_dequeue(T& value)
    {
        return core.try_dequeue(&value);
    }

    std::uint64_t drain_notifications()
    {
        return core.drain_notifications();
    }

private:
    WaitableQueueCore core;
};

} // namespace exasol::udf::v2
