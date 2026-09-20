#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

#include <exasol/udf/v2/waitable_queue.hpp>

namespace exasol::udf::v2::mull_test
{

class WaitableSpscQueueInt
{
public:
    WaitableSpscQueueInt();
    ~WaitableSpscQueueInt();

    WaitableSpscQueueInt(const WaitableSpscQueueInt&)            = delete;
    WaitableSpscQueueInt& operator=(const WaitableSpscQueueInt&) = delete;

    int native_handle() const;
    bool enqueue(int value);
    std::size_t enqueue_batch(std::span<const int> values);
    bool try_dequeue(int& value);
    std::uint64_t drain_notifications();

private:
    WaitableSpscQueue<int> queue_;
};

class WaitableMpmcQueueInt
{
public:
    WaitableMpmcQueueInt();
    ~WaitableMpmcQueueInt();

    WaitableMpmcQueueInt(const WaitableMpmcQueueInt&)            = delete;
    WaitableMpmcQueueInt& operator=(const WaitableMpmcQueueInt&) = delete;

    int native_handle() const;
    bool enqueue(int value);
    bool try_dequeue(int& value);
    std::uint64_t drain_notifications();

private:
    WaitableMpmcQueue<int> queue_;
};

} // namespace exasol::udf::v2::mull_test
