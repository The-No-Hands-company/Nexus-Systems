#!/bin/bash
# Nexus Systems — run all per-app quality gates
set -euo pipefail
cd "$(dirname "$0")/.."
FAILED=0
for app in apps/Nexus-*/; do
    [ -f "$app/check.sh" ] || continue
    echo -n "[$(basename "$app")] "
    (cd "$app" && ./check.sh) || { FAILED=1; echo "FAILED"; }
done
[ $FAILED -eq 0 ] && echo "ALL PASSED" || echo "SOME FAILED"
exit $FAILED
