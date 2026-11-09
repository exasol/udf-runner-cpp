# Worker-Facing Low-Level Context Interface

## Purpose

This document defines the worker-facing interface for one protocol-v2 connection. It is a design contract only. It
does not define sockets, framing internals, threads, queues, codecs, or other transport implementation details.

The worker sees three abstractions:

- `Context` for connection-wide readiness, inbound call acceptance, and outbound call opening;
- `Call` for one logical call stream;
- `ControlStream` for connection-level traffic on stream `0`.

The C++ declarations for these abstractions and their related builders, views, content types, results, statuses, and
timeouts reside in the `exasol::udf::v2::interface` namespace. Generated wire-format types remain an internal
transport detail in the separate `exasol::udf::protocol` namespace.

The context consumes and dispatches transport messages internally. A worker never receives a socket, file descriptor,
or wire frame. Message views expose `stream_id()` as read-only metadata: the context uses it to dispatch received
messages, and a worker may inspect it to identify a message or for diagnostics. Worker-facing operations select
streams through their `Context`, `Call`, or `ControlStream` object and do not accept a numeric stream ID.

## Result and status concepts

Readiness operations report status without consuming a message:

```text
MessageStatus =
    MessageAvailable
  | NoMessage
  | TimedOut
  | Cancelled
  | PeerClosed
  | ConnectionError
```

Consuming operations return either a value or a terminal result:

```text
OperationStatus =
    Ok
  | TimedOut
  | Cancelled
  | PeerClosed
  | ProtocolError
  | TransportError
```

The conceptual generic result type is:

```text
Result<T> {
    OperationStatus status
    optional<T> value
    optional<ErrorInfo> error
}
```

`Result<void>` represents an operation that has no value on success. A successful result has `status == Ok` and a
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
    create_opening_call_message_builder() -> unique_ptr<OpeningCallMessageBuilder>
    open_call(unique_ptr<OpeningCallMessageBuilder>) -> Result<unique_ptr<Call>>
    control_stream() -> ControlStream&
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

`create_opening_call_message_builder()` reserves a new locally owned stream ID and returns an
`OpeningCallMessageBuilder`
bound to it. The worker adds the required `open_call` field and passes that builder to `open_call()`. A reserved stream
ID is not reused if its builder is abandoned or `open_call()` fails.

## Call interface

The conceptual call contract is:

```text
Call {
    create_message_builder() -> unique_ptr<CallMessageBuilder>
    receive_status() -> MessageStatus
    receive(optional<Duration> timeout) -> Result<unique_ptr<CallMessageView>>
    send(unique_ptr<CallMessageBuilder>&&, optional<Duration> timeout) -> Result<void>
}
```

`Call::receive_status()` is non-blocking, observes only this call's stream, and never consumes a message.

`Call::receive(timeout)` waits only for this call's stream. An omitted timeout means wait indefinitely. It returns the
next complete call message, returns `TimedOut` when the timeout expires, and returns a terminal result for
cancellation, peer closure, or connection failure.

The first successful `Call::receive()` returns the opening `CallMessageView`, including its `open_call` field and any
payloads, `DataSchema`, `Reset`, `RecordBatch`, error, or close fields carried in the same protocol message. Subsequent
messages must not contain `open_call`.

For a call returned by `open_call()`, the opening message has already been sent by the context. Its first successful
`Call::receive()` returns the peer's first message and must not contain `open_call`. An `open_call` field is not valid
for a `CallMessageBuilder` passed to `Call::send()` after the call object has been created.

If a control message or another call's message arrives while `Call::receive()` is blocked, the context dispatches it
to its own stream and the call receive continues waiting. It does not return unrelated traffic.

`Call::send()` waits for the context to grant permission for the next data batch when required by flow control. An
absent timeout waits indefinitely, a zero timeout does not block, and an expired timeout returns `TimedOut` without
consuming the builder or its Arrow ownership. Passing the `unique_ptr` by rvalue reference does not itself transfer
ownership: ownership transfers only when the call accepts the message. On any failure, the `unique_ptr` remains
non-null and retains the builder and its Arrow objects. The background thread owns the authoritative credit and any
internal batch splitting; the worker-facing send operation submits one logical batch.

## Composite call messages

The call-message builder accepts an Arrow schema with its correlation-field flags:

```text
data_schema(ArrowSchemaPtr schema, bool has_group_id, bool has_row_id)
```

Inbound schema ownership is exposed through `DataSchemaView`; the wire-level metadata type is
`protocol::DataSchema`.

