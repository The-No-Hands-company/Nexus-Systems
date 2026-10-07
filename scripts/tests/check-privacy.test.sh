#!/usr/bin/env bash
set -uo pipefail
G="$(cd "$(dirname "$0")/.." && pwd)/check-privacy.sh"
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
rc=0; ok(){ echo "ok - $1"; }; no(){ echo "NOT OK - $1"; rc=1; }
mkdir -p "$T/dirty/apps/X/src" "$T/dirty/apps/Y/migrations" "$T/clean/apps/X/src" "$T/clean/apps/Y/migrations"
echo 'const a = req.headers.get("x-forwarded-for");' > "$T/dirty/apps/X/src/a.ts"
echo 'ALTER TABLE t ADD COLUMN ip_address inet;' > "$T/dirty/apps/Y/migrations/1.sql"
echo 'const a = 1;' > "$T/clean/apps/X/src/a.ts"
echo 'ALTER TABLE t DROP COLUMN ip_address;' > "$T/clean/apps/Y/migrations/1.sql"
out=$(ROOT="$T/dirty" bash "$G" 2>&1); s=$?
[ $s = 1 ] && echo "$out" | grep -q 'apps/X/src/a.ts' && echo "$out" | grep -q 'apps/Y/migrations/1.sql' && ok "dirty fails naming both" || no "dirty: $s $out"
out=$(ROOT="$T/clean" bash "$G" 2>&1); s=$?
[ $s = 0 ] && echo "$out" | grep -q PASS && ok "clean passes (DROP not flagged)" || no "clean: $s $out"
out=$(ROOT="$T/dirty" PRIVACY_ALLOW_FILES="apps/Y/migrations/1.sql" bash "$G" 2>&1)
echo "$out" | grep -q 'migrations/1.sql' && no "listed file not skipped" || ok "listed file skipped"
exit $rc
