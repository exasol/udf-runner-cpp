#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <system_error>
#include <vector>

#include <exasol/udf/v2/waitable_queue.hpp>

namespace {

void test_check(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "waitable queue test failure: %s\n", message);
        std::abort();
    }
}

void add_to_epoll(int epoll_fd, int fd, std::uint32_t events) {
    epoll_event event{};
    event.events = events;
    event.data.fd = fd;
    test_check(::epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event) == 0,
               "epoll_ctl failed");
}

void close_pair(const std::array<int, 2>& sockets) {
    ::close(sockets[0]);
    ::close(sockets[1]);
}

}  // namespace

int main() {
    exasol::udf::v2::WaitableSpscQueue<int> queue;
    const int epoll_fd = ::epoll_create1(EPOLL_CLOEXEC);
    test_check(epoll_fd != -1, "epoll_create1 failed");

    std::array<int, 2> sockets{};
    test_check(::socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0,
                            sockets.data()) == 0,
               "socketpair failed");
    add_to_epoll(epoll_fd, queue.native_handle(), EPOLLIN);
    add_to_epoll(epoll_fd, sockets[1], EPOLLIN);

    test_check(queue.enqueue(42), "queue enqueue failed");
    const char byte = 'x';
    test_check(::write(sockets[0], &byte, sizeof(byte)) == sizeof(byte),
               "socket write failed");

    std::array<epoll_event, 2> events{};
    const int event_count = ::epoll_wait(epoll_fd, events.data(),
                                         events.size(), 1000);
    test_check(event_count == 2, "epoll_wait did not report both descriptors");

    bool queue_ready = false;
    bool socket_ready = false;
    for (int i = 0; i < event_count; ++i) {
        queue_ready |= events[i].data.fd == queue.native_handle();
        socket_ready |= events[i].data.fd == sockets[1];
    }
    test_check(queue_ready, "queue descriptor was not ready");
    test_check(socket_ready, "socket descriptor was not ready");

    test_check(queue.drain_notifications() == 1,
               "unexpected queue notification count");
    int value = 0;
    test_check(queue.try_dequeue(value), "queue dequeue failed");
    test_check(value == 42, "unexpected dequeued value");

    const std::vector<int> batch{1, 2, 3};
    test_check(queue.enqueue_batch(batch.begin(), batch.end()) == batch.size(),
               "batch enqueue failed");
    test_check(queue.drain_notifications() == 1,
               "unexpected batch notification count");
    for (int expected : batch) {
        test_check(queue.try_dequeue(value), "batch dequeue failed");
        test_check(value == expected, "unexpected batch value");
    }
    test_check(!queue.try_dequeue(value), "queue should be empty");

    exasol::udf::v2::WaitableMpmcQueue<int> mpmc;
    test_check(mpmc.enqueue(7), "MPMC queue enqueue failed");
    test_check(mpmc.drain_notifications() == 1,
               "unexpected MPMC notification count");
    test_check(mpmc.try_dequeue(value), "MPMC queue dequeue failed");
    test_check(value == 7, "unexpected MPMC value");

    close_pair(sockets);
    ::close(epoll_fd);
}
