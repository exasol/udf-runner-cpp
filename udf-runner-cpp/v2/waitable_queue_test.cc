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

class MockQueueAdapter final : public exasol::udf::v2::QueueInterface
{
public:
    explicit MockQueueAdapter(std::size_t capacity = std::numeric_limits<std::size_t>::max())
        : capacity(capacity)
    {
    }

    bool enqueue_copy(const void* value) override
    {
        if (values.size() == capacity)
        {
            return false;
        }
        values.push_back(*static_cast<const int*>(value));
        return true;
    }

    bool enqueue_move(void* value) override
    {
        if (values.size() == capacity)
        {
            return false;
        }
        values.push_back(*static_cast<int*>(value));
        return true;
    }

    std::size_t enqueue_batch(const void* batch, std::size_t count) override
    {
        std::size_t enqueued          = 0;
        const auto* values_to_enqueue = static_cast<const int*>(batch);
        for (; enqueued < count; ++enqueued)
        {
            if (!enqueue_copy(&values_to_enqueue[enqueued]))
            {
                break;
            }
        }
        return enqueued;
    }

    bool try_dequeue(void* value) override
    {
        if (values.empty())
        {
            return false;
        }
        *static_cast<int*>(value) = values.front();
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

struct MoveOnlyValue
{
    explicit MoveOnlyValue(const int value) : value(value)
    {
    }

    MoveOnlyValue(const MoveOnlyValue&)            = delete;
    MoveOnlyValue& operator=(const MoveOnlyValue&) = delete;

    MoveOnlyValue(MoveOnlyValue&& other) noexcept : value(std::exchange(other.value, -1))
    {
    }

    MoveOnlyValue& operator=(MoveOnlyValue&& other) noexcept
    {
        value = std::exchange(other.value, -1);
        return *this;
    }

    ~MoveOnlyValue() = default;

    [[nodiscard]] int get() const noexcept
    {
        return value;
    }

private:
    int value;
};

class MoveOnlyQueueAdapter final : public exasol::udf::v2::QueueInterface
{
public:
    bool enqueue_copy(const void* value) override
    {
        values.emplace_back(static_cast<const MoveOnlyValue*>(value)->get());
        return true;
    }

    bool enqueue_move(void* value) override
    {
        values.push_back(std::move(*static_cast<MoveOnlyValue*>(value)));
        return true;
    }

    std::size_t enqueue_batch(const void* values_to_enqueue, const std::size_t count) override
    {
        const auto* typed_values = static_cast<const MoveOnlyValue*>(values_to_enqueue);
        for (std::size_t index = 0; index < count; ++index)
        {
            values.emplace_back(typed_values[index].get());
        }
        return count;
    }

    bool try_dequeue(void* value) override
    {
        if (values.empty())
        {
            return false;
        }
        *static_cast<MoveOnlyValue*>(value) = std::move(values.front());
        values.pop_front();
        return true;
    }

private:
    std::deque<MoveOnlyValue> values;
};

using WaitableQueue     = exasol::udf::v2::WaitableQueue<int>;
using WaitableQueueCore = exasol::udf::v2::WaitableQueueCore;
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

TEST(WaitableQueueCoreTest, ForwardsOpaqueValuesSynchronously)
{
    auto event_fd = std::make_unique<StrictMock<MockEventFd>>();
    EXPECT_CALL(*event_fd, write_notification()).WillOnce(Return());

    auto queue = WaitableQueueCore(make_queue(), std::move(event_fd));
    int input  = 11;
    ASSERT_TRUE(queue.enqueue_copy(&input));

    int output = 0;
    ASSERT_TRUE(queue.try_dequeue(&output));
    EXPECT_EQ(output, input);
}

TEST(WaitableQueueCoreTest, RejectsNullElementPointers)
{
    auto event_fd = std::make_unique<StrictMock<MockEventFd>>();
    auto queue    = WaitableQueueCore(make_queue(), std::move(event_fd));

    EXPECT_THROW(static_cast<void>(queue.enqueue_copy(nullptr)), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(queue.enqueue_move(nullptr)), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(queue.enqueue_batch(nullptr, 1)), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(queue.try_dequeue(nullptr)), std::invalid_argument);
}

TEST(WaitableQueueTest, MovesNonTrivialValuesBeforeReturning)
{
    auto event_fd = std::make_unique<StrictMock<MockEventFd>>();
    EXPECT_CALL(*event_fd, write_notification()).WillOnce(Return());

    auto queue = exasol::udf::v2::WaitableQueue<MoveOnlyValue>(
        std::make_unique<MoveOnlyQueueAdapter>(), std::move(event_fd));
    MoveOnlyValue input{17};
    ASSERT_TRUE(queue.enqueue(std::move(input)));
    MoveOnlyValue output{0};
    ASSERT_TRUE(queue.try_dequeue(output));
    EXPECT_EQ(output.get(), 17);
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
    auto null_queue = std::unique_ptr<exasol::udf::v2::QueueInterface>{};
    EXPECT_THROW(
        { auto queue = WaitableQueue(std::move(null_queue), std::move(event_fd)); },
        std::invalid_argument);
}
