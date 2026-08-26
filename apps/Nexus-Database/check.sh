#!/bin/bash
# Quality gate. Runs from any cwd; invoked identically by a developer and by CI.
set -euo pipefail
cd "$(dirname "$0")"
echo "nexus-database..."

# --all-targets so test and bench code is compiled too. A bare `cargo check`
# only looks at the library's normal build, so a test module that no longer
# compiles passes this step and fails later, or never runs at all.
cargo check -p database-engine --all-targets

# Output is not piped through `tail -1`, which reduced a failing run to its
# summary line and discarded the panic message and the assertion.
#
# Scope deliberately unchanged (--lib). Widening it was considered and backed
# out: this suite currently HANGS — btree, buffer, index and sequence unit tests
# all report "running for over 60 seconds" and never finish, with or without
# --lib — so a wider scope could not be verified, and shipping an unverified
# widening is the thing this whole pass exists to stop.
#
# Two known gaps, recorded rather than silently carried:
#   - the hang above means this gate cannot currently pass;
#   - crates/database-server has its own integration suite
#     (tests/integration.rs) that no gate runs at all.
cargo test -p database-engine --lib

echo "PASS"
