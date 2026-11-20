#include <exasol/udf/v2/internal/standard_thread_factory.hpp>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <exception>
#include <latch>
#include <memory>
#include <mutex>
#include <new>
#include <stdexcept>
#include <system_error>
#include <thread>
#include <utility>

#include <gtest/gtest.h>

namespace
{

using namespace std::chrono_literals;
using exasol::udf::v2::internal::StandardThreadFactory;
using exasol::udf::v2::internal::Thread;
using exasol::udf::v2::internal::ThreadLifecycleError;
using exasol::udf::v2::internal::ThreadTask;
using exasol::udf::v2::internal::ThreadTaskError;
using exasol::udf::v2::internal::detail::NativeThread;
using exasol::udf::v2::internal::detail::NativeThreadCreator;
using exasol::udf::v2::internal::detail::StdThreadFacade;

class TaskGate
{
public:
    void wait()
    {
        std::unique_lock lock(this->mutex);
        EXPECT_TRUE(this->opened.wait_for(lock, 10s, [this] { return this->open; }));
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
    std::atomic executions{0};
    auto thread = factory.create([&executions] { ++executions; });

    EXPECT_EQ(executions.load(), 0);

    thread->start();
    thread->join();

    EXPECT_EQ(executions.load(), 1);
}

TEST(StandardThreadFactoryTest, TimedJoinLeavesRunningTaskJoinable)
{
    StandardThreadFactory factory;
    TaskGate gate;
    auto thread = factory.create([&gate] { gate.wait(); });
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
    auto thread = factory.create([] {
        // Intentionally empty: this test exercises Thread lifecycle checks only.
    });

    EXPECT_THROW(thread->join(), ThreadLifecycleError);
    EXPECT_THROW(static_cast<void>(thread->join_for(-1ms)), std::invalid_argument);

    thread->start();
    EXPECT_THROW(thread->start(), ThreadLifecycleError);
    thread->join();
}

TEST(StandardThreadFactoryTest, RejectsEmptyTask)
{
    StandardThreadFactory factory;

    EXPECT_THROW(static_cast<void>(factory.create(ThreadTask{})), ThreadTaskError);
}

TEST(StandardThreadFactoryTest, RejectsSelfJoin)
{
    StandardThreadFactory factory;
    Thread* thread_pointer = nullptr;
    std::atomic rejected{false};
    auto thread    = factory.create([&thread_pointer, &rejected] {
        try
        {
            thread_pointer->join();
        }
        catch (const ThreadLifecycleError&)
        {
            rejected = true;
        }
    });
    thread_pointer = thread.get();

    thread->start();
    thread->join();

    EXPECT_TRUE(rejected.load());
}

TEST(StandardThreadFactoryTest, SerializesConcurrentJoiners)
{
    StandardThreadFactory factory;
    TaskGate task_gate;
    auto thread = factory.create([&task_gate] { task_gate.wait(); });
    thread->start();

    std::latch joiners_ready(2);
    std::latch start_joining(1);
    std::atomic successful_joins{0};
    auto join_task = [&thread, &joiners_ready, &start_joining, &successful_joins] {
        joiners_ready.count_down();
        start_joining.wait();
        if (thread->join_for(1s))
        {
            ++successful_joins;
        }
    };
    std::jthread first_joiner(join_task);
    std::jthread second_joiner(join_task);

    joiners_ready.wait();
    start_joining.count_down();
    task_gate.release();

    first_joiner.join();
    second_joiner.join();

    EXPECT_EQ(successful_joins.load(), 2);
}

TEST(StandardThreadFactoryTest, SupportsMaximumRelativeTimeoutAndRepeatedJoins)
{
    StandardThreadFactory factory;
    auto thread = factory.create([] {
        // Intentionally empty: this test exercises timeout conversion and repeated joins only.
    });
    thread->start();

    EXPECT_TRUE(thread->join_for(std::chrono::steady_clock::duration::max()));
    EXPECT_TRUE(thread->join_for(0ms));
}


enum class NativeFailure
{
    None,
    System,
    Allocation,
};

void throw_native_failure(const NativeFailure failure)
{
    if (failure == NativeFailure::System)
    {
        throw std::system_error(std::make_error_code(std::errc::resource_unavailable_try_again));
    }
    if (failure == NativeFailure::Allocation)
    {
        throw std::bad_alloc();
    }
}

struct NativeThreadControl
{
    std::atomic<NativeFailure> start_failure{NativeFailure::None};
    std::atomic<NativeFailure> join_failure{NativeFailure::None};
    std::atomic<bool> pause_next_join{false};
    std::atomic<int> start_calls{0};
    std::atomic<int> join_calls{0};
    std::atomic<int> successful_joins{0};
    TaskGate join_entered;
    TaskGate release_join;
};

class ControlledNativeThread final : public NativeThread
{
public:
    explicit ControlledNativeThread(std::shared_ptr<NativeThreadControl> control)
        : control(std::move(control))
    {
    }