The call-message builder accepts a record batch with its group-boundary flag:

```text
record_batch(ArrowArrayPtr array, bool is_end_of_group)
```

Inbound record-batch ownership is exposed through `RecordBatchView`; the wire-level metadata type is
`protocol::DataRecordBatchMetadata`.

Each call message is composite; its fields are independently optional:

```text
Call message {
    optional open_call
    optional payloads
    optional data_schema: DataSchema
    optional reset
    optional record_batch: RecordBatch
    optional error
    optional close_call
}
```

A single message may contain one or multiple fields. A worker sends fields separately by submitting separate
`CallMessageBuilder` instances; no field-specific send methods are required.

For inbound calls, `open_call` is required on the first received message and forbidden on subsequent received messages.
For outbound calls, `open_call` is required only in the opening message supplied to `Context::open_call()` and is
forbidden in all later received or sent messages. An opening message may contain any other protocol-permitted call
fields.

The design permits, subject to protocol-state validation:

- payloads alone;
- `open_call` alone;
- `open_call` with payloads, `DataSchema`, `Reset`, a `RecordBatch`, error, or close;
- a `DataSchema` alone;
- `Reset` alone;
- a `RecordBatch` alone after its schema has been announced;
- `DataSchema` and the first `RecordBatch` together;
- payloads, `DataSchema`, `Reset`, and a `RecordBatch` together;
- an error alone;
- close alone;
- close combined with payloads, `DataSchema`, `Reset`, a `RecordBatch`, and/or an error.

If multiple fields arrive in one protocol message, `Call::receive()` returns them together in one `CallMessageView`.

An error without `close_call` is a non-terminal diagnostic. A message containing `close_call` terminates the call
after the message is processed.

## Control stream interface

`Context::control_stream()` returns the connection-level stream for `stream_id == 0`. Its conceptual contract is:

```text
ControlStream {
    create_message_builder() -> unique_ptr<ControlMessageBuilder>
    receive_status() -> MessageStatus
    receive(optional<Duration> timeout) -> Result<unique_ptr<ControlMessageView>>
    send(unique_ptr<ControlMessageBuilder>&&) -> Result<void>
}
```

The control stream's status and receive functions observe and consume only stream-0 messages. `ControlStream::send()`
transfers ownership only when the stream accepts the message. On failure, the caller's `unique_ptr` remains non-null
and retains ownership of the builder.

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
not expose `OpenCall`, `CloseCall`, `Reset`, `ArrowSchemaPtr`, or `ArrowArrayPtr` objects.

An error without `close_connection` is a non-terminal diagnostic. `CloseConnection` starts or acknowledges connection
shutdown and may be combined with an error or other permitted connection-level fields.

## Specialized message builders and views

Call and control traffic use separate builders and views. This prevents call-only fields from being sent on stream `0`
and prevents connection-only fields from being sent on a call stream.

### Call message builder

```text
CallMessageBuilderBase {
    add_payload(name, value, binary = false) -> CallMessageBuilderBase&
    data_schema(ArrowSchemaPtr schema, has_group_id, has_row_id) -> CallMessageBuilderBase&
    reset(row_id) -> CallMessageBuilderBase&
    record_batch(ArrowArrayPtr array, is_end_of_group) -> CallMessageBuilderBase&
    error(code, message) -> CallMessageBuilderBase&
    close_call() -> CallMessageBuilderBase&
}

OpeningCallMessageBuilder : CallMessageBuilderBase {
    open_call(call_name) -> OpeningCallMessageBuilder&
}
```

The ordinary call builder describes one composite message after a call has been opened; it cannot add `open_call`.
The opening builder adds `open_call` to the shared set of composite fields. Repeated `add_payload()` calls append
payloads in order. After `data_schema()` or `record_batch()`, the builder owns the supplied Arrow objects. `Context`
creates an `OpeningCallMessageBuilder` and binds it to the stream ID it reserves for the new call. After the call is
opened, `Call` creates ordinary `CallMessageBuilder` instances bound to that call's stream ID. Passing the opening
builder to `Context::open_call()` submits it to that operation. Passing an ordinary builder to `Call::send()` submits
it to the call, but ownership transfers only when the operation accepts the message. On failure, the caller's
`unique_ptr` remains non-null and retains the builder and its Arrow objects.

### Control message builder

```text
ControlMessageBuilder {
    server_capabilities(major, minor, endianness, number_of_supported_workers)
        -> ControlMessageBuilder&
    keep_alive() -> ControlMessageBuilder&
    add_payload(name, value, binary = false) -> ControlMessageBuilder&
    error(code, message) -> ControlMessageBuilder&
    close_connection() -> ControlMessageBuilder&
}
```

