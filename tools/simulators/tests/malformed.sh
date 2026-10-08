#!/usr/bin/env bash
set -euo pipefail

runner="$1"
peer="$2"

for mode in zero_length invalid_stream truncated_buffer; do
    socket_path="/tmp/udf-v2-malformed-$$-${mode}.sock"
    "$runner" "$socket_path" &
    runner_pid=$!
    trap 'kill "$runner_pid" 2>/dev/null || true; rm -f "$socket_path"' EXIT
    for _ in {1..100}; do
        [[ -S "$socket_path" ]] && break
        sleep 0.05
    done
    [[ -S "$socket_path" ]]
    "$peer" "$socket_path" "$mode"
    if wait "$runner_pid"; then
        echo "runner accepted malformed $mode traffic" >&2
        exit 1
    fi
    [[ ! -e "$socket_path" ]]
done
