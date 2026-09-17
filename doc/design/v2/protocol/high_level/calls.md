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
`StreamMessage`. Calls that have no result payload may still close normally.

## Typical Semantics

### `Run`

- opened by `DB`
- uses `call_metadata` and `column_metadata`, sent before any call, between calls, or with the opening message
- may carry the first input batch together with the opening message
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

For example, one logical Arrow record batch may contain the following columns and rows:

```text
DataSchema {
  has_group_id: true,
  has_row_id: true,
  fields: [group_id: uint64, row_id: uint64, group_id: utf8, value: utf8]
}

DataRecordBatch metadata {
  length: 3,
  is_end_of_group: true,
  nodes: ...,
  buffers: ...
}

reconstructed record batch {
  group_id: [7, 7, 8],
  row_id:   [1, 2, 1],
  group_id: ["a", "b", "c"],
  value:    ["left", "right", "only"]
}
```

Here, the first two rows belong to group 7 and have row IDs 1 and 2; the row ID starts at 1 again for group 8.
This restart is only an example. Group IDs and row IDs are independent opaque identifiers: row IDs may also continue
across groups, and equal or consecutive row-ID values in different groups do not imply any relationship between
those groups. The meaningful association is the `(group_id, row_id)` pair at one record position. The third column is
user data and deliberately has the same name as the reserved group ID column. The reserved correlation columns are
identified by their positions—the first column is the group ID and the second is the row ID—not by field names. All
four columns are part of the same record batch; they are not metadata carried separately from the batch. On the wire,
the corresponding `DataRecordBatch` contains only the batch metadata (`length`, `nodes`, and `buffers`), plus
`is_end_of_group`; the column buffer bytes are transported separately according to `buffer_transport`. The
`is_end_of_group` flag indicates that the final group in this batch is complete.

The other supported correlation layouts are:

```text
DataSchema {
  has_group_id: false,
  has_row_id: true,
  fields: [row_id: uint64, value: utf8]
}

reconstructed record batch {
  row_id: [1, 2],
  value:  ["left", "right"]
}
```

Here, the first column is the row ID because `has_row_id` is set and `has_group_id` is not. A batch without either
correlation column has no reserved prefix:

```text
DataSchema {
  has_group_id: false,
  has_row_id: false,
  fields: [value: utf8]
}

reconstructed record batch {
  value: ["left", "right"]
}
```

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
| Group ID, either direction | [`RunEndEncoded`](https://arrow.apache.org/docs/format/Columnar.html#run-end-encoded-layout) over unsigned 64-bit IDs when groups contain repeated rows. | Plain unsigned 64-bit. |
| Row ID, `DB` to `UDFRunner` | [`exasol.udf.range_run`](range_run.md) extension array. | Plain unsigned 64-bit. |
| Row ID, `UDFRunner` to `DB`, `RETURNS` UDF | [`exasol.udf.range_run`](range_run.md) extension array. | Plain unsigned 64-bit. |
| Row ID, `UDFRunner` to `DB`, `EMITS` UDF | [`RunEndEncoded`](https://arrow.apache.org/docs/format/Columnar.html#run-end-encoded-layout) over unsigned 64-bit IDs. | Plain unsigned 64-bit. |

The preferences reflect the expected shape of each direction's data. For `DB` to `UDFRunner`, group IDs are
usually repeated because a group contains multiple rows, while row IDs are usually ordered and often consecutive.
For a `RETURNS` UDF, `UDFRunner` produces exactly one output row per input row or group, so its row-ID references are
usually ordered and its group IDs are usually repeated. For an `EMITS` UDF, a group usually contains multiple output
rows, and multiple output rows usually reference the same input row; therefore both group IDs and row-ID references
are usually repeated. In the `UDFRunner` to `DB` direction, both the group ID and row ID are references to the
inbound `(group_id, row_id)` pair; they are not newly allocated output identifiers.

For example, the following `DB` → `UDFRunner` input contains two groups in one record batch. The groups may
have been produced as separate group-sized segments, for example by different threads, and then fused into this
batch:

```text
record batch:  group_id  row_id
               10        101
               10        102
               20        103
               20        104

group_id: [10, 10, 20, 20] -> RunEndEncoded(run_ends=[2, 4], values=[10, 20])
row_id:   [101, 102, 103, 104] -> range_run(run_ends=[2, 4], values=[101, 103])
```

The consecutive row IDs enable compression within each group-sized run. The `range_run` run ends intentionally align
with the group ends, even though the row IDs continue from 102 to 103. Row IDs 102 and 103 have no semantic
relationship merely because they are adjacent or belong to different groups. The group and row-ID values remain the
same whether the batch was produced by one thread or fused from segments produced by different threads.

For `UDFRunner` → `DB`, the output references the inbound rows by the same pair:

```text
inbound rows:  (group_id, row_id) = [(10, 101), (10, 102), (20, 103)]

RETURNS output (one output per input row):
  group_id: [10, 10, 20]
  row_id:   [101, 102, 103] -> range_run(run_ends=[2, 3], values=[101, 103])

EMITS output (multiple outputs for input row (10, 101)):
  group_id: [10, 10, 10, 20]
  row_id:   [101, 101, 101, 103] -> RunEndEncoded(run_ends=[3, 4], values=[101, 103])
```

The same repeated-value encoding applies to group IDs in both `UDFRunner` output directions. Plain unsigned 64-bit
encoding remains the compatible fallback when the preferred encoding is unavailable or not beneficial.

`exasol.udf.range_run` uses [`RunEndEncoded`](https://arrow.apache.org/docs/format/Columnar.html#run-end-encoded-layout)
as its Arrow storage type. Its `run_ends` child is a signed 64-bit integer array and its unsigned 64-bit `values`
child stores the first row ID for each run; each following logical value in that run increases by one. The field
sets `ARROW:extension:name` to `exasol.udf.range_run`; no extension metadata is required in version 1.

### Function Operations

- opened by `DB` with one of the Function operation names
- each call uses `call_metadata` and `column_metadata`, sent before any call, between calls, or with the opening message
- has no attached data stream in the current model
- has the operation-specific request and result payloads defined in
  [payloads.md](payloads.md)

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

See [nested_calls.svg](nested_calls.svg) and
[run_sequence.svg](run_sequence.svg).

## Scheduling Policy

The source material implies the following DB/UDFRunner deadlock-avoidance rules. These rules govern high-level
call orchestration and do not alter the generic Client/Server stream rules in the low-level protocol.

### `UDFRunner`

1. run socket handling and user-code execution as independently wakeable activities
2. wait for either DB socket activity or user-code activity; do not block solely on socket receive
3. use `Next(...)` byte budgets to bound data in flight; do not impose a message-count limit
4. send regular `KeepAlive` messages so `DB` can continue housekeeping

### `DB`

1. prioritize nested-call responses before data-stream work
2. if nothing is ready to send, block waiting for new incoming messages
3. monitor peer liveness and terminate unhealthy sessions when needed

See [endpoint_scheduling.svg](endpoint_scheduling.svg).

## Forward-Looking Ideas Still Open
<>
- `ExecuteScript` and `execute_query` call shapes and data streams
- whether `UDFRunner` may open its own pquery-style call to `DB`
- whether table-prefetch-like declarations should be added for future call setup

## Relationship To Other Docs

- low-level protocol lives in [../low_level/protocol.md](../low_level/protocol.md)
- high-level payload contracts live in [payloads.md](payloads.md)
