#!/usr/bin/env bash
set -euo pipefail

socket_path="/tmp/udf-v2-simulator-$$.sock"
runner="$1"
client="$2"

if [[ $# -eq 4 ]]; then
    "$runner" "$socket_path" "$3" &
else
    "$runner" "$socket_path" &
fi
runner_pid=$!
trap 'kill "$runner_pid" 2>/dev/null || true; rm -f "$socket_path"' EXIT

for _ in {1..100}; do
    [[ -S "$socket_path" ]] && break
    sleep 0.05
done
[[ -S "$socket_path" ]]

if [[ $# -eq 4 ]]; then
    "$client" "$socket_path" "$4"
else
    "$client" "$socket_path"
fi
wait "$runner_pid"
[[ ! -e "$socket_path" ]]