    void start(ThreadTask task) override
    {
        ++this->control->start_calls;
        throw_native_failure(this->control->start_failure.exchange(NativeFailure::None));
        this->native.start(std::move(task));
    }

    void join() override
    {
        ++this->control->join_calls;
        if (this->control->pause_next_join.exchange(false))
        {
            this->control->join_entered.release();
            this->control->release_join.wait();
        }
        throw_native_failure(this->control->join_failure.exchange(NativeFailure::None));
        this->native.join();
        ++this->control->successful_joins;
    }

    [[nodiscard]] std::thread::id get_id() const noexcept override
    {
        return this->native.get_id();
    }

private:
    std::shared_ptr<NativeThreadControl> control;
    StdThreadFacade native;
};

class ControlledThreadTest : public testing::Test
{
protected:
    void SetUp() override
    {
        StandardThreadFactory factory([control = this->control] {
            return std::make_unique<ControlledNativeThread>(control);
        });
        this->thread = factory.create([this] { this->task_gate.wait(); });
    }

    void TearDown() override
    {
        this->task_gate.release();
        this->control->release_join.release();
        if (this->joiner.joinable())
        {
            this->joiner.join();
        }
        this->control->join_failure = NativeFailure::None;
        try
        {
            this->thread->join();
        }
        catch (const ThreadLifecycleError&)
        {
            // A failed start leaves no native resource to join.
        }
    }

    NativeThreadControl& get_control()
    {
        return *this->control;
    }

    TaskGate& get_task_gate()
    {
        return this->task_gate;
    }

    Thread& get_thread()
    {
        return *this->thread;
    }

