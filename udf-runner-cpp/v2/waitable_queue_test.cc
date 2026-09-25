#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <memory>
#include <span>
#include <system_error>
#include <variant>
#include <vector>

#include <exasol/udf/v2/waitable_queue.hpp>

namespace
{

void test_check(bool condition, const char* message)
{
    if (!condition)
    {
        std::fprintf(stderr, "waitable queue test failure: %s\n", message);
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

class LimitedQueue
{
public:
    explicit LimitedQueue(std::size_t capacity) : remaining(capacity)
    {
    }

    bool enqueue(int value)
    {
        static_cast<void>(value);
        if (remaining == 0)
        {
            return false;
        }
        --remaining;
        return true;
    }

private:
    std::size_t remaining;
};

class MockEventFd final : public exasol::udf::v2::EventFd
{
public:
    using read_action  = std::variant<std::uint64_t, std::errc>;
    using write_action = std::variant<std::monostate, std::errc>;

    void set_read_actions(std::deque<read_action> actions)
    {
        read_actions = std::move(actions);
    }

    void set_write_actions(std::deque<write_action> actions)
    {
        write_actions = std::move(actions);
    }

    [[nodiscard]] int native_handle() const noexcept override
    {
        return 42;
    }

    std::uint64_t read_notification() override
    {
        const auto action = read_actions.empty()
                                ? read_action{std::errc::resource_unavailable_try_again}
                                : read_actions.front();
        if (!read_actions.empty())
        {
            read_actions.pop_front();
        }
        if (const auto* error = std::get_if<std::errc>(&action))
        {
            throw std::system_error(std::make_error_code(*error), "mock eventfd read");
        }
        return std::get<std::uint64_t>(action);
    }

    void write_notification() override
    {
        const auto action =
            write_actions.empty() ? write_action{std::monostate{}} : write_actions.front();
        if (!write_actions.empty())
        {
            write_actions.pop_front();
        }
        if (const auto* error = std::get_if<std::errc>(&action))
        {
            throw std::system_error(std::make_error_code(*error), "mock eventfd write");
        }
    }

private:
    std::deque<read_action> read_actions;
    std::deque<write_action> write_actions;
};

template <typename Function>
void expect_system_error(Function&& function, std::errc expected, const char* message)
{
    try
    {
        function();
        test_check(false, message);
    }
    catch (const std::system_error& error)
    {
        test_check(error.code() == std::make_error_code(expected), "unexpected system error");
    }
}

template <typename Function>
void expect_invalid_argument(Function&& function, const char* message)
{
    try
    {
        function();
        test_check(false, message);
    }
    catch (const std::invalid_argument& error)
    {
        test_check(error.what() != nullptr, "invalid-argument error did not contain a message");
    }
}

template <typename Queue>
void self_move_assign(Queue& queue)
{
    using move_assignment        = Queue& (Queue::*)(Queue&&) noexcept;
    const move_assignment assign = &Queue::operator=;
    (queue.*assign)(std::move(queue));
}

} // namespace

int main()
{
    try
    {
        exasol::udf::v2::WaitableSpscQueue<int> queue;
        const int epoll_fd = ::epoll_create1(EPOLL_CLOEXEC);
        test_check(epoll_fd != -1, "epoll_create1 failed");

        std::array<int, 2> sockets{};
        test_check(::socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sockets.data()) == 0,
                   "socketpair failed");
        add_to_epoll(epoll_fd, queue.native_handle(), EPOLLIN);
        add_to_epoll(epoll_fd, sockets[1], EPOLLIN);

        test_check(queue.enqueue(42), "queue enqueue failed");
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
        test_check(queue.try_dequeue(value), "queue dequeue failed");
        test_check(value == 42, "unexpected dequeued value");

        const std::vector<int> batch{1, 2, 3};
        test_check(queue.enqueue_batch(batch.begin(), batch.end()) == batch.size(),
                   "batch enqueue failed");
        test_check(queue.drain_notifications() == 1, "unexpected batch notification count");
        for (int expected : batch)
        {
            test_check(queue.try_dequeue(value), "batch dequeue failed");
            test_check(value == expected, "unexpected batch value");
        }
        test_check(!queue.try_dequeue(value), "queue should be empty");

        const std::array<int, 0> empty_batch{};
        test_check(queue.enqueue_batch(empty_batch.begin(), empty_batch.end()) == 0,
                   "empty batch should not enqueue values");
        test_check(queue.drain_notifications() == 0, "empty batch should not notify");

        const auto& const_queue = queue;
        test_check(&const_queue.queue() == &queue.queue(), "const queue access failed");

        exasol::udf::v2::WaitableSpscQueue<int> moved_queue;
        const int moved_handle = moved_queue.native_handle();
        exasol::udf::v2::WaitableSpscQueue<int> move_constructed(std::move(moved_queue));
        test_check(move_constructed.native_handle() == moved_handle,
                   "move construction changed handle");

        exasol::udf::v2::WaitableSpscQueue<int> move_assigned;
        move_assigned = std::move(move_constructed);
        test_check(move_assigned.native_handle() == moved_handle, "move assignment changed handle");
        self_move_assign(move_assigned);

        auto limited_queue = exasol::udf::v2::WaitableQueue{LimitedQueue{1}};
        const std::array<int, 2> limited_batch{1, 2};
        test_check(limited_queue.enqueue_batch(limited_batch.begin(), limited_batch.end()) == 1,
                   "limited queue should stop at capacity");
        test_check(limited_queue.drain_notifications() == 1,
                   "limited queue should notify successful enqueue");
        test_check(!limited_queue.enqueue(3), "full queue should reject enqueue");
        test_check(limited_queue.drain_notifications() == 0, "failed enqueue should not notify");

        auto read_event_fd = std::make_unique<MockEventFd>();
        read_event_fd->set_read_actions(
            {std::errc::interrupted, std::uint64_t{7}, std::errc::resource_unavailable_try_again});
        auto read_queue = exasol::udf::v2::WaitableQueue(LimitedQueue{1}, std::move(read_event_fd));
        test_check(read_queue.native_handle() == 42, "mock eventfd handle was not retained");
        test_check(read_queue.drain_notifications() == 7, "mock notification drain failed");

        auto write_event_fd = std::make_unique<MockEventFd>();
        write_event_fd->set_write_actions(
            {std::errc::interrupted, std::monostate{}, std::errc::resource_unavailable_try_again});
        auto write_queue =
            exasol::udf::v2::WaitableQueue(LimitedQueue{2}, std::move(write_event_fd));
        test_check(write_queue.enqueue(1), "interrupted mock write should retry");
        test_check(write_queue.enqueue(2), "saturated mock write should be ignored");

        auto error_event_fd = std::make_unique<MockEventFd>();
        error_event_fd->set_read_actions({std::errc::io_error});
        auto error_queue =
            exasol::udf::v2::WaitableQueue(LimitedQueue{1}, std::move(error_event_fd));
        expect_system_error([&error_queue] { error_queue.drain_notifications(); },
                            std::errc::io_error, "read error should be propagated");

        auto write_error_event_fd = std::make_unique<MockEventFd>();
        write_error_event_fd->set_write_actions({std::errc::io_error});
        auto write_error_queue =
            exasol::udf::v2::WaitableQueue(LimitedQueue{1}, std::move(write_error_event_fd));
        expect_system_error(
            [&write_error_queue] { static_cast<void>(write_error_queue.enqueue(1)); },
            std::errc::io_error, "write error should be propagated");

        expect_invalid_argument(
            [] {
                auto null_event_fd = std::unique_ptr<exasol::udf::v2::EventFd>{};
                auto queue =
                    exasol::udf::v2::WaitableQueue(LimitedQueue{1}, std::move(null_event_fd));
                static_cast<void>(queue);
            },
            "null eventfd implementation should be rejected");

        exasol::udf::v2::WaitableMpmcQueue<int> mpmc;
        test_check(mpmc.enqueue(7), "MPMC queue enqueue failed");
        test_check(mpmc.drain_notifications() == 1, "unexpected MPMC notification count");
        test_check(mpmc.try_dequeue(value), "MPMC queue dequeue failed");
        test_check(value == 7, "unexpected MPMC value");

        close_pair(sockets);
        ::close(epoll_fd);
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "waitable queue test failure: %s\n", error.what());
        return 1;
    }
}