The control builder cannot construct `OpenCall`, `CloseCall`, `Reset`, `DataSchema`, or record-batch fields.
`ControlStream` creates `ControlMessageBuilder` instances bound to stream 0. Passing its `unique_ptr` to
`ControlStream::send()` submits it to the stream. Ownership transfers only when the stream accepts the message; on
failure, the caller's `unique_ptr` remains non-null and retains ownership of the builder.

### Call message view

```text
CallMessageView {
    stream_id() -> uint64
    open_call() -> std::unique_ptr<OpenCallView>
    payloads() -> std::unique_ptr<PayloadsView>
    data_schema() -> std::unique_ptr<DataSchemaView>
    reset() -> std::unique_ptr<ResetView>
    record_batch() -> std::unique_ptr<RecordBatchView>
    error() -> std::unique_ptr<ErrorView>
    close_call() -> std::unique_ptr<CloseCallView>
}
```

`stream_id()` identifies the logical stream carrying this message. Every view is backed by a verified decoded frame,
so the stream ID is always present. Constructing a view without a decoded frame is an internal invariant violation.

The `open_call()`, `payloads()`, `reset()`, `error()`, and `close_call()` accessors return typed, read-only interface
wrappers, or `nullptr` when the field is absent. The wrappers expose semantic values and never expose generated
FlatBuffer tables, vectors, unions, or runtime types. This keeps message consumers mockable and independent of wire
serialization changes.

The wrapper contracts are:

```text
OpenCallView { call_name() -> optional<string_view> }
PayloadsView { size() -> size_t; at(index) -> unique_ptr<PayloadView> }
PayloadView { name() -> optional<string_view>; value() -> unique_ptr<PayloadValueView> }
PayloadValueView {
    type() -> PayloadType
    string_value() -> optional<string_view>
    binary_value() -> optional<span<const uint8_t>>
}
ResetView { row_id() -> uint64 }
ErrorView { code() -> optional<string_view>; message() -> optional<string_view> }
CloseCallView { }
```

`PayloadType` has `String` or `Binary` values. Accessing a value using the wrong representation returns an empty
optional. Payload indexing outside the available range returns `nullptr`.

`DataSchemaView` exposes the Arrow schema ownership and its correlation flags. `RecordBatchView` exposes the Arrow
array ownership and `is_end_of_group`. The move operations are rvalue-qualified and leave the corresponding message
field empty. An empty view, or a second extraction after ownership has been taken, returns an empty smart pointer.

The specialized view contracts are:

```text
DataSchemaView {
    take_schema() && -> ArrowSchemaPtr
    has_group_id() -> bool
    has_row_id() -> bool
}

RecordBatchView {
    take_array() && -> ArrowArrayPtr
    is_end_of_group() -> bool
}
```

### Received Reset value

```text
ResetView { row_id() -> uint64 }
```

`Reset` requests that the peer seek to `row_id` for batches that are not already in flight. It is independent of the
`next_send_budget_in_bytes` field, which carries flow-control credit and is consumed internally by the background
thread. The worker-facing builder exposes `reset(row_id)` and does not set a send budget.

### Control message view

```text
ControlMessageView {
    stream_id() -> uint64
    server_capabilities() -> std::unique_ptr<ServerCapabilitiesView>
    keep_alive() -> std::unique_ptr<KeepAliveView>
    payloads() -> std::unique_ptr<PayloadsView>
    error() -> std::unique_ptr<ErrorView>
    close_connection() -> std::unique_ptr<CloseConnectionView>
}
```

The control view cannot expose call-specific fields. Its `stream_id()` identifies the frame's connection-level
stream, normally stream `0`. Its wrappers are:

```text
VersionView { major() -> uint32; minor() -> uint32 }
ServerCapabilitiesView {
    supported_version() -> unique_ptr<VersionView>
    endianness() -> Endianness
    number_of_supported_workers() -> uint32
}
KeepAliveView { }
CloseConnectionView { }
```

`Endianness` is an interface-level enum with `Little` and `Big` values.

### View lifetime and verification

Each top-level specialized view and every wrapper it creates retains a verified, immutable storage lease for the
received frame. Internal generated FlatBuffer accessors point into that storage and remain valid while their wrapper
is alive. Receiving another message does not invalidate an existing view or wrapper. Malformed FlatBuffer data
produces an error rather than a view.

