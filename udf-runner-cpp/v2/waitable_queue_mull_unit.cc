#include "waitable_queue_mull_unit.hpp"

namespace exasol::udf::v2::mull_test
{

WaitableSpscQueueInt::WaitableSpscQueueInt() = default;

WaitableSpscQueueInt::~WaitableSpscQueueInt() = default;

int WaitableSpscQueueInt::native_handle() const
{
    return queue_.native_handle();
}

bool WaitableSpscQueueInt::enqueue(int value)
{
    return queue_.enqueue(value);
}

std::size_t WaitableSpscQueueInt::enqueue_batch(std::span<const int> values)
{
    return queue_.enqueue_batch(values.begin(), values.end());
}

bool WaitableSpscQueueInt::try_dequeue(int& value)
{
    return queue_.try_dequeue(value);
}

std::uint64_t WaitableSpscQueueInt::drain_notifications()
{
    return queue_.drain_notifications();
}

WaitableMpmcQueueInt::WaitableMpmcQueueInt() = default;

WaitableMpmcQueueInt::~WaitableMpmcQueueInt() = default;

int WaitableMpmcQueueInt::native_handle() const
{
    return queue_.native_handle();
}

bool WaitableMpmcQueueInt::enqueue(int value)
{
    return queue_.enqueue(value);
}

bool WaitableMpmcQueueInt::try_dequeue(int& value)
{
    return queue_.try_dequeue(value);
}

std::uint64_t WaitableMpmcQueueInt::drain_notifications()
{
    return queue_.drain_notifications();
}

} // namespace exasol::udf::v2::mull_test
