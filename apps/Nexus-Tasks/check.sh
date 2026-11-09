#!/usr/bin/env bash
set -e
echo "=== Checking Nexus-Tasks ==="
cd "$(dirname "$0")"
bun test
echo "Nexus-Tasks checks passed!"
