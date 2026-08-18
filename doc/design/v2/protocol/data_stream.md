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

Each direction announces its own schema. The normative schema and first-batch requirements are defined in
[Rules](#rules).

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
2. A direction sends its schema before, or in the same `StreamMessage` as, its first `DataRecordBatch`.
3. A standalone schema may precede `Next(...)`; no batch may precede its schema.
4. Only the first batch in a direction may precede `Next(...)`.
5. Non-first batches require prior transfer credit.
6. Except for the permitted first batch, a sender must not exceed its peer's granted `Next(...)` credit.
7. `byte_budget` bounds the transfer window.
8. `reset` and `row_id` describe resume position only.

## Buffer Transfer

`DataRecordBatch.buffer_transport` selects how the batch buffers identified by its metadata are delivered.

- `Inline` is supported by every transport binding and is the only permitted mode on TCP/TLS.
- `Memfd` and `OutOfBand` are reserved for future Unix-domain-socket modes that require file-descriptor passing.
- An implementation that cannot establish compatible local handoff must select `Inline`; no fallback is implied
  after a batch has been announced with another mode.
- The metadata and any referenced local buffer must be validated as one integrity boundary before the batch is
  consumed.

## Open Questions

- exact end-of-stream marker
- exact semantics of `reset`
- exact encoding of schema plus first-batch bundling
- descriptor-to-batch correlation, ownership, lifetime, and cleanup for Unix file-descriptor handoff
- GPU-memory handle types and validation rules for future `OutOfBand` transfer
