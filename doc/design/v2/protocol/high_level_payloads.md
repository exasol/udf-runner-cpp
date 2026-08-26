# UDF Protocol v2: High-Level Payload Contracts

This document defines the named payloads used by the current high-level protocol. Every payload is carried in
`Payloads(...)`. A scalar value uses `StringPayload` directly. A payload marked JSON uses a `StringPayload` whose
value is UTF-8 JSON conforming to its linked schema.

## Script Metadata

After the `Server` sends `ServerCapabilities`, `DB` may set the connection's script metadata on control stream
`0`. It sends these `StringPayload` values together or in separate control messages:

| Name | Value | Meaning |
| --- | --- | --- |
| `script_name` | string | Name of the script used by subsequent `Run` or Function calls. |
| `script_source` | string | Source code of that script. |

Script metadata must be sent before a `Run` or Function call that uses it. It may be replaced only between such
calls; the last received values apply to subsequent calls. It is not carried on a `Run` or Function stream.

## Call Metadata

Every `Run` and Function call carries one `call_metadata` JSON payload in the same `StreamMessage` as `OpenCall`.
It conforms to [call_metadata.schema.json](../../../../udf-runner-cpp/v2/json_schema/call_metadata.schema.json) and
supplies the per-invocation execution context and the input/output iterator and column definitions. The script name
and source are deliberately excluded because they are connection-scoped script metadata.
Column `type` values use official Exasol type families, while `type_name` carries the complete parameterized Exasol
SQL declaration. Their Arrow physical representation and metadata rules are defined in
[high_level_type_mapping.md](high_level_type_mapping.md).
The shared column-definition contract is defined in [column.schema.json](../../../../udf-runner-cpp/v2/json_schema/column.schema.json)
and is referenced by both call metadata and import specifications.

See the [call metadata example](examples/call_metadata.json).

In a column definition, `type` is the official Exasol type family constrained by the schema's `exasol_type` enum, while `type_name` is the complete SQL
declaration. Parsed parameters such as `precision` and `scale` are included where applicable. The corresponding
Arrow physical storage type and field metadata are defined by the [high-level type mapping](high_level_type_mapping.md).

The existing `size`, `precision`, and `scale` properties remain part of the column API. The following definitions show
how the additional mapped types expose their parameters without requiring the consumer to parse `type_name`:

See the [column definitions example](examples/column_definitions.json).

Unsigned 64-bit values are decimal strings so JSON implementations do not lose precision.

## Function Calls

`Function` is a family of DB-opened, non-streaming calls. Each operation uses its operation name as
`OpenCall.call_name`, carries `call_metadata`, and has the following operation-specific payload contract.

| Call name | Request payload | Result payload |
| --- | --- | --- |
| `default_output_columns` | None | `default_output_columns_result`: `StringPayload` |
| `virtual_schema_adapter` | `virtual_schema_request`: `StringPayload` | `virtual_schema_result`: `StringPayload` |
| `generate_sql_for_import_spec` | `import_specification`: JSON | `import_specification_result`: `StringPayload` |
| `generate_sql_for_export_spec` | `export_specification`: JSON | `export_specification_result`: `StringPayload` |

The virtual-schema request and all Function results retain their existing string representation. Their contents are
defined by the respective script API, not by this transport protocol.

`import_specification` conforms to
[import_specification.schema.json](../../../../udf-runner-cpp/v2/json_schema/import_specification.schema.json).
See the [import specification example](examples/import_specification.json).

`export_specification` conforms to
[export_specification.schema.json](../../../../udf-runner-cpp/v2/json_schema/export_specification.schema.json).
See the [export specification example](examples/export_specification.json).

## Nested Calls

`UDFRunner` may open these nested calls while a `Run` or Function call is active:

| Call name | Request payload | Result payload |
| --- | --- | --- |
| `get_connection` | `connection_name`: `StringPayload` | `connection_information`: JSON |
| `get_script` | `script_name`: `StringPayload` | `script`: `StringPayload` |

`connection_information` conforms to
[connection_information.schema.json](../../../../udf-runner-cpp/v2/json_schema/connection_information.schema.json).
See the [connection information example](examples/connection_information.json).

## Deferred Calls

`Script` and `execute_query` are not part of the current normative call set. They remain future extensions and
have no payload or data-stream contract in this version.
