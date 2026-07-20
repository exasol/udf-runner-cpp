#!/bin/bash

set -euo pipefail

if [ $# -lt 1 ]; then
    echo "You must provide output path as argument"
    exit 1
fi

output_path=$1
mkdir -p "$output_path"
echo "Security scan skipped for bootstrap flavor." > "$output_path/noop_security_scan.txt"
