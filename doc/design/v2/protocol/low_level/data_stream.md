# UDF Protocol v2: Data Stream

This document describes the generic low-level data-stream behavior attached to a call. It excludes framing details
and high-level command semantics.

## Scope

This document covers:

- the one-data-stream-per-call model
- per-direction schema behavior
- `next_send_budget_in_bytes` and `Reset(row_id)` flow control
- `RecordBatch` sequencing rules
- completion behavior and open questions

Related diagram:

- [data_stream_flow.svg](data_stream_flow.svg)

## Model

- a data stream exists only attached to a call
- a call may have at most one bidirectional data stream
- each side controls its own outbound direction within that data stream
- the Client outbound direction is Client -> Server; the Server outbound direction is Server -> Client
- ordering is total within one direction
- the two directions do not need to share a schema

## Schema and First Batch Behavior

Each direction announces its own schema in one `DataSchema` control attribute. The schema is a native FlatBuffers
representation of the direction's Arrow-compatible column layout; it is not an Apache Arrow IPC schema message.
`DataSchema` must be sent before the first record batch, or in the same `Frame` as that first batch. Each later
batch in the direction reuses the previously announced schema. The two directions of one call may use different
schemas.

The `DataSchema.schema.fields` vector contains the top-level columns in transfer order. Nested fields are represented
by the `children` vectors of their parent fields. Field names, nullability, type parameters, extension annotations,
and child structure are preserved from the Arrow schema. Fields are not sorted or reordered by name or type.

### Arrow Schema Conversion Order

`DataSchema` is created from the Arrow schema for the data-stream direction. The converter walks the top-level fields
in the order provided by `arrow::Schema::fields()`. It visits fields recursively in depth-first preorder: the current
field is emitted first, followed by each child in the order provided by Arrow.

For example, an Arrow schema shaped as:

```text
a: int32
b: struct<x: int64, y: list<utf8>>
c: bool
```

is represented with top-level `Schema.fields = [a, b, c]`, with `b.children = [x, y]` and `y.children` containing
the list element field. The corresponding flattened field order is:

```text
a, b, x, y, element, c
```

This depth-first preorder is used for flattened `DataRecordBatchMetadata.nodes` and for `variadic_buffer_counts` entries
belonging to variable-buffer fields. Buffer order itself follows the corresponding Arrow array buffer layout and is
independent of field-name or type sorting.

`has_group_id` and `has_row_id` identify correlation columns at the start of the field list according to the
call-specific rules. The low-level converter preserves those fields and does not infer or insert them.

## Send budget and `Reset(row_id)`

`next_send_budget_in_bytes` is a flow-control hint sent by the receiver of a direction to its sender. `Reset(...)`
is an independent seek request. Neither field selects a record batch or directly pauses, cancels, or otherwise
controls the sender.

- `next_send_budget_in_bytes` is a preferred maximum byte size for one or multiple record batches; zero means no new
  credit
- the first record batch may be sent without a received send budget
- subsequent batches require available credit from a previously received non-zero send budget
- the sender may send less than the hinted budget and may send multiple batches while budget remains
- an indivisible batch may exceed the remaining budget
- a present `Reset` requests a seek for batches that are not already in flight, and `row_id` identifies the position
  from which the sender resumes
