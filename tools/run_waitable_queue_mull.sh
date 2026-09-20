#!/usr/bin/env bash

set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
readonly script_dir
repository_root="$(dirname -- "$script_dir")"
readonly repository_root

cd "$repository_root"

export USE_BAZEL_VERSION="${USE_BAZEL_VERSION:-8.3.1}"
export MULL_BAZEL_OUTPUT_ROOT="${MULL_BAZEL_OUTPUT_ROOT:-/tmp/lima/udf-runner-cpp-bazel-mull}"
export MULL_BAZEL_BUILD_JOBS="${MULL_BAZEL_BUILD_JOBS:-2}"

# Stop servers that may still reference a previous workspace-local output
# root before removing its generated files.
bazel shutdown >/dev/null 2>&1 || true
bazel \
    --output_user_root="$MULL_BAZEL_OUTPUT_ROOT" \
    shutdown >/dev/null 2>&1 || true

# Keep the Bazel output root out of stale state between Lima runs.
rm -rf -- "$MULL_BAZEL_OUTPUT_ROOT"
rm -rf -- "$repository_root/.build_output/bazel-mull"

poetry run -- nox --sessions=mull -- --target waitable_queue_test
