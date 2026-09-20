#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>

namespace exasol::udf::v2::mull_test
{

class WaitableSpscQueueInt
{
public:
    WaitableSpscQueueInt();
    ~WaitableSpscQueueInt();

    WaitableSpscQueueInt(const WaitableSpscQueueInt&)            = delete;
    WaitableSpscQueueInt& operator=(const WaitableSpscQueueInt&) = delete;
    WaitableSpscQueueInt(WaitableSpscQueueInt&&) noexcept;
    WaitableSpscQueueInt& operator=(WaitableSpscQueueInt&&) noexcept;

    [[nodiscard]] int native_handle() const;
    bool enqueue(int value);
    std::size_t enqueue_batch(std::span<const int> values);
    bool try_dequeue(int& value);
    std::uint64_t drain_notifications();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

class WaitableMpmcQueueInt
{
public:
    WaitableMpmcQueueInt();
    ~WaitableMpmcQueueInt();

    WaitableMpmcQueueInt(const WaitableMpmcQueueInt&)            = delete;
    WaitableMpmcQueueInt& operator=(const WaitableMpmcQueueInt&) = delete;
    WaitableMpmcQueueInt(WaitableMpmcQueueInt&&) noexcept;
    WaitableMpmcQueueInt& operator=(WaitableMpmcQueueInt&&) noexcept;

    [[nodiscard]] int native_handle() const;
    bool enqueue(int value);
    bool try_dequeue(int& value);
    std::uint64_t drain_notifications();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace exasol::udf::v2::mull_test
