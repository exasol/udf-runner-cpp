# UDF Protocol v2: High-Level Calls

This document describes the high-level protocol calls built on top of the generic call and data-stream mechanisms.

## Scope

This document covers:

- `Run` and Function operations
- `cleanup`
- `get_connection` and `get_script`
- which calls carry data streams
- representative message sequences
- DB/UDFRunner scheduling policy

Related diagrams:

- [call_model.svg](call_model.svg)
- [nested_calls.svg](nested_calls.svg)
- [run_sequence.svg](run_sequence.svg)
- [endpoint_scheduling.svg](endpoint_scheduling.svg)

## Call Families

### `DB`-opened calls

| Call | Data stream | Notes |
| --- | --- | --- |
| `Run` | Yes, bidirectional | Each direction carries group and row correlation in the data itself. |
| Function operation | No | One of `default_output_columns`, `virtual_schema_adapter`, `generate_sql_for_import_spec`, or `generate_sql_for_export_spec`. |
| `cleanup` | No | DB-opened between calls to let UDFRunner release resources retained from completed calls. |

### Nested `UDFRunner`-opened calls

| Call | Data stream | Notes |
| --- | --- | --- |
| `get_connection` | No | Returns connection information. |
| `get_script` | No | Returns script content. |

These `UDFRunner`-opened calls are ordinary nested calls, not a separate callback transport. `UDFRunner` does not open
top-level calls while idle; it can open them only while handling an active DB call.

## Call-Specific Result Payloads

Each high-level call defines the names and bodies of its own result payloads, carried by `Payloads(...)`. A result
payload may be sent while the call remains active or together with `CloseCall` in the same composite
`StreamMessage`. `CloseCall` is unilateral: the sender does not send further messages on the stream, and the receiver
does not acknowledge it or send further messages on that stream. Messages already in transit may arrive afterward and
are ignored. Calls that have no result payload may still close normally.

## Typical Semantics

### `Run`

- opened by `DB`
- uses `call_metadata` and `column_metadata`, sent before any call, between calls, or with the opening message
- may carry the first input batch together with the opening message
- may stay active while nested calls such as `get_script` or `get_connection` execute
- group and row correlation belong in the data, not in `Next(...)`

See [Group and Row Correlation](correlation.md).

### Function Operations

- opened by `DB` with one of the Function operation names
- each call uses `call_metadata` and `column_metadata`, sent before any call, between calls, or with the opening message
- has no attached data stream in the current model
- has the operation-specific request and result payloads defined in
  [payloads.md](payloads.md)

### `cleanup`

- opened by `DB` between calls, after one or more preceding calls have completed
- has no attached data stream, request payload, or normal result payload
- gives `UDFRunner` an explicit opportunity to release resources retained from completed calls or nested calls
- must complete cleanup before sending the normal `CloseCall`; `CloseCall` with `Error` reports cleanup failure
- does not close the connection or prevent later calls after a normal close
- may be repeated periodically by `DB`

The v1 implementation contains an `MT_CLEANUP` message type, but uses it as a cleanup/termination response signal
while processing older request types rather than as a standalone DB-issued operation. The v2 `cleanup` call keeps the
name for compatibility while defining an explicit between-call lifecycle.

### `get_connection`

- opened by `UDFRunner`
- returns `Payloads(connection_information)`
- may still carry additional named payload traffic while active

### `get_script`

- opened by `UDFRunner`
- returns `Payloads(script)`
- may still carry additional named payload traffic while active

## Payload Contracts

The complete call-metadata, script-metadata, Function, and nested-call payload contracts are defined in
[payloads.md](payloads.md). `StringPayload` is used directly for
scalar strings; JSON is used only where a payload has structured fields.

## Representative Sequences

The current design keeps the high-level sequences intentionally simple:

- nested callback-style calls execute while a parent `Run` or Function call remains active
- `Run` combines `OpenCall`, `call_metadata`, input schema announcement, and the first input batch when practical
- `cleanup` is sequenced between calls: the previous call closes, `DB` opens `cleanup`, and the next call starts only
  after the cleanup call closes
- normal completion of the call's data stream is marked by unilateral `CloseCall`; late in-flight stream messages are ignored

See [nested_calls.svg](nested_calls.svg) and
[run_sequence.svg](run_sequence.svg).

## Scheduling Policy

The source material implies the following DB/UDFRunner deadlock-avoidance rules. These rules govern high-level
call orchestration and do not alter the generic Client/Server stream rules in the low-level protocol.

### `UDFRunner`

1. run socket handling and user-code execution as independently wakeable activities
2. wait for either DB socket activity or user-code activity; do not block solely on socket receive
3. queue record batches produced by user code until the first-batch rule or a usable `Next(...)` byte budget permits emission
4. use `Next(...)` byte budgets to bound data in flight; do not impose a message-count limit
5. send regular `KeepAlive` messages so `DB` can continue housekeeping

### `DB`

1. prioritize nested-call responses before data-stream work
2. receive and process record batches from `UDFRunner` as a distinct data-stream event; the first batch may accompany
   call-opening/control traffic
3. queue DB-produced input batches until the first-batch rule or a usable `Next(...)` byte budget permits emission
4. if nothing is ready to send, block waiting for new incoming messages
5. monitor peer liveness and terminate unhealthy sessions when needed
6. optionally open `cleanup` between calls and wait for its `CloseCall` before starting the next call

See [endpoint_scheduling.svg](endpoint_scheduling.svg).

## Forward-Looking Ideas Still Open
<>
- `ExecuteScript` and `execute_query` call shapes and data streams
- whether `UDFRunner` may open its own pquery-style call to `DB`
- whether table-prefetch-like declarations should be added for future call setup

## Relationship To Other Docs

- low-level protocol lives in [../low_level/protocol.md](../low_level/protocol.md)
- high-level payload contracts live in [payloads.md](payloads.md)
