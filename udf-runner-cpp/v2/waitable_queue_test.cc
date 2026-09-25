#include <cstdint>
#include <deque>
#include <exception>
#include <limits>
#include <memory>
#include <system_error>
#include <utility>
#include <variant>
#include <vector>

#include <exasol/udf/v2/waitable_queue.hpp>
#include <gtest/gtest.h>

namespace
{

void test_check(bool condition, const char* message)
{
    EXPECT_TRUE(condition) << message;
}

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
void expect_system_error(Function&& function, std::errc expected)
{
    try
    {
        function();
        test_check(false, "expected system error");
    }
    catch (const std::system_error& error)
    {
        test_check(error.code() == std::make_error_code(expected), "unexpected system error");
    }
}

template <typename Queue>
void self_move_assign(Queue& queue)
{
    using move_assignment        = Queue& (Queue::*)(Queue&&) noexcept;
    const move_assignment assign = &Queue::operator=;
    (queue.*assign)(std::move(queue));
}

void expect_invalid_argument()
{
    try
    {
        auto null_event_fd = std::unique_ptr<exasol::udf::v2::EventFd>{};
        auto queue         = exasol::udf::v2::WaitableQueue(MockQueue{}, std::move(null_event_fd));
        static_cast<void>(queue);
        test_check(false, "null eventfd implementation was accepted");
    }
    catch (const std::invalid_argument& error)
    {
        test_check(error.what() != nullptr, "invalid-argument error did not contain a message");
    }
}

void test_mock_queue_operations()
{
    using WaitableQueue = exasol::udf::v2::WaitableQueue<MockQueue>;

    auto event_fd = std::make_unique<MockEventFd>();
    event_fd->set_read_actions({std::uint64_t{3}, std::errc::resource_unavailable_try_again});
    auto queue = WaitableQueue(MockQueue{}, std::move(event_fd));
    test_check(queue.native_handle() == 42, "mock eventfd handle was not retained");
    test_check(queue.enqueue(1), "mock queue enqueue failed");
    test_check(queue.drain_notifications() == 3, "mock notification drain failed");

    int value = 0;
    test_check(queue.try_dequeue(value), "mock queue dequeue failed");
    test_check(value == 1, "unexpected mock queue value");
    test_check(!queue.try_dequeue(value), "mock queue should be empty");

    const std::vector batch{2, 3};
    test_check(queue.enqueue_batch(batch.begin(), batch.end()) == batch.size(),
               "mock batch enqueue failed");
    test_check(queue.queue().size() == batch.size(), "mock queue accessor returned wrong queue");
    const auto& const_queue = queue;
    test_check(&const_queue.queue() == &queue.queue(), "const queue access failed");

    const std::vector<int> empty_batch;
    test_check(queue.enqueue_batch(empty_batch.begin(), empty_batch.end()) == 0,
               "empty mock batch should not enqueue values");
}

void test_mock_notification_paths()
{
    using enum std::errc;
    using WaitableQueue = exasol::udf::v2::WaitableQueue<MockQueue>;

    auto read_event_fd = std::make_unique<MockEventFd>();
    read_event_fd->set_read_actions(
        {interrupted, std::uint64_t{7}, resource_unavailable_try_again});
    auto read_queue = WaitableQueue(MockQueue{}, std::move(read_event_fd));
    test_check(read_queue.drain_notifications() == 7, "interrupted read was not retried");

    auto write_event_fd = std::make_unique<MockEventFd>();
    write_event_fd->set_write_actions(
        {interrupted, std::monostate{}, resource_unavailable_try_again});
    auto write_queue = WaitableQueue(MockQueue{}, std::move(write_event_fd));
    test_check(write_queue.enqueue(1), "interrupted write was not retried");
    test_check(write_queue.enqueue(2), "saturated write was not ignored");

    auto read_error_event_fd = std::make_unique<MockEventFd>();
    read_error_event_fd->set_read_actions({io_error});
    auto read_error_queue = WaitableQueue(MockQueue{}, std::move(read_error_event_fd));
    expect_system_error([&read_error_queue] { read_error_queue.drain_notifications(); }, io_error);

    auto write_error_event_fd = std::make_unique<MockEventFd>();
    write_error_event_fd->set_write_actions({io_error});
    auto write_error_queue = WaitableQueue(MockQueue{}, std::move(write_error_event_fd));
    expect_system_error([&write_error_queue] { static_cast<void>(write_error_queue.enqueue(1)); },
                        io_error);
}

void test_mock_capacity_and_moves()
{
    using WaitableQueue = exasol::udf::v2::WaitableQueue<MockQueue>;

    auto limited_queue = WaitableQueue(MockQueue{1}, std::make_unique<MockEventFd>());
    const std::vector batch{1, 2};
    test_check(limited_queue.enqueue_batch(batch.begin(), batch.end()) == 1,
               "mock queue should stop at capacity");
    test_check(!limited_queue.enqueue(3), "full mock queue should reject enqueue");

    auto empty_queue = WaitableQueue(MockQueue{0}, std::make_unique<MockEventFd>());
    test_check(empty_queue.enqueue_batch(batch.begin(), batch.end()) == 0,
               "empty mock queue should reject a batch");

    WaitableQueue moved_queue(MockQueue{}, std::make_unique<MockEventFd>());
    const int moved_handle = moved_queue.native_handle();
    WaitableQueue move_constructed(std::move(moved_queue));
    test_check(move_constructed.native_handle() == moved_handle,
               "mock move construction changed handle");
    WaitableQueue move_assigned(MockQueue{}, std::make_unique<MockEventFd>());
    move_assigned = std::move(move_constructed);
    test_check(move_assigned.native_handle() == moved_handle,
               "mock move assignment changed handle");
    self_move_assign(move_assigned);
}

} // namespace

TEST(WaitableQueueTest, MockEventFdAndQueueOperations)
{
    try
    {
        test_mock_queue_operations();
        test_mock_notification_paths();
        test_mock_capacity_and_moves();
        expect_invalid_argument();
    }
    catch (const std::exception& error)
    {
        FAIL() << error.what();
    }
}
