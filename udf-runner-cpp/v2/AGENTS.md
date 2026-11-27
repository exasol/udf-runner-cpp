# v2 agent instructions

Read the canonical [v2 developer guide](../../doc/developer_guide/v2/v2.md)
before making v2 changes. It contains the build, test, coverage, code-quality,
fuzzing, and dependency-isolation guidance.

v2 is the primary development area for new features and behavior. Use the
developer guide for ordinary development unless the user explicitly requests
work on v1 and confirms that v1 may be changed.

## Agent-specific fuzzing behavior

Local fuzzing must always be time-bounded. Use a short duration for exploratory
runs, or exactly the duration requested by the user. Do not run fuzzing
indefinitely, use the CI campaign duration locally, or run all targets unless
the user explicitly asks for that. Run one target at a time by default.

## Protocol and runner design boundaries

When documenting runner or context behavior, treat the protocol documents in
`doc/design/v2/protocol/` as the source of truth for wire-level rules. Link to
the relevant protocol sections and keep runner-specific implementation policy
in the runner design documents. Do not change protocol rules as part of runner
design work unless a deliberate protocol change is explicitly requested; when
such a change is requested, update the affected protocol and runner documents
together.
