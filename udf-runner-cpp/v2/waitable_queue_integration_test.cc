#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <memory>
#include <span>
#include <utility>
#include <vector>

#include <exasol/udf/v2/linux_epoll.hpp>
#include <exasol/udf/v2/linux_waitable_queue.hpp>
#include <gtest/gtest.h>

namespace
{

constexpr std::uint64_t queue_token  = 1;
constexpr std::uint64_t socket_token = 2;

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
        ASSERT_TRUE(epoll_storage.is_open());
        ASSERT_EQ(::socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sockets_storage.data()), 0);
        queue_registration =
            epoll_storage.add(queue_storage.native_handle(),
                              exasol::udf::v2::EpollInterest{.readable = true}, queue_token);
        socket_registration = epoll_storage.add(
            sockets_storage[1], exasol::udf::v2::EpollInterest{.readable = true}, socket_token);
    }

    void TearDown() override
    {
        queue_registration.reset();
        socket_registration.reset();
        ::close(sockets_storage[0]);
        ::close(sockets_storage[1]);
    }

    exasol::udf::v2::WaitableSpscQueue<int>& queue() noexcept
    {
        return queue_storage;
    }

    [[nodiscard]] exasol::udf::v2::LinuxEpoll& epoll() noexcept
    {
        return epoll_storage;
    }

    [[nodiscard]] const std::array<int, 2>& sockets() const noexcept
    {
        return sockets_storage;
    }

private:
    exasol::udf::v2::WaitableSpscQueue<int> queue_storage =
        exasol::udf::v2::make_waitable_spsc_queue<int>();
    exasol::udf::v2::NativeLinuxEpoll epoll_storage;
    std::array<int, 2> sockets_storage{};
    std::unique_ptr<exasol::udf::v2::EpollRegistration> queue_registration;
    std::unique_ptr<exasol::udf::v2::EpollRegistration> socket_registration;
};

} // namespace

TEST_F(SpscEpollTest, ReportsQueueAndSocketReadiness)
{
    ASSERT_TRUE(queue().enqueue(42));
    const char byte = 'x';
    ASSERT_EQ(::write(sockets()[0], &byte, sizeof(byte)), sizeof(byte));

    std::array<exasol::udf::v2::EpollReadyEvent, 2> events{};
    const std::size_t event_count = epoll().wait(events, std::chrono::seconds(1));
    ASSERT_EQ(event_count, 2);

    bool queue_ready  = false;
    bool socket_ready = false;
    for (const auto& event : std::span(events).first(event_count))
    {
        queue_ready |= event.token == queue_token && event.readable;
        socket_ready |= event.token == socket_token && event.readable;
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
    auto moved_queue       = exasol::udf::v2::make_waitable_spsc_queue<int>();
    const int moved_handle = moved_queue.native_handle();
    auto move_constructed  = std::move(moved_queue);
    EXPECT_EQ(move_constructed.native_handle(), moved_handle);

    auto move_assigned = exasol::udf::v2::make_waitable_spsc_queue<int>();
    move_assigned      = std::move(move_constructed);
    EXPECT_EQ(move_assigned.native_handle(), moved_handle);
    self_move_assign(move_assigned);
}
