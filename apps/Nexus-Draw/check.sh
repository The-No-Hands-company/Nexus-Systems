#!/bin/bash
set -euo pipefail
echo -n "$(basename $(dirname $0))... "
bun test 2>&1 | tail -1
echo "PASS"
