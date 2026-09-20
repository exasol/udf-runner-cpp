#include "waitable_queue_mull_unit.hpp"

#include <memory>

#include <exasol/udf/v2/waitable_queue.hpp>

namespace exasol::udf::v2::mull_test
{

class WaitableSpscQueueInt::Impl
{
public:
    WaitableSpscQueue<int> queue;
};

WaitableSpscQueueInt::WaitableSpscQueueInt() : impl_(std::make_unique<Impl>())
{
}

WaitableSpscQueueInt::~WaitableSpscQueueInt() = default;

WaitableSpscQueueInt::WaitableSpscQueueInt(WaitableSpscQueueInt&&) noexcept = default;

WaitableSpscQueueInt& WaitableSpscQueueInt::operator=(WaitableSpscQueueInt&&) noexcept = default;

int WaitableSpscQueueInt::native_handle() const
{
    return impl_->queue.native_handle();
}

bool WaitableSpscQueueInt::enqueue(int value)
{
    return impl_->queue.enqueue(value);
}

std::size_t WaitableSpscQueueInt::enqueue_batch(std::span<const int> values)
{
    return impl_->queue.enqueue_batch(values.begin(), values.end());
}

bool WaitableSpscQueueInt::try_dequeue(int& value)
{
    return impl_->queue.try_dequeue(value);
}

std::uint64_t WaitableSpscQueueInt::drain_notifications()
{
    return impl_->queue.drain_notifications();
}

class WaitableMpmcQueueInt::Impl
{
public:
    WaitableMpmcQueue<int> queue;
};

WaitableMpmcQueueInt::WaitableMpmcQueueInt() : impl_(std::make_unique<Impl>())
{
}

WaitableMpmcQueueInt::~WaitableMpmcQueueInt() = default;

WaitableMpmcQueueInt::WaitableMpmcQueueInt(WaitableMpmcQueueInt&&) noexcept = default;

WaitableMpmcQueueInt& WaitableMpmcQueueInt::operator=(WaitableMpmcQueueInt&&) noexcept = default;

int WaitableMpmcQueueInt::native_handle() const
{
    return impl_->queue.native_handle();
}

bool WaitableMpmcQueueInt::enqueue(int value)
{
    return impl_->queue.enqueue(value);
}

bool WaitableMpmcQueueInt::try_dequeue(int& value)
{
    return impl_->queue.try_dequeue(value);
}

std::uint64_t WaitableMpmcQueueInt::drain_notifications()
{
    return impl_->queue.drain_notifications();
}

} // namespace exasol::udf::v2::mull_test
