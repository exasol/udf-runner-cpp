# Worker-Facing Low-Level Context Interface

## Purpose

This document defines the worker-facing interface for one protocol-v2 connection. It is a design contract only. It
does not define sockets, framing, FlatBuffer serialization, threads, queues, codecs, or other implementation details.

The worker sees three abstractions:

- `Context` for connection-wide readiness, inbound call acceptance, and outbound call opening;
- `Call` for one logical call stream;
- `ControlStream` for connection-level traffic on stream `0`.

The context consumes and dispatches transport messages internally. A worker never receives a socket, file descriptor,
frame, or numeric stream identifier.

## Result and status concepts

Readiness operations report status without consuming a message:

```text
MessageStatus =
    message_available
  | no_message
  | timed_out
  | cancelled
  | peer_closed
  | connection_error
```

Consuming operations return either a value or a terminal result:

```text
OperationStatus =
    ok
  | timed_out
  | cancelled
  | peer_closed
  | protocol_error
  | transport_error
```

The conceptual generic result type is:

```text
Result<T> {
    OperationStatus status
    optional<T> value
    optional<ErrorInfo> error
}
```

`Result<void>` represents an operation that has no value on success. A successful result has `status == ok` and a
value where applicable. A failed result has no value and may include an error code and message.

All timeout parameters use the conceptual type `optional<Duration>`. An absent timeout waits indefinitely; a zero
duration performs no blocking wait.

A successful operation returns its value. A failed operation returns no value and may include an error code and message.
Timeout is distinct from protocol and transport failure and does not consume or discard a later message.

## Context interface

The conceptual context contract is:

```text
Context {
    receive_status() -> MessageStatus
    wait_for_message(optional<Duration> timeout) -> MessageStatus
    accept_call() -> Result<unique_ptr<Call>>
    open_call(CallMessage) -> Result<unique_ptr<Call>>
    control_stream() -> ControlStream&
    cancel() -> void
}
```

### Connection-wide readiness

`receive_status()` is non-blocking. It reports whether an unread message is available on any stream and never
consumes or returns that message.

`wait_for_message(timeout)` waits until an unread message is available on any stream or a terminal condition occurs.
An omitted timeout means wait indefinitely. The function also never consumes or identifies a message.

The global readiness functions do not identify the stream with pending data. The worker consumes data through
`accept_call()`, a `Call`, or the `ControlStream`.

### Call acceptance

`accept_call()` waits for the next inbound composite message containing `open_call` and returns a `Call` object bound
to that call stream. It does not return messages belonging to an already accepted call or to stream `0`.

The complete opening message is buffered as the first message of the returned call. `accept_call()` does not expose
or discard any fields from that message. Messages for the call that arrive before the worker invokes `Call::receive()`
remain available to that call in protocol order.

`open_call(opening_message)` creates a worker-initiated call. The opening message must contain `open_call` and is sent
as the first message on a new logical call stream. It may contain any other protocol-permitted opening fields. On
successful local registration and transmission, the function returns the `Call` object without waiting for a peer
response. The peer's response is received through that call's `receive()` operation.

## Call interface

The conceptual call contract is:

```text
Call {
    receive_status() -> MessageStatus
    receive(optional<Duration> timeout) -> Result<CallMessage>
    send(CallMessage) -> Result<void>
}
```

`Call::receive_status()` is non-blocking, observes only this call's stream, and never consumes a message.

`Call::receive(timeout)` waits only for this call's stream. An omitted timeout means wait indefinitely. It returns the
next complete call message, returns `timed_out` when the timeout expires, and returns a terminal result for
cancellation, peer closure, or connection failure.

The first successful `Call::receive()` returns the opening `CallMessage`, including its `open_call` field and any
 payloads, `DataSchema`, `Next`, `RecordBatch`, error, or close fields carried in the same protocol message. Subsequent
messages must not contain `open_call`.

For a call returned by `open_call()`, the opening message has already been sent by the context. Its first successful
`Call::receive()` returns the peer's first message and must not contain `open_call`. A `CallMessage` containing
`open_call` is not valid for `Call::send()` after the call object has been created.

If a control message or another call's message arrives while `Call::receive()` is blocked, the context dispatches it
to its own stream and the call receive continues waiting. It does not return unrelated traffic.

## Composite call messages

The worker-facing data-schema value is:

```text
DataSchema {
    ArrowSchema schema
    bool has_group_id
    bool has_row_id
}
```

This is an interface-level type. It is not the wire-level FlatBuffer `DataSchema` table.

The worker-facing record-batch value is:

```text
RecordBatch {
    ArrowArray array
    bool is_end_of_group
}
```

This is an interface-level type. It is not the wire-level FlatBuffer `DataRecordBatch` table.

`CallMessage` is one composite message whose fields are independently optional:

```text
CallMessage {
    optional open_call
    optional payloads
    optional data_schema: DataSchema
    optional next
    optional record_batch: RecordBatch
    optional error
    optional close_call
}
```

