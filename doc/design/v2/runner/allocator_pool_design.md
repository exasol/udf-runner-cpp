# Allocator Pool Design

## Purpose and boundary

Each `Context` owns an abstract `AllocatorPool`. The `EventLoop` uses that pool to allocate storage for inbound
messages. This document defines the pool, allocator, and buffer-release contracts. The event loop's socket handling,
queues, and scheduling are defined in the [background-thread design](background_thread_design.md).

The pool does not prescribe a concrete allocator or arena implementation. An allocator may own or refer to an arena,
but that choice remains internal to the allocator.

## Pool and allocator use

The pool belongs to one `Context` and is not shared between contexts. The `EventLoop` directly acquires an allocator
from the pool; `BackgroundThreadImpl` only owns and schedules the loop and does not use the pool itself.

When the event loop begins receiving a message, it acquires one allocator for that message. It uses that allocator for
every buffer belonging to the message, including messages with multiple buffers, and does not reuse it for another
message. Once the event loop verifies and publishes the message, each buffer's release action transfers with the
message.

The inbound `BlockingReaderWriterQueue` only stores and transfers completed message entries. It does not use the
allocator pool or allocate message buffers.

## Allocator interfaces and buffer release

An allocator implements two interfaces with separate roles:

- `MessageBufferReleaser` releases non-RecordBatch message buffers.
- Arrow's `arrow::MemoryPool` serves Arrow allocations. An Arrow `RecordBatch` may retain or use this interface when
  its ownership requires it.

Non-RecordBatch buffer owners use only `MessageBufferReleaser`; they do not expose or retain the Arrow memory-pool
interface. `MessageBufferReleaser::Free` has the same signature as `arrow::MemoryPool::Free`, so one `Allocator::Free`
override satisfies both interfaces.

Every non-RecordBatch buffer release action calls `Free` with the buffer address, allocated size, and alignment. The
pool reports whether its allocators have global lifetime:

- For a global-lifetime allocator, the release action carries a non-owning `MessageBufferReleaser` reference. The
  pool's lifetime guarantee keeps that reference valid, and the message does not own the allocator.
- Otherwise, the release action retains a `std::shared_ptr<MessageBufferReleaser>` until after `Free` returns.

The conceptual C++ contracts are:

```cpp
class MessageBufferReleaser
{
public:
    virtual ~MessageBufferReleaser() = default;
    virtual void Free(std::uint8_t* buffer, std::int64_t size, std::int64_t alignment) = 0;
};

class Allocator : public MessageBufferReleaser, public arrow::MemoryPool
{
public:
    ~Allocator() override = default;
};

class AllocatorPool
{
public:
    virtual ~AllocatorPool() = default;
    [[nodiscard]] virtual std::shared_ptr<Allocator> acquire() = 0;
    [[nodiscard]] virtual bool allocators_have_global_lifetime() const noexcept = 0;
};
```

Allocation and Arrow memory-pool operations remain available through `arrow::MemoryPool`. An Arrow `RecordBatch` may
retain a `std::shared_ptr<arrow::MemoryPool>` separately when its ownership requires it. Ordinary message-buffer
owners carry no `arrow::MemoryPool` handle.
