# v1 agent instructions

## Build

The v1 runner is built with Bazel 8.3.1 in CI. Native dependency discovery is
configured through the environment variables used by the repository workflows,
including `PROTOBUF_BIN`, `PROTOBUF_LIBRARY_PREFIX`,
`PROTOBUF_INCLUDE_PREFIX`, `ZMQ_LIBRARY_PREFIX`, and
`ZMQ_INCLUDE_PREFIX`.

From this directory, the main build targets are:

```bash
bazel build --lockfile_mode=off --config no-tty -c dbg \
  --config fast-binary --verbose_failures \
  //:udf_runner_cpp_v1_gen //:udf_runner_cpp_v1_static_gen
```

`build.sh` forwards arguments to `bazel build`; `build_local.sh` additionally
loads `.env`; and `build_local_all.sh` adds the `no-tty` and `slow-wrapper`
configurations. The generated binaries are the normal and static runner
variants. Enable optional VM surfaces only when needed, for example:

```bash
./build_local.sh --define bash=true --define benchmark=true \
  //:udf_runner_cpp_v1_gen
```

## Tests and checks

The v1 unit tests are in the `base` module. Run the standard test targets from
`udf-runner-cpp/v1/base`:

```bash
bazel test //exaudflib/test:exaudflib-test
bazel test //script_options_parser/ctpg/test:script-options-ctpg-parser-test
bazel test //script_options_parser/legacy/test:script-options-legacy-parser-test
```

The CI matrix also runs the same tests with Valgrind and ASan. Use the
corresponding `--run_under='valgrind --leak-check=yes' --config=valgrind` or
`--config=asan` options when investigating memory and leak behavior.

For a runner executable smoke check, use `base/test_udfclient.sh` with the
built executable; it verifies that the binary produces its usage output.
The SLC flavor provides the container-level smoke test described in the root
[`AGENTS.md`](../../AGENTS.md).

## Design constraints

The runner and `libexaudflib` intentionally use separate linker namespaces.
ZeroMQ and Protobuf belong to the exaudflib side. Do not add those dependencies
directly to the top-level runner or language-container targets unless the change
is specifically intended to test namespace isolation. Add or update tests when
changing loading, parsing, dependency boundaries, or VM behavior.
