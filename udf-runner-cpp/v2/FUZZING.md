# v2 Bazel fuzzing

The v2 package contains libFuzzer targets for the FlatBuffers frame verifier
and each JSON schema. Invalid input is treated as a normal fuzzing outcome;
memory-safety and undefined-behavior findings remain fatal.

Build the instrumented targets with AddressSanitizer and libFuzzer:

```sh
bazel build --config=asan-libfuzzer \
  //:frame_fuzz_test_bin \
  //:call_metadata_fuzz_test_bin \
  //:connection_information_fuzz_test_bin \
  //:export_specification_fuzz_test_bin \
  //:import_specification_fuzz_test_bin
```

Run a target through the rules_fuzzing launcher. The launcher stores generated
corpus entries and crash artifacts below `/tmp/fuzzing` by default:

```sh
bazel run --config=asan-libfuzzer //:frame_fuzz_test_run -- \
  --timeout_secs=60
```

Run the checked-in corpus as a bounded regression test:

```sh
bazel test --config=asan-libfuzzer --test_output=errors \
  //:frame_fuzz_test \
  //:call_metadata_fuzz_test \
  //:connection_information_fuzz_test \
  //:export_specification_fuzz_test \
  //:import_specification_fuzz_test
```

Use `--config=asan-ubsan-libfuzzer` for combined sanitizer coverage or
`--config=asan-replay` with a fuzz target's `_run` launcher and
`--regression` to replay a corpus/crash input.
