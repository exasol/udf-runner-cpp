# Agent instructions

## Repository layout

This repository contains the extracted C++ UDF runner and its Script Language
Container (SLC) integration.

- [`udf-runner-cpp/v1`](udf-runner-cpp/v1) is the retained runner and its SLC
  flavor. Change it only after asking the user for confirmation.
- [`udf-runner-cpp/v2`](udf-runner-cpp/v2) is the newer, independently built
  Bazel module and the primary development area.
- [`flavors/test-udf-runner-cpp-v1`](flavors/test-udf-runner-cpp-v1) defines the
  SLC build and smoke-test workflow for v1.

For version-specific native development instructions, read:

- [`udf-runner-cpp/v1/AGENTS.md`](udf-runner-cpp/v1/AGENTS.md)
- [`udf-runner-cpp/v2/AGENTS.md`](udf-runner-cpp/v2/AGENTS.md)

Normal feature development should target v2. Before changing anything under
v1, ask the user for confirmation. This applies to v1 source code, Bazel
configuration, tests, SLC flavor files, test-container integration, runtime
behavior, and documentation. Do not infer permission to change v1 merely
because a task concerns the repository or the UDF runner. After confirmation,
keep v1 changes limited to the confirmed scope.

## Ticket, PR, and changelog workflow

Every change must have an associated GitHub ticket before implementation. Use
the ticket as the source of scope and acceptance criteria.

Use this exact PR title format:

```text
#<ticket-number>: <short PR title>
```

The PR description must link the ticket with `Fixes #<ticket-number>` or
`Closes #<ticket-number>`, describe what changed, and list the validation that
was run. Mention any checks that could not be run and explain why. A useful
minimum structure is:

```markdown
## Summary

- Describe the changes.

## Validation

- List tests and checks run.
- Mention unavailable checks and why.

Fixes #123
```

Every ticket addressed by a PR must also be mentioned in
[`doc/changes/unreleased.md`](doc/changes/unreleased.md), with the ticket
number and a concise description under the appropriate section:

- `Bug Fixes`
- `Features / Enhancements`
- `Refactorings`
- `Internal`

For a PR addressing multiple tickets, use the primary ticket in the PR title,
link every ticket in the PR description, and include every ticket in the
changelog. Do not omit a changelog entry because a change is documentation-only
or internal; use the appropriate category. The release tooling consumes the
unreleased changelog, so update it before the PR is merged.

## SLC development

Use `exaslct` for local SLC development. Install the project’s Poetry
dependencies and ensure Docker is available:

```bash
poetry install
```

If no Docker daemon is available on the host, use the repository’s Lima
instance. It starts a Docker daemon with the dependencies needed for local
SLC and v1 development:

```bash
limactl start ./ext/lima_vm_templates/docker-udf-client.yaml
export DOCKER_HOST="$(limactl list docker-udf-client \
  --format 'unix://{{.Dir}}/sock/docker.sock')"
```

After setting `DOCKER_HOST`, run the normal `exaslct` commands from the host.
The Lima template provides Bazel, Protobuf, ZeroMQ, SWIG, Python, Poetry, and
the native dependency environment variables used by the v1 build.

Export the checked-in v1 flavor to a local archive with:

```bash
poetry run -- exaslct export \
  --flavor-path=./flavors/test-udf-runner-cpp-v1 \
  --export-path=.build_output/slc
```

If you only need the Docker images/stages during development and do not need a
deployable archive, use `build` instead:

```bash
poetry run -- exaslct build \
  --flavor-path=./flavors/test-udf-runner-cpp-v1
```

`build` builds or pulls the flavor stages into Docker without exporting the
SLC archive. Use `export` when the result must be uploaded to BucketFS or used
for deployment.

Run focused database tests locally with `run-db-test`. It can select one test
file, one test folder, or a test restriction. For example:

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
the full `exaslc-ci run-tests` workflow locally; it can take a long time and is
intended for CI.

For deployment, `exaslct deploy` uploads the exported container to BucketFS.
`exaslct generate-language-activation` generates the corresponding
`ALTER SESSION` statement when the container was uploaded separately.

## SLC CI

The checked-in flavor is `test-udf-runner-cpp-v1`. SLC CI is managed by
`exasol-script-languages-container-ci` and the generated workflows under
`.github/workflows/`. The workflow performs these stages:

1. Prepare the SLC test container.
2. Build and scan the selected flavor with
   `exaslc-ci export-and-scan-vulnerabilities`.
3. Run the configured test set with `exaslc-ci run-tests`.

Do not normally run the final test stage locally. The CI workflow invokes it
with the selected test set, for example:

```bash
poetry run -- exaslc-ci run-tests \
  --flavor test-udf-runner-cpp-v1 \
  --docker-user "$DOCKER_USERNAME" \
  --docker-password "$DOCKER_PASSWORD" \
  --test-set-name smoke \
  --slc-directory "$SLC_DIRECTORY" \
  --commit-sha "$COMMIT_SHA"
```

### Adding tests to SLC CI

Add pytest tests below:

```text
test_container/tests/test/<test-folder>/
```

Then register them in the flavor’s `ci.json` under
`test_config.test_sets[].files` or `test_config.test_sets[].folders`:

```json
{
  "name": "smoke",
  "files": [],
  "folders": ["smoke", "cpp_test", "new_test"],
  "goal": "base_test_build_run",
  "generic_language_tests": []
}
```

Use `folders` to include all tests below a directory and `files` to select
individual test files. Adding a test file without registering its file or
folder does not make CI execute it.

Check that CI recognizes the registration without running the long test job:

```bash
poetry run -- exaslc-ci get-test-matrix \
  --flavor test-udf-runner-cpp-v1 \
  --github-output-var test_matrix
```

Confirm that the expected test set, runner, and `base_test_build_run` goal are
present in the generated matrix. The full `exaslc-ci run-tests` invocation is
performed by CI.

The `.github/workflows/slc_ci*.yml` files are generated artifacts. Never edit
them manually. Workflow changes must be made through the
`script-languages-container-ci-setup` tooling and its supported regeneration
process. Flavor-specific test changes belong in `flavors/.../ci.json` and the
test-container tree.

The v1 flavor builds the reduced runner bundle, publishes language definitions
for benchmark/streaming modes, and runs the configured lightweight smoke test.
Native Bazel tests remain necessary for full unit, sanitizer, and static/linker
verification.
