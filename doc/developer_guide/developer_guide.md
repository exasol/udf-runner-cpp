# Developer Guide

This guide is for contributors working on `udf-runner-cpp`, the extracted C++
runner from `exasol/script-languages`. The active code lives under
[`udf-runner-cpp/v1`](../../udf-runner-cpp/v1), and the repository is organized
around Bazel modules and helper scripts for local development.

## Repository Layout

- [`udf-runner-cpp/v1/BUILD`](../../udf-runner-cpp/v1/BUILD) contains the main
  Bazel targets for the runner binaries.
- [`udf-runner-cpp/v1/base`](../../udf-runner-cpp/v1/base) contains the shared
  Bazel module and most of the reusable implementation code.
- [`udf-runner-cpp/v1/benchmark_container`](../../udf-runner-cpp/v1/benchmark_container),
  [`udf-runner-cpp/v1/streaming_container`](../../udf-runner-cpp/v1/streaming_container),
  and [`udf-runner-cpp/v1/test_container`](../../udf-runner-cpp/v1/test_container)
  provide optional VM surfaces used when the corresponding Bazel defines are
  enabled.
- [`udf-runner-cpp/v1/docs`](../../udf-runner-cpp/v1/docs) contains design
  notes and diagrams for the script option parser and runner internals.

## Prerequisites

The repository is built with Bazel and expects the following toolchain versions:

- `bazel-7.2.1`
- `swig-2.0.4` or `swig-3.0.12`
- `protobuf 3.12.4` with matching compiler and runtime libraries
- `zmq 4.3.4`

The retained runner modes in this repository do not require additional
language-specific toolchains.

For local builds, copy [`udf-runner-cpp/v1/.env.template`](../../udf-runner-cpp/v1/.env.template)
to `.env` and fill in the discovery prefixes for the native dependencies. The
template also exposes `VERBOSE_BUILD` for extra Bazel output.

## Build

The main entry points are the wrapper scripts in `udf-runner-cpp/v1`:

- [`build.sh`](../../udf-runner-cpp/v1/build.sh) runs `bazel build`.
- [`build_local.sh`](../../udf-runner-cpp/v1/build_local.sh) sources `.env` and
  forwards arguments to `build.sh`.
- [`build_local_all.sh`](../../udf-runner-cpp/v1/build_local_all.sh) adds the
  `no-tty` and `slow-wrapper` Bazel configs that are useful for a full local
  build.

Common build targets:

- `//:udf_runner_cpp_v1_gen` produces the default `udf_runner_cpp_v1` binary.
- `//:udf_runner_cpp_v1_static_gen` produces the static variant used to verify
  linker-namespace behavior.

Example local build:

```bash
cd udf-runner-cpp/v1
./build_local.sh --config no-tty --config fast-binary //:udf_runner_cpp_v1_gen
```

If you are working in the containerized setup, use `build.sh` directly and pass
the required Bazel flags and targets on the command line.

## Run

The runner wrapper scripts follow the same pattern as the build scripts:

- [`run.sh`](../../udf-runner-cpp/v1/run.sh) runs `bazel run`.
- [`run_local.sh`](../../udf-runner-cpp/v1/run_local.sh) sources `.env`, enables
  verbose Bazel output, and runs the retained benchmark and bash VMs by
  default.

For local debugging, start with `run_local.sh` and pass the Bazel target you
want to execute.

## Test

Use Bazel for unit and integration tests:

```bash
cd udf-runner-cpp/v1
bazel test //...
```

The repository includes tests for the script option parser and the extracted
`exaudflib` components under `udf-runner-cpp/v1/base/.../test`.

When you need the retained VM surfaces, enable the corresponding Bazel defines:

- `--define bash=true`
- `--define benchmark=true`
- `--define test_vm=true`

## Architecture Notes

The runner is split into two linker namespaces:

- The primary runner namespace loads the executable and optional VM surfaces.
- `libexaudflib.so` is loaded separately and owns the ZeroMQ and Protobuf
  dependencies used for database communication.

This separation is intentional. Do not add direct ZeroMQ or Protobuf
dependencies to the top-level runner unless the change is specifically meant to
test or preserve namespace isolation.

## Working Guidelines

- Keep changes targeted to the relevant Bazel target or module.
- Update or add tests when you touch parsing, loading, or namespace-sensitive
  code.
- Prefer the existing scripts and Bazel targets over ad hoc commands.
- Read the notes in [`udf-runner-cpp/v1/docs`](../../udf-runner-cpp/v1/docs)
  when changing parser or runner behavior, especially the script option
  parser design documents.
