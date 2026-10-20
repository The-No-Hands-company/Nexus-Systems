#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
ARTIFACT="$ROOT/docs/audits/public-surface-audit-v13.html"
README="$ROOT/docs/audits/README.md"
EXPECTED="224e9f0126f03464769485d331275adcdf5010a34d5ab07b5165c9d51ea447fd" # pragma: allowlist secret

test -s "$ARTIFACT"
grep -Fq 'Public surface audit v13' "$README"
grep -Fq './public-surface-audit-v13.html' "$README"
actual="$(sha256sum "$ARTIFACT" | awk '{print $1}')"
test "$actual" = "$EXPECTED"
