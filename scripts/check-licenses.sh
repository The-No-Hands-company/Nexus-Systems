#!/usr/bin/env bash
# The monorepo is AGPL at the root and Apache-2.0 under packages/. Each package
# must say so in a LICENSE file and in its manifest, so tools agree with humans.
set -euo pipefail
cd "$(dirname "$0")/.."
fail=0
grep -q 'GNU AFFERO GENERAL PUBLIC LICENSE' LICENSE 2>/dev/null || { echo "FAIL: root LICENSE is not AGPL-3.0"; fail=1; }
[ -f LICENSING.md ] || { echo "FAIL: LICENSING.md missing"; fail=1; }
for d in packages/*/; do
  p=${d%/}
  grep -q 'Apache License' "$p/LICENSE" 2>/dev/null || { echo "FAIL: $p/LICENSE is not Apache-2.0"; fail=1; }
  if [ -f "$p/package.json" ]; then
    [ "$(jq -r '.license // ""' "$p/package.json")" = "Apache-2.0" ] || { echo "FAIL: $p/package.json license"; fail=1; }
  fi
  if [ -f "$p/Cargo.toml" ]; then
    grep -qE '^license *= *"Apache-2.0"' "$p/Cargo.toml" || { echo "FAIL: $p/Cargo.toml license"; fail=1; }
  fi
done
[ "$fail" = 0 ] && echo PASS || exit 1
