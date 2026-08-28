#!/usr/bin/env bash
set -euo pipefail

output_dir="$(dirname "$1")"
shift
mkdir -p "$output_dir"

for source in "$@"; do
    header="$(basename "$source")"
    sed \
        -e 's#"flatbuffers/#"exasol/udf/v2/third_party/flatbuffers/#g' \
        -e 's/FLATBUFFERS_/EXASOL_UDF_V2_FLATBUFFERS_/g' \
        "$source" > "$output_dir/$header"
done