Where a view exposes Arrow data, it retains the Arrow schema and array ownership until the caller invokes the matching
move operation. Arrow release callbacks are invoked exactly once. A frame that contains only wire-level data metadata
must not claim to provide a usable Arrow object until the corresponding adapter has populated the owned pointer.
After a successful move, the caller owns the `ArrowSchemaPtr` or `ArrowArrayPtr` and the view no longer owns that
object.

### Builder and view implementation contract

The builders define the worker-facing message construction interface. The transport implementation creates one
`flatbuffers::FlatBufferBuilder` for the accepted message and produces exactly one composite `StreamMessage`.
Generated FlatBuffer offsets must be created from that implementation-owned builder and must not escape before the
builder is finalized. The interface does not prescribe when serialization occurs. A builder submitted for sending must
not be reused after ownership has been accepted by the context.

The receive path verifies the complete frame with `flatbuffers::Verifier` before obtaining the generated `Frame` and
`StreamMessage` accessors. The resulting specialized view retains a shared immutable storage lease containing the
verified bytes and any Arrow ownership leases. The view is a view, not a copy, but remains valid independently of
later receives.

On accepted send, the context takes responsibility for the finalized FlatBuffer storage and attached Arrow objects.
If validation, finalization, queueing, or transport acceptance fails before acceptance, the caller's `unique_ptr`
remains non-null and the builder retains ownership of its Arrow objects.

## Arrow schemas and record batches

Data schemas and record batches are transferred through the Arrow C Data Interface, not as serialized FlatBuffer
objects. The conceptual `data_schema` field contains the interface-level `DataSchema` value, whose `schema` field is
an `ArrowSchemaPtr`. The conceptual `record_batch` field contains the interface-level `RecordBatch` value, whose
`array` field is an `ArrowArrayPtr`.

`ArrowSchemaPtr` and `ArrowArrayPtr` are move-only `std::unique_ptr` aliases with custom deleters. Their deleters call
the Arrow C Data Interface `release` callback, when present; they never apply C++ `delete` to an Arrow object. This
makes ownership explicit while preserving the C Data Interface as the worker-facing type.

`DataSchema.schema` describes one direction of a call's data stream. It is sent once per direction, before or together
with that direction's first `RecordBatch`. A later `RecordBatch` uses the previously announced schema and flags and
does not repeat them.

`RecordBatch.is_end_of_group` is meaningful only when `DataSchema.has_group_id` is true. Groups before the final group
in a batch end automatically when the group identifier changes. The flag controls the group identified by that
batch's final row: `true` ends it, while `false` means it may continue in a later batch. The flag is not an
end-of-stream marker, and an empty batch must not set it to true.

The correlation flags describe the reserved prefix of `DataSchema.schema`:

- both flags false: no reserved correlation fields are present;
- only `has_group_id`: field `0` is the group ID;
- only `has_row_id`: field `0` is the row ID;
- both flags true: field `0` is the group ID and field `1` is the row ID;
- user data fields follow the reserved prefix;
- correlation fields are identified by position, not by field name.

FlatBuffers remain the internal representation for protocol metadata and control fields, including payloads, `Reset`,
errors, close messages, capabilities, keepalive, and connection close. Specialized views expose validated semantic
accessors without exposing raw frame buffers or requiring workers to manage FlatBuffer view lifetimes.

The context, call, and control-stream contracts expose the Arrow C Data Interface only. Arrow C++ conversion is left
to the caller and is not part of these interface declarations. Arrow IPC internals and transport-specific buffer
handling remain hidden from the context contract.

A typical conversion sequence is:

```text
schema_view = call_view.data_schema()
batch_view = call_view.record_batch()
schema = move(*schema_view).take_schema()
array = move(*batch_view).take_array()
batch = arrow::ImportRecordBatch(array.get(), schema.get())
```

The specialized accessors return `nullptr` when the message has no corresponding Arrow data. Check both pointers
before dereferencing them.

The top-level `CallMessageView` must remain alive until both specialized views have extracted their owners. The
resulting `arrow::RecordBatch` owns the imported Arrow data independently of the protocol view.

Ownership follows the Arrow release contract:

- received `DataSchema.schema` and `RecordBatch.array` objects remain owned by the view until `take_schema()` or
  `take_array()` moves them to the caller;
- a moved object is passed to the caller's Arrow C++ conversion or released by its receiving owner exactly once;
- sent `ArrowSchemaPtr` and `ArrowArrayPtr` objects remain owned by the builder/caller until the context accepts the
  send;
