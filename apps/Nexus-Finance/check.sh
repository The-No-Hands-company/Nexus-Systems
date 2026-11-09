#!/usr/bin/env bash
set -e
echo "=== Checking Nexus-Finance ==="
cd "$(dirname "$0")"
bun test
echo "Nexus-Finance checks passed!"
