#!/usr/bin/env bash
set -e
echo "=== Checking Nexus-Files ==="
cd "$(dirname "$0")"
bun test
echo "Nexus-Files checks passed!"
