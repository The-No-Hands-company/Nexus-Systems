#!/bin/bash
set -euo pipefail
echo -n "nexus-database... "
cargo check -p database-engine 2>&1 | tail -1
cargo test -p database-engine --lib 2>&1 | tail -1
echo "PASS"
