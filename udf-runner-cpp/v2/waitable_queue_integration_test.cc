#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

#include <exasol/udf/v2/waitable_queue.hpp>
#include <gtest/gtest.h>

namespace
{

bool add_to_epoll(int epoll_fd, int fd, std::uint32_t events)
{
    epoll_event event{};
    event.events  = events;
    event.data.fd = fd;
    return ::epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event) == 0;
}

void close_pair(const std::array<int, 2>& sockets)
{
    ::close(sockets[0]);
    ::close(sockets[1]);
}

template <typename Queue>
void self_move_assign(Queue& queue)
{
    using move_assignment        = Queue& (Queue::*)(Queue&&) noexcept;
    const move_assignment assign = &Queue::operator=;
    (queue.*assign)(std::move(queue));
}

} // namespace

TEST(WaitableQueueIntegrationTest, SpscEpollAndQueueOperations)
{
    exasol::udf::v2::WaitableSpscQueue<int> queue;
    const int epoll_fd = ::epoll_create1(EPOLL_CLOEXEC);
    ASSERT_NE(epoll_fd, -1);

    std::array<int, 2> sockets{};
    ASSERT_EQ(::socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sockets.data()), 0);
    ASSERT_TRUE(add_to_epoll(epoll_fd, queue.native_handle(), EPOLLIN));
    ASSERT_TRUE(add_to_epoll(epoll_fd, sockets[1], EPOLLIN));

    ASSERT_TRUE(queue.enqueue(42));
    const char byte = 'x';
    ASSERT_EQ(::write(sockets[0], &byte, sizeof(byte)), sizeof(byte));

    std::array<epoll_event, 2> events{};
    const int event_count = ::epoll_wait(epoll_fd, events.data(), events.size(), 1000);
    ASSERT_EQ(event_count, 2);

    bool queue_ready  = false;
    bool socket_ready = false;
    for (const auto& event : std::span(events).first(static_cast<std::size_t>(event_count)))
    {
        queue_ready |= event.data.fd == queue.native_handle();
        socket_ready |= event.data.fd == sockets[1];
    }
    EXPECT_TRUE(queue_ready);
    EXPECT_TRUE(socket_ready);

    EXPECT_EQ(queue.drain_notifications(), 1);
    int value = 0;
    ASSERT_TRUE(queue.try_dequeue(value));
    EXPECT_EQ(value, 42);

    const std::vector batch{1, 2, 3};
    EXPECT_EQ(queue.enqueue_batch(batch.begin(), batch.end()), batch.size());
    EXPECT_EQ(queue.drain_notifications(), 1);
    for (int expected : batch)
    {
        ASSERT_TRUE(queue.try_dequeue(value));
        EXPECT_EQ(value, expected);
    }
    EXPECT_FALSE(queue.try_dequeue(value));

    const std::array<int, 0> empty_batch{};
    EXPECT_EQ(queue.enqueue_batch(empty_batch.begin(), empty_batch.end()), 0);
    EXPECT_EQ(queue.drain_notifications(), 0);

    const auto& const_queue = queue;
    EXPECT_EQ(&const_queue.queue(), &queue.queue());

    exasol::udf::v2::WaitableSpscQueue<int> moved_queue;
    const int moved_handle = moved_queue.native_handle();
    exasol::udf::v2::WaitableSpscQueue<int> move_constructed(std::move(moved_queue));
    EXPECT_EQ(move_constructed.native_handle(), moved_handle);
    exasol::udf::v2::WaitableSpscQueue<int> move_assigned;
    move_assigned = std::move(move_constructed);
    EXPECT_EQ(move_assigned.native_handle(), moved_handle);
    self_move_assign(move_assigned);

    close_pair(sockets);
    ASSERT_EQ(::close(epoll_fd), 0);
}

TEST(WaitableQueueIntegrationTest, MpmcQueueOperations)
{
    exasol::udf::v2::WaitableMpmcQueue<int> queue;
    ASSERT_TRUE(queue.enqueue(7));
    EXPECT_EQ(queue.drain_notifications(), 1);

    int value = 0;
    ASSERT_TRUE(queue.try_dequeue(value));
    EXPECT_EQ(value, 7);

    const std::vector batch{8, 9};
    EXPECT_EQ(queue.enqueue_batch(batch.begin(), batch.end()), batch.size());
    EXPECT_EQ(queue.drain_notifications(), 1);
    for (int expected : batch)
    {
        ASSERT_TRUE(queue.try_dequeue(value));
        EXPECT_EQ(value, expected);
    }

    const std::array<int, 0> empty_batch{};
    EXPECT_EQ(queue.enqueue_batch(empty_batch.begin(), empty_batch.end()), 0);
    EXPECT_EQ(queue.drain_notifications(), 0);

    const auto& const_queue = queue;
    EXPECT_EQ(&const_queue.queue(), &queue.queue());

    exasol::udf::v2::WaitableMpmcQueue<int> moved_queue;
    const int moved_handle = moved_queue.native_handle();
    exasol::udf::v2::WaitableMpmcQueue<int> move_constructed(std::move(moved_queue));
    EXPECT_EQ(move_constructed.native_handle(), moved_handle);
    exasol::udf::v2::WaitableMpmcQueue<int> move_assigned;
    move_assigned = std::move(move_constructed);
    EXPECT_EQ(move_assigned.native_handle(), moved_handle);
    self_move_assign(move_assigned);
}
