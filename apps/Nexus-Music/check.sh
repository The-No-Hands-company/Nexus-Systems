#!/usr/bin/env bash
set -e
echo "=== Checking Nexus-Music ==="
cd "$(dirname "$0")"
bun test
echo "Nexus-Music checks passed!"
