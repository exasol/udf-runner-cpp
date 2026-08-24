# UDF Protocol v2: Low-Level Protocol

This document captures the low-level wire rules for the new UDF protocol. It intentionally excludes
high-level call semantics and scheduling policy; those live in
[high_level_calls.md](high_level_calls.md).

## Scope

This document covers:

- framing and message layering
- control-stream traffic on `stream_id = 0`
- stream ownership rules
- generic call-attached data-stream rules
- transport binding and buffer handoff

The generic data-stream rules are defined in the companion
[data_stream.md](data_stream.md). They apply below the high-level `DB`/`UDFRunner`
protocol layer.

Related diagrams:

- [low_level_connection_lifecycle.svg](low_level_connection_lifecycle.svg)

## Roles

- `Server` accepts the transport-level connection.
- `Client` initiates the transport-level connection.

These roles define connection establishment and stream ownership only. A higher layer maps concrete components to
these roles. Transport-level roles do not limit which side may later open a call.

## Message Layering

The protocol uses two layers that should remain distinct:

- `Frame` is the low-level length-framed unit on the transport connection.
- `StreamMessage` is the typed protocol payload carried inside a `Frame`.

Receive path:

1. bytes on the socket
2. one decoded `Frame`
3. `stream_id` selection
4. one decoded `StreamMessage`

This separation is important because the transport boundary and the typed protocol payload evolve independently.

## Transport Binding

The initial protocol binding uses Unix-domain stream sockets. The transport carries the length-framed `Frame` byte
stream unchanged; it does not alter logical stream ownership, call semantics, control traffic, or flow control.

Future bindings may use TCP with the same framing. Any remote TCP deployment must use TLS together with endpoint
authentication and peer identity validation. TCP/TLS supports only inline record-batch buffers; it has no portable
file-descriptor transfer mechanism.

Unix-domain sockets may later add descriptor passing for local buffer handoff. This is an optional local
optimization and is not required to implement the base protocol.

## Control Stream

`stream_id = 0` is reserved for traffic that is independent of any active call.

The current control-stream message set is:

- `ServerCapabilities(version)` sent by the `Server` during initialization
- `KeepAlive`
- `Payloads(...)` when named payloads need to be exchanged without any active call
- `CloseConnection` for orderly connection shutdown

`StreamMessage` is a composite message with independently optional fields. This allows related fields, such as a
close field and `Error`, to travel together in one `Frame`.

## Stream Ownership

Each physical connection multiplexes multiple logical streams. A logical stream belongs to exactly one connection;
its protocol identity is the pair `(connection, stream_id)`. The connection is implicit in the transport session and
is therefore not carried in `Frame`.

The stream-ID rules are:

- `stream_id` is 64-bit
- odd `stream_id` values belong to the `Client`
- even `stream_id` values belong to the `Server`
- `stream_id = 0` is the control stream which can be used by the `Client` and `Server`
- a `stream_id` is unique for the lifetime of its connection and is not reused after its logical stream closes
- different connections may use the same numeric `stream_id`, including `0`; those pairs identify different
  logical streams

These rules allocate ownership and connection scope, but they do not yet fully define stream creation or teardown
beyond call-scoped usage.

## Connection Lifecycle

At the low level, a session progresses through:

1. connection establishment
2. control-stream initialization, including the `Server` advertising `ServerCapabilities(version)` on
   `stream_id = 0`
3. normal call/control traffic
4. a two-way `CloseConnection` exchange on stream `0`, followed by transport close; or abort

See [low_level_connection_lifecycle.svg](low_level_connection_lifecycle.svg).

## Close Semantics

`CloseCall` closes the call on its non-zero stream. Either peer may send it; no call-close acknowledgement is
required. A `CloseCall` with `Error` is an abnormal call termination. Without `Error`, it is normal termination.
After sending or receiving `CloseCall`, neither peer sends further call-scoped traffic on that stream.

`CloseConnection` closes the entire connection and is valid only on stream `0`. Either peer may initiate shutdown
by sending it. The receiver sends `CloseConnection` in reply, optionally with its own `Error`, then closes the
underlying transport. The initiator closes the transport after receiving that reply. A `CloseConnection` with
`Error` is abnormal termination; without `Error`, it is normal termination. If both peers initiate shutdown at the
same time, each treats the received `CloseConnection` as the reply and does not send another one.

After sending or receiving `CloseConnection`, neither peer opens a stream or sends ordinary call, data, or
control traffic. Every active non-control stream is terminated when the underlying transport closes.

`Error` without either close field is a non-terminal diagnostic for the enclosing stream. It does not by itself
close a call or connection; peers may continue processing when the error is recoverable.

## Buffer Handoff By Binding

`Inline` buffers are supported by every binding. Their bytes follow the `DataRecordBatch` metadata on the framed
connection according to the data-stream rules.

`Memfd` and `OutOfBand` buffers are reserved for Unix-domain sockets using file-descriptor passing. `Memfd`
denotes file-descriptor-backed shared buffers. `OutOfBand` may later cover GPU-memory handles transferred through
the same Unix-only mechanism; it is not a portable GPU-memory transport.

The initial binding does not define descriptor-to-batch correlation, supported GPU handle types, buffer ownership,
lifetime, cleanup, or capability negotiation. An implementation that cannot establish a compatible local handoff
must use `Inline` buffers.

## Serialization Mapping

- low-frequency call/control metadata: FlatBuffers
- record batches: Apache Arrow IPC-compatible payloads
- named metadata payload bodies such as connection objects or script content: typically JSON

This document only records the mapping at a protocol level. It does not redefine the `.fbs` schema itself.
Data streams carry Arrow-compatible physical schemas and buffers; Exasol column selection and logical metadata are
defined by the high-level protocol.

## Open Questions

- descriptor-to-batch correlation, ownership, lifetime, cleanup, and capability negotiation for Unix FD handoff
- GPU-memory handle types and validation requirements for any future `OutOfBand` binding
