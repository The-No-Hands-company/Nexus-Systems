#!/bin/bash
# Quality gate for the production edge — the reverse proxy and the SSO gate.
#
# This is the highest-blast-radius code in the repository: every request to
# every host on the domain passes through it, and until now it had no gate and
# no CI. A regression in gate.ts affects every app at once, which is precisely
# why two of its own tests sat red on main for days without anyone noticing.
set -euo pipefail
cd "$(dirname "$0")"
echo "nexus-edge-proxy..."

# Scoped to the two unit suites on purpose.
#
# tests/terminal-public-hop.test.ts is excluded. It spawns a real Auth stub, a
# real Nexus-Terminal, a real Dashboard and the proxy, then opens WebSockets to
# real PTYs — and it hangs waiting for the terminal service to report two active
# shells. That failure is NOT caused by anything the gate does: it reproduces
# with GATE_SKIP_AUTH=true, and it reproduces at the commit before the gate was
# rewritten. It is a long-standing break in the Dashboard-to-Terminal PTY chain
# that has been invisible because this directory had no gate to show it in.
#
# Excluded rather than silently deleted, and named here rather than left for
# someone to rediscover. Putting the proxy and the gate under CI is worth more
# than leaving all three uncovered until that one is fixed — but it is a real
# gap and it should be closed.
bun test tests/gate.test.ts tests/proxy.test.ts

echo "PASS"