- an absent `Reset` requests no seek; batches already in flight continue unaffected
- `Reset.row_id` defaults to `0` when omitted ([FlatBuffers schema defaults](https://flatbuffers.dev/schema/))

This `row_id` usage is distinct from any row correlation carried inside the data itself for high-level call
semantics such as scalar-return `Run`.

## Endpoint State Models

The state models use one local endpoint's perspective. That endpoint has one outbound direction and one inbound
direction. The endpoint controls its outbound direction; its peer controls the reverse direction. For example, the
Client controls Client -> Server output, while the Server controls Server -> Client output.

### Outbound Direction

The outbound model tracks data sent by the local endpoint and the latest flow-control hint received from its peer.
Sending or receiving `CloseCall` makes the local endpoint's outbound direction terminal: it sends no further schema,
batch, send budget, `Reset(...)`, or other call-scoped message. A `CloseCall` is not acknowledged. Messages already in flight may
still arrive after either endpoint sends or receives `CloseCall`. Delayed messages for inactive peer-owned stream IDs
are discarded under the [stream-management rules](../../runner/stream_management.md); messages for inactive locally
owned IDs are protocol errors.

| State | Meaning |
| --- | --- |
| `OutboundIdle` | The local endpoint has sent neither schema nor batch data. |
| `SchemaAnnounced` | The local endpoint sent its schema, but not its first batch. |
| `BudgetAvailable` | The local endpoint has a usable budget hint for one or more batches. |
| `WaitingForNext` | The local endpoint has no usable budget for another non-first batch. |
| `OutboundCompleted` | The local endpoint has no more outbound data. |

| From | Local event | To |
| --- | --- | --- |
| `OutboundIdle` | Send schema | `SchemaAnnounced` |
| `OutboundIdle` | Send schema + first batch | `WaitingForNext` |
| `SchemaAnnounced` | Send first batch | `WaitingForNext` |
| `OutboundIdle` | Receive send budget | `BudgetAvailable` |
| `SchemaAnnounced` | Receive send budget | `BudgetAvailable` |
| `BudgetAvailable` | Send one or more batches while budget remains | `BudgetAvailable` |
| `BudgetAvailable` | No usable budget remains | `WaitingForNext` |
| `WaitingForNext` | Receive send budget | `BudgetAvailable` |
| Any outbound state | Send or receive `CloseCall` | `OutboundCompleted` |

### Inbound Direction

The inbound model tracks data received by the local endpoint and the send budgets it grants to its peer. Sending a
budget does not make the peer's sender wait, cancel, or select a particular batch.

| State | Meaning |
| --- | --- |
| `InboundIdle` | The local endpoint received neither schema nor batch data. |
| `SchemaReceived` | The local endpoint received the peer's schema, but not its first batch. |
| `BatchesInFlight` | One or more batches are in flight and are unaffected by later seek hints. |
| `InboundCompleted` | The local endpoint expects no more inbound data. |

| From | Local event | To |
| --- | --- | --- |
| `InboundIdle` | Receive schema | `SchemaReceived` |
| `InboundIdle` | Send budget | `InboundIdle` |
| `InboundIdle` | Receive schema + first batch | `BatchesInFlight` |
| `SchemaReceived` | Receive first batch | `BatchesInFlight` |
| `SchemaReceived` | Send budget | `SchemaReceived` |
| `BatchesInFlight` | Receive batch | `BatchesInFlight` |
| `BatchesInFlight` | Send budget | `BatchesInFlight` |
| Any inbound state | Receive `CloseCall` | `InboundCompleted` |

## Rules

1. Each direction sends exactly one `DataSchema`.
2. A direction sends its schema before, or in the same `Frame` as, its first record batch.
3. A standalone schema may precede a send budget; no batch may precede its schema.
4. Only the first batch in a direction may be sent without a previously received non-zero send budget.
5. A sender may send one or more batches while the current send budget remains usable.
6. A sender may send less than the hinted budget; an indivisible batch may exceed the remaining budget.
7. After the current budget is no longer usable, a sender waits for another send budget before sending a later batch.
8. `Reset.row_id` applies only to batches that are not already in flight.
9. In-flight batches continue unaffected by a later seek hint and may reference different positions.
10. Sending or receiving `CloseCall` terminates the local endpoint's participation in the stream; the receiver does
    not send a close acknowledgement, and late messages already in transit are ignored.

## Buffer Transfer

`DataRecordBatchMetadata` contains the batch metadata required to reconstruct the Arrow-compatible layout. `length` is the
row count. `nodes` contains one `FieldNode` for each flattened field in schema preorder; each node carries the array
length and null count. `buffers` describes the buffers in the corresponding Arrow buffer order. For view fields,
`variadic_buffer_counts` gives the number of variable buffers belonging to each variable-buffer field in schema
preorder.

The metadata is part of the length-framed FlatBuffer `Frame`. The buffer bytes themselves are transferred separately;
they are not embedded in `DataRecordBatchMetadata`.

`DataRecordBatchMetadata.buffer_transport` selects how those buffers are delivered.

- `Inline` is supported by every transport binding and is the only permitted mode on TCP/TLS. The wire sequence for
  an inline batch is:

  ```text
  uint32 frame_length (little-endian)
  serialized FlatBuffer Frame
  buffer 0 bytes
  buffer 1 bytes
  ...
  ```

  The 4-byte prefix contains only the serialized `Frame` length; it excludes the prefix itself and all trailing buffers.
  The buffers follow immediately after the complete frame in `DataRecordBatchMetadata.buffers` order. They have no
  individual framing, stream offsets, or per-buffer length prefixes. `Buffer.length` is the exact number of bytes to
  transfer for each buffer; `Buffer.offset` is not used for inline transport.
- A sender may construct one `iovec` for the frame and one for each buffer and transmit them with `writev`. A receiver
  reads and verifies the frame first, allocates one destination for each advertised `Buffer.length`, and may receive
  the buffers with `readv`. Partial reads and writes must be continued until the complete sequence has been consumed
  or sent.
- `Memfd` and `OutOfBand` are reserved for future Unix-domain-socket modes that require file-descriptor passing.
- An implementation that cannot establish compatible local handoff must select `Inline`; no fallback is implied
  after a batch has been announced with another mode.
- The frame, metadata, buffer count, buffer lengths, and total byte budget must be validated before the batch is
  consumed. Truncated data or unexpected bytes between the frame and its inline buffers are protocol errors.

## Open Questions

- exact semantics of `reset`
