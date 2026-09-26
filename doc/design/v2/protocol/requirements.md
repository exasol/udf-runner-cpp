# UDF Protocol v2 Requirements

This document is the normative requirement source for the UDF Protocol v2 design. The linked detailed design
documents provide the `dsn` coverage for these requirements.

## Features

### Protocol communication

`feat~udf-v2-communication~1`

The protocol provides efficient, evolvable communication between `DB` and `UDFRunner`.

Needs: req

### Protocol execution

`feat~udf-v2-execution~1`

The protocol supports UDF execution, callbacks, table transfer, and correlation of input and output data.

Needs: req

### Protocol deployment

`feat~udf-v2-deployment~1`

The protocol supports safe local and remote deployment with controlled resource usage.

Needs: req

## Functional requirements

### Run call

`req~udf-v2-run-call~1`

The protocol shall support a DB-opened `Run` call with call metadata, column metadata, and an attached bidirectional
data stream.

Needs: dsn

Covers:
- `feat~udf-v2-execution~1`

### Default output columns call

`req~udf-v2-default-output-columns-call~1`

The protocol shall support the DB-opened `default_output_columns` call, which returns the default output-column
definition for the script.

Needs: dsn

Covers:
- `feat~udf-v2-execution~1`

### Virtual schema adapter call

`req~udf-v2-virtual-schema-adapter-call~1`

The protocol shall support the DB-opened `virtual_schema_adapter` call, which accepts a virtual-schema request and
returns the adapter result.

Needs: dsn

Covers:
- `feat~udf-v2-execution~1`

### Generate SQL for import specification call

`req~udf-v2-generate-sql-for-import-spec-call~1`

The protocol shall support the DB-opened `generate_sql_for_import_spec` call, which accepts an import specification
and returns generated SQL.

Needs: dsn

Covers:
- `feat~udf-v2-execution~1`

### Generate SQL for export specification call

`req~udf-v2-generate-sql-for-export-spec-call~1`

The protocol shall support the DB-opened `generate_sql_for_export_spec` call, which accepts an export specification
and returns generated SQL.

Needs: dsn

Covers:
- `feat~udf-v2-execution~1`

### Cleanup call

`req~udf-v2-cleanup-call~1`

The protocol shall support a DB-opened, non-streaming `cleanup` call between calls so `UDFRunner` can release
resources retained from completed calls or nested calls.

Needs: dsn

Covers:
- `feat~udf-v2-execution~1`

### Get-connection callback

`req~udf-v2-get-connection-callback~1`

The protocol shall support a `UDFRunner`-opened `get_connection` nested callback that returns the requested
connection information while its parent call remains active.

Needs: dsn

Covers:
- `feat~udf-v2-execution~1`

### Get-script callback

`req~udf-v2-get-script-callback~1`

The protocol shall support a `UDFRunner`-opened `get_script` nested callback that returns script content while its
parent call remains active.

Needs: dsn

Covers:
- `feat~udf-v2-execution~1`

### Call extensibility

`req~udf-v2-call-extensibility~1`

The generic call mechanism shall allow future DB- or `UDFRunner`-opened call types to be added without redesigning
call opening, payload exchange, stream ownership, or closure semantics.

Needs: dsn

Covers:
- `feat~udf-v2-communication~1`
- `feat~udf-v2-execution~1`

### Callback extensibility

`req~udf-v2-callback-extensibility~1`

The callback mechanism shall allow future nested operations, such as `execute_query`, to be added without creating a
separate callback transport or changing the generic nested-call mechanism.

Needs: dsn

Covers:
- `feat~udf-v2-communication~1`
- `feat~udf-v2-execution~1`

### Logical streams

`req~udf-v2-logical-streams~1`

The protocol shall multiplex multiple logical streams on a connection, preserve total ordering within a stream, and
allow partial ordering between streams. A stream identity shall be scoped to its physical connection.

Needs: dsn

Covers:
- `feat~udf-v2-communication~1`
- `feat~udf-v2-execution~1`

### Flow-controlled data transfer

`req~udf-v2-flow-controlled-data~1`

Table transfer shall use explicit flow control, including `Next(...)` byte budgets, and shall not permit unbounded
data buffering by either endpoint.

Needs: dsn

Covers:
- `feat~udf-v2-execution~1`
- `feat~udf-v2-deployment~1`

### Correlation and type mapping

`req~udf-v2-data-contract~1`

Run data shall define deterministic group and row correlation, schema traversal, Exasol-to-Arrow physical mapping,
and extension metadata.

Needs: dsn

Covers:
- `feat~udf-v2-execution~1`

## Communication requirements

### Framing and serialization

`req~udf-v2-framing-and-serialization~1`

The protocol shall use length-framed FlatBuffers control metadata and Arrow-compatible record-batch metadata with
well-defined inline buffer ordering.

Needs: dsn

Covers:
- `feat~udf-v2-communication~1`

### Evolvable metadata

`req~udf-v2-evolvable-metadata~1`

Protocol, script, call, callback, and schema metadata shall be independently extensible and versioned without
requiring unrelated protocol components to change.

Needs: dsn

Covers:
- `feat~udf-v2-communication~1`

### Lifecycle and liveness

`req~udf-v2-lifecycle-and-liveness~1`

The protocol shall define connection and call closure, error handling, keepalive behavior, peer failure detection,
and behavior for late messages.

Needs: dsn

Covers:
- `feat~udf-v2-communication~1`
- `feat~udf-v2-deployment~1`

## Deployment and security requirements

### Transport bindings

`req~udf-v2-transport-bindings~1`

The initial binding shall use Unix-domain stream sockets while preserving the same framing and protocol semantics for
future TCP/TLS bindings. Inline buffers shall work on every binding.

Needs: dsn

Covers:
- `feat~udf-v2-deployment~1`

### Peer security

`req~udf-v2-peer-security~1`

Remote bindings shall provide authentication, encryption, peer identity validation, and protection for sensitive
metadata and table data.

Needs: dsn

Covers:
- `feat~udf-v2-deployment~1`

### Defensive validation

`req~udf-v2-defensive-validation~1`

Endpoints shall validate frame lengths, schemas, metadata, stream state, buffer references, and resource ownership
before using received data.

Needs: dsn

Covers:
- `feat~udf-v2-deployment~1`

### Resource safety

`req~udf-v2-resource-safety~1`

The protocol shall avoid send/receive deadlocks and shall bound or clean up buffers, file descriptors, memory, and
other resources across normal and abnormal termination.

Needs: dsn

Covers:
- `feat~udf-v2-deployment~1`

## Design status

The requirements above describe the target protocol. Open questions and future calls remain explicitly marked in the
detailed design documents. Current implementation and test coverage is provided for the frame/schema subset; the
remaining protocol behavior is design work and must not be represented as implemented coverage.
