# UDF Protocol v2: High-Level Calls

This document describes the high-level protocol calls built on top of the generic call and data-stream mechanisms.

## Scope

This document covers:

- `Run` and Function operations
- `get_connection` and `get_script`
- which calls carry data streams
- representative message sequences
- DB/UDFRunner scheduling policy

Related diagrams:

- [high_level_call_model.svg](high_level_call_model.svg)
- [high_level_nested_calls.svg](high_level_nested_calls.svg)
- [high_level_run_sequence.svg](high_level_run_sequence.svg)
- [high_level_endpoint_scheduling.svg](high_level_endpoint_scheduling.svg)

## Call Families

### `DB`-opened calls

| Call | Data stream | Notes |
| --- | --- | --- |
| `Run` | Yes, bidirectional | Each direction carries group and row correlation in the data itself. |
| Function operation | No | One of `default_output_columns`, `virtual_schema_adapter`, `generate_sql_for_import_spec`, or `generate_sql_for_export_spec`. |

### `UDFRunner`-opened calls

| Call | Data stream | Notes |
| --- | --- | --- |
| `get_connection` | No | Returns connection information. |
| `get_script` | No | Returns script content. |

These `UDFRunner`-opened calls are ordinary nested calls, not a separate callback transport.

## Call-Specific Result Payloads

Each high-level call defines the names and bodies of its own result payloads, carried by `Payloads(...)`. A result
payload may be sent while the call remains active or together with `CloseCall` in the same composite
`StreamMessage`. Calls that have no result payload may still close normally.

## Typical Semantics

### `Run`

- opened by `DB`
- carries `call_metadata` with the opening message and may carry the first input batch together
- may stay active while nested calls such as `get_script` or `get_connection` execute
- group and row correlation belong in the data, not in `Next(...)`

#### Group and Row Correlation

Each `Run` direction may combine multiple logical groups in one `DataRecordBatch`. Its `DataSchema` sets both
`has_group_id` and `has_row_id` to `true`, adding an ordered reserved prefix before user data columns:

| Position | Column | Purpose |
| --- | --- | --- |
| `0` | Group ID | Identifies the logical input group. |
| `1` | Row ID | Identifies the input row to which an output row maps. |
| `2+` | User data | Input or output columns defined by the call. |

Groups may span multiple rows. In particular, a `SET ... EMITS` UDF may receive multiple input rows in one group.
The group ID and row ID columns are correlation fields identified only by this prefix layout, not by field names.

`DataRecordBatch.is_end_of_group` marks whether the group identified by the batch's final row is complete. It is
defined only for a `Run` direction whose schema sets `has_group_id` to `true`:

- `true` means no later batch in that direction contains the trailing group.
- `false` means the trailing group continues in a later batch.
- a change in group ID still delimits each non-trailing group within the same batch.
- an empty batch does not complete a group.

This is a group-boundary marker, not an end-of-stream marker. Generic stream-completion semantics remain a
low-level open question and are not encoded in `DataRecordBatch` metadata.

The preferred encodings are:

| Column and direction | Preferred encoding | Compatible fallback |
| --- | --- | --- |
| Group ID, either direction | `RunEndEncoded` over unsigned 64-bit IDs when groups contain repeated rows. | Plain unsigned 64-bit. |
| Row ID, `DB` to `UDFRunner` | `exasol.udf.range_run` extension array. | Plain unsigned 64-bit. |
| Row ID, `UDFRunner` to `DB`, `RETURNS` UDF | `exasol.udf.range_run` extension array. | Plain unsigned 64-bit. |
| Row ID, `UDFRunner` to `DB`, `EMITS` UDF | `RunEndEncoded` over unsigned 64-bit IDs. | Plain unsigned 64-bit. |

`exasol.udf.range_run` uses `RunEndEncoded` as its Arrow storage type. Its `run_ends` child is a signed 64-bit
integer array and its unsigned 64-bit `values` child stores the first row ID for each run; each following logical
value in that run increases by one. The field sets `ARROW:extension:name` to `exasol.udf.range_run`; no extension
metadata is required in version 1.

### Function Operations

- opened by `DB` with one of the Function operation names
- each call carries `call_metadata` with the opening message
- has no attached data stream in the current model
- has the operation-specific request and result payloads defined in
  [high_level_payloads.md](high_level_payloads.md)

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
[high_level_payloads.md](high_level_payloads.md). `StringPayload` is used directly for
scalar strings; JSON is used only where a payload has structured fields.

## Representative Sequences

The current design keeps the high-level sequences intentionally simple:

- nested callback-style calls execute while a parent `Run` or Function call remains active
- `Run` combines `OpenCall`, `call_metadata`, input schema announcement, and the first input batch when practical

See [high_level_nested_calls.svg](high_level_nested_calls.svg) and
[high_level_run_sequence.svg](high_level_run_sequence.svg).

## Scheduling Policy

The source material implies the following DB/UDFRunner deadlock-avoidance rules. These rules govern high-level
call orchestration and do not alter the generic Client/Server stream rules in the low-level protocol.

### `UDFRunner`

1. prioritize receiving over sending
2. only block during receive
3. do not receive more messages than were requested
4. send regular `KeepAlive` messages so `DB` can continue housekeeping

### `DB`

1. prioritize nested-call responses before data-stream work
2. if nothing is ready to send, block waiting for new incoming messages
3. monitor peer liveness and terminate unhealthy sessions when needed

See [high_level_endpoint_scheduling.svg](high_level_endpoint_scheduling.svg).

## Forward-Looking Ideas Still Open
<>
- `Script` and `execute_query` call shapes and data streams
- whether `UDFRunner` may open its own pquery-style call to `DB`
- whether table-prefetch-like declarations should be added for future call setup

## Relationship To Other Docs

- low-level framing and generic stream rules live in [low_level.md](low_level.md)
- generic open/close call behavior is part of the low-level protocol
- generic data-stream behavior lives in [data_stream.md](data_stream.md)
- high-level payload contracts live in [high_level_payloads.md](high_level_payloads.md)
