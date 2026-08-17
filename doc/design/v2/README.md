# UDF Protocol v2 Design Documents

This directory contains the split protocol design for the new UDF protocol.

- [udf_protocol_design_draft.md](udf_protocol_design_draft.md) is the umbrella design draft.
- [udf_protocol_low_level.md](udf_protocol_low_level.md) describes the wire-level and control-stream rules.
- [udf_protocol_call_lifecycle.md](udf_protocol_call_lifecycle.md) describes the generic call abstraction.
- [udf_protocol_data_stream.md](udf_protocol_data_stream.md) is the low-level companion for call-attached
  data-stream behavior.
- [udf_protocol_high_level_calls.md](udf_protocol_high_level_calls.md) describes `Run`, Function operations,
  `get_connection`, `get_script`, and DB/UDFRunner scheduling policy.
- [udf_protocol_high_level_payloads.md](udf_protocol_high_level_payloads.md) defines their named string and JSON
  payload contracts.

Mermaid sources and rendered SVGs use matching names and scopes so the textual and visual material stays aligned.
