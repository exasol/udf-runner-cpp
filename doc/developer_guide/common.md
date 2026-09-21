# Common Development Guide

This repository contains the extracted C++ runner from
`exasol/script-languages`. It contains two development surfaces:

- [`udf-runner-cpp/v1`](../../udf-runner-cpp/v1) — the legacy runner.
- [`udf-runner-cpp/v2`](../../udf-runner-cpp/v2) — the v2 protocol and support
  libraries.

## Prerequisites

Install Python 3.10 through 3.13, Poetry 2.3 or newer, and a supported Bazel
installation. Install the project dependencies with:

```bash
poetry install --with dev
```

Use the Bazel version and native dependencies required by the specific [v1
guide](v1.md) or [v2 guide](v2/v2.md).

## Nox sessions

List available repository tasks with:

```bash
poetry run nox -l
```

Run a task with `poetry run nox -s <session>`. Repository-wide examples
include release validation and preparation, and the v2 quality checks described
in the [v2 guide](v2/v2.md).

## Release process

1. Create an issue to prepare the release.
2. Run the `prepare-release` Nox session with the intended version.
3. Commit the documentation and changelog to a developer branch and create a
   pull request.
4. Submit the pull request for approval.
5. Merge it into `main`.
6. Create and push the release tag.

## Working guidelines

- Keep changes targeted to the relevant version, Bazel target, or module.
- Update or add tests when changing parsing, loading, protocol, or
  namespace-sensitive code.
- Prefer the existing Poetry, Nox, and Bazel entry points over ad hoc commands.
- Keep generated files and build output out of commits.
