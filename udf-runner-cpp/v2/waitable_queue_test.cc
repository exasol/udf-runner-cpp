#include <cstdint>
#include <deque>
#include <limits>
#include <memory>
#include <span>
#include <stdexcept>
#include <system_error>
#include <utility>
#include <vector>

#include <exasol/udf/v2/waitable_queue.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace
{

class MockQueueAdapter final : public exasol::udf::v2::QueueInterface<int>
{
public:
    explicit MockQueueAdapter(std::size_t capacity = std::numeric_limits<std::size_t>::max())
        : capacity(capacity)
    {
    }

    bool enqueue(const int& value) override
    {
        if (values.size() == capacity)
        {
            return false;
        }
        values.push_back(value);
        return true;
    }

    bool enqueue(int&& value) override
    {
        return enqueue(value);
    }

    std::size_t enqueue_batch(std::span<const int> batch) override
    {
        std::size_t enqueued = 0;
        for (const int value : batch)
        {
            if (!enqueue(value))
            {
                break;
            }
            ++enqueued;
        }
        return enqueued;
    }

    bool try_dequeue(int& value) override
    {
        if (values.empty())
        {
            return false;
        }
        value = values.front();
        values.pop_front();
        return true;
    }

    [[nodiscard]] std::size_t size() const noexcept
    {
        return values.size();
    }

private:
    std::size_t capacity;
    std::deque<int> values;
};

class MockEventFd : public exasol::udf::v2::EventFd
{
public:
    [[nodiscard]] int native_handle() const noexcept override
    {
        return 42;
    }

    MOCK_METHOD(std::uint64_t, read_notification, (), (override));
    MOCK_METHOD(void, write_notification, (), (override));
};

using WaitableQueue = exasol::udf::v2::WaitableQueue<int>;
using testing::Return;
using testing::StrictMock;
using testing::Throw;

void self_move_assign(WaitableQueue& queue)
{
    using move_assignment        = WaitableQueue& (WaitableQueue::*)(WaitableQueue&&) noexcept;
    const move_assignment assign = &WaitableQueue::operator=;
    (queue.*assign)(std::move(queue));
}

std::unique_ptr<MockQueueAdapter> make_queue(
    std::size_t capacity = std::numeric_limits<std::size_t>::max())
{
    return std::make_unique<MockQueueAdapter>(capacity);
}

} // namespace

TEST(WaitableQueueTest, EnqueuesDequeuesAndDrainsNotifications)
{
    auto event_fd = std::make_unique<StrictMock<MockEventFd>>();
    EXPECT_CALL(*event_fd, write_notification()).WillOnce(Return());
    EXPECT_CALL(*event_fd, read_notification())
        .WillOnce(Return(3))
        .WillOnce(Throw(
            std::system_error(std::make_error_code(std::errc::resource_unavailable_try_again))));

    auto queue = WaitableQueue(make_queue(), std::move(event_fd));
    EXPECT_EQ(queue.native_handle(), 42);
    ASSERT_TRUE(queue.enqueue(1));
    EXPECT_EQ(queue.drain_notifications(), 3);

    int value = 0;
    ASSERT_TRUE(queue.try_dequeue(value));
    EXPECT_EQ(value, 1);
    EXPECT_FALSE(queue.try_dequeue(value));
}

TEST(WaitableQueueTest, EnqueuesBatches)
{
    auto event_fd = std::make_unique<StrictMock<MockEventFd>>();
    EXPECT_CALL(*event_fd, write_notification()).WillOnce(Return());
    auto mock_queue           = make_queue();
    const auto* queue_storage = mock_queue.get();
    auto queue                = WaitableQueue(std::move(mock_queue), std::move(event_fd));
    const std::vector batch{2, 3};
    EXPECT_EQ(queue.enqueue_batch(batch), batch.size());
    EXPECT_EQ(queue_storage->size(), batch.size());
}

TEST(WaitableQueueTest, DoesNotNotifyForEmptyBatch)
{
    auto event_fd = std::make_unique<StrictMock<MockEventFd>>();
    auto queue    = WaitableQueue(make_queue(), std::move(event_fd));
    const std::vector<int> empty_batch;
    EXPECT_EQ(queue.enqueue_batch(empty_batch), 0);
}

