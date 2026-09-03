# UDF Protocol v2: High-Level Payload Contracts

This document defines the named payloads used by the current high-level protocol. Every payload is carried in
`Payloads(...)`. A scalar value uses `StringPayload` directly. A payload marked JSON uses a `StringPayload` whose
value is UTF-8 JSON conforming to its linked schema.

## Script Metadata

After the `Server` sends `ServerCapabilities`, `DB` may set the connection's script metadata on control stream `0`
before any call is sent, between calls, or in a call-opening message. It sends these `StringPayload` values together or
in separate messages:

| Name | Value | Meaning |
| --- | --- | --- |
| `script_name` | string | Name of the script used by subsequent `Run` or Function calls. |
| `script_source` | string | Source code of that script. |

Script metadata must be received before a `Run` or Function call that uses it. Metadata sent before any call or between
calls applies to subsequent calls. Metadata sent with `OpenCall` applies to that call and subsequent calls. The latest
received value replaces the previous value; metadata is not sent during an active call.

## Call Metadata

Every `Run` and Function call uses one `call_metadata` JSON payload. It may be sent on control stream `0` before any
call is sent or between calls, or in the same `StreamMessage` as `OpenCall`. It conforms to
[call_metadata.schema.json](../../../../../udf-runner-cpp/v2/json_schema/call_metadata.schema.json) and supplies the
per-invocation execution context and iterator settings. Metadata sent before any call or between calls applies to
subsequent calls. Metadata sent with `OpenCall` applies to that call and subsequent calls. The latest received value
replaces the previous value. It is not sent during an active call.

Column definitions are carried separately in one `column_metadata` JSON payload. It conforms to
[column_metadata.schema.json](../../../../../udf-runner-cpp/v2/json_schema/column_metadata.schema.json) and may be
sent on control stream `0` before any call is sent or between calls, or in the same `StreamMessage` as `OpenCall`.
Metadata sent before any call or between calls applies to subsequent calls. Metadata sent with `OpenCall` applies to
that call and subsequent calls. The latest received value replaces the previous value. It is not sent during an active
call. Column `type` values use official Exasol type families, while `type_name` carries the
complete parameterized Exasol SQL declaration. Their Arrow physical representation and metadata rules are defined in
[type_mapping.md](type_mapping.md). The shared column-definition contract is defined in
[column.schema.json](../../../../../udf-runner-cpp/v2/json_schema/column.schema.json) and is referenced by both column
metadata and import specifications.

See the [call metadata example](examples/call_metadata.json).

The existing `size`, `precision`, and `scale` properties remain part of the column API. The following definitions show
how the additional mapped types expose their parameters without requiring the consumer to parse `type_name`:

See the [column definitions example](examples/column_definitions.json).

Unsigned 64-bit values in JSON payload bodies are decimal strings so JSON implementations do not lose precision.

## Function Calls

`Function` is a family of DB-opened, non-streaming calls. Each operation uses its operation name as
`OpenCall.call_name`, uses the applicable `call_metadata` and `column_metadata`, and has the following operation-specific payload contract.

| Call name | Request payload | Result payload |
| --- | --- | --- |
| `default_output_columns` | None | `default_output_columns_result`: `StringPayload` |
| `virtual_schema_adapter` | `virtual_schema_request`: `StringPayload` | `virtual_schema_result`: `StringPayload` |
| `generate_sql_for_import_spec` | `import_specification`: JSON | `import_specification_result`: `StringPayload` |
| `generate_sql_for_export_spec` | `export_specification`: JSON | `export_specification_result`: `StringPayload` |

The virtual-schema request and all Function results retain their existing string representation. Their contents are
defined by the respective script API, not by this transport protocol.

`import_specification` conforms to
[import_specification.schema.json](../../../../../udf-runner-cpp/v2/json_schema/import_specification.schema.json).
See the [import specification example](examples/import_specification.json).

`export_specification` conforms to
[export_specification.schema.json](../../../../../udf-runner-cpp/v2/json_schema/export_specification.schema.json).
See the [export specification example](examples/export_specification.json).

## Nested Calls

`UDFRunner` may open these nested calls only while a `Run` or Function call is active:

| Call name | Request payload | Result payload |
| --- | --- | --- |
| `get_connection` | `connection_name`: `StringPayload` | `connection_information`: JSON |
| `get_script` | `script_name`: `StringPayload` | `script`: `StringPayload` |

`connection_information` conforms to
[connection_information.schema.json](../../../../../udf-runner-cpp/v2/json_schema/connection_information.schema.json).
See the [connection information example](examples/connection_information.json).

## Deferred Calls

`ExecuteScript` and `execute_query` are not part of the current normative call set. They remain future extensions and
have no payload or data-stream contract in this version.
