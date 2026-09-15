# UDF Protocol v2: Data Stream

This document describes the generic low-level data-stream behavior attached to a call. It excludes framing details
and high-level command semantics.

## Scope

This document covers:

- the one-data-stream-per-call model
- per-direction schema behavior
- `Next(byte_budget, reset, row_id)` flow control
- `RecordBatch` sequencing rules
- completion behavior and open questions

Related diagram:

- [data_stream_flow.svg](data_stream_flow.svg)

## Model

- a data stream exists only attached to a call
- a call may have at most one bidirectional data stream
- each side owns one logical direction within that data stream
- ordering is total within one direction
- the two directions do not need to share a schema

## Schema and First Batch Behavior

Each direction announces its own schema in one `DataSchema` control attribute. The schema is a native FlatBuffers
representation of the direction's Arrow-compatible column layout; it is not an Apache Arrow IPC schema message.
`DataSchema` must be sent before the first `DataRecordBatch`, or in the same `Frame` as that first batch. Each later
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

This depth-first preorder is used for flattened `DataRecordBatch.nodes` and for `variadic_buffer_counts` entries
belonging to variable-buffer fields. Buffer order itself follows the corresponding Arrow array buffer layout and is
independent of field-name or type sorting.

`has_group_id` and `has_row_id` identify correlation columns at the start of the field list according to the
call-specific rules. The low-level converter preserves those fields and does not infer or insert them.

## `Next(byte_budget, reset, row_id)`

`Next(...)` is the transfer-credit mechanism.

- `byte_budget` is a byte budget, not a row count
- `reset` indicates that transfer should resume from `row_id`
- `row_id` is a seek position for resumed transfer, not a per-batch correlation field

This `row_id` usage is distinct from any row correlation carried inside the data itself for high-level call
semantics such as scalar-return `Run`.

## Endpoint State Models

The state models use one local endpoint's perspective. That endpoint has one outbound direction and one inbound
direction; its peer runs the same two models with the directions reversed.

### Outbound Direction

The outbound model tracks data sent by the local endpoint and `Next(...)` credit received from its peer.

| State | Meaning |
| --- | --- |
| `OutboundIdle` | The local endpoint has sent neither schema nor batch data. |
| `SchemaAnnounced` | The local endpoint sent its schema, but not its first batch. |
| `FirstBatchCreditGranted` | The peer granted credit before the local endpoint announced its schema. |
| `CreditGranted` | The peer granted credit after the schema or a prior batch. |
| `WaitingForNext` | The local endpoint needs additional credit before sending another batch. |
| `OutboundCompleted` | The local endpoint has no more outbound data. |

| From | Local event | To |
| --- | --- | --- |
| `OutboundIdle` | Send schema | `SchemaAnnounced` |
| `OutboundIdle` | Send schema + first batch | `WaitingForNext` |
| `SchemaAnnounced` | Send first batch | `WaitingForNext` |
| `OutboundIdle` | Receive `Next(...)` | `FirstBatchCreditGranted` |
| `SchemaAnnounced` | Receive `Next(...)` | `CreditGranted` |
| `FirstBatchCreditGranted` | Send schema + first batch | `WaitingForNext` |
| `CreditGranted` | Send batch | `WaitingForNext` |
| `WaitingForNext` | Receive `Next(...)` | `CreditGranted` |
| `WaitingForNext` | Finish stream | `OutboundCompleted` |

### Inbound Direction

The inbound model tracks data received by the local endpoint and `Next(...)` credit it sends to its peer.

| State | Meaning |
| --- | --- |
| `InboundIdle` | The local endpoint received neither schema nor batch data. |
| `SchemaReceived` | The local endpoint received the peer's schema, but not its first batch. |
| `WaitingForFirstBatch` | The local endpoint sent `Next(...)` and awaits the first batch. |
| `ReadyToRequest` | The local endpoint received a batch and may request another. |
| `WaitingForBatch` | The local endpoint sent `Next(...)` and awaits a non-first batch. |
| `InboundCompleted` | The local endpoint expects no more inbound data. |

| From | Local event | To |
| --- | --- | --- |
| `InboundIdle` | Receive schema | `SchemaReceived` |
| `InboundIdle` | Receive schema + first batch | `ReadyToRequest` |
| `InboundIdle` | Send `Next(...)` | `WaitingForFirstBatch` |
| `SchemaReceived` | Receive first batch | `ReadyToRequest` |
| `SchemaReceived` | Send `Next(...)` | `WaitingForFirstBatch` |
| `WaitingForFirstBatch` | Receive schema | `WaitingForFirstBatch` |
| `WaitingForFirstBatch` | Receive schema + first batch | `ReadyToRequest` |
| `WaitingForFirstBatch` | Receive first batch | `ReadyToRequest` |
| `ReadyToRequest` | Send `Next(...)` | `WaitingForBatch` |
| `WaitingForBatch` | Receive batch | `ReadyToRequest` |
| `WaitingForFirstBatch` | Finish stream | `InboundCompleted` |
| `WaitingForBatch` | Finish stream | `InboundCompleted` |

## Rules

1. Each direction sends exactly one `DataSchema`.
2. A direction sends its schema before, or in the same `Frame` as, its first `DataRecordBatch`.
3. A standalone schema may precede `Next(...)`; no batch may precede its schema.
4. Only the first batch in a direction may precede `Next(...)`.
5. Non-first batches require prior transfer credit.
6. Except for the permitted first batch, a sender must not exceed its peer's granted `Next(...)` credit.
7. `byte_budget` bounds the transfer window.
8. `reset` and `row_id` describe resume position only.

## Buffer Transfer

`DataRecordBatch` contains the batch metadata required to reconstruct the Arrow-compatible layout. `length` is the
row count. `nodes` contains one `FieldNode` for each flattened field in schema preorder; each node carries the array
length and null count. `buffers` describes the buffers in the corresponding Arrow buffer order. For view fields,
`variadic_buffer_counts` gives the number of variable buffers belonging to each variable-buffer field in schema
preorder.

The metadata is part of the length-framed FlatBuffer `Frame`. The buffer bytes themselves are transferred separately;
they are not embedded in `DataRecordBatch`.

`DataRecordBatch.buffer_transport` selects how those buffers are delivered.

- `Inline` is supported by every transport binding and is the only permitted mode on TCP/TLS. The wire sequence for
  an inline batch is:

  ```text
  frame-length prefix
  serialized FlatBuffer Frame
  buffer 0 bytes
  buffer 1 bytes
  ...
  ```

  The buffers follow immediately after the complete frame in `DataRecordBatch.buffers` order. They have no individual
  framing, stream offsets, or per-buffer length prefixes. `Buffer.length` is the exact number of bytes to transfer
  for each buffer; `Buffer.offset` is not used for inline transport.
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