- after successful acceptance, the context takes the unique pointers and therefore the release responsibility;
- a failed send before acceptance leaves the unique pointers with the builder/caller;
- successful Arrow C++ import transfers the usable data into `arrow::RecordBatch` and must not be followed by manual
  calls to `ArrowArray::release` or `ArrowSchema::release`;
- no object may be released more than once.

## Dispatch and consumption rules

The context internally dispatches incoming traffic synchronously when the worker invokes any context, call, or
control-stream function. It does not use a dispatcher thread. Incoming traffic is routed by `stream_id` to:

- a pending inbound-call queue;
- one logical pending sequence per accepted call;
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

One worker thread owns a context and all of its calls and control-stream objects. Each logical stream has one consumer,
and concurrent operations on the same context, including receives from different calls, are outside this interface.

After `accept_call()` succeeds, the accepted call's pending-message sequence begins with its opening composite
message. The opening message is consumed exactly once by the first successful `Call::receive()`.

After `open_call()` succeeds, the call's pending-message sequence begins with the peer's first response. The locally
sent opening message is not returned by `Call::receive()`.

## Lifecycle and terminal behavior

The context owns the connection after successful context creation. A call object represents one logical call and is
valid until call close, connection close, internal cancellation, peer disconnect, or fatal error.

Internal cancellation, peer disconnect, connection close, and fatal errors wake all affected blocked operations. After
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
- internal cancellation and transport failure are reported as terminal operation results.

## Validation responsibilities

The context validates, before delivering or sending messages:

- protocol message structure and configured size limits;
- stream ownership and stream identity rules;
- call lifecycle ordering;
- outbound opening messages must contain `open_call` exactly once;
- `open_call` is rejected after the opening message;
- control-message field combinations;
- call-message field combinations;
- FlatBuffer buffers are verified before a specialized view is created;
- call builders cannot construct control-only fields;
- control builders cannot construct call-only fields;
- `DataSchema` must precede or accompany the first `RecordBatch`;
- the schema field prefix must match `has_group_id` and `has_row_id`;
- a second schema or changed correlation flags in one direction are rejected;
- `RecordBatch.is_end_of_group` must be false when `has_group_id` is false;
- an empty `RecordBatch` must not set `is_end_of_group` to true;
- groups before the final group in a batch end automatically when the group identifier changes;
- the worker-provided flag controls only the final group in a batch;
- one data stream per call;
- `next_send_budget_in_bytes` flow-control rules and `Reset` seek semantics;
- close and error semantics;
- Arrow schema and array ownership and release requirements;
- supported record-batch transport;
- rejection of ordinary traffic after connection shutdown.

Invalid messages produce protocol errors and follow the applicable call or connection termination behavior.

## Examples

### Composite call message

The peer may open a call with one message containing `open_call`, payloads, `DataSchema`, and the first `RecordBatch`.
The worker first calls `accept_call()`, then receives one `CallMessageView` containing all four fields.

### Outbound call

The worker creates a nested call with a `CallMessageBuilder` containing `open_call` and its opening payloads. It calls
`Context::open_call()`, receives a `Call` object, and then uses `Call::receive()` for the peer's response. The local
opening message is not returned by that receive operation.

### Opening close

The peer may open and close a call in one message. `accept_call()` returns the call, and its first `receive()` returns
the opening `CallMessageView` containing both `open_call` and `close_call`; the call then enters its closed state.

### Separate call messages

The peer may send a `DataSchema`, then a `Reset`, then a `RecordBatch` as three separate messages. The worker receives
three successive call-message views.

### Error-only message

The worker may send a `CallMessageBuilder` containing only `error`. The call remains active unless `close_call` is also
present.

### Close with final data

The worker may send one `CallMessageBuilder` containing a final record batch, payloads, an error, and `close_call`,
subject to the call's protocol state.

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
- record batches use an `ArrowArrayPtr` plus `is_end_of_group` metadata;
- data schemas use an `ArrowSchemaPtr` and preserve `has_group_id` and `has_row_id`;
- `DataSchemaView::take_schema()` and `RecordBatchView::take_array()` make Arrow ownership transfer explicit;
- Arrow C++ conversion is performed by the caller and is not part of `Context`, `Call`, or `ControlStream`;
- Arrow C++ `RecordBatch` ownership begins only after successful import;
- Arrow ownership and release responsibility are explicit;
- cancellation, closure, timeout, and failure behavior is defined;
- one worker thread owns each context and concurrent operations on that context are outside the contract;
- no implementation details have leaked into the worker-facing contract.
