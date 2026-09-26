# UDF Protocol v2 Design Overview

This document is the entry point for the v2 protocol design. Normative requirements live in
[requirements.md](requirements.md); detailed design coverage is distributed across the low-level and high-level
documents below.

## Design artifacts

- [Low-level protocol](low_level/protocol.md): framing, streams, lifecycle, and transport.
- [Call lifecycle](low_level/call_lifecycle.md): generic call state and nested calls.
- [Data stream](low_level/data_stream.md): schemas, batches, buffers, and flow control.
- [Transport](low_level/transport.md): transport alternatives and operational constraints.
- [High-level calls](high_level/calls.md): Run, Function, callback, and scheduling behavior.
- [Payload contracts](high_level/payloads.md): named payloads and JSON contracts.
- [Correlation](high_level/correlation.md): group and row identity in record batches.
- [Type mapping](high_level/type_mapping.md): Exasol-to-Arrow physical and logical mapping.

## Architecture proposition

`DB` and `UDFRunner` communicate over a length-framed, transport-independent protocol. The first binding uses
Unix-domain stream sockets. FlatBuffers carry control and metadata, while Arrow-compatible record batches carry table
data. Multiple logical streams share one connection, and explicit byte budgets prevent unbounded in-flight data.

The current implementation provides the FlatBuffers frame schema, defensive frame verification, JSON schema
validation, and representative tests. Full call scheduling, socket transport, remote security, and optional
out-of-band buffers remain design-stage work.

## Design decisions

- Stream identity is `(connection, stream_id)`; stream IDs are not reused on a connection.
- `stream_id = 0` is the control stream; non-zero streams carry calls.
- `CloseCall` is unilateral; `CloseConnection` is a two-way connection shutdown exchange.
- Inline buffers are the portable baseline; Unix descriptor handoff is optional.
- Remote TCP bindings require TLS, endpoint authentication, and peer identity validation.

Open questions remain in the detailed documents and are intentionally not presented as implemented behavior.

## Implemented frame/schema boundary

`dsn~udf-v2-frame-schema-implementation~1`

The current v2 implementation defines and verifies FlatBuffers frames and validates the JSON payload schemas used by
the protocol. These are the implemented portions of the broader protocol design.

Covers:
- `req~udf-v2-framing-and-serialization~1`
- `req~udf-v2-defensive-validation~1`

Needs: impl, utest
