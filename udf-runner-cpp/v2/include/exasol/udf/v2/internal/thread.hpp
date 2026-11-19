#pragma once

#include <chrono>
#include <functional>
#include <stdexcept>

namespace exasol::udf::v2::internal
{

using ThreadTask = std::move_only_function<void()>;

/// Reports an invalid Thread lifecycle operation.
class ThreadLifecycleError final : public std::logic_error
{
public:
    using std::logic_error::logic_error;
};

/// Represents one execution of a task created by a ThreadFactory.
class Thread
{
public:
    virtual ~Thread() = default;

    Thread(const Thread&)            = delete;
    Thread& operator=(const Thread&) = delete;
    Thread(Thread&&)                 = delete;
    Thread& operator=(Thread&&)      = delete;

    /// Starts the task exactly once.
    virtual void start() = 0;

    /// Waits until the task has completed and reclaims its native thread resource.
    virtual void join() = 0;

    /// Waits for task completion for at most `timeout`.
    ///
    /// @returns `true` after successful completion and joining, or `false` if
    /// the timeout expires first.
    [[nodiscard]] virtual bool join_for(std::chrono::steady_clock::duration timeout) = 0;

    /// Waits for task completion until `deadline`.
    ///
    /// @returns `true` after successful completion and joining, or `false` if
    /// the deadline expires first.
    [[nodiscard]] virtual bool join_until(std::chrono::steady_clock::time_point deadline) = 0;

protected:
    Thread() = default;
};

} // namespace exasol::udf::v2::internal