    std::jthread& get_joiner()
    {
        return this->joiner;
    }

private:
    std::shared_ptr<NativeThreadControl> control = std::make_shared<NativeThreadControl>();
    TaskGate task_gate;
    std::unique_ptr<Thread> thread;
    std::jthread joiner;
};

TEST_F(ControlledThreadTest, RetriesStartAfterNativeSystemError)
{
    this->get_control().start_failure = NativeFailure::System;

    EXPECT_THROW(this->get_thread().start(), std::system_error);
    EXPECT_THROW(this->get_thread().join(), ThreadLifecycleError);
    EXPECT_EQ(this->get_control().join_calls.load(), 0);

    this->get_thread().start();
    this->get_task_gate().release();
    this->get_thread().join();

    EXPECT_EQ(this->get_control().start_calls.load(), 2);
    EXPECT_EQ(this->get_control().successful_joins.load(), 1);
}

TEST_F(ControlledThreadTest, RetriesStartAfterAllocationFailure)
{
    this->get_control().start_failure = NativeFailure::Allocation;

    EXPECT_THROW(this->get_thread().start(), std::bad_alloc);
    EXPECT_THROW(static_cast<void>(this->get_thread().join_for(0ms)), ThreadLifecycleError);

    this->get_thread().start();
    this->get_task_gate().release();
    this->get_thread().join();

    EXPECT_EQ(this->get_control().start_calls.load(), 2);
    EXPECT_EQ(this->get_control().successful_joins.load(), 1);
}

TEST_F(ControlledThreadTest, RetriesJoinAfterNativeSystemErrorWithExpiredDeadline)
{
    this->get_thread().start();
    this->get_task_gate().release();
    this->get_control().join_failure = NativeFailure::System;

    EXPECT_THROW(this->get_thread().join(), std::system_error);
    EXPECT_TRUE(this->get_thread().join_until(std::chrono::steady_clock::time_point::min()));
    EXPECT_TRUE(this->get_thread().join_for(0ms));
    this->get_thread().join();

    EXPECT_EQ(this->get_control().join_calls.load(), 2);
    EXPECT_EQ(this->get_control().successful_joins.load(), 1);
}

TEST_F(ControlledThreadTest, ReleasesJoinClaimAfterNonSystemException)
{
    this->get_thread().start();
    this->get_task_gate().release();
    this->get_control().join_failure = NativeFailure::Allocation;

    EXPECT_THROW(this->get_thread().join(), std::bad_alloc);
    EXPECT_TRUE(this->get_thread().join_for(0ms));

    EXPECT_EQ(this->get_control().join_calls.load(), 2);
    EXPECT_EQ(this->get_control().successful_joins.load(), 1);
}

TEST_F(ControlledThreadTest, TimesOutWhileAnotherCallerOwnsNativeJoin)
{
    this->get_control().pause_next_join = true;
    this->get_thread().start();
    this->get_task_gate().release();
    this->get_joiner() = std::jthread([this] { this->get_thread().join(); });
    this->get_control().join_entered.wait();

    EXPECT_FALSE(this->get_thread().join_for(0ms));
    EXPECT_FALSE(this->get_thread().join_until(std::chrono::steady_clock::now()));
    EXPECT_EQ(this->get_control().join_calls.load(), 1);
    EXPECT_EQ(this->get_control().successful_joins.load(), 0);

    this->get_control().release_join.release();
    this->get_joiner().join();

    EXPECT_TRUE(this->get_thread().join_for(0ms));
    EXPECT_EQ(this->get_control().join_calls.load(), 1);
    EXPECT_EQ(this->get_control().successful_joins.load(), 1);
}

TEST_F(ControlledThreadTest, ConcurrentCallersPerformOneNativeJoin)
{
    this->get_control().pause_next_join = true;
    this->get_thread().start();
    this->get_task_gate().release();
    this->get_joiner() = std::jthread([this] { this->get_thread().join(); });
    this->get_control().join_entered.wait();

    // A second indefinite caller must observe the first caller's completed join.
    TaskGate second_calling;
    std::jthread second_joiner([this, &second_calling] {
        second_calling.release();
        this->get_thread().join();
    });
    second_calling.wait();
    this->get_control().release_join.release();
    this->get_joiner().join();
    second_joiner.join();

    EXPECT_EQ(this->get_control().join_calls.load(), 1);
    EXPECT_EQ(this->get_control().successful_joins.load(), 1);
}

TEST_F(ControlledThreadTest, ConcurrentCallerCanRetryAfterFailedNativeJoin)
{
    this->get_control().pause_next_join = true;
    this->get_control().join_failure    = NativeFailure::System;
    this->get_thread().start();
    this->get_task_gate().release();
    std::atomic failed{false};
    this->get_joiner() = std::jthread([this, &failed] {
        try
        {
            this->get_thread().join();
        }
        catch (const std::system_error&)
        {
            failed = true;
        }
    });
    this->get_control().join_entered.wait();

    EXPECT_FALSE(this->get_thread().join_for(0ms));
    this->get_control().release_join.release();
    this->get_thread().join();
    this->get_joiner().join();

    EXPECT_TRUE(failed.load());
    EXPECT_EQ(this->get_control().join_calls.load(), 2);
    EXPECT_EQ(this->get_control().successful_joins.load(), 1);
}

TEST(StandardThreadFactoryTest, RejectsEmptyNativeCreator)
{
    EXPECT_THROW(static_cast<void>(StandardThreadFactory(NativeThreadCreator{})),
                 std::invalid_argument);
}

TEST(StdThreadFacadeTest, OwnsNativeIdentityUntilJoined)
{
    StdThreadFacade native;
    EXPECT_EQ(native.get_id(), std::thread::id{});
    EXPECT_THROW(native.join(), std::system_error);

    std::thread::id task_id;
    native.start([&task_id] { task_id = std::this_thread::get_id(); });
    const auto native_id = native.get_id();
    native.join();

    EXPECT_NE(native_id, std::thread::id{});
    EXPECT_EQ(native_id, task_id);
    EXPECT_EQ(native.get_id(), std::thread::id{});
}

TEST(StandardThreadFactoryTest, RejectsNullNativeThread)
{
    StandardThreadFactory factory([] { return std::unique_ptr<NativeThread>{}; });

    EXPECT_THROW(static_cast<void>(factory.create([] {})), std::logic_error);
}

TEST(StandardThreadFactoryTest, RejectsEmptyTaskBeforeInvokingNativeCreator)
{
    bool invoked = false;
    StandardThreadFactory factory([&invoked] {
        invoked = true;
        return std::make_unique<StdThreadFacade>();
    });

    EXPECT_THROW(static_cast<void>(factory.create(ThreadTask{})), ThreadTaskError);
    EXPECT_FALSE(invoked);
}

TEST(StandardThreadFactoryTest, SupportsMoveOnlyCreatorAndTaskAfterFactoryDestruction)
{
    std::atomic executions{0};
    std::unique_ptr<Thread> thread;
    {
        StandardThreadFactory factory([owned = std::make_unique<int>(1)] {
            EXPECT_EQ(*owned, 1);
            return std::make_unique<StdThreadFacade>();
        });
        thread = factory.create(
            [owned = std::make_unique<int>(1), &executions] { executions += *owned; });
    }

    thread->start();
    thread->join();

    EXPECT_EQ(executions.load(), 1);
}

TEST(StandardThreadFactoryTest, CreatesIndependentNativeThreads)
{
    StandardThreadFactory factory;
    std::atomic executions{0};
    auto first  = factory.create([&executions] { ++executions; });
    auto second = factory.create([&executions] { ++executions; });

    first->start();
    second->start();
    first->join();
    second->join();

    EXPECT_EQ(executions.load(), 2);
}

TEST(StandardThreadFactoryTest, RejectsEveryJoinVariantBeforeStart)
{
    StandardThreadFactory factory;
    auto thread = factory.create([] {});

    EXPECT_THROW(thread->join(), ThreadLifecycleError);
    EXPECT_THROW(static_cast<void>(thread->join_for(0ms)), ThreadLifecycleError);
    EXPECT_THROW(static_cast<void>(thread->join_until(std::chrono::steady_clock::now())),
                 ThreadLifecycleError);
}

TEST(StandardThreadFactoryTest, RejectsEverySelfJoinVariant)
{
    StandardThreadFactory factory;
    Thread* pointer = nullptr;
    std::atomic rejected{0};
    auto thread = factory.create([&pointer, &rejected] {
        const auto expect_rejected = [&rejected](auto join) {
            try
            {
                join();
            }
            catch (const ThreadLifecycleError&)
            {
                ++rejected;
            }
        };
        expect_rejected([&pointer] { pointer->join(); });
        expect_rejected([&pointer] { static_cast<void>(pointer->join_for(0ms)); });
        expect_rejected([&pointer] {
            static_cast<void>(pointer->join_until(std::chrono::steady_clock::now()));
        });
    });
    pointer     = thread.get();

    thread->start();
    thread->join();

    EXPECT_EQ(rejected.load(), 3);
    EXPECT_THROW(thread->start(), ThreadLifecycleError);
}

TEST(StandardThreadFactoryDeathTest, TerminatesWhenTaskThrows)
{
    EXPECT_EXIT(
        {
            std::set_terminate([] { std::_Exit(EXIT_FAILURE); });
            StandardThreadFactory factory;
            auto thread = factory.create([] { throw std::runtime_error("task failure"); });
            thread->start();
            thread->join();
        },
        testing::ExitedWithCode(EXIT_FAILURE), "");
}

TEST(StandardThreadFactoryDeathTest, TerminatesWhenStartedThreadIsDestroyedWithoutJoin)
{
    EXPECT_EXIT(
        {
            std::set_terminate([] { std::_Exit(EXIT_FAILURE); });
            TaskGate gate;
            StandardThreadFactory factory;
            auto thread = factory.create([&gate] { gate.wait(); });
            thread->start();
            thread.reset();
        },
        testing::ExitedWithCode(EXIT_FAILURE), "");
}

} // namespace
