#!/bin/bash
# Nexus Systems — run every per-app quality gate.
#
# Usage:
#   scripts/check.sh                 # every app that has a check.sh
#   scripts/check.sh Nexus-Auth ...  # only the named apps
#   CHECK_TIMEOUT=1200 scripts/check.sh
#
# This script used to run each gate with no time limit and no summary. One app
# that hung took the whole run with it: a 15-minute invocation produced no
# output at all and had to be killed, so in practice nobody could run it and
# nobody did. Three things fix that — a per-app timeout, a line printed as each
# app finishes, and a closing list naming exactly which ones failed.
set -uo pipefail
cd "$(dirname "$0")/.."

# Generous by default: Nexus-Modeling builds a C++ kernel and runs ~2500 tests,
# and Nexus-AI has ~1200. A gate that is slow is still a gate; one that is
# unbounded is a hang.
TIMEOUT="${CHECK_TIMEOUT:-900}"

if [ "$#" -gt 0 ]; then
  APPS=()
  for name in "$@"; do APPS+=("apps/$name/"); done
else
  APPS=(apps/Nexus-*/)
fi

PASSED=(); FAILED=(); TIMEDOUT=(); SKIPPED=()

for app in "${APPS[@]}"; do
  name="$(basename "$app")"
  if [ ! -f "$app/check.sh" ]; then
    SKIPPED+=("$name")
    continue
  fi

  printf '━━━ %s\n' "$name"
  started=$SECONDS
  # `bash check.sh`, not `./check.sh`: these files are mode 100644 in git, so a
  # fresh clone cannot execute them directly.
  ( cd "$app" && timeout "$TIMEOUT" bash check.sh )
  rc=$?
  elapsed=$(( SECONDS - started ))

  case $rc in
    0)   PASSED+=("$name");   printf '    PASS    %s (%ss)\n' "$name" "$elapsed" ;;
    124) TIMEDOUT+=("$name"); printf '    TIMEOUT %s (killed after %ss)\n' "$name" "$TIMEOUT" ;;
    *)   FAILED+=("$name");   printf '    FAIL    %s (exit %s, %ss)\n' "$name" "$rc" "$elapsed" ;;
  esac
done

echo
echo "━━━ Summary ━━━"
printf '  passed:   %s\n' "${#PASSED[@]}"
printf '  failed:   %s%s\n'  "${#FAILED[@]}"   "$([ ${#FAILED[@]}   -gt 0 ] && printf '  → %s' "${FAILED[*]}")"
printf '  timedout: %s%s\n'  "${#TIMEDOUT[@]}" "$([ ${#TIMEDOUT[@]} -gt 0 ] && printf '  → %s' "${TIMEDOUT[*]}")"
printf '  no gate:  %s%s\n'  "${#SKIPPED[@]}"  "$([ ${#SKIPPED[@]}  -gt 0 ] && printf '  → %s' "${SKIPPED[*]}")"

# KNOWN GAP — a failure here is not yet trustworthy on its own.
#
# Observed, twice: Nexus-Cloud fails an etag/304 assertion in
# src/api/handlers.routes.test.ts when the machine is busy, and the failing line
# moves between runs. On an idle machine the same suite passes 8 out of 8.
# Nexus-Auth behaved the same way once — non-zero in a back-to-back sweep,
# 80/80 and exit 0 on its own.
#
# The mechanism is NOT established. Contention is the obvious suspect, since
# both failures coincided with a heavy suite running alongside, and some of
# these apps also share the one live Postgres. Neither has been proven, so
# neither is asserted here.
#
# What follows from it is concrete: re-run a single app before believing a
# failure from this script.
#
#     scripts/check.sh Nexus-Cloud
#
# Making this trustworthy means removing the timing sensitivity and giving each
# suite its own database, so neither load nor ordering can change the answer.

if [ ${#FAILED[@]} -eq 0 ] && [ ${#TIMEDOUT[@]} -eq 0 ]; then
  echo "ALL PASSED"
  exit 0
fi
echo "SOME FAILED"
exit 1
