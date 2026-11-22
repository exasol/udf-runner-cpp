# Context Implementation Design

## Purpose and boundary

This document describes the internal implementation of the worker-facing `Context` defined in
[context_interface.md](context_interface.md). It does not change the worker-facing API and does not expose queues,
eventfds, sockets, or background threads to the worker.

Context communicates with socket I/O through `BackgroundThreadInterface`. It owns worker-side stream routing and
lifecycle state; the event loop, queue ownership and semantics, readiness handling, and scheduling are specified in the
[background-thread design](background_thread_design.md). This document refers to that boundary without restating its
implementation.

## Context construction

The endpoint role is a required, strongly typed construction setting rather
than a worker-facing option or a value inferred from the first message. The
`Runner` constructs its `ContextManager` with `EndpointRole::Server`, because
accepted UDFRunner connections always act as the protocol `Server`.
`ContextManager` passes that value into each `ContextImpl`, which initializes
its stream registry with the same immutable role:

```text
Runner
  -> ContextManager(endpoint_role = Server)
       -> create(connected_socket)
            -> ContextImpl(endpoint_role)
                 -> StreamRegistry(endpoint_role)
```

Use an internal `enum class EndpointRole { Client, Server }` with no implicit
default, so every context-construction path states its role. The current
production setup has one fixed `Server` setting and needs no runtime flag or
protocol negotiation. Tests can construct contexts for either role to verify
ID allocation and parity checks. If a future product creates both client and
server connections, it can configure the corresponding `ContextManager`
explicitly without changing the worker-facing `Context` API.

### Dispatcher interaction

Context dispatches each inbound verified message to the logical stream identified by its `stream_id()`. It checks
context-owned pending messages before waiting for more input, then checks them again after new messages have been
routed. The background-thread boundary and its wait contract are specified in the
[background-thread design](background_thread_design.md).

A timeout applies to the current worker operation and does not consume a message that arrives later. A terminal result
ends the operation with the corresponding Context status. Global readiness operations use the same dispatch logic but
leave routed messages in their per-stream pending lists.

Worker send operations validate builders and Context state before transferring accepted messages across the
background-thread boundary. A successful transfer accepts the message but does not report socket-write completion.
The dedicated design specifies transfer mechanics and ownership at that boundary.

### Multiplexing and logical stream dispatch

The context-owned call registry tracks active streams and the high-water state described in
[stream management](stream_management.md#close-and-id-lifetime). The close-pending transition and the point at which
the registry releases a call are defined in the [call-close rules](stream_management.md#call-close-ordering).

`close_connection` is handled as a context shutdown event rather than as ordinary stream traffic. `close_call` remains
part of its call's pending message and follows the ordering defined in the
[stream-management call-close rules](stream_management.md#call-close-ordering).

`accept_call()` removes the next opening call from the context's unaccepted-call state and returns a `Call` bound to
its stream ID. `Call::receive()` and `ControlStream::receive()` inspect their own pending messages and return only
messages for their selected logical stream. Messages for different streams may be interleaved, but they preserve FIFO
order within each stream. The background-thread design specifies how inbound messages reach this dispatcher.

## Background-thread boundary

The background thread's interface, queue ownership and types, message transfer, event loop, readiness handling,
scheduling, and transport lifecycle are specified in the
[background-thread design](background_thread_design.md). Context depends on that contract for accepted outbound
messages, inbound availability, terminal results, and shutdown. Those mechanics are not specified here.

## Storage and views

Worker-facing message views retain their verified storage and Arrow ownership for their documented lifetime. Receiving
later messages does not invalidate an earlier view or wrapper. Storage verification and transfer across the
background-thread boundary are specified in the [background-thread design](background_thread_design.md).

## Terminal state

Context maintains a lifecycle state:

```text
Active
ClosingByWorker
ClosingByPeer
Failed
Closed
```

After a background transport or protocol failure, Context records the first terminal error, rejects later worker
operations, and returns the corresponding terminal `Result`. The background-thread design specifies how failure is
reported across the interface.

The terminal operation statuses distinguish at least:

```text
closed
TransportError
ProtocolError
```

Timeout remains a normal non-terminal receive result. It is not converted into a context exception.

State transitions are one-way:

```text
Active
  -> ClosingByWorker  (worker close, cancel, or destruction)
  -> ClosingByPeer    (peer close or EOF)
  -> Failed           (transport/protocol/queue failure)

ClosingByWorker / ClosingByPeer / Failed
  -> Closed
```

The first terminal cause wins. A later close or transport error may be recorded as cleanup information but must not
replace the original error returned to the worker. Synchronization with background work follows the
[background-thread contract](background_thread_design.md).

ContextManager publishes a Context only after its background-thread implementation is ready. Startup mechanics and
startup-failure handling are specified in the [background-thread design](background_thread_design.md).

## Worker-initiated close

When the worker requests connection close, Context transitions to `ClosingByWorker`, rejects further operations, and
submits `CloseConnection` as the terminal connection message. It waits for the close exchange or the absolute shutdown
deadline. The [background-thread design](background_thread_design.md) specifies message transfer and transport-side
close handling. After close, worker operations return `ContextClosed`.

The worker should process messages before requesting close if it needs their contents. Context releases undelivered
inbound views during shutdown rather than exposing them after the close boundary.

## Context destruction

The proposed lifetime anchor is a shared internal `ContextState`. `ContextImpl`
owns it strongly; `CallImpl` handles hold weak references to it. The state owns
the `StreamRegistry`, background-thread interface, and terminal lifecycle
state. Thus a `Call` object may remain allocated after its `Context` is
destroyed without retaining the connection or dereferencing freed memory.
`ContextImpl` requests background shutdown before releasing its strong reference; completion and resource release are
specified in the [background-thread design](background_thread_design.md). A later call operation whose weak reference
cannot be locked, or whose state is already closed, returns `ContextClosed`.
The worker-thread contract still requires context destruction to be serialized
with operations on that context.

If the context lifetime ends before the worker explicitly sends `CloseConnection`, Context transitions to
`ClosingByWorker`, rejects new operations, requests bounded background shutdown, releases context-owned pending views,
and then marks the state closed. The [background-thread design](background_thread_design.md) specifies the close
exchange and completion behavior.

If Context has already received a terminal transport failure, it skips the close request and retains that failure
for diagnostics.

`Context::control_stream()` returns a reference to a context-owned
`ControlStream` object, so that reference is valid only while the `Context`
object exists. Its implementation also uses a weak `ContextState` reference;
changing the public method to return an independently owned handle would be
required if a caller must retain the `ControlStream` object beyond the
`Context` lifetime.

## Peer-initiated close

When the background-thread interface reports peer connection closure, Context transitions to `ClosingByPeer`, stops
accepting worker operations, and then transitions to `Closed`. Later worker send and receive operations return
`ContextClosed`. Transport handling is specified in the
[background-thread design](background_thread_design.md).

## Ownership and cleanup rules

- A rejected worker send leaves builder and Arrow ownership with the caller.
- An accepted worker send transfers builder and Arrow ownership across the background-thread boundary.
- A worker-facing view remains valid for its documented lifetime, independent of later receives.
- Context releases any undelivered views when it enters terminal shutdown.
- A received Arrow C-interface object is released exactly once by its owning view or lease.
- A generated FlatBuffer accessor is never used after its storage lease is released.
