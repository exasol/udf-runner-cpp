# Low-Level Context Stream Management

## Purpose and ownership

This document specifies how the low-level context manages logical streams for
one protocol-v2 connection. It complements
[context_implementation.md](context_implementation.md), which defines the
cross-thread queues and background I/O loop, and
[context_interface.md](context_interface.md), which defines the worker-facing
`Context`, `Call`, and `ControlStream` contracts.

The low-level context is the sole owner of all logical stream state. It owns
the control stream on stream ID `0`, allocates its locally owned nonzero stream
IDs, and maintains a registry from every live call stream ID to its
worker-visible `Call` state. The background component owns transport work but
does not own `Call` or `ControlStream` instances and does not dispatch to
worker-facing objects.

## Stream-ID ownership and allocation

The low-level context applies the protocol's
[stream-ownership rules](../protocol/low_level/protocol.md#stream-ownership).
The DB is the transport `Client` and UDFRunner is the transport `Server`:

- `0` is the shared control stream;
- odd nonzero IDs are owned by the DB/Client;
- even nonzero IDs are owned by UDFRunner/Server;
- IDs are 64-bit and are unique for the lifetime of one connection.

Each context is created with an immutable endpoint role (`Client` or `Server`)
for the lifetime of its connection. The role determines the locally owned
stream-ID parity and validation of peer-created streams; it must not change
while the connection is active. This runner creates only `Server` contexts.

`Context::open_call()` and any UDFRunner-initiated nested call allocate the
next unused even nonzero ID, beginning at `2` and advancing by `2`. Exhaustion
or wraparound is a local failure; an ID must never be reused. An inbound call
opening must use a previously unknown odd ID. A peer attempt to open an even
ID, or a local attempt to open an odd ID, is a protocol error.

Parity determines which endpoint creates a stream, not the direction of later
traffic. Once a call exists, either peer may send permitted call messages on
that stream. In particular, UDFRunner may send responses on a DB-created odd
stream, and DB may send responses on a UDFRunner-created even stream.

## Queue envelopes

The queue boundary preserves the stream ID. A builder or view variant alone is
insufficient: a call builder has no stream ID, and a received view must be
routed to its call before it can be returned to the worker. The internal queue
items are therefore:

```text
OutboundEnvelope {
    StreamId stream_id                 // 0 for ControlMessageBuilder
    CallMessageBuilder | ControlMessageBuilder builder
}

InboundEnvelope {
    StreamId stream_id                 // 0 for ControlMessageView
    CallMessageView | ControlMessageView view
}
```

The stream ID and variant must agree. Stream `0` always carries the control
alternative; a nonzero stream ID always carries the call alternative. A
mismatch is an internal error on the outbound side and a protocol error on the
inbound side.

The background component uses the ID only to encode or decode the wire message
and select connection-local transport state. It does not need access to the
context's call registry.

## Inbound dispatch

The background thread is the only producer of the physical inbound queue. It
appends envelopes in the order in which complete protocol messages are
accepted from the socket. It does not call worker code and does not block for a
particular call.

The low-level context's dispatcher is the only consumer of that queue. It
drains a batch and routes each envelope as follows:

- stream `0` goes to the control-stream pending list;
- an envelope for a known nonzero `stream_id` goes to that stream's pending
  list;
- an opening message for a never-created odd stream creates an inbound-call
  entry in the unaccepted-call list;
- a message for a call that has not yet been accepted remains with that stream
  entry and is not returned by another call;
- a message for an already-closed stream is discarded as delayed traffic;
- other traffic for a never-created stream is a protocol error.

The dispatcher validates stream state before routing. `open_call` is legal
only in the first call message, stream `0` messages are decoded as control
messages, and `close_connection` is handled as a context shutdown event rather
than being placed in the control-stream pending list. `close_call` remains in
the call's pending `CallMessageView` so the worker can observe the final call
message before that call is marked closed.

`accept_call()` removes the next entry from the unaccepted-call list and
returns a `Call` bound to its `stream_id`. Its already received opening message
remains at the head of that call's pending list. `Call::receive()` and
`ControlStream::receive()` first inspect their pending list, then drain and
route a shared inbound batch, and only then wait. This provides stream-local
consumer semantics while retaining one shared inbound queue.

The dispatcher preserves FIFO order within every stream. Messages for distinct
streams may be interleaved in the physical queue and must not make an unrelated
`receive()` return. A global readiness query is true when either the shared
inbound queue or any worker-owned pending list contains an unread message.

## Outbound routing

`open_call()` allocates the next unused even stream ID, creates and registers
the `Call` state, and enqueues an outbound envelope containing that ID and the
opening builder. It rolls back the registration if enqueue fails.

`Call::send()` obtains its ID from its context-managed call state and enqueues
an outbound envelope. `ControlStream` always enqueues with ID `0`; neither
worker-facing abstraction exposes numeric IDs to worker code.

## Close and ID lifetime

The worker-owned call registry keeps a closed-stream set keyed by `stream_id`.
A local or peer call close records the stream ID before its pending sequence is
released. Delayed traffic for that ID can then be discarded without confusing
it with a new call. Stream IDs are never reused during a connection.

Connection close transitions every live stream to its terminal result and wakes
its waiters.

## Invariants

This design retains exactly two cross-thread queues: one worker-to-background
outbound queue and one background-to-worker inbound queue. The call registry,
unaccepted-call list, closed-stream set, and per-stream pending messages are
low-level-context-owned dispatch state, not background-thread queues.