TEST(WaitableQueueTest, RetriesInterruptedReads)
{
    auto event_fd = std::make_unique<StrictMock<MockEventFd>>();
    EXPECT_CALL(*event_fd, read_notification())
        .WillOnce(Throw(std::system_error(std::make_error_code(std::errc::interrupted))))
        .WillOnce(Return(7))
        .WillOnce(Throw(
            std::system_error(std::make_error_code(std::errc::resource_unavailable_try_again))));

    auto queue = WaitableQueue(make_queue(), std::move(event_fd));
    EXPECT_EQ(queue.drain_notifications(), 7);
}

TEST(WaitableQueueTest, RetriesInterruptedWritesAndIgnoresSaturation)
{
    auto event_fd = std::make_unique<StrictMock<MockEventFd>>();
    EXPECT_CALL(*event_fd, write_notification())
        .WillOnce(Throw(std::system_error(std::make_error_code(std::errc::interrupted))))
        .WillOnce(Return())
        .WillOnce(Throw(
            std::system_error(std::make_error_code(std::errc::resource_unavailable_try_again))));

    auto queue = WaitableQueue(make_queue(), std::move(event_fd));
    EXPECT_TRUE(queue.enqueue(1));
    EXPECT_TRUE(queue.enqueue(2));
}

TEST(WaitableQueueTest, PropagatesReadErrors)
{
    auto event_fd = std::make_unique<StrictMock<MockEventFd>>();
    EXPECT_CALL(*event_fd, read_notification())
        .WillOnce(Throw(std::system_error(std::make_error_code(std::errc::io_error))));

    auto queue = WaitableQueue(make_queue(), std::move(event_fd));
    EXPECT_THROW(static_cast<void>(queue.drain_notifications()), std::system_error);
}

TEST(WaitableQueueTest, PropagatesWriteErrors)
{
    auto event_fd = std::make_unique<StrictMock<MockEventFd>>();
    EXPECT_CALL(*event_fd, write_notification())
        .WillOnce(Throw(std::system_error(std::make_error_code(std::errc::io_error))));

    auto queue = WaitableQueue(make_queue(), std::move(event_fd));
    EXPECT_THROW(static_cast<void>(queue.enqueue(1)), std::system_error);
}

TEST(WaitableQueueTest, StopsAtLimitedCapacity)
{
    auto event_fd = std::make_unique<StrictMock<MockEventFd>>();
    EXPECT_CALL(*event_fd, write_notification()).WillOnce(Return());

    auto queue = WaitableQueue(make_queue(1), std::move(event_fd));
    const std::vector batch{1, 2};
    EXPECT_EQ(queue.enqueue_batch(batch), 1);
    EXPECT_FALSE(queue.enqueue(3));
}

TEST(WaitableQueueTest, HandlesZeroCapacity)
{
    auto event_fd = std::make_unique<StrictMock<MockEventFd>>();
    auto queue    = WaitableQueue(make_queue(0), std::move(event_fd));
    const std::vector batch{1, 2};
    EXPECT_EQ(queue.enqueue_batch(batch), 0);
}

TEST(WaitableQueueTest, SupportsMoveConstruction)
{
    auto event_fd = std::make_unique<StrictMock<MockEventFd>>();
    WaitableQueue moved_queue(make_queue(), std::move(event_fd));
    WaitableQueue move_constructed(std::move(moved_queue));
    EXPECT_EQ(move_constructed.native_handle(), 42);
}

TEST(WaitableQueueTest, SupportsMoveAssignmentAndSelfMove)
{
    auto source_event_fd = std::make_unique<StrictMock<MockEventFd>>();
    auto target_event_fd = std::make_unique<StrictMock<MockEventFd>>();
    WaitableQueue source(make_queue(), std::move(source_event_fd));
    WaitableQueue target(make_queue(), std::move(target_event_fd));
    target = std::move(source);
    EXPECT_EQ(target.native_handle(), 42);
    self_move_assign(target);
}

TEST(WaitableQueueTest, RejectsNullEventFd)
{
    auto null_event_fd = std::unique_ptr<exasol::udf::v2::EventFd>{};
    EXPECT_THROW(
        { auto queue = WaitableQueue(make_queue(), std::move(null_event_fd)); },
        std::invalid_argument);
}

TEST(WaitableQueueTest, RejectsNullQueue)
{
    auto event_fd   = std::make_unique<StrictMock<MockEventFd>>();
    auto null_queue = std::unique_ptr<exasol::udf::v2::QueueInterface<int>>{};
    EXPECT_THROW(
        { auto queue = WaitableQueue(std::move(null_queue), std::move(event_fd)); },
        std::invalid_argument);
}
