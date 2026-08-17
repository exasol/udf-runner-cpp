# UDF Protocol v2: High-Level Calls

This document describes the high-level protocol calls built on top of the generic call and data-stream mechanisms.

## Scope

This document covers:

- `Run`, `Function`, and `Script`
- `get_connection`, `get_script`, and `execute_query`
- which calls carry data streams
- representative message sequences
- DB/UDFRunner scheduling policy

Related diagrams:

- [udf_protocol_high_level_call_model.svg](udf_protocol_high_level_call_model.svg)
- [udf_protocol_high_level_nested_calls.svg](udf_protocol_high_level_nested_calls.svg)
- [udf_protocol_high_level_run_sequence.svg](udf_protocol_high_level_run_sequence.svg)
- [udf_protocol_high_level_endpoint_scheduling.svg](udf_protocol_high_level_endpoint_scheduling.svg)

## Call Families

### `DB`-opened calls

| Call | Data stream | Notes |
| --- | --- | --- |
| `Run` | Yes, bidirectional | Each direction carries group and row correlation in the data itself. |
| `Function` | No | Callback-driven work only. |
| `Script` | Yes, bidirectional | One direction carries input parameters, the other direction carries table output. |

### `UDFRunner`-opened calls

| Call | Data stream | Notes |
| --- | --- | --- |
| `get_connection` | No | Returns connection information. |
| `get_script` | No | Returns script content. |
| `execute_query` | Optional | Returns query status and may carry row-correlated result rows. |

These `UDFRunner`-opened calls are ordinary nested calls, not a separate callback transport.

## Call-Specific Result Payloads

Each high-level call defines the names and bodies of its own result payloads, carried by `Payloads(...)`. A result
payload may be sent while the call remains active or together with `CloseCall` in the same composite
`StreamMessage`. Calls that have no result payload may still close normally.

## Typical Semantics

### `Run`

- opened by `DB`
- may carry opening payloads and the first input batch together
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

### `Function`

- opened by `DB`
- has no attached data stream in the current model
- relies on nested calls for callback-style interaction

### `Script`

- opened by `DB`
- uses one bidirectional data stream
- input and output directions may use different schemas

### `get_connection`

- opened by `UDFRunner`
- returns `Payloads(connection_information, ...)`
- may still carry additional named payload traffic while active

### `get_script`

- opened by `UDFRunner`
- returns `Payloads(script, ...)`
- may still carry additional named payload traffic while active

### `execute_query`

- opened by `UDFRunner`
- returns `Payloads(result_set_status, ...)`
- may optionally carry a result-row data stream
- a row-correlated result stream may set `has_group_id` to `false` and `has_row_id` to `true`; its row ID then
  occupies prefix position `0`

## Representative Sequences

The current design keeps the high-level sequences intentionally simple:

- nested callback-style calls execute while a parent `Run`, `Function`, or `Script` call remains active
- `Run` may combine opening call metadata, input schema announcement, and the first input batch
- `execute_query` may or may not open a result-row data stream after its status result

See [udf_protocol_high_level_nested_calls.svg](udf_protocol_high_level_nested_calls.svg) and
[udf_protocol_high_level_run_sequence.svg](udf_protocol_high_level_run_sequence.svg).

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

See [udf_protocol_high_level_endpoint_scheduling.svg](udf_protocol_high_level_endpoint_scheduling.svg).

## Forward-Looking Ideas Still Open

- whether `UDFRunner` may open its own pquery-style call to `DB` beyond the current `execute_query` shape
- whether table-prefetch-like declarations should be added for future call setup

## Relationship To Other Docs

- low-level framing and generic stream rules live in [udf_protocol_low_level.md](udf_protocol_low_level.md)
- generic open/close call behavior lives in [udf_protocol_call_lifecycle.md](udf_protocol_call_lifecycle.md)
- generic data-stream behavior lives in [udf_protocol_data_stream.md](udf_protocol_data_stream.md)
