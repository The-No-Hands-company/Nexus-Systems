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

# --- widened coverage -------------------------------------------------------
expect_fail(){ # name, relpath, content
  local d="$T/w_$1"; mkdir -p "$d/$(dirname "$2")"; printf '%s\n' "$3" > "$d/$2"
  local o s; o=$(ROOT="$d" bash "$G" 2>&1); s=$?
  [ $s = 1 ] && echo "$o" | grep -q "$2" && ok "flags $1" || no "flags $1: $s $o"
}
expect_pass(){
  local d="$T/p_$1"; mkdir -p "$d/$(dirname "$2")"; printf '%s\n' "$3" > "$d/$2"
  local o s; o=$(ROOT="$d" bash "$G" 2>&1); s=$?
  [ $s = 0 ] && ok "allows $1" || no "allows $1: $s $o"
}
for h in cf-connecting-ip cf-connecting-ipv6 cf-pseudo-ipv4 x-forwarded-for x-real-ip true-client-ip cf-ipcountry cf-ray forwarded x-client-ip cf-visitor cf-ew-via cdn-loop; do
  expect_fail "header $h dq" apps/A/src/a.ts "h.get(\"$h\")"
  expect_fail "header $h sq" apps/A/src/a.ts "h.get('$h')"
  expect_fail "header $h bt" apps/A/src/a.ts "h.get(\`$h\`)"
done
expect_pass "x-forwarded-host/proto" apps/A/src/a.ts 'h.get("x-forwarded-host"); h.get("x-forwarded-proto")'
expect_fail "req.ip" apps/A/src/a.ts 'const i = req.ip;'
expect_fail "request.ip" apps/A/src/a.ts 'const i = request.ip;'
expect_fail "remoteAddress" apps/A/src/a.ts 'socket.remoteAddress'
expect_fail "requestIP(" apps/A/src/a.ts 'server.requestIP(req)'
expect_fail "ConnectInfo" apps/A/src/a.rs 'ConnectInfo<SocketAddr>'
expect_fail "peer_addr" apps/A/src/a.rs 'stream.peer_addr()'
expect_fail "X_FORWARDED_FOR" apps/A/src/a.py 'META["X_FORWARDED_FOR"]'
expect_fail "XForwardedFor" apps/A/src/a.go 'XForwardedFor'
expect_pass "req.ipv (word boundary)" apps/A/src/a.ts 'const i = req.ipv6Only;'
for e in cjs jsx mts cts svelte vue sh; do expect_fail "ext $e" "apps/A/src/a.$e" 'h.get("x-real-ip")'; done
expect_pass "tests dir skipped" apps/A/tests/a.ts 'h.get("x-real-ip")'
expect_pass ".test. skipped" apps/A/src/a.test.ts 'h.get("x-real-ip")'
expect_fail "src/build/ not skipped" apps/A/src/build/a.ts 'h.get("x-real-ip")'
expect_fail "src/test/ not skipped" apps/A/src/test/a.ts 'h.get("x-real-ip")'
expect_pass "node_modules skipped" apps/A/node_modules/x/a.js 'h.get("x-real-ip")'
# retired dirs
d="$T/retired"; mkdir -p "$d/apps/Nexus-API/src"; echo 'req.ip' > "$d/apps/Nexus-API/src/a.ts"
out=$(ROOT="$d" bash "$G" 2>&1); [ $? = 0 ] && ok "RETIRED_DIRS skipped" || no "retired: $out"
mkdir -p "$d/deploy/production"; echo 'cd "$ROOT/apps/Nexus-AI"' > "$d/deploy/production/deploy.sh"
out=$(ROOT="$d" bash "$G" 2>&1); [ $? = 1 ] && echo "$out" | grep -q 'apps/Nexus-AI/ is excluded' && ok "excluded dir started by deploy.sh fails" || no "deployed retired: $out"
# submodules: the services live there, so their code is scanned, and an
# unfetched one fails instead of scanning as empty
g(){ git -c user.name=t -c user.email=t@t -c protocol.file.allow=always "$@" >/dev/null 2>&1; }
sub="$T/subsrc"; mkdir -p "$sub/src"; echo 'h.get("x-real-ip")' > "$sub/src/a.ts"
g -C "$sub" init -q; g -C "$sub" add .; g -C "$sub" commit -qm s
sp="$T/super"; mkdir -p "$sp"; g -C "$sp" init -q; g -C "$sp" submodule add "$sub" apps/S; g -C "$sp" commit -qm s
out=$(ROOT="$sp" bash "$G" 2>&1); [ $? = 1 ] && echo "$out" | grep -q 'apps/S/src/a.ts' && ok "flags code inside a submodule" || no "submodule code: $out"
g -C "$sp" submodule deinit -f apps/S
out=$(ROOT="$sp" bash "$G" 2>&1); [ $? = 1 ] && echo "$out" | grep -q 'submodule apps/S is not checked out' && ok "unfetched submodule fails" || no "unfetched submodule: $out"
exit $rc
