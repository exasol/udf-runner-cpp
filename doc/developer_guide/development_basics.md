# Development Basics

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

Run a task with:

```bash
poetry run nox -s <session>
```

Repository-wide sessions include release validation and preparation. The v2
guide documents the version-specific quality, coverage, and fuzzing sessions.

## Release process

1. Create an issue to prepare the release.
2. Run the `prepare-release` Nox session with the intended version.
3. Commit the documentation and changelog to a developer branch and create a
   pull request.
4. Submit the pull request for approval.
5. Merge it into `main`.
6. Create and push the release tag.
