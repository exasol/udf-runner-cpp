# udf-runner-cpp

This repository contains the extracted `exaudfclient` C++ code and related build tooling from `exasol/script-languages`.

The current implementation lives under `udf-runner-cpp/v1`.

## Design specifications

The repository's [write-spec skill](.agents/skills/write-spec/SKILL.md) guides the creation and revision of design specifications.

## v2 logging

Set the `UDF_LOG_LEVEL` environment variable to choose which messages are enabled:

| Value | Enabled messages |
| --- | --- |
| `1` | Warning, error |
| `2` | Warning, error, notice |
| `3` | Warning, error, notice, debug |
| `4` | All of the above, plus verbose traces in Debug builds |

Unset or invalid values default to `1`.
