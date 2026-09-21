# v2 Build and Test Guide

The v2 implementation is under
[`udf-runner-cpp/v2`](../../udf-runner-cpp/v2). Its Bazel module is defined by
[`MODULE.bazel`](../../udf-runner-cpp/v2/MODULE.bazel), and its targets are in
[`BUILD.bazel`](../../udf-runner-cpp/v2/BUILD.bazel).

## Build and test

Run the v2 tests from the v2 module directory:

```bash
cd udf-runner-cpp/v2
bazel test //...
```

The test suite covers the FlatBuffers protocol, Arrow support, JSON schemas,
queue implementations, and fuzz-target regression tests. For fuzzing-specific
commands, see the [v2 fuzzing guide](v2_fuzzing.md).

For static analysis and formatting checks, see the [v2 code-quality
guide](v2_code_quality.md).
