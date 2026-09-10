#!/usr/bin/env bash
set -euo pipefail

# The first argument is the directory containing the generated private
# headers. All remaining arguments are the public FlatBuffers headers supplied
# by Bazel from @flatbuffers//:public_headers.
output_dir="$(dirname "$1")"
shift
mkdir -p "$output_dir"

for source in "$@"; do
    header="$(basename "$source")"
    # Use a private include prefix so ordinary flatbuffers/... headers can be
    # included in the same translation unit. Prefix every FlatBuffers macro
    # to isolate include guards and configuration macros as well.
    sed \
        -e 's#"flatbuffers/#"exasol/udf/v2/third_party/flatbuffers/#g' \
        -e 's/FLATBUFFERS_/EXASOL_UDF_V2_FLATBUFFERS_/g' \
        "$source" > "$output_dir/$header"
done
