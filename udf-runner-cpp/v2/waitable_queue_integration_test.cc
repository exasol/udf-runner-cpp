#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <cstdlib>
#include <cstdint>
#include <exception>
#include <iostream>
#include <span>
#include <utility>
#include <vector>

#include <exasol/udf/v2/waitable_queue.hpp>

namespace
{

void test_check(bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << "waitable queue integration test failure: " << message << '\n';
        std::abort();
    }
}

void add_to_epoll(int epoll_fd, int fd, std::uint32_t events)
{
    epoll_event event{};
    event.events  = events;
    event.data.fd = fd;
    test_check(::epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event) == 0, "epoll_ctl failed");
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

void test_spsc_epoll_and_queue_operations()
{
    exasol::udf::v2::WaitableSpscQueue<int> queue;
    const int epoll_fd = ::epoll_create1(EPOLL_CLOEXEC);
    test_check(epoll_fd != -1, "epoll_create1 failed");

    std::array<int, 2> sockets{};
    test_check(::socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sockets.data()) == 0,
               "socketpair failed");
    add_to_epoll(epoll_fd, queue.native_handle(), EPOLLIN);
    add_to_epoll(epoll_fd, sockets[1], EPOLLIN);

    test_check(queue.enqueue(42), "SPSC queue enqueue failed");
    const char byte = 'x';
    test_check(::write(sockets[0], &byte, sizeof(byte)) == sizeof(byte), "socket write failed");

    std::array<epoll_event, 2> events{};
    const int event_count = ::epoll_wait(epoll_fd, events.data(), events.size(), 1000);
    test_check(event_count == 2, "epoll_wait did not report both descriptors");

    bool queue_ready  = false;
    bool socket_ready = false;
    for (const auto& event : std::span(events).first(static_cast<std::size_t>(event_count)))
    {
        queue_ready |= event.data.fd == queue.native_handle();
        socket_ready |= event.data.fd == sockets[1];
    }
    test_check(queue_ready, "queue descriptor was not ready");
    test_check(socket_ready, "socket descriptor was not ready");

    test_check(queue.drain_notifications() == 1, "unexpected queue notification count");
    int value = 0;
    test_check(queue.try_dequeue(value), "SPSC queue dequeue failed");
    test_check(value == 42, "unexpected SPSC queue value");

    const std::vector<int> batch{1, 2, 3};
    test_check(queue.enqueue_batch(batch.begin(), batch.end()) == batch.size(),
               "SPSC batch enqueue failed");
    test_check(queue.drain_notifications() == 1, "unexpected SPSC batch notification count");
    for (int expected : batch)
    {
        test_check(queue.try_dequeue(value), "SPSC batch dequeue failed");
        test_check(value == expected, "unexpected SPSC batch value");
    }
    test_check(!queue.try_dequeue(value), "SPSC queue should be empty");

    const std::array<int, 0> empty_batch{};
    test_check(queue.enqueue_batch(empty_batch.begin(), empty_batch.end()) == 0,
               "empty SPSC batch should not enqueue values");
    test_check(queue.drain_notifications() == 0, "empty SPSC batch should not notify");

    const auto& const_queue = queue;
    test_check(&const_queue.queue() == &queue.queue(), "const SPSC queue access failed");

    exasol::udf::v2::WaitableSpscQueue<int> moved_queue;
    const int moved_handle = moved_queue.native_handle();
    exasol::udf::v2::WaitableSpscQueue<int> move_constructed(std::move(moved_queue));
    test_check(move_constructed.native_handle() == moved_handle,
               "SPSC move construction changed handle");
    exasol::udf::v2::WaitableSpscQueue<int> move_assigned;
    move_assigned = std::move(move_constructed);
    test_check(move_assigned.native_handle() == moved_handle,
               "SPSC move assignment changed handle");
    self_move_assign(move_assigned);

    close_pair(sockets);
    ::close(epoll_fd);
}

void test_mpmc_queue_operations()
{
    exasol::udf::v2::WaitableMpmcQueue<int> queue;
    test_check(queue.enqueue(7), "MPMC queue enqueue failed");
    test_check(queue.drain_notifications() == 1, "unexpected MPMC notification count");

    int value = 0;
    test_check(queue.try_dequeue(value), "MPMC queue dequeue failed");
    test_check(value == 7, "unexpected MPMC queue value");

    const std::vector<int> batch{8, 9};
    test_check(queue.enqueue_batch(batch.begin(), batch.end()) == batch.size(),
               "MPMC batch enqueue failed");
    test_check(queue.drain_notifications() == 1, "unexpected MPMC batch notification count");
    for (int expected : batch)
    {
        test_check(queue.try_dequeue(value), "MPMC batch dequeue failed");
        test_check(value == expected, "unexpected MPMC batch value");
    }

    const std::array<int, 0> empty_batch{};
    test_check(queue.enqueue_batch(empty_batch.begin(), empty_batch.end()) == 0,
               "empty MPMC batch should not enqueue values");
    test_check(queue.drain_notifications() == 0, "empty MPMC batch should not notify");

    const auto& const_queue = queue;
    test_check(&const_queue.queue() == &queue.queue(), "const MPMC queue access failed");

    exasol::udf::v2::WaitableMpmcQueue<int> moved_queue;
    const int moved_handle = moved_queue.native_handle();
    exasol::udf::v2::WaitableMpmcQueue<int> move_constructed(std::move(moved_queue));
    test_check(move_constructed.native_handle() == moved_handle,
               "MPMC move construction changed handle");
    exasol::udf::v2::WaitableMpmcQueue<int> move_assigned;
    move_assigned = std::move(move_constructed);
    test_check(move_assigned.native_handle() == moved_handle,
               "MPMC move assignment changed handle");
    self_move_assign(move_assigned);
}

} // namespace

int main()
{
    try
    {
        test_spsc_epoll_and_queue_operations();
        test_mpmc_queue_operations();
    }
    catch (const std::exception& error)
    {
        std::cerr << "waitable queue integration test failure: " << error.what() << '\n';
        return 1;
    }
}
