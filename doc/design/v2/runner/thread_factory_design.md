# Thread Factory Design

## Purpose

The runner needs background execution without depending directly on
`std::thread`, so another thread runtime can be selected when needed. This
design defines an Internal-namespace `Thread` abstraction and a factory that
creates it from a task. Context and other runner internals depend on this
contract, while the composition root selects the appropriate implementation
for the build environment.

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
called. Because the standard library has no timed join, the adapter owns a
completion flag, mutex, and condition variable. After the task returns, its
thread function sets the completion flag while holding the mutex and then
notifies the condition variable. A join operation waits on that condition
variable with the completion flag as its predicate. After observing completion,
exactly one join caller invokes native `std::thread::join()`; the adapter
serializes concurrent join callers around that operation.

```text
thread function:
    task()
    lock completion mutex
    set completed = true
    unlock completion mutex
    completion condition variable.notify_all()

join_for/join_until:
    wait for completed predicate until the requested deadline
    if the wait times out: return false
    claim responsibility for the native join, or wait for another caller to finish it
    perform native join once without holding the completion mutex
    publish joined = true and notify waiting join callers
    return true
```

The completion notification is separate from the native join: it makes a
timed wait possible without polling or attempting a native join before task
completion. A timeout leaves the completion flag unchanged and the native
thread joinable. `join_for()` and `join_until()` therefore wait for the same
predicate and differ only in how they establish their deadline.

The completion predicate must be read and written while holding its mutex, so
a notification that occurs before a join starts waiting is not lost. The same
state protects the one native-join transition. A caller that loses the claim
to perform the native join waits for `joined` under its own deadline rather
than calling `std::thread::join()` concurrently.

The adapter does not catch task exceptions. It preserves the native
`std::thread` failure behavior for an exception escaping a task.

## Context integration

`ContextManager` receives the selected `ThreadFactory` during composition and
uses it to create each context's background I/O thread. The context retains the
returned `Thread`. The precise construction order remains an implementation
detail, but the context must request shutdown and successfully join the thread
before releasing state the task may access. The factory does not change the
worker-facing `Context` contract or the protocol.

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
- Test that task completion wakes an indefinite join and a timed join, while a
  timeout leaves the task joinable for a later successful join.

## Assumptions

- The first consumer is the context background I/O loop; no worker-pool or
  cancellation API is introduced by this ticket.
- `std::move_only_function` is available because v2 is built as C++23.
- Task exceptions retain the behavior of the selected native thread runtime;
  error transport is outside this abstraction.
