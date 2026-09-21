# v2 Bazel Fuzzing

The v2 package contains libFuzzer targets for the FlatBuffers frame verifier,
each JSON schema, and queue behavior. Invalid input is a normal fuzzing
outcome; memory-safety and undefined-behavior findings remain fatal.

## Build instrumented targets

```sh
cd udf-runner-cpp/v2
bazel build --config=asan-libfuzzer \
  //:frame_fuzz_test_bin \
  //:call_metadata_fuzz_test_bin \
  //:connection_information_fuzz_test_bin \
  //:export_specification_fuzz_test_bin \
  //:import_specification_fuzz_test_bin \
  //:queue_fuzz_test_bin
```

## Run a fuzzing campaign

Run a target through the rules_fuzzing launcher. Generated corpus entries and
crash artifacts are stored below `/tmp/fuzzing` by default:

```sh
bazel run --config=asan-libfuzzer //:frame_fuzz_test_run -- \
  --timeout_secs=60
```

The available fuzz targets are discovered from Bazel with the Nox task:

```sh
poetry run -- nox --sessions=v2-fuzzing-targets
```

Run one discovered target through Nox with:

```sh
poetry run -- nox --sessions=v2-fuzzing -- \
  --target frame --timeout-secs 300
```

## Regression and replay

Run the checked-in corpus as a bounded regression test:

```sh
bazel test --config=asan-libfuzzer --test_output=errors \
  //:frame_fuzz_test \
  //:call_metadata_fuzz_test \
  //:connection_information_fuzz_test \
  //:export_specification_fuzz_test \
  //:import_specification_fuzz_test \
  //:queue_fuzz_test
```

Use `--config=asan-ubsan-libfuzzer` for combined sanitizer coverage. Use
`--config=asan-replay` with a fuzz target's `_run` launcher and
`--regression` to replay a corpus or crash input.
