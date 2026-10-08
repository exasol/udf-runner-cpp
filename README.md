# udf-runner-cpp

This repository contains the extracted `exaudfclient` C++ code and related build tooling from `exasol/script-languages`.

The current implementation lives under `udf-runner-cpp/v1`.

## UDF protocol v2 simulators

The C++ database-client and UDF-runner simulators live under
[`tools/simulators`](tools/simulators). They exchange v2 FlatBuffers frames and
inline Arrow-compatible RecordBatch buffers over a Unix domain stream socket.
They build bindings from the v2 protocol schema with Bazel; no
Python, PyArrow, `flatc`, or FlatBuffers runtime is needed in the SLC.

```bash
cd tools/simulators
bazel build //:client //:udf_runner
bazel test //...
```

The simulator's Bazel module reads the neighboring `udf-runner-cpp/v2` sources
through its own minimal build wrapper; the v2 BUILD and MODULE files remain
unchanged. When `udf_protocol.fbs` changes, Bazel regenerates the C++ bindings
as part of the build, using its FlatBuffers compiler dependency. There are no
checked-in generated simulator files to update.

Start the runner simulator in one terminal, then the client simulator in another:

```bash
tools/simulators/bazel-bin/udf_runner /tmp/udf-v2-simulator.sock
tools/simulators/bazel-bin/client /tmp/udf-v2-simulator.sock
```

Each command accepts an optional scenario path and timeout in seconds as
positional arguments: `SOCKET_PATH [SCENARIO.json] [TIMEOUT_SECONDS]`.
A scenario is a JSON array of ordered `send` and `expect` steps. Each step has
`action`, `kind`, and `stream_id`. Send steps may specify `call_name`, `payloads`
(a map of string names to string values), `byte_budget`, `schema`,
`has_group_id`, `has_row_id`, `is_end_of_group`, and `columns`. A column has
`name`, `type`, `nullable`, and `values`; supported types are signed and unsigned
8/16/32/64-bit integers, `float64`, `bool`, `utf8`, and `binary`. Expect steps
may check `call_name`, `rows`, `columns`, `payloads`, `byte_budget`, and
`is_end_of_group`. Binary `values` are Base64 strings.
Send steps may also attach `error` and `error_code` to a close message.
Set `"batch": false` on a `schema` send step to announce the schema without
sending its first batch.
With no scenario file, the two commands run a two-batch `Run` exchange,
including `Next` flow control and orderly shutdown. Example scalar-type
scenarios are in `tools/simulators/tests`. The runner binds its socket path
and removes it after its run; it refuses to replace an existing file there.
The minimal wire implementation limits frames to 16 MiB, individual buffers
to 64 MiB, batches to 128 MiB, and batches to one million rows; larger inputs
are reported as protocol errors.

To simulate the database client inside an SLC, copy the built `client` binary
to `/exaudf/exaudfclient` in an SLC with a compatible architecture and C++
runtime. The binary requires no Python packages or separate FlatBuffers/Arrow
shared libraries. The database must pass a Unix-socket path compatible with
the simulator's CLI; its URI handling may need adapting. Check the binary's
remaining system-library requirements with `ldd` in the target SLC.
