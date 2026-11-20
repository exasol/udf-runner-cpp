#pragma once

#include <exasol/udf/v2/internal/thread.hpp>

#include <functional>
#include <memory>
#include <thread>

namespace exasol::udf::v2::internal::detail
{

/// Owns one native thread, leaving lifecycle coordination to Thread.
class NativeThread
{
public:
    virtual ~NativeThread() = default;

    NativeThread(const NativeThread&)            = delete;
    NativeThread& operator=(const NativeThread&) = delete;
    NativeThread(NativeThread&&)                 = delete;
    NativeThread& operator=(NativeThread&&)      = delete;

    /// Starts asynchronously. Failure must leave no task running.
    virtual void start(ThreadTask task) = 0;

    /// Joins the native thread. Failure must leave it available for retry.
    virtual void join() = 0;

    [[nodiscard]] virtual std::thread::id get_id() const noexcept = 0;

protected:
    NativeThread() = default;
};

/// Creates a fresh, unstarted native thread on each invocation.
using NativeThreadCreator = std::move_only_function<std::unique_ptr<NativeThread>()>;

/// Owns a std::thread without joining or detaching it implicitly.
class StdThreadFacade final : public NativeThread
{
public:
    void start(ThreadTask task) override;
    void join() override;
    [[nodiscard]] std::thread::id get_id() const noexcept override;

private:
    // std::jthread would silently join and violate Thread's explicit-join contract.
    std::thread thread;
};

} // namespace exasol::udf::v2::internal::detail
