#include <array>
#include <utility>
#include <vector>

#include <exasol/udf/v2/linux_waitable_queue.hpp>
#include <gtest/gtest.h>

// These tests exercise the Linux waitable-queue factory functions directly
// (a real eventfd, no epoll/socket readiness checks), so they run under
// normal coverage instrumentation. The epoll/socketpair-based readiness
// tests live in waitable_queue_integration_test.cc, which is excluded from
// coverage because it depends on real OS I/O timing.

namespace
{

template <typename Queue>
void self_move_assign(Queue& queue)
{
    using move_assignment        = Queue& (Queue::*)(Queue&&) noexcept;
    const move_assignment assign = &Queue::operator=;
    (queue.*assign)(std::move(queue));
}

} // namespace

TEST(LinuxWaitableQueueTest, SpscFactoryDefaultConstructsQueue)
{
    auto queue = exasol::udf::v2::make_waitable_spsc_queue<int>();
    ASSERT_TRUE(queue.enqueue(4));
    EXPECT_EQ(queue.drain_notifications(), 1);

    int value = 0;
    ASSERT_TRUE(queue.try_dequeue(value));
    EXPECT_EQ(value, 4);
}

TEST(LinuxWaitableQueueTest, SpscFactoryWrapsProvidedQueue)
{
    auto queue = exasol::udf::v2::make_waitable_spsc_queue<int>(exasol::udf::v2::SpscQueue<int>(4));
    ASSERT_TRUE(queue.enqueue(5));
    EXPECT_EQ(queue.drain_notifications(), 1);

    int value = 0;
    ASSERT_TRUE(queue.try_dequeue(value));
    EXPECT_EQ(value, 5);
}

TEST(LinuxWaitableQueueTest, MpmcSingleValueOperations)
{
    auto queue = exasol::udf::v2::make_waitable_mpmc_queue<int>();
    ASSERT_TRUE(queue.enqueue(7));
    EXPECT_EQ(queue.drain_notifications(), 1);

    int value = 0;
    ASSERT_TRUE(queue.try_dequeue(value));
    EXPECT_EQ(value, 7);
}

TEST(LinuxWaitableQueueTest, MpmcBatchAndEmptyBatchOperations)
{
    auto queue = exasol::udf::v2::make_waitable_mpmc_queue<int>();
    const std::vector batch{8, 9};
    EXPECT_EQ(queue.enqueue_batch(batch.begin(), batch.end()), batch.size());
    EXPECT_EQ(queue.drain_notifications(), 1);

    int value = 0;
    for (int expected : batch)
    {
        ASSERT_TRUE(queue.try_dequeue(value));
        EXPECT_EQ(value, expected);
    }

    const std::array<int, 0> empty_batch{};
    EXPECT_EQ(queue.enqueue_batch(empty_batch.begin(), empty_batch.end()), 0);
    EXPECT_EQ(queue.drain_notifications(), 0);
}

TEST(LinuxWaitableQueueTest, MpmcProvidesQueueAccess)
{
    auto queue              = exasol::udf::v2::make_waitable_mpmc_queue<int>();
    const auto& const_queue = queue;
    EXPECT_EQ(&const_queue.queue(), &queue.queue());
}

TEST(LinuxWaitableQueueTest, MpmcSupportsMoves)
{
    auto moved_queue       = exasol::udf::v2::make_waitable_mpmc_queue<int>();
    const int moved_handle = moved_queue.native_handle();
    auto move_constructed  = std::move(moved_queue);
    EXPECT_EQ(move_constructed.native_handle(), moved_handle);

    auto move_assigned = exasol::udf::v2::make_waitable_mpmc_queue<int>();
    move_assigned      = std::move(move_constructed);
    EXPECT_EQ(move_assigned.native_handle(), moved_handle);
    self_move_assign(move_assigned);
}

TEST(LinuxWaitableQueueTest, MpmcFactoryWrapsProvidedQueue)
{
    auto queue = exasol::udf::v2::make_waitable_mpmc_queue<int>(exasol::udf::v2::MpmcQueue<int>(4));
    ASSERT_TRUE(queue.enqueue(6));
    EXPECT_EQ(queue.drain_notifications(), 1);

    int value = 0;
    ASSERT_TRUE(queue.try_dequeue(value));
    EXPECT_EQ(value, 6);
}
