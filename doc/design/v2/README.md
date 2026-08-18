# UDF Protocol v2 Design Documents

This directory contains the split protocol design for the new UDF protocol.

- [protocol/design_draft.md](protocol/design_draft.md) is the umbrella design draft.
- [protocol/low_level.md](protocol/low_level.md) describes the wire-level and control-stream rules.
- [protocol/call_lifecycle.md](protocol/call_lifecycle.md) describes the generic call abstraction.
- [protocol/data_stream.md](protocol/data_stream.md) is the low-level companion for call-attached
  data-stream behavior.
- [protocol/high_level_calls.md](protocol/high_level_calls.md) describes `Run`, Function operations,
  `get_connection`, `get_script`, and DB/UDFRunner scheduling policy.
- [protocol/high_level_payloads.md](protocol/high_level_payloads.md) defines their named string and JSON
  payload contracts.

Mermaid sources and rendered SVGs use matching names and scopes so the textual and visual material stays aligned.
