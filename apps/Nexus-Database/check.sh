#!/bin/bash
# Quality gate. Runs from any cwd; invoked identically by a developer and by CI.
set -euo pipefail
cd "$(dirname "$0")"
echo "nexus-database..."

# --all-targets so test and bench code is compiled too. A bare `cargo check`
# only looks at the library's normal build, so a test module that no longer
# compiles passes this step and fails later, or never runs at all.
export CARGO_TARGET_DIR="${CARGO_TARGET_DIR:-$HOME/.cache/cargo-target}"
cargo check -p database-engine --all-targets
cargo check -p database-server --all-targets
cargo test -p database-server --test integration

echo "PASS"
