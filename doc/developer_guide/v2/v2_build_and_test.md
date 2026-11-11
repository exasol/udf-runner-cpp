# v2 Build and Test Guide

The v2 implementation is under
[`udf-runner-cpp/v2`](../../../udf-runner-cpp/v2). Its Bazel module is defined by
[`MODULE.bazel`](../../../udf-runner-cpp/v2/MODULE.bazel), and its targets are in
[`BUILD.bazel`](../../../udf-runner-cpp/v2/BUILD.bazel).

The v2 module is built independently with Bazel 8.8.2 in CI.

v2 is currently validated by the native Bazel workflow and is not included in
the checked-in `test-udf-runner-cpp-v1` SLC flavor.

## Podman build container

For a reproducible containerized v2 build on a host without a usable rootless
Podman runtime, use the Dockerfile derived from the development Lima template:

```bash
podman build \
  --userns=host \
  --isolation=chroot \
  --network=host \
  --tag udf-runner-cpp-v2-build \
  --file ext/lima_vm_templates/udf-runner-cpp-v2.Dockerfile \
  .
```

If the host runtime directory is read-only, provide temporary Podman roots and
runtime state explicitly:

```bash
podman_runtime_dir="$(mktemp -d)"
podman_storage_dir="$(mktemp -d)"
chmod 700 "$podman_runtime_dir" "$podman_storage_dir"

XDG_RUNTIME_DIR="$podman_runtime_dir" podman \
  --runroot "$podman_runtime_dir/libpod" \
  --root "$podman_storage_dir/libpod" \
  build --userns=host --isolation=chroot --network=host \
  --tag udf-runner-cpp-v2-build \
  --file ext/lima_vm_templates/udf-runner-cpp-v2.Dockerfile \
  .
```

Build the current working tree inside the image by mounting the repository:

```bash
XDG_RUNTIME_DIR="$podman_runtime_dir" podman \
  --runroot "$podman_runtime_dir/libpod" \
  --root "$podman_storage_dir/libpod" \
  run --rm --userns=host --network=host --security-opt label=disable \
  --volume "$PWD:/workspace:Z" \
  udf-runner-cpp-v2-build build --verbose_failures //...
```

The same container can run the full v2 test suite by replacing `build` with
`test`.

## Build and test

Run the v2 tests from the v2 module directory:

```bash
cd udf-runner-cpp/v2
bazel build --verbose_failures //...
bazel test //...
```

The regular suite can also be run through Nox with the sanitizer and memory
checking modes used in CI:

```bash
poetry run nox -s v2-tests
poetry run nox -s v2-tests-asan
poetry run nox -s v2-tests-ubsan
poetry run nox -s v2-tests-valgrind
```

The CI quality checks are also available as independent Nox sessions:

```bash
poetry run nox -s v2-clang-tidy
poetry run nox -s v2-clang-format
poetry run nox -s v2-coverage
poetry run nox -s v2-sonar
```

The Sonar session expects the coverage report from `v2-coverage` to already
exist and requires a locally installed `sonar-scanner` (or a path supplied via
the `SONAR_SCANNER` environment variable).

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
