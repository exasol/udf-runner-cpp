# SLC Development and CI

## Local SLC development

Use [`exaslct`](https://docs.exasol.com/db/latest/database_concepts/udf_scripts/adding_new_packages_script_languages.htm)
for local SLC development. Install the project’s Poetry dependencies and
ensure Docker is available:

```bash
poetry install
```

If no Docker daemon is available on the host, use the repository’s Lima
instance:

```bash
limactl start ./ext/lima_vm_templates/docker-udf-client.yaml
export DOCKER_HOST="$(limactl list docker-udf-client \
  --format 'unix://{{.Dir}}/sock/docker.sock')"
```

After setting `DOCKER_HOST`, run normal `exaslct` commands from the host. The
Lima template provides Bazel, Protobuf, ZeroMQ, SWIG, Python, Poetry, and the
native dependency environment used by the v1 build.

Export the checked-in v1 flavor to a local archive with:

```bash
poetry run -- exaslct export \
  --flavor-path=./flavors/test-udf-runner-cpp-v1 \
  --export-path=.build_output/slc
```

To build Docker images/stages without exporting an archive, use:

```bash
poetry run -- exaslct build \
  --flavor-path=./flavors/test-udf-runner-cpp-v1
```

Run focused database tests with a file or folder restriction. These examples
select the existing legacy test suites:

```bash
poetry run -- exaslct run-db-test \
  --flavor-path=./flavors/test-udf-runner-cpp-v1 \
  --test-container-folder=./test_container \
  --test-file=./test_container/tests/test/smoke/smoke_test.py

poetry run -- exaslct run-db-test \
  --flavor-path=./flavors/test-udf-runner-cpp-v1 \
  --test-container-folder=./test_container \
  --test-folder=./test_container/tests/test/cpp_test
```

Use `--reuse-test-environment` while iterating when appropriate. Avoid running
the full `exaslc-ci run-tests` workflow locally because it is intended for CI.

The existing tests under `test_container/tests/test/` use the legacy
[`exasol-python-test-framework`](https://github.com/exasol/exasol-python-test-framework),
which is based on `unittest`. Its dependency is retained for compatibility.

For deployment, `exaslct deploy` uploads an exported container to BucketFS and
`exaslct generate-language-activation` generates the corresponding activation
statement.

## SLC CI

The checked-in flavor is `test-udf-runner-cpp-v1`. SLC CI is managed by
`exasol-script-languages-container-ci` and generated workflows under
`.github/workflows/`. It prepares the test container, builds and scans the
flavor, and runs the configured test set.

Register test files or folders in the flavor’s `ci.json` under
`test_config.test_sets[].files` or `test_config.test_sets[].folders`.

Check that CI recognizes the registration without running the long test job:

```bash
poetry run -- exaslc-ci get-test-matrix \
  --flavor test-udf-runner-cpp-v1 \
  --github-output-var test_matrix
```

The `.github/workflows/slc_ci*.yml` files are generated artifacts and must not
be edited manually. Workflow changes belong in the supported setup tooling;
flavor-specific test changes belong in `flavors/.../ci.json` and the
test-container tree.
