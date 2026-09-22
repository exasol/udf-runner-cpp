# Agent instructions

Read the canonical [developer guide](doc/developer_guide/developer_guide.md),
the [development basics](doc/developer_guide/development_basics.md), the
[v1 guide](doc/developer_guide/v1.md), or the [v2 guide](doc/developer_guide/v2/v2.md)
before making repository changes.

## Repository layout

This repository contains the extracted C++ UDF runner and its Script Language
Container (SLC) integration.

- [`udf-runner-cpp/v1`](udf-runner-cpp/v1) is the retained runner and its SLC
  flavor.
- [`udf-runner-cpp/v2`](udf-runner-cpp/v2) is the newer, independently built
  Bazel module and the primary development area.
- [`flavors/test-udf-runner-cpp-v1`](flavors/test-udf-runner-cpp-v1) defines the
  SLC build and smoke-test workflow for v1.

For version-specific agent behavior, read:

- [`udf-runner-cpp/v1/AGENTS.md`](udf-runner-cpp/v1/AGENTS.md)
- [`udf-runner-cpp/v2/AGENTS.md`](udf-runner-cpp/v2/AGENTS.md)

Normal feature development should target v2. Before changing anything under
v1, ask the user for confirmation. Do not infer permission to modify v1 merely
because a task concerns the repository or the UDF runner.

## Ticket, PR, and changelog workflow

Every change must have an associated GitHub ticket before implementation. Use
the ticket as the source of scope and acceptance criteria.

Use this exact PR title format:

```text
#<ticket-number>: <short PR title>
```

The PR description must link the ticket with `Fixes #<ticket-number>` or
`Closes #<ticket-number>`, describe what changed, and list the validation that
was run. Mention any checks that could not be run and explain why. A useful
minimum structure is:

```markdown
## Summary

- Describe the changes.

## Validation

- List tests and checks run.
- Mention unavailable checks and why.

Fixes #123
```

Every ticket addressed by a PR must also be mentioned in
[`doc/changes/unreleased.md`](doc/changes/unreleased.md), with the ticket
number and a concise description under the appropriate section:

- `Bug Fixes`
- `Features / Enhancements`
- `Refactorings`
- `Internal`

For a PR addressing multiple tickets, use the primary ticket in the PR title,
link every ticket in the PR description, and include every ticket in the
changelog. Do not omit a changelog entry because a change is documentation-only
or internal; use the appropriate category.

## Agent working guidelines

- Keep changes targeted to the relevant version, Bazel target, or module.
- Update or add tests when changing parsing, loading, protocol, or
  namespace-sensitive code.
- Prefer the existing Poetry, Nox, and Bazel entry points over ad hoc commands.
- Keep generated files and build output out of commits.
