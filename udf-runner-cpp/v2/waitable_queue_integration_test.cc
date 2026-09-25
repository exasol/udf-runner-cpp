#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

#include <exasol/udf/v2/linux_waitable_queue.hpp>
#include <gtest/gtest.h>

namespace
{

template <typename Queue>
void self_move_assign(Queue& queue)
{
    using move_assignment        = Queue& (Queue::*)(Queue&&) noexcept;
    const move_assignment assign = &Queue::operator=;
    (queue.*assign)(std::move(queue));
}

class SpscEpollTest : public testing::Test
{
protected:
    void SetUp() override
    {
        ASSERT_NE(epoll_fd_storage, -1);
        ASSERT_EQ(::socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sockets_storage.data()), 0);
        ASSERT_TRUE(add_to_epoll(queue_storage.native_handle(), EPOLLIN));
        ASSERT_TRUE(add_to_epoll(sockets_storage[1], EPOLLIN));
    }

    void TearDown() override
    {
        ::close(sockets_storage[0]);
        ::close(sockets_storage[1]);
        ::close(epoll_fd_storage);
    }

    bool add_to_epoll(int fd, std::uint32_t events) const
    {
        epoll_event event{};
        event.events  = events;
        event.data.fd = fd;
        return ::epoll_ctl(epoll_fd_storage, EPOLL_CTL_ADD, fd, &event) == 0;
    }

    exasol::udf::v2::WaitableSpscQueue<int>& queue() noexcept
    {
        return queue_storage;
    }

    [[nodiscard]] int epoll_fd() const noexcept
    {
        return epoll_fd_storage;
    }

    [[nodiscard]] const std::array<int, 2>& sockets() const noexcept
    {
        return sockets_storage;
    }

private:
    exasol::udf::v2::WaitableSpscQueue<int> queue_storage;
    int epoll_fd_storage = ::epoll_create1(EPOLL_CLOEXEC);
    std::array<int, 2> sockets_storage{};
};

} // namespace

TEST_F(SpscEpollTest, ReportsQueueAndSocketReadiness)
{
    ASSERT_TRUE(queue().enqueue(42));
    const char byte = 'x';
    ASSERT_EQ(::write(sockets()[0], &byte, sizeof(byte)), sizeof(byte));

    std::array<epoll_event, 2> events{};
    const int event_count = ::epoll_wait(epoll_fd(), events.data(), events.size(), 1000);
    ASSERT_EQ(event_count, 2);

    bool queue_ready  = false;
    bool socket_ready = false;
    for (const auto& event : std::span(events).first(static_cast<std::size_t>(event_count)))
    {
        queue_ready |= event.data.fd == queue().native_handle();
        socket_ready |= event.data.fd == sockets()[1];
    }
    EXPECT_TRUE(queue_ready);
    EXPECT_TRUE(socket_ready);
    EXPECT_EQ(queue().drain_notifications(), 1);
}

TEST_F(SpscEpollTest, SupportsQueueOperationsAndBatches)
{
    ASSERT_TRUE(queue().enqueue(42));
    EXPECT_EQ(queue().drain_notifications(), 1);

    int value = 0;
    ASSERT_TRUE(queue().try_dequeue(value));
    EXPECT_EQ(value, 42);

    const std::vector batch{1, 2, 3};
    EXPECT_EQ(queue().enqueue_batch(batch.begin(), batch.end()), batch.size());
    EXPECT_EQ(queue().drain_notifications(), 1);
    for (int expected : batch)
    {
        ASSERT_TRUE(queue().try_dequeue(value));
        EXPECT_EQ(value, expected);
    }
    EXPECT_FALSE(queue().try_dequeue(value));

    const std::array<int, 0> empty_batch{};
    EXPECT_EQ(queue().enqueue_batch(empty_batch.begin(), empty_batch.end()), 0);
    EXPECT_EQ(queue().drain_notifications(), 0);

    const auto& const_queue = queue();
    EXPECT_EQ(&const_queue.queue(), &queue().queue());
}

TEST_F(SpscEpollTest, SupportsMoves)
{
    exasol::udf::v2::WaitableSpscQueue<int> moved_queue;
    const int moved_handle = moved_queue.native_handle();
    exasol::udf::v2::WaitableSpscQueue<int> move_constructed(std::move(moved_queue));
    EXPECT_EQ(move_constructed.native_handle(), moved_handle);

    exasol::udf::v2::WaitableSpscQueue<int> move_assigned;
    move_assigned = std::move(move_constructed);
    EXPECT_EQ(move_assigned.native_handle(), moved_handle);
    self_move_assign(move_assigned);
}

TEST(WaitableQueueIntegrationTest, MpmcSingleValueOperations)
{
    exasol::udf::v2::WaitableMpmcQueue<int> queue;
    ASSERT_TRUE(queue.enqueue(7));
    EXPECT_EQ(queue.drain_notifications(), 1);

    int value = 0;
    ASSERT_TRUE(queue.try_dequeue(value));
    EXPECT_EQ(value, 7);
}

TEST(WaitableQueueIntegrationTest, MpmcBatchAndEmptyBatchOperations)
{
    exasol::udf::v2::WaitableMpmcQueue<int> queue;
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

TEST(WaitableQueueIntegrationTest, MpmcProvidesQueueAccess)
{
    exasol::udf::v2::WaitableMpmcQueue<int> queue;
    const auto& const_queue = queue;
    EXPECT_EQ(&const_queue.queue(), &queue.queue());
}

TEST(WaitableQueueIntegrationTest, MpmcSupportsMoves)
{
    exasol::udf::v2::WaitableMpmcQueue<int> moved_queue;
    const int moved_handle = moved_queue.native_handle();
    exasol::udf::v2::WaitableMpmcQueue<int> move_constructed(std::move(moved_queue));
    EXPECT_EQ(move_constructed.native_handle(), moved_handle);

    exasol::udf::v2::WaitableMpmcQueue<int> move_assigned;
    move_assigned = std::move(move_constructed);
    EXPECT_EQ(move_assigned.native_handle(), moved_handle);
    self_move_assign(move_assigned);
}
