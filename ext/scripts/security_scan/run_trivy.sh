#!/bin/bash

set -euo pipefail

if [ $# -lt 1 ]; then
    echo "You must provide output path as argument"
    exit 1
fi

TRIVY_CACHE_LOCATION="https://dli4ip9yror05.cloudfront.net"

mkdir -p "$HOME/.cache/"
pushd "$HOME/.cache/"

curl -s -o trivy_cache.tar.gz "${TRIVY_CACHE_LOCATION}/trivy_cache.tar.gz"
tar xf trivy_cache.tar.gz

popd

output_path=$1

trivy rootfs --no-progress --offline-scan --format json --timeout 15m0s --skip-java-db-update --skip-db-update --config /trivy.yaml --output "$output_path/trivy_report.json" / > /dev/null
trivy rootfs --no-progress --offline-scan --format table --timeout 15m0s --skip-java-db-update --skip-db-update --config /trivy.yaml --output "$output_path/trivy_report.txt" / > /dev/null
trivy rootfs --no-progress --offline-scan --timeout 15m0s --skip-db-update --skip-java-db-update --config /trivy.yaml --show-suppressed --severity "HIGH,CRITICAL" --exit-code 1 /