The same conceptual type is used for sending and receiving. A single message may contain one or multiple fields. A
worker sends fields separately by sending separate `CallMessage` values; no field-specific send methods are required.

For inbound calls, `open_call` is required on the first received message and forbidden on subsequent received messages.
For outbound calls, `open_call` is required only in the opening message supplied to `Context::open_call()` and is
forbidden in all later received or sent messages. An opening message may contain any other protocol-permitted call
fields.

The design permits, subject to protocol-state validation:

- payloads alone;
- `open_call` alone;
- `open_call` with payloads, `DataSchema`, `Next`, a `RecordBatch`, error, or close;
- a `DataSchema` alone;
- `Next` alone;
- a `RecordBatch` alone after its schema has been announced;
- `DataSchema` and the first `RecordBatch` together;
- payloads, `DataSchema`, `Next`, and a `RecordBatch` together;
- an error alone;
- close alone;
- close combined with payloads, `DataSchema`, `Next`, a `RecordBatch`, and/or an error.

If multiple fields arrive in one protocol message, `Call::receive()` returns them together in one `CallMessage`.

An error without `close_call` is a non-terminal diagnostic. A message containing `close_call` terminates the call
after the message is processed.

## Control stream interface

`Context::control_stream()` returns the connection-level stream for `stream_id == 0`. Its conceptual contract is:

```text
ControlStream {
    receive_status() -> MessageStatus
    receive(optional<Duration> timeout) -> Result<ControlMessage>
    send(ControlMessage) -> Result<void>
}
```

The control stream's status and receive functions observe and consume only stream-0 messages.

`ControlMessage` contains independently optional connection-level fields:

```text
ControlMessage {
    optional server_capabilities
    optional keep_alive
    optional payloads
    optional error
    optional close_connection
}
```

The control stream supports capabilities, keepalive, connection-level payloads, errors, and connection close. It must
not expose `OpenCall`, `CloseCall`, `Next`, `ArrowSchema`, or `ArrowArray` objects.

An error without `close_connection` is a non-terminal diagnostic. `CloseConnection` starts or acknowledges connection
shutdown and may be combined with an error or other permitted connection-level fields.

## Arrow schemas and record batches

Data schemas and record batches are transferred through the Arrow C Data Interface, not as serialized FlatBuffer
objects. The conceptual `data_schema` field contains the interface-level `DataSchema` value, whose `schema` field is
an `ArrowSchema`. The conceptual `record_batch` field contains the interface-level `RecordBatch` value, whose
`array` field is an `ArrowArray`.

`DataSchema.schema` describes one direction of a call's data stream. It is sent once per direction, before or together
with that direction's first `RecordBatch`. A later `RecordBatch` uses the previously announced schema and flags and
does not repeat them.

`RecordBatch.is_end_of_group` is meaningful only when `DataSchema.has_group_id` is true. It indicates whether the
group identified by the batch's final row is complete. A true value means no later batch in that direction contains
the trailing group; a false value means the trailing group may continue. The flag is not an end-of-stream marker, and
an empty batch must not set it to true.

The correlation flags describe the reserved prefix of `DataSchema.schema`:

- both flags false: no reserved correlation fields are present;
- only `has_group_id`: field `0` is the group ID;
- only `has_row_id`: field `0` is the row ID;
- both flags true: field `0` is the group ID and field `1` is the row ID;
- user data fields follow the reserved prefix;
- correlation fields are identified by position, not by field name.

FlatBuffers remain the representation for protocol metadata and control fields, including payloads, `Next`, errors,
close messages, capabilities, keepalive, and connection close. The worker-facing interface does not expose the
wire-level FlatBuffer `DataSchema` or `DataRecordBatch` objects.

The worker-facing contract exposes the Arrow C Data Interface only. Arrow C++ types, Arrow IPC internals, and
transport-specific buffer handling remain hidden.

Ownership follows the Arrow release contract:

- received `DataSchema.schema` and `RecordBatch.array` objects become owned by the caller;
- the caller releases each received Arrow object exactly once;
- sent objects remain caller-owned until the context accepts the send;
- after successful acceptance, release responsibility transfers to the context;
- a failed send before acceptance leaves ownership with the caller;
- no object may be released more than once.

## Dispatch and consumption rules

The context internally dispatches incoming traffic to:

- a pending inbound-call queue;
- one logical queue per accepted call;
- a stream-0 control queue;
- terminal connection state.

These are observable requirements, not implementation prescriptions.

The following rules apply:

- global readiness observes all streams without consuming messages;
- `accept_call()` identifies and registers only inbound messages containing `open_call`;
- `open_call()` registers and sends one outbound opening message containing `open_call`;
- `Call::receive()` consumes only messages for its call;
- `ControlStream::receive()` consumes only stream-0 messages;
- each protocol message is consumed exactly once;
- messages remain queued until consumed by the correct abstraction;
- a call receive never returns control-stream or other-call traffic;
- a control receive never returns call traffic.

