#include <cstdint>
#include <deque>
#include <limits>
#include <memory>
#include <stdexcept>
#include <system_error>
#include <utility>
#include <vector>

#include <exasol/udf/v2/waitable_queue.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace
{

class MockQueue
{
public:
    explicit MockQueue(std::size_t capacity = std::numeric_limits<std::size_t>::max())
        : capacity(capacity)
    {
    }

    bool enqueue(int value)
    {
        if (values.size() == capacity)
        {
            return false;
        }
        values.push_back(value);
        return true;
    }

    bool try_dequeue(int& value)
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
    MOCK_METHOD(int, native_handle, (), (const, noexcept, override));
    MOCK_METHOD(std::uint64_t, read_notification, (), (override));
    MOCK_METHOD(void, write_notification, (), (override));
};

using testing::Return;
using testing::StrictMock;
using testing::Throw;

} // namespace

TEST(WaitableQueueTest, EnqueuesDequeuesAndDrainsNotifications)
{
    auto event_fd = std::make_unique<StrictMock<MockEventFd>>();
    EXPECT_CALL(*event_fd, native_handle()).WillOnce(Return(42));
    EXPECT_CALL(*event_fd, write_notification()).WillOnce(Return());
    EXPECT_CALL(*event_fd, read_notification())
        .WillOnce(Return(3))
        .WillOnce(Throw(
            std::system_error(std::make_error_code(std::errc::resource_unavailable_try_again))));

    using WaitableQueue = exasol::udf::v2::WaitableQueue<MockQueue>;
    auto queue          = WaitableQueue(MockQueue{}, std::move(event_fd));
    EXPECT_EQ(queue.native_handle(), 42);
    ASSERT_TRUE(queue.enqueue(1));
    EXPECT_EQ(queue.drain_notifications(), 3);

    int value = 0;
    ASSERT_TRUE(queue.try_dequeue(value));
    EXPECT_EQ(value, 1);
    EXPECT_FALSE(queue.try_dequeue(value));
}

TEST(WaitableQueueTest, EnqueuesBatchesAndProvidesQueueAccessors)
{
    auto event_fd = std::make_unique<StrictMock<MockEventFd>>();
    EXPECT_CALL(*event_fd, write_notification()).WillOnce(Return());

    using WaitableQueue = exasol::udf::v2::WaitableQueue<MockQueue>;
    auto queue          = WaitableQueue(MockQueue{}, std::move(event_fd));
    const std::vector batch{2, 3};
    EXPECT_EQ(queue.enqueue_batch(batch.begin(), batch.end()), batch.size());
    EXPECT_EQ(queue.queue().size(), batch.size());
    const auto& const_queue = queue;
    EXPECT_EQ(&const_queue.queue(), &queue.queue());
}

TEST(WaitableQueueTest, DoesNotNotifyForEmptyBatch)
{
    auto event_fd       = std::make_unique<StrictMock<MockEventFd>>();
    using WaitableQueue = exasol::udf::v2::WaitableQueue<MockQueue>;
    auto queue          = WaitableQueue(MockQueue{}, std::move(event_fd));
    const std::vector<int> empty_batch;
    EXPECT_EQ(queue.enqueue_batch(empty_batch.begin(), empty_batch.end()), 0);
}

TEST(WaitableQueueTest, RetriesInterruptedReads)
{
    auto event_fd = std::make_unique<StrictMock<MockEventFd>>();
    EXPECT_CALL(*event_fd, read_notification())
        .WillOnce(Throw(std::system_error(std::make_error_code(std::errc::interrupted))))
        .WillOnce(Return(7))
        .WillOnce(Throw(
            std::system_error(std::make_error_code(std::errc::resource_unavailable_try_again))));

    using WaitableQueue = exasol::udf::v2::WaitableQueue<MockQueue>;
    auto queue          = WaitableQueue(MockQueue{}, std::move(event_fd));
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

    using WaitableQueue = exasol::udf::v2::WaitableQueue<MockQueue>;
    auto queue          = WaitableQueue(MockQueue{}, std::move(event_fd));
    EXPECT_TRUE(queue.enqueue(1));
    EXPECT_TRUE(queue.enqueue(2));
}

