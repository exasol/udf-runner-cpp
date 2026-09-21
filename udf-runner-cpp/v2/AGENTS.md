# v2 agent instructions

v2 is the primary development area for new features and behavior. Use this
guide for ordinary development unless the user explicitly requests work on v1
and confirms that v1 may be changed.

## Build and test

The v2 module is built independently from this directory with Bazel 8.3.1:

```bash
bazel build --verbose_failures //...
bazel test --verbose_failures //...
```

The CI test command also collects coverage and generates the SonarQube coverage
report:

```bash
bazel test --verbose_failures --collect_code_coverage \
  --combined_report=lcov \
  --coverage_report_generator=@bazel_sonarqube//:sonarqube_coverage_generator \
  //...
```

## Required checks

Run the repository’s static checks before submitting v2 changes:

```bash
bazel build --verbose_failures --config clang-tidy //...
bazel build --verbose_failures --config clang-format //...
```

The Linux-only symbol-leak tests inspect shared objects and static archives
with `nm`. Preserve and extend these tests when changing exported interfaces,
linking, generated code, or third-party dependencies.

## Fuzzing

The v2 fuzz targets are listed in [`FUZZING.md`](FUZZING.md). Discover the
targets with:

```bash
poetry run -- nox --sessions=v2-fuzzing-targets
```

Local fuzzing must always be bounded. Use a short duration for exploratory
runs, or use exactly the duration requested by the user. Do not run fuzzing
indefinitely, use the CI campaign duration locally, or run all targets unless
the user explicitly asks for that. Run one target at a time by default:

```bash
poetry run -- nox --sessions=v2-fuzzing -- \
  --timeout-secs 30 \
  --target frame \
  --output-root /tmp/fuzzing
```

The `--timeout-secs` value must be changed to the user-requested duration when
one is provided. The direct Bazel launcher is also allowed for a bounded run:

```bash
bazel run --config=asan-libfuzzer //:frame_fuzz_test_run -- \
  --timeout_secs=30
```

The CI workflow runs the discovered targets as a separate bounded campaign;
normal v2 test and coverage runs exclude targets tagged `fuzz-test`.

## Third-party dependency and symbol-leak policy

Whenever a third-party dependency is added or modified, evaluate whether its
symbols, headers, macros, or runtime dependencies can leak into the public v2
library or shared-object interface. The evaluation must cover the relevant
shared and static artifacts and must result in tests or documented evidence.

If symbols can leak, take measures to prevent unintended exposure. Suitable
strategies include:

- Keep the dependency behind a private Bazel target or implementation wrapper.
- Rewrite namespaces, include paths, and include guards for vendored headers
  when the dependency can collide with a consumer’s installation.
- Compile with hidden visibility and explicitly export only the required API.
- Use linker version scripts and `--exclude-libs` to suppress archive symbols.
- Isolate the dependency in a dedicated shared library or linker namespace.
- Avoid propagating third-party dependencies through public Bazel targets.
- Add or update `nm`-based tests for dynamic symbols and static archives.

The existing private FlatBuffers runtime, rewritten generated includes, hidden
visibility, version scripts, and symbol-leak tests are examples of these
patterns. Do not treat a successful compile as sufficient evidence that a new
dependency is isolated.

## Scope

v2 is currently validated by the native Bazel workflow. It is not included in
the checked-in `test-udf-runner-cpp-v1` SLC flavor.