One consumer is expected for each call and for the control stream. Concurrent receives from different calls are
supported by the contract; concurrent receives from the same stream are outside this interface.

After `accept_call()` succeeds, the accepted call's pending-message sequence begins with its opening composite
message. The opening message is consumed exactly once by the first successful `Call::receive()`.

After `open_call()` succeeds, the call's pending-message sequence begins with the peer's first response. The locally
sent opening message is not returned by `Call::receive()`.

## Lifecycle and terminal behavior

The context owns the connection after successful context creation. A call object represents one logical call and is
valid until call close, connection close, cancellation, peer disconnect, or fatal error.

Cancellation, peer disconnect, connection close, and fatal errors wake all affected blocked operations. After
connection close begins:

- no new calls are accepted;
- no new outbound calls are opened;
- ordinary call traffic is no longer sent;
- active calls receive terminal results;
- pending status, wait, accept, call-receive, and control-receive operations do not remain blocked indefinitely.

Normal and abnormal termination are distinct:

- call close without error is normal call termination;
- call close with error is abnormal call termination;
- connection close without error is normal connection termination;
- connection close with error is abnormal connection termination;
- cancellation and transport failure are reported as terminal operation results.

## Validation responsibilities

The context validates, before delivering or sending messages:

- protocol message structure and configured size limits;
- stream ownership and stream identity rules;
- call lifecycle ordering;
- outbound opening messages must contain `open_call` exactly once;
- `open_call` is rejected after the opening message;
- control-message field combinations;
- call-message field combinations;
- `DataSchema` must precede or accompany the first `RecordBatch`;
- the schema field prefix must match `has_group_id` and `has_row_id`;
- a second schema or changed correlation flags in one direction are rejected;
- `RecordBatch.is_end_of_group` must be false when `has_group_id` is false;
- an empty `RecordBatch` must not set `is_end_of_group` to true;
- one data stream per call;
- `Next` byte-credit rules;
- close and error semantics;
- Arrow schema and array ownership and release requirements;
- supported record-batch transport;
- rejection of ordinary traffic after connection shutdown.

Invalid messages produce protocol errors and follow the applicable call or connection termination behavior.

## Examples

### Composite call message

The peer may open a call with one message containing `open_call`, payloads, `DataSchema`, and the first `RecordBatch`.
The worker first calls `accept_call()`, then receives one `CallMessage` containing all four fields.

### Outbound call

The worker creates a nested call with a `CallMessage` containing `open_call` and its opening payloads. It calls
`Context::open_call()`, receives a `Call` object, and then uses `Call::receive()` for the peer's response. The local
opening message is not returned by that receive operation.

### Opening close

The peer may open and close a call in one message. `accept_call()` returns the call, and its first `receive()` returns
the opening `CallMessage` containing both `open_call` and `close_call`; the call then enters its closed state.

### Separate call messages

The peer may send a `DataSchema`, then a `Next`, then a `RecordBatch` as three separate messages. The worker receives
three successive `CallMessage` values.

### Error-only message

The worker may send a `CallMessage` containing only `error`. The call remains active unless `close_call` is also
present.

### Close with final data

The worker may send one `CallMessage` containing a final record batch, payloads, an error, and `close_call`, subject to
the call's protocol state.

### Control message during call receive

While the worker is blocked in `Call::receive()`, a `KeepAlive` arrives. The context dispatches it to
`ControlStream`; the call receive remains blocked until a message for that call arrives or a terminal condition occurs.

## Relationship to existing documents

This document refines the `Context` responsibilities in [architecture.md](architecture.md) and the worker/context
lifecycle in [lifecycle.md](lifecycle.md). It relies on the stream, close, call, and data-stream rules defined by the
low-level protocol documents and does not redefine their wire format.

## Design review checklist

The design is complete when it is clear that:

- readiness can be checked without consuming a message;
- global readiness and stream-specific receives have separate responsibilities;
- context, call, and control receives all support optional timeouts;
- unrelated stream traffic cannot be returned by a stream-specific receive;
- calls are accepted through `accept_call()` and then read through their call object;
- outbound calls are created through `open_call()` and return a call object for subsequent use;
- the first inbound call receive returns the complete opening message;
- the first outbound call receive returns the peer's first response;
- inbound subsequent messages and all post-opening outbound messages do not contain `open_call`;
- call fields can be combined or sent separately;
- errors can be sent alone;
- close messages can carry other permitted fields;
- record batches use an Arrow `ArrowArray` plus `is_end_of_group` metadata;
- data schemas use `ArrowSchema` and preserve `has_group_id` and `has_row_id`;
- Arrow ownership and release responsibility are explicit;
- cancellation, closure, timeout, and failure behavior is defined;
- no implementation details have leaked into the worker-facing contract.
