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

### Calls and callbacks

`req~udf-v2-calls-and-callbacks~1`

The protocol shall support `Run`, Function operations, script metadata, connection lookup, and script lookup, with
an extension path for future callback operations.

Needs: dsn

Covers:
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
