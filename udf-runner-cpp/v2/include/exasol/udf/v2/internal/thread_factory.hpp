#pragma once

#include <exasol/udf/v2/internal/thread.hpp>

#include <memory>
#include <stdexcept>

namespace exasol::udf::v2::internal
{

/// Reports an invalid task passed to a ThreadFactory.
class ThreadTaskError final : public std::invalid_argument
{
public:
    using std::invalid_argument::invalid_argument;
};

/// Creates Internal threads without exposing a native thread runtime to callers.
class ThreadFactory
{
public:
    virtual ~ThreadFactory() = default;

    ThreadFactory(const ThreadFactory&)            = delete;
    ThreadFactory& operator=(const ThreadFactory&) = delete;
    ThreadFactory(ThreadFactory&&)                 = delete;
    ThreadFactory& operator=(ThreadFactory&&)      = delete;

    /// Creates, but does not start, a thread that executes `task`.
    [[nodiscard]] virtual std::unique_ptr<Thread> create(ThreadTask task) = 0;

protected:
    ThreadFactory() = default;
};

} // namespace exasol::udf::v2::internal
