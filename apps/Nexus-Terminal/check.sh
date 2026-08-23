#!/bin/bash
set -euo pipefail
echo -n "nexus-terminal... "
bun test 2>&1 | tail -1
echo "PASS"
