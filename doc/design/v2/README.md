# UDF Protocol v2 Design Documents

This directory contains the split protocol design for the new UDF protocol.

- [protocol/design_draft.md](protocol/design_draft.md) is the umbrella design draft.
- [protocol/low_level/protocol.md](protocol/low_level/protocol.md) describes the wire-level rules, generic call lifecycle, and control
  stream.
- [protocol/high_level/calls.md](protocol/high_level/calls.md) describes `Run`, Function operations,
  `get_connection`, `get_script`, and DB/UDFRunner scheduling policy.
- [protocol/high_level/payloads.md](protocol/high_level/payloads.md) defines their named string and JSON
  payload contracts.
- [protocol/high_level/type_mapping.md](protocol/high_level/type_mapping.md) is the high-level Exasol-to-Arrow
  column conversion contract, including physical types, parameters, and extension metadata.

Mermaid sources and rendered SVGs use matching names and scopes so the textual and visual material stays aligned.
The low-level schema defines reusable Arrow-compatible physical type capabilities. Exasol type selection and
logical/extension metadata are defined by `high_level/type_mapping.md`.

The JSON schemas and external examples can be validated with `poetry run nox -s validate-json-schemas`.
