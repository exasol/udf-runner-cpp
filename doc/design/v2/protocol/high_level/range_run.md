# `exasol.udf.range_run` Extension Type

`exasol.udf.range_run` is an Arrow extension type for row IDs that form consecutive ranges. In the `UDFRunner` to
`DB` direction, these row IDs are references to input row IDs, not newly allocated output-row sequences. It is used
for row-ID columns in the directions listed in [calls.md](calls.md#run). Row IDs are opaque and independent of group
IDs; a consecutive range across a group boundary is a compression opportunity only and does not create a semantic
relationship between the groups.

- `DB` to `UDFRunner`;
- `UDFRunner` to `DB` for a `RETURNS` UDF.

## Storage Type

The extension uses Arrow [`RunEndEncoded`](https://arrow.apache.org/docs/format/Columnar.html#run-end-encoded-layout)
storage with exactly two children, in this order:

1. `run_ends`: a signed 64-bit integer array containing exclusive cumulative run ends;
2. `values`: an unsigned 64-bit integer array containing the first row ID of each run.

For a run beginning at logical row ID `s` and ending before logical position `e`, the corresponding logical row IDs
are `s, s + 1, ..., s + (e - previous_end) - 1`. The first run uses `previous_end = 0`. The number of entries in
`run_ends` and `values` is identical, and the final run end is the logical length of the row-ID array.
When a row-ID column is carried together with group IDs, runs end at group boundaries, even if the numeric row IDs
continue consecutively into the next group.

The field metadata contains:

```text
ARROW:extension:name = "exasol.udf.range_run"
```

In protocol version 1, omit `ARROW:extension:metadata`; the extension name fully defines the encoding.

## Examples

The logical row IDs:

```text
[1, 2, 3, 7, 8]
```

are encoded as:

```text
run_ends: [3, 5]
values:   [1, 7]
```

The first run has length `3` and starts at row ID `1`, producing `[1, 2, 3]`. The second run has length `2` and
starts at row ID `7`, producing `[7, 8]`.

A single consecutive range such as `[42, 43, 44]` is encoded as:

```text
run_ends: [3]
values:   [42]
```

The physical Arrow buffers and the enclosing `DataRecordBatch` metadata are transported according to the generic
data-stream rules; this page defines only the logical extension-type encoding.
