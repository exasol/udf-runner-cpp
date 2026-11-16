# Thread Factory Design

## Purpose

The runner needs background execution without depending directly on either
`std::thread` or the database thread-management runtime. This design defines
an Internal-namespace `Thread` abstraction and a factory that creates it from
a task. Context and other runner internals depend on this contract, while the
composition root selects the appropriate implementation for the build
environment.

The abstraction represents one execution of one task. It is not a worker pool,
a cancellation mechanism, or a general executor.

## Interfaces

```cpp
namespace exasol::udf::v2::internal {

using ThreadTask = std::move_only_function<void()>;

class Thread {
public:
    virtual ~Thread() = default;

    virtual void start() = 0;

    virtual void join() = 0;

    // Returns false only when the timeout expires before task completion.
    [[nodiscard]] virtual bool join_for(std::chrono::steady_clock::duration timeout) = 0;

    // Returns false only when the deadline expires before task completion.
    [[nodiscard]] virtual bool join_until(
        std::chrono::steady_clock::time_point deadline) = 0;
};

class ThreadFactory {
public:
    virtual ~ThreadFactory() = default;

    [[nodiscard]] virtual std::unique_ptr<Thread> create(ThreadTask task) = 0;
};

} // namespace exasol::udf::v2::internal
```

`create()` takes ownership of the task but does not start it. A successfully
created thread invokes the task exactly once after `start()`. The factory
rejects an empty task with `std::invalid_argument`.

`start()` starts that execution once. Calling it more than once is a
programming error and throws `std::logic_error`. Failure to create the native
execution resource is reported as `std::system_error`.

## Join contract

The three join operations have one lifecycle effect: on successful completion,
they wait for the task to return and reclaim the selected native thread
resource. After one successful join, all later joins complete immediately.

- `join()` waits indefinitely and returns only after a successful join.
- `join_for(timeout)` waits for at most the supplied relative steady-clock
  duration.
- `join_until(deadline)` waits until the supplied absolute steady-clock
  deadline.
- The timed operations return `true` after a successful join and `false` only
  when their timeout or deadline expires before task completion.

`join_for()` is convenience for callers that have a local timeout. It converts
the duration to a `steady_clock` deadline once at entry and then follows the
same behavior as `join_until()`. `join_until()` is for callers that already
have an operation-wide deadline and must not reset its time budget while
performing nested work. The contract deliberately uses `steady_clock`; a
wall-clock adjustment must never shorten or extend a join wait.

A zero timeout, or a deadline that has already passed, does not block. It
returns `true` when the task has already completed and can be joined, otherwise
`false`. Negative durations passed to `join_for()` are rejected with
`std::invalid_argument`. If computing the deadline for an extremely large
duration would overflow `steady_clock::time_point`, `join_for()` treats it as
`steady_clock::time_point::max()`.

A timed-out join leaves the thread running and joinable; it never detaches,
cancels, or destroys the task. A caller may call any join operation again.
Timeout accuracy is subject to the selected native runtime's scheduling and
clock granularity. An implementation must use a monotonic clock and recheck
the requested deadline between internal waits; operating-system scheduling may
still delay the caller's return after that deadline.

Calling any join operation before `start()` or from the task itself is a
programming error and throws `std::logic_error`. Concurrent join calls are
allowed: implementations serialize native joining so that exactly one caller
reclaims the native thread resource. Other join callers wait for that operation
under their own timeout/deadline and then observe the same joined state.

A started thread must be joined successfully before it is destroyed. The
abstraction never detaches work or silently blocks in its destructor. Destroying
an unjoined started thread is a programming error.

## Implementations

### Standard implementation

The standalone v2 implementation creates a `std::thread` when `start()` is
called. Because the standard library has no timed join, the adapter records
task completion under a mutex and notifies a condition variable. A timed
join waits on that completion state; after completion it performs the native
`std::thread::join()` exactly once. It serializes concurrent join callers
before invoking the native operation.

The adapter does not catch task exceptions. It preserves the native
`std::thread` failure behavior for an exception escaping a task.

### Database implementation

The database-only implementation creates an internal subclass of
`Threads::Thread`. Its `run()` implementation invokes the stored `ThreadTask`:

```cpp
class DatabaseThread final : public Threads::Thread {
private:
    void run() override
    {
        this->task();
    }

    ThreadTask task;
};
```

It uses `Threads::Thread::start()` to schedule the task through the database
thread-management runtime. `join_for()` calculates one common deadline and
delegates to `join_until()`. The database adapter calculates the remaining
duration before every database join wait and converts that value to the
database millisecond timeout. Sub-millisecond positive durations are rounded
up to one millisecond so the adapter does not time out early solely through
unit conversion. Durations exceeding the database timeout's representable
range are waited in successive bounded intervals while the original common
deadline remains in effect.

The adapter never requests `delete_after_execution`: the factory caller owns
the wrapper through `std::unique_ptr<Thread>`, and database auto-deletion
would leave that owner dangling. The adapter also enforces the common
single-start, self-join, destruction, and concurrent-join rules rather than
exposing the database type's differing lifecycle behavior. The database
implementation is built only in the database integration; the standalone Bazel
module must not include database headers or libraries.

## Context integration

`ContextManager` receives the selected `ThreadFactory` during composition and
uses it to create each context's background I/O thread. The context retains the
returned `Thread`, starts it only after its queues and transport-side state are
ready, requests background shutdown through its existing control path, and
joins it before releasing queues, socket state, or storage arenas. The factory
does not change the worker-facing `Context` contract or the protocol.

## Tests and integration

- Test the common lifecycle: creation does not execute the task, `start()`
  executes it once, indefinite join completes, each timed join reports timeout
  when appropriate, and a later join succeeds.
- Test zero and expired waits, negative relative waits, `join_for()` to
  `join_until()` deadline conversion, and repeated joins after completion.
- Test concurrent join callers to prove that the underlying native thread is
  joined exactly once and each caller observes its own timeout or completion.
- Test invalid lifecycle calls: empty task, repeated start, join before start,
  and self-join.
- Test the standard implementation with deterministic task gates rather than
  scheduler-dependent sleeps.
- Test the database adapter in its database build: it invokes the task through
  `Threads::Thread`, converts relative and absolute waits to millisecond
  timeouts without enabling auto-deletion, and preserves the common lifecycle
  contract.
- Keep the standalone v2 Bazel targets independent of database headers and
  libraries.

## Assumptions

- The first consumer is the context background I/O loop; no worker-pool or
  cancellation API is introduced by this ticket.
- `std::move_only_function` is available because v2 is built as C++23.
- Task exceptions retain the behavior of the selected native thread runtime;
  error transport is outside this abstraction.