TEST(WaitableQueueTest, PropagatesReadErrors)
{
    auto event_fd = std::make_unique<StrictMock<MockEventFd>>();
    EXPECT_CALL(*event_fd, read_notification())
        .WillOnce(Throw(std::system_error(std::make_error_code(std::errc::io_error))));

    using WaitableQueue = exasol::udf::v2::WaitableQueue<MockQueue>;
    auto queue          = WaitableQueue(MockQueue{}, std::move(event_fd));
    try
    {
        static_cast<void>(queue.drain_notifications());
        ADD_FAILURE() << "expected read error";
    }
    catch (const std::system_error& error)
    {
        EXPECT_EQ(error.code(), std::make_error_code(std::errc::io_error));
    }
}

TEST(WaitableQueueTest, PropagatesWriteErrors)
{
    auto event_fd = std::make_unique<StrictMock<MockEventFd>>();
    EXPECT_CALL(*event_fd, write_notification())
        .WillOnce(Throw(std::system_error(std::make_error_code(std::errc::io_error))));

    using WaitableQueue = exasol::udf::v2::WaitableQueue<MockQueue>;
    auto queue          = WaitableQueue(MockQueue{}, std::move(event_fd));
    try
    {
        static_cast<void>(queue.enqueue(1));
        ADD_FAILURE() << "expected write error";
    }
    catch (const std::system_error& error)
    {
        EXPECT_EQ(error.code(), std::make_error_code(std::errc::io_error));
    }
}

TEST(WaitableQueueTest, StopsAtLimitedCapacity)
{
    auto event_fd = std::make_unique<StrictMock<MockEventFd>>();
    EXPECT_CALL(*event_fd, write_notification()).WillOnce(Return());

    using WaitableQueue = exasol::udf::v2::WaitableQueue<MockQueue>;
    auto queue          = WaitableQueue(MockQueue{1}, std::move(event_fd));
    const std::vector batch{1, 2};
    EXPECT_EQ(queue.enqueue_batch(batch.begin(), batch.end()), 1);
    EXPECT_FALSE(queue.enqueue(3));
}

TEST(WaitableQueueTest, HandlesZeroCapacity)
{
    auto event_fd       = std::make_unique<StrictMock<MockEventFd>>();
    using WaitableQueue = exasol::udf::v2::WaitableQueue<MockQueue>;
    auto queue          = WaitableQueue(MockQueue{0}, std::move(event_fd));
    const std::vector batch{1, 2};
    EXPECT_EQ(queue.enqueue_batch(batch.begin(), batch.end()), 0);
}

TEST(WaitableQueueTest, SupportsMoveConstruction)
{
    auto event_fd = std::make_unique<StrictMock<MockEventFd>>();
    EXPECT_CALL(*event_fd, native_handle()).WillOnce(Return(42));

    using WaitableQueue = exasol::udf::v2::WaitableQueue<MockQueue>;
    WaitableQueue moved_queue(MockQueue{}, std::move(event_fd));
    WaitableQueue move_constructed(std::move(moved_queue));
    EXPECT_EQ(move_constructed.native_handle(), 42);
}

TEST(WaitableQueueTest, SupportsMoveAssignmentAndSelfMove)
{
    auto source_event_fd = std::make_unique<StrictMock<MockEventFd>>();
    EXPECT_CALL(*source_event_fd, native_handle()).WillOnce(Return(42));
    auto target_event_fd = std::make_unique<StrictMock<MockEventFd>>();

    using WaitableQueue = exasol::udf::v2::WaitableQueue<MockQueue>;
    WaitableQueue source(MockQueue{}, std::move(source_event_fd));
    WaitableQueue target(MockQueue{}, std::move(target_event_fd));
    target = std::move(source);
    EXPECT_EQ(target.native_handle(), 42);
    using move_assignment        = WaitableQueue& (WaitableQueue::*)(WaitableQueue&&) noexcept;
    const move_assignment assign = &WaitableQueue::operator=;
    (target.*assign)(std::move(target));
}

TEST(WaitableQueueTest, RejectsNullEventFd)
{
    auto null_event_fd = std::unique_ptr<exasol::udf::v2::EventFd>{};
    EXPECT_THROW(
        { auto queue = exasol::udf::v2::WaitableQueue(MockQueue{}, std::move(null_event_fd)); },
        std::invalid_argument);
}
