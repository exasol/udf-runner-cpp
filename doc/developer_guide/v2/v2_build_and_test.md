# v2 Build and Test Guide

The v2 implementation is under
[`udf-runner-cpp/v2`](../../../udf-runner-cpp/v2). Its Bazel module is defined by
[`MODULE.bazel`](../../../udf-runner-cpp/v2/MODULE.bazel), and its targets are in
[`BUILD.bazel`](../../../udf-runner-cpp/v2/BUILD.bazel).

The v2 module is built independently with Bazel 8.3.1 in CI.

v2 is currently validated by the native Bazel workflow and is not included in
the checked-in `test-udf-runner-cpp-v1` SLC flavor.

## Build and test

Run the v2 tests from the v2 module directory:

```bash
cd udf-runner-cpp/v2
bazel build --verbose_failures //...
bazel test //...
```

The test suite covers the FlatBuffers protocol, Arrow support, JSON schemas,
queue implementations, and fuzz-target regression tests. For fuzzing-specific
commands, see the [v2 fuzzing guide](v2_fuzzing.md).

## Benchmarks

Build and run the waitable-queue benchmark with:

```bash
cd udf-runner-cpp/v2
bazel run //:waitable_queue_benchmark
```

Benchmark results are diagnostic measurements. CPU frequency, scheduler
activity, build mode, and system load can affect the results, so benchmarks
must not be used as deterministic pass/fail tests.

For static analysis and formatting checks, see the [v2 code-quality
guide](v2_code_quality.md).
