# UDF Protocol v2: Group and Row Correlation

This document defines how group and row correlation columns are represented in `Run` record batches.

## Correlation Columns

Each `Run` direction may combine multiple logical groups in one record batch. Its `DataSchema` sets
`has_group_id` and `has_row_id` according to the correlation columns present. When both are set, they form an
ordered reserved prefix before user data columns:

| Position | Column | Purpose |
| --- | --- | --- |
| `0` | Group ID | Identifies the logical input group. |
| `1` | Row ID | Identifies the input row to which an output row maps. |
| `2+` | User data | Input or output columns defined by the call. |

The supported layouts are:

```text
DataSchema {
  has_group_id: true,
  has_row_id: true,
  fields: [group_id: uint64, row_id: uint64, group_id: utf8, value: utf8]
}

DataRecordBatchMetadata {
  length: 3,
  is_end_of_group: true,
  nodes: ...,
  buffers: ...
}

record batch {
  group_id: [7, 7, 8],
  row_id:   [1, 2, 1],
  group_id: ["a", "b", "c"],
  value:    ["left", "right", "only"]
}
```

The third column is user data and deliberately has the same name as the reserved group ID column. Correlation
columns are identified by their positions—the first column is the group ID and the second is the row ID—not by
field names. `DataRecordBatchMetadata` contains only batch metadata; the column buffer bytes are transported
separately according to `buffer_transport`.

With only a row ID, the row ID is the first column:

```text
DataSchema {
  has_group_id: false,
  has_row_id: true,
  fields: [row_id: uint64, value: utf8]
}

record batch {
  row_id: [1, 2],
  value:  ["left", "right"]
}
```

A batch without either correlation column has no reserved prefix:

```text
DataSchema {
  has_group_id: false,
  has_row_id: false,
  fields: [value: utf8]
}

record batch {
  value: ["left", "right"]
}
```

## Group and Row ID Semantics

Groups may span multiple rows. Group IDs and row IDs are independent opaque identifiers: row IDs may restart at any
value when the group changes or continue across groups. Equal or consecutive row-ID values in different groups do
not imply any relationship between those groups. The meaningful association is the `(group_id, row_id)` pair at one
record position.

In the `UDFRunner` to `DB` direction, both the group ID and row ID are references to the inbound `(group_id, row_id)`
pair from `DB` to `UDFRunner`; they are not newly allocated output identifiers. This applies to both `RETURNS` and
`EMITS` UDFs.

`DataRecordBatchMetadata.is_end_of_group` marks whether the group identified by the batch's final row is complete. It
is defined only for a `Run` direction whose schema sets `has_group_id` to `true`:

- `true` means no later batch in that direction contains the trailing group;
- `false` means the trailing group continues in a later batch;
- a change in group ID still delimits each non-trailing group within the same batch; and
- an empty batch does not complete a group.

This is a group-boundary marker, not an end-of-stream marker. Generic stream-completion semantics remain a low-level
open question and are not encoded in `DataRecordBatchMetadata`.

## Record Batch Mappings

For `DB` to `UDFRunner`, group IDs are usually repeated because a group contains multiple rows, while row IDs are
usually ordered and often consecutive. The following input contains two groups in one record batch. The groups may
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

The `range_run` run ends align with the group ends, even though the row IDs continue from 102 to 103. The
consecutive values enable compression within each group-sized run, but row IDs 102 and 103 have no semantic
relationship merely because they are adjacent or belong to different groups. Fusion does not change the correlation
values.

For `UDFRunner` to `DB`, the output references the inbound rows by the same pair:

```text
inbound rows:  (group_id, row_id) = [(10, 101), (10, 102), (20, 103)]

RETURNS output (one output per input row):
  group_id: [10, 10, 20]
  row_id:   [101, 102, 103] -> range_run(run_ends=[2, 3], values=[101, 103])

EMITS output (multiple outputs for input row (10, 101)):
  group_id: [10, 10, 10, 20]
  row_id:   [101, 101, 101, 103] -> RunEndEncoded(run_ends=[3, 4], values=[101, 103])
```

For `RETURNS`, `UDFRunner` usually produces exactly one output row per input row or group, so row-ID references are
usually ordered and group IDs are usually repeated. For `EMITS`, a group usually contains multiple output rows and
multiple output rows usually reference the same input row, so both group IDs and row-ID references are usually
repeated.

## Preferred Encodings

| Column and direction | Preferred encoding | Compatible fallback |
| --- | --- | --- |
| Group ID, either direction | [`RunEndEncoded`](https://arrow.apache.org/docs/format/Columnar.html#run-end-encoded-layout) over unsigned 64-bit IDs when groups contain repeated rows. | Plain unsigned 64-bit. |
| Row ID, `DB` to `UDFRunner` | [`exasol.udf.range_run`](range_run.md) extension array. | Plain unsigned 64-bit. |
| Row ID, `UDFRunner` to `DB`, `RETURNS` UDF | [`exasol.udf.range_run`](range_run.md) extension array. | Plain unsigned 64-bit. |
| Row ID, `UDFRunner` to `DB`, `EMITS` UDF | [`RunEndEncoded`](https://arrow.apache.org/docs/format/Columnar.html#run-end-encoded-layout) over unsigned 64-bit IDs. | Plain unsigned 64-bit. |

Plain unsigned 64-bit encoding remains the compatible fallback when the preferred encoding is unavailable or not
beneficial. The detailed `exasol.udf.range_run` storage encoding is specified in [range_run.md](range_run.md).
