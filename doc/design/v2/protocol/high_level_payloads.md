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

```json
{
  "database_name": "EXASOL",
  "database_version": "8.0",
  "session_id": "42",
  "statement_id": 1,
  "node_count": 1,
  "node_id": 0,
  "vm_id": "7",
  "maximal_memory_limit": "1073741824",
  "script_schema": "SYS",
  "input_iter_type": "EXACTLY_ONCE",
  "output_iter_type": "EXACTLY_ONCE",
  "input_columns": [],
  "output_columns": [],
  "single_call_mode": false
}
```

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
Example:

```json
{
  "is_subselect": true,
  "connection_name": "REMOTE_CONNECTION",
  "subselect_column_specification": [{ "name": "ID", "type_name": "DECIMAL(18,0)" }],
  "parameters": [{ "key": "encoding", "value": "UTF-8" }]
}
```

`export_specification` conforms to
[export_specification.schema.json](../../../../udf-runner-cpp/v2/json_schema/export_specification.schema.json).
Example:

```json
{
  "has_truncate": true,
  "has_replace": false,
  "source_column_names": ["ID", "NAME"],
  "connection_name": "REMOTE_CONNECTION"
}
```

## Nested Calls

`UDFRunner` may open these nested calls while a `Run` or Function call is active:

| Call name | Request payload | Result payload |
| --- | --- | --- |
| `get_connection` | `connection_name`: `StringPayload` | `connection_information`: JSON |
| `get_script` | `script_name`: `StringPayload` | `script`: `StringPayload` |

`connection_information` conforms to
[connection_information.schema.json](../../../../udf-runner-cpp/v2/json_schema/connection_information.schema.json).
Example:

```json
{
  "kind": "JDBC",
  "address": "jdbc:example://host/database",
  "user": "user",
  "password": "secret"
}
```

## Deferred Calls

`Script` and `execute_query` are not part of the current normative call set. They remain future extensions and
have no payload or data-stream contract in this version.
