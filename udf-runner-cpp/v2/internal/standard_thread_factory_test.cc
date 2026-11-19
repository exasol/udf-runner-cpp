#include <exasol/udf/v2/internal/standard_thread_factory.hpp>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <latch>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>

#include <gtest/gtest.h>

namespace
{

using namespace std::chrono_literals;
using exasol::udf::v2::internal::StandardThreadFactory;
using exasol::udf::v2::internal::Thread;
using exasol::udf::v2::internal::ThreadTask;

class TaskGate
{
public:
    void wait()
    {
        std::unique_lock lock(this->mutex);
        this->opened.wait(lock, [this] { return this->open; });
    }

    void release()
    {
        {
            std::lock_guard lock(this->mutex);
            this->open = true;
        }
        this->opened.notify_all();
    }

private:
    std::condition_variable opened;
    std::mutex mutex;
    bool open = false;
};

TEST(StandardThreadFactoryTest, DoesNotExecuteTaskBeforeStart)
{
    StandardThreadFactory factory;
    std::atomic<int> executions{0};
    std::unique_ptr<Thread> thread = factory.create([&executions] { ++executions; });

    EXPECT_EQ(executions.load(), 0);

    thread->start();
    thread->join();

    EXPECT_EQ(executions.load(), 1);
}

TEST(StandardThreadFactoryTest, TimedJoinLeavesRunningTaskJoinable)
{
    StandardThreadFactory factory;
    TaskGate gate;
    std::unique_ptr<Thread> thread = factory.create([&gate] { gate.wait(); });
    thread->start();

    EXPECT_FALSE(thread->join_for(0ms));
    EXPECT_FALSE(thread->join_until(std::chrono::steady_clock::now()));

    gate.release();

    EXPECT_TRUE(thread->join_for(1s));
    EXPECT_TRUE(thread->join_until(std::chrono::steady_clock::now()));
}

TEST(StandardThreadFactoryTest, RejectsInvalidLifecycleOperations)
{
    StandardThreadFactory factory;
    std::unique_ptr<Thread> thread = factory.create([] {});

    EXPECT_THROW(thread->join(), std::logic_error);
    EXPECT_THROW(static_cast<void>(thread->join_for(-1ms)), std::invalid_argument);

    thread->start();
    EXPECT_THROW(thread->start(), std::logic_error);
    thread->join();
}

TEST(StandardThreadFactoryTest, RejectsEmptyTask)
{
    StandardThreadFactory factory;

    EXPECT_THROW(static_cast<void>(factory.create(ThreadTask{})), std::invalid_argument);
}

TEST(StandardThreadFactoryTest, RejectsSelfJoin)
{
    StandardThreadFactory factory;
    Thread* thread_pointer = nullptr;
    std::atomic<bool> rejected{false};
    std::unique_ptr<Thread> thread = factory.create([&thread_pointer, &rejected] {
        try
        {
            thread_pointer->join();
        }
        catch (const std::logic_error&)
        {
            rejected = true;
        }
    });
    thread_pointer                 = thread.get();

    thread->start();
    thread->join();

    EXPECT_TRUE(rejected.load());
}

TEST(StandardThreadFactoryTest, SerializesConcurrentJoiners)
{
    StandardThreadFactory factory;
    TaskGate task_gate;
    std::unique_ptr<Thread> thread = factory.create([&task_gate] { task_gate.wait(); });
    thread->start();

    std::latch joiners_ready(2);
    std::latch start_joining(1);
    std::atomic<int> successful_joins{0};
    auto join_task = [&thread, &joiners_ready, &start_joining, &successful_joins] {
        joiners_ready.count_down();
        start_joining.wait();
        if (thread->join_for(1s))
        {
            ++successful_joins;
        }
    };
    std::thread first_joiner(join_task);
    std::thread second_joiner(join_task);

    joiners_ready.wait();
    start_joining.count_down();
    task_gate.release();

    first_joiner.join();
    second_joiner.join();

    EXPECT_EQ(successful_joins.load(), 2);
}

} // namespace
