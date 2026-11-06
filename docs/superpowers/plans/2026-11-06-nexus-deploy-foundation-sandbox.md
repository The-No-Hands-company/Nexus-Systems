# Nexus Deploy Foundation — Plan 3 of 5: The Sandbox and `doctor`

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking. **Task 3 is for the node's operator, not for an agent:** it needs `sudo`, which an agent does not have.

**Goal:** Give Deploy a sandbox it can be trusted with: one rootless Docker daemon, on its own account, its own size-capped disk and its own network filter, plus a `doctor` that proves each of those properties from inside a container. Nothing is restarted and no member can deploy anything yet.

**Architecture:** `sandbox/setup.sh` renders and installs every file the sandbox needs (daemon config, a systemd user unit, a tmpfiles rule that shares the socket with the Deploy account only, an nftables egress filter scoped to the sandbox account by uid, an fstab line for the storage image). `--print` shows all of it without root, so the files are read and tested before anything is installed. `npm run doctor` runs ten checks, each a pure function of an injectable context, using the same isolation flags real containers get.

**Tech Stack:** Bash (the setup), Node ESM + TypeScript with `node:test` (the checks), systemd, nftables, rootless Docker. No new dependencies.

**Spec:** `docs/superpowers/specs/2026-11-06-nexus-deploy-foundation-design.md` (§5 unit 3, §8.1 sandbox boundary, §8.2 capped logs, §9 disk, §10 item 1, §12 `doctor`, §14 tasks 2 and 7). **Earlier plans:** Plan 1 (control plane) and Plan 2 (edge), both built.

## Where this plan sits

The spec's "Plan 3" turned out to be three separate pieces of work, each with its own risk, so it is delivered as three plans. This one comes first because everything else runs inside the sandbox.

| Plan | Delivers | Needs |
|---|---|---|
| **3 — Sandbox and `doctor`** (this plan) | The sandbox setup script, its egress filter, the storage cap, `npm run doctor` | Tasks 1–2 an agent; Task 3 the operator (`sudo`) |
| 4 — Builder and `verify` | Choose and install the builder, bundled fixture apps (Node, Python, Dockerfile, image), `npm run verify` and the rest of `doctor` (builder, Cloud registration, DNS) | A working sandbox |
| 5 — Dashboard and bring-up | Build the dashboard on the shared theme, `deploy.sh` starts Deploy with the new environment, Cloud registration, the disposable second-node proof | Plans 3 and 4, and a healthy production |

## Spec coverage

| Spec section | Where it is built |
|---|---|
| §5 unit 3 Sandbox runtime: rootless daemon, dedicated user, capped storage, capped logs | Tasks 1, 3 |
| §8.1 Containers must not reach host loopback services | Tasks 1 (daemon setting), 2 (proved by `doctor`) |
| §8.1 "Containers can reach the operator's LAN, a documented limitation" | **Closed** rather than documented: see "What the investigation found", item 2 |
| §8.2 Container logs capped (`local`, 10 MB × 2, no compression) | Tasks 1, 2 |
| §9 Disk: fixed-size storage image | Tasks 1, 3 |
| §10 item 1 Sandbox setup script, idempotent, checked by `doctor` | Tasks 1, 2, 3 |
| §12 `doctor`: sandbox reachable, rootless, log caps, storage cap, hardening | Task 2 |
| §12 `doctor`: builder present, domain resolution, Cloud registration, web UI built | Plan 4 and Plan 5 |
| §14 task 2 (rootless blocks host loopback; SELinux enforcing) | Verified here; see item 1 |
| §14 task 6 (why Deploy is not running) | Answered; see item 6 |
| §14 task 7 (storage image filesystem and mount) | Designed here, confirmed by the operator in Task 3 |
| Builder, fixtures, `verify`, dashboard, bring-up, second-node proof | Plans 4 and 5 |

## What the investigation found

Written while preparing this plan, on this node, by running a throwaway rootless daemon as the ordinary user (no `sudo`, no effect on production; it was removed afterwards).

1. **The sandbox works here.** A rootless Docker 29.8 daemon starts under systemd with SELinux *enforcing*, uses the `local` log driver with the 10 MB × 2 caps, publishes a container on `127.0.0.1` only, builds an image, resolves names and reaches the internet. A container cannot reach a service on the *host's loopback* (`DOCKERD_ROOTLESS_ROOTLESSKIT_DISABLE_HOST_LOOPBACK=true`): confirmed against the real Auth on `127.0.0.1:4310`. The same setting turned off lets it through, so `doctor` can tell the difference (checked with a second daemon).
2. **But the host's loopback is not the only way in.** A container reached the host through its **LAN address** (`192.168.0.179`) and through every Docker bridge address, so any service the machine binds to all interfaces is open to a member's code. On this node that is **Auth (`0.0.0.0:4310`)**, the ecosystem proxy (`:8080`) and others (`:7700`, `:9042`, `:18080`). The spec accepted LAN reach as a limitation; this plan closes it instead, because Auth is one of the services in question. A rootless daemon's outbound traffic leaves from the sandbox account's own process, so an nftables rule matching `meta skuid` can drop it. The rules were run in a throwaway user and network namespace (no root): the sandbox uid is blocked from loopback, 10/8, 172.16/12 and 192.168/16 and allowed to send DNS to the machine's resolvers; a control with no rules delivers normally. Applying them for real needs root, so it is in Task 3.
3. **Disk is the one cost, and this node has little of it.** The storage image reserves its size up front (so a runaway build can fail but cannot starve production). Today `/` has 35 GB free of 237 GB (14.8% free, already under the project's own 15% line) and `Data1` has 51 GB free of 902 GB (5.6% free). `setup.sh` therefore **refuses by default** (`--min-free-percent 15`) and the operator has to choose. Room can be made without deleting anything of Deploy's: the host Docker daemon holds 34 GB of build cache (17.6 GB reclaimable) and 10.7 GB of reclaimable images. That is the operator's call, and this plan does not touch it. The image lives on `/` by default because it is the faster disk and `Data1` is a FUSE-mounted volume (`fuseblk`), which is slower and a poor home for a loop-mounted filesystem.
4. **Sharing the socket across accounts is designed, not yet proven.** The socket lives in `/run/nexus-deploy-sandbox/`, a directory the sandbox account owns and the Deploy account is granted by ACL (a default ACL, so each restart's new socket inherits it). I confirmed that a default ACL reaches a socket created in the directory, but not across two real accounts. `doctor` run as the Deploy account is the proof, and it is a step of Task 3.
5. **The builder is a Plan 4 decision, with the facts gathered.** Nixpacks is in maintenance mode and its README points to Railpack ([sources](https://www.bitdoze.com/md/nixpacks-vs-railpack.md)). Railpack is MIT-licensed, installed with `mise` or a script, and its documented flow needs a BuildKit instance ([repository](https://github.com/railwayapp/railpack), [installation](https://railpack.com/installation)). Docker 29's daemon has BuildKit built in, which may remove the need for a separate one. Plan 4 settles it by building a real fixture.
6. **Why Deploy is not running (§14 task 6).** Nobody started it since it became startable. `deploy.sh` starts it only when `apps/Nexus-Deploy/dist/index.js` exists, and the one there is a stale v0.5.0 build from before this work. It needs a rebuild and a `deploy.sh` run. Plan 5 does both.
7. **A production fault found along the way, not caused by this work:** Nexus Cloud (`:8787`) was down while this plan was written. The five-minute watchdog (`nexus-watchdog.timer`) does restart it, but its `deploy.sh bg` run then fails at `nexus-draw-web has an occupied but unverifiable port :8091 — refusing to start`, exits 1, and systemd stops the services that run had just started (they live in the watchdog unit's control group). So the watchdog brings Cloud up and kills it again on every cycle. The current Cloud code starts cleanly on a spare port, so it is not the new code. This plan does not touch it; it matters to Plan 5, because Deploy cannot be brought up on a node whose Cloud is down.

## Global Constraints

- Free; no billing, no paid dependency. The only resource the sandbox takes is disk, reserved up front and chosen by the operator.
- Deploy talks only to the sandbox daemon and never falls back to the host's. `SANDBOX_DOCKER_HOST` empty means it refuses to run anything (Plan 1); `doctor` fails if it names the host's own socket.
- Containers run with `--cap-drop ALL`, `no-new-privileges`, memory, CPU and process caps, and the `local` log driver capped at 10 MB × 2 files with compression off. `doctor` proves it with the real flags from `isolationArgs` (extracted from Plan 1's `buildRunArgs` without changing its output).
- Containers cannot reach the host's loopback services or any address of the host or its LAN. DNS to the machine's own resolvers is the only exception.
- No visitor address is read or stored; nothing here touches request handling.
- **An agent never runs `sudo`, restarts a service, edits a `.env`, or installs anything outside the repository.** `sandbox/setup.sh` is read and tested with `--print`; running it for real is Task 3, by the operator.
- New code passes Biome; `tsc` is clean for source and tests. Commits go to `main` of `Nexus-Deploy` (solo developer: no branches, no pull requests), in `type(scope): description` form.

## Running things

All commands run from `apps/Nexus-Deploy`. The repository sits on a mount that cannot execute binaries, so esbuild and Biome run from copies:

```bash
cd apps/Nexus-Deploy
mkdir -p /tmp/nexus-deploy-tools
cp node_modules/@esbuild/linux-x64/bin/esbuild /tmp/nexus-deploy-tools/esbuild
cp node_modules/@biomejs/cli-linux-x64/biome /tmp/nexus-deploy-tools/biome
chmod +x /tmp/nexus-deploy-tools/esbuild /tmp/nexus-deploy-tools/biome
export ESBUILD_BINARY_PATH=/tmp/nexus-deploy-tools/esbuild
node node_modules/typescript/bin/tsc -p tsconfig.json --noEmit
node node_modules/typescript/bin/tsc -p tsconfig.test.json
node --import tsx --test tests/*.test.ts
```

**Baseline before Task 1:** 220 tests pass, both `tsc` runs clean. The egress-rule tests use `unshare`, `nft` and `ip` with unprivileged namespaces; where those are missing they are skipped, not failed.

---

## Tasks

### Task 1: The sandbox setup script, its egress filter, and their tests

One script installs everything the sandbox needs and, with `--print`, shows every file it would write without root, so the same text is read by the operator, asserted by tests and installed. The egress filter is the part the investigation added: a rootless daemon keeps containers off the host's loopback but not off its LAN address, so an nftables rule scoped by the sandbox account's uid drops that account's connections to loopback and private ranges, after allowing DNS to the machine's own resolvers. The egress tests apply the generated rules inside a throwaway user and network namespace, so they need no root and change nothing on the machine.

**Repository:** `apps/Nexus-Deploy` (its own git repository). Paths are relative to it.

**Files:**

- Create: `tests/sandbox-setup.test.ts`
- Create: `tests/egress-rules.test.ts`
- Create: `sandbox/setup.sh`
- Create: `sandbox/README.md`
- Modify: `.env.example`

**Interfaces:**

- Consumes: Nothing from earlier tasks.
- Produces: `sandbox/setup.sh` (`--print`, `--deploy-user`, `--user`, `--size-gb`, `--image`, `--mount`, `--min-free-percent`, `--force`), `sandbox/README.md`, and the settings `SANDBOX_DOCKER_HOST`, `SANDBOX_STORAGE_PATH`, `SANDBOX_STORAGE_SIZE_GB` in `.env.example`.

- [ ] **Step 1: Write the tests**

**Create `tests/sandbox-setup.test.ts`:**

````typescript
import assert from "node:assert/strict";
import { spawnSync } from "node:child_process";
import { test } from "node:test";

const SCRIPT = "sandbox/setup.sh";

function run(args: string[]) {
  return spawnSync("bash", [SCRIPT, ...args], { encoding: "utf8" });
}

/** The files `--print` would install, by name. */
function printed(extra: string[] = []): Record<string, string> {
  const res = run(["--print", "--deploy-user", "alice", ...extra]);
  assert.equal(res.status, 0, res.stderr);
  const files: Record<string, string> = {};
  let name = "";
  for (const line of res.stdout.split("\n")) {
    const header = /^### (.+)$/.exec(line);
    if (header?.[1]) {
      name = header[1];
      files[name] = "";
    } else if (name) {
      files[name] += `${line}\n`;
    }
  }
  return files;
}

test("the script is valid bash", () => {
  const res = spawnSync("bash", ["-n", SCRIPT], { encoding: "utf8" });
  assert.equal(res.status, 0, res.stderr);
});

test("--print writes nothing and prints every file the setup installs", () => {
  const files = printed();
  assert.deepEqual(Object.keys(files), [
    "daemon.json",
    "nexus-sandbox-docker.service",
    "tmpfiles",
    "egress.nft",
    "egress.service",
    "fstab",
    "env",
  ]);
});

test("the daemon caps every container's logs, as the Charter promises", () => {
  const daemon = JSON.parse(printed()["daemon.json"] ?? "") as {
    "log-driver": string;
    "log-opts": Record<string, string>;
  };
  assert.equal(daemon["log-driver"], "local");
  assert.deepEqual(daemon["log-opts"], { "max-size": "10m", "max-file": "2", compress: "false" });
});

test("the daemon is rootless, keeps containers off the host's loopback, and stores on the capped mount", () => {
  const unit = printed()["nexus-sandbox-docker.service"] ?? "";
  assert.match(unit, /dockerd-rootless\.sh/);
  assert.match(unit, /DOCKERD_ROOTLESS_ROOTLESSKIT_DISABLE_HOST_LOOPBACK=true/);
  assert.match(unit, /RequiresMountsFor=\/var\/lib\/nexus-deploy-sandbox\/storage\n/);
  assert.match(unit, /--data-root=\/var\/lib\/nexus-deploy-sandbox\/storage\/docker/);
  assert.match(unit, /--host=unix:\/\/\/run\/nexus-deploy-sandbox\/docker\.sock/);
  assert.match(unit, /Restart=on-failure/);
});

test("only the Deploy account is given the socket", () => {
  const tmpfiles = printed()["tmpfiles"] ?? "";
  assert.match(tmpfiles, /^d \/run\/nexus-deploy-sandbox 0750 nexus-deploy nexus-deploy -$/m);
  const grants = tmpfiles.split("\n").filter((l) => l.startsWith("a+"));
  assert.equal(grants.length, 2);
  for (const line of grants) assert.match(line, /user:alice:/);
  assert.ok(!/other|everyone|::/.test(tmpfiles));
});

test("the egress filter is scoped to the sandbox account and drops private and local ranges", () => {
  const nft = printed()["egress.nft"] ?? "";
  assert.match(nft, /^table inet nexus_deploy_sandbox \{/m);
  assert.match(nft, /hook output/);
  // Every rule is for the sandbox account only, so nothing else on the machine is touched.
  const rules = nft.split("\n").filter((l) => l.trim().startsWith("meta "));
  assert.ok(rules.length >= 2);
  for (const rule of rules) assert.match(rule, /^\s*meta skuid \d+ /);
  for (const range of [
    "127.0.0.0/8",
    "10.0.0.0/8",
    "172.16.0.0/12",
    "192.168.0.0/16",
    "169.254.0.0/16",
  ]) {
    assert.ok(nft.includes(range), `drops ${range}`);
  }
  assert.match(nft, /ip6 daddr \{ ::1, fc00::\/7/);
  // Any DNS allowance comes before the drop.
  const lines = rules.map((r) => r.includes(" drop"));
  const firstDrop = lines.indexOf(true);
  assert.ok(firstDrop >= 0);
  assert.ok(
    !lines
      .slice(firstDrop)
      .some((isDrop, i) => !isDrop && rules[firstDrop + i]?.includes("accept")),
  );
});

test("the storage image is mounted nodev,nosuid at the configured place and the node is told where", () => {
  const files = printed(["--image", "/data/sbx.img", "--mount", "/mnt/sbx", "--size-gb", "20"]);
  assert.equal(files["fstab"], "/data/sbx.img /mnt/sbx ext4 loop,nodev,nosuid,noatime 0 0\n");
  assert.match(
    files["env"] ?? "",
    /^SANDBOX_DOCKER_HOST=unix:\/\/\/run\/nexus-deploy-sandbox\/docker\.sock$/m,
  );
  assert.match(files["env"] ?? "", /^SANDBOX_STORAGE_PATH=\/mnt\/sbx$/m);
  assert.match(files["env"] ?? "", /^SANDBOX_STORAGE_SIZE_GB=20$/m);
  assert.match(files["nexus-sandbox-docker.service"] ?? "", /--data-root=\/mnt\/sbx\/docker/);
});

test("bad or missing arguments are refused before anything is done", () => {
  for (const [args, message] of [
    [["--print"], /--deploy-user is required/],
    [["--print", "--deploy-user", "alice", "--size-gb", "2"], /at least 4/],
    [["--print", "--deploy-user", "alice", "--size-gb", "lots"], /whole number/],
    [["--print", "--deploy-user", "nexus-deploy"], /must differ/],
    [["--print", "--deploy-user", "Al ice"], /not a valid account name/],
    [["--print", "--deploy-user", "alice", "--image", "relative.img"], /absolute/],
    [["--print", "--deploy-user", "alice", "--nope"], /unknown option/],
  ] as const) {
    const res = run([...args]);
    assert.notEqual(res.status, 0, args.join(" "));
    assert.match(res.stderr, message, args.join(" "));
  }
});

test("a real run without root is refused", { skip: process.getuid?.() === 0 }, () => {
  const res = run(["--deploy-user", "alice"]);
  assert.notEqual(res.status, 0);
  assert.match(res.stderr, /run as root/);
});
````

**Create `tests/egress-rules.test.ts`:**

````typescript
import assert from "node:assert/strict";
import { spawnSync } from "node:child_process";
import { test } from "node:test";

/**
 * Applies the egress rules the setup script generates inside a throwaway user and
 * network namespace (no root, no effect on this machine) and sends datagrams as
 * the account the rules name. UDP is used so the test needs no listener to reply:
 * a dropped send fails with EPERM at the sender, a delivered one arrives.
 */

const have = (cmd: string) => spawnSync("sh", ["-c", `command -v ${cmd}`]).status === 0;
const namespacesWork =
  have("unshare") &&
  have("nft") &&
  have("ip") &&
  spawnSync("unshare", ["-rn", "true"]).status === 0;
const skip = namespacesWork ? false : "needs unshare, nft and ip with unprivileged namespaces";

function renderedRules(): string {
  const res = spawnSync("bash", ["sandbox/setup.sh", "--print", "--deploy-user", "alice"], {
    encoding: "utf8",
  });
  assert.equal(res.status, 0, res.stderr);
  const text = res.stdout.split("### egress.nft\n")[1]?.split("### ")[0] ?? "";
  // Inside the namespace the account is root, uid 0.
  return text.replaceAll(/meta skuid \d+/g, "meta skuid 0");
}

/** Send one UDP datagram to addr:port from inside the namespace; report what happened. */
const PROBE = `
const dgram = require("node:dgram");
const [addr, port] = process.argv.slice(1);
const rx = dgram.createSocket("udp4");
rx.on("error", () => {});
const tx = dgram.createSocket("udp4");
let outcome = "silent";
rx.on("message", () => { outcome = "delivered"; });
rx.bind(Number(port), addr, () => {
  tx.send("x", Number(port), addr, (err) => {
    if (err) outcome = err.code;
    setTimeout(() => { console.log(outcome); process.exit(0); }, 300);
  });
});
rx.on("error", () => { console.log("bind-failed"); process.exit(0); });
`;

function probe(rules: string | null, addrOnLo: string, port: number): string {
  const script = [
    "set -e",
    "ip link set lo up",
    `ip addr add ${addrOnLo}/32 dev lo 2>/dev/null || true`,
    rules ? `nft -f - <<'RULES'\n${rules}\nRULES` : "true",
    `node -e '${PROBE}' ${addrOnLo} ${port}`,
  ].join("\n");
  const res = spawnSync("unshare", ["-rn", "bash", "-c", script], { encoding: "utf8" });
  return res.stdout.trim().split("\n").pop() ?? `no output: ${res.stderr}`;
}

test(
  "without the rules, a datagram to a local address is delivered (the control)",
  { skip },
  () => {
    assert.equal(probe(null, "127.0.0.1", 40001), "delivered");
  },
);

test(
  "with the rules, the sandbox account cannot send to loopback or private networks",
  { skip },
  () => {
    const rules = renderedRules();
    for (const [addr, port] of [
      ["127.0.0.1", 40002],
      ["10.1.2.3", 40003],
      ["172.20.0.1", 40004],
      ["192.168.77.5", 40005],
    ] as const) {
      assert.equal(probe(rules, addr, port), "EPERM", `${addr} must be blocked`);
    }
  },
);

test("with the rules, a public address is not blocked by them", { skip }, () => {
  // Nothing is listening and there is no route in an empty namespace, so the
  // datagram goes nowhere; what matters is that the filter did not stop it.
  const outcome = probe(renderedRules(), "93.184.216.34", 40006);
  assert.notEqual(outcome, "EPERM");
});

test("with the rules, DNS to a resolver the machine uses is allowed", { skip }, () => {
  const rules = renderedRules();
  const match = /ip daddr (\d+\.\d+\.\d+\.\d+) udp dport 53 accept/.exec(rules);
  if (!match?.[1]) return; // this machine names no IPv4 resolver; nothing to allow
  assert.equal(probe(rules, match[1], 53), "delivered");
});
````

- [ ] **Step 2: Run them and watch them fail**

```bash
cd apps/Nexus-Deploy
export ESBUILD_BINARY_PATH=/tmp/nexus-deploy-tools/esbuild
node --import tsx --test tests/sandbox-setup.test.ts tests/egress-rules.test.ts
```

Expected: FAIL — `bash: sandbox/setup.sh: No such file or directory` (and the egress tests fail to render rules).

- [ ] **Step 3: Implement**

**Create `sandbox/setup.sh`:**

````bash
#!/usr/bin/env bash
#
# Nexus Deploy sandbox setup.
#
# Creates the rootless Docker daemon that runs and builds every member's app, on
# its own user, its own size-capped filesystem and behind its own egress filter,
# so a member's code never shares a daemon, a disk or a network path with the
# rest of the node.
#
#   sudo bash sandbox/setup.sh --deploy-user <who runs Nexus Deploy> [options]
#   bash sandbox/setup.sh --print --deploy-user <name> [options]   # no root, changes nothing
#
# Idempotent: running it again repairs and re-applies, and never shrinks or
# reformats an existing storage image.
#
# Options
#   --deploy-user NAME   account that runs Nexus Deploy; gets access to the socket (required)
#   --user NAME          sandbox account to create               (default: nexus-deploy)
#   --size-gb N          storage image size, reserved up front    (default: 12)
#   --image PATH         storage image file    (default: /var/lib/nexus-deploy-sandbox/storage.img)
#   --mount PATH         where it is mounted   (default: /var/lib/nexus-deploy-sandbox/storage)
#   --min-free-percent N refuse if the image would leave less than N% free on its disk (default: 15)
#   --force              skip the free-space refusal (the operator has decided)
#   --print              print every file this would write, then exit; needs no root
#
set -euo pipefail

SANDBOX_USER="nexus-deploy"
DEPLOY_USER=""
SIZE_GB=12
IMAGE="/var/lib/nexus-deploy-sandbox/storage.img"
MOUNT="/var/lib/nexus-deploy-sandbox/storage"
MIN_FREE_PERCENT=15
FORCE=0
PRINT=0
SOCKET_DIR="/run/nexus-deploy-sandbox"
SOCKET="$SOCKET_DIR/docker.sock"
LOG_MAX_SIZE="10m"
LOG_MAX_FILE="2"

usage() { sed -n '3,25p' "$0" | sed 's/^# \{0,1\}//'; }
die() { printf 'sandbox-setup: %s\n' "$*" >&2; exit 1; }
say() { printf 'sandbox-setup: %s\n' "$*"; }

while [ $# -gt 0 ]; do
  case "$1" in
    --deploy-user) DEPLOY_USER="${2:?--deploy-user needs a name}"; shift 2 ;;
    --user) SANDBOX_USER="${2:?--user needs a name}"; shift 2 ;;
    --size-gb) SIZE_GB="${2:?--size-gb needs a number}"; shift 2 ;;
    --image) IMAGE="${2:?--image needs a path}"; shift 2 ;;
    --mount) MOUNT="${2:?--mount needs a path}"; shift 2 ;;
    --min-free-percent) MIN_FREE_PERCENT="${2:?--min-free-percent needs a number}"; shift 2 ;;
    --force) FORCE=1; shift ;;
    --print) PRINT=1; shift ;;
    -h|--help) usage; exit 0 ;;
    *) die "unknown option: $1 (try --help)" ;;
  esac
done

[ -n "$DEPLOY_USER" ] || die "--deploy-user is required"
[[ "$SIZE_GB" =~ ^[0-9]+$ ]] && [ "$SIZE_GB" -ge 4 ] || die "--size-gb must be a whole number of at least 4"
[[ "$MIN_FREE_PERCENT" =~ ^[0-9]+$ ]] || die "--min-free-percent must be a whole number"
[[ "$SANDBOX_USER" =~ ^[a-z_][a-z0-9_-]{0,30}$ ]] || die "--user is not a valid account name"
[[ "$DEPLOY_USER" =~ ^[a-z_][a-z0-9_-]{0,30}$ ]] || die "--deploy-user is not a valid account name"
[ "$SANDBOX_USER" != "$DEPLOY_USER" ] || die "the sandbox account must differ from the account that runs Deploy"
[[ "$IMAGE" = /* && "$MOUNT" = /* ]] || die "--image and --mount must be absolute paths"

# ── The files this writes ─────────────────────────────────────────────────────
# Each is a function so --print and the real run share one source of truth, and
# tests/sandbox-setup.test.ts can read exactly what would be installed.

# DNS servers the egress filter must let the sandbox reach: slirp4netns forwards
# a container's lookups to whatever /etc/resolv.conf names, from the sandbox
# account, so a filter that blocked them would break every build.
resolvers() {
  { cat /etc/resolv.conf /run/systemd/resolve/resolv.conf 2>/dev/null || true; } \
    | awk '$1 == "nameserver" && $2 ~ /^[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+$/ { print $2 }' | sort -u
}

render_daemon_json() {
  cat <<EOF
{
  "log-driver": "local",
  "log-opts": { "max-size": "$LOG_MAX_SIZE", "max-file": "$LOG_MAX_FILE", "compress": "false" }
}
EOF
}

render_unit() {
  cat <<EOF
[Unit]
Description=Nexus Deploy sandbox Docker daemon (rootless)
After=network-online.target
Wants=network-online.target
RequiresMountsFor=$MOUNT

[Service]
Environment=DOCKERD_ROOTLESS_ROOTLESSKIT_NET=slirp4netns
# Containers must not reach the host's own loopback services (Auth, Cloud, the databases).
Environment=DOCKERD_ROOTLESS_ROOTLESSKIT_DISABLE_HOST_LOOPBACK=true
Environment=DOCKERD_ROOTLESS_ROOTLESSKIT_STATE_DIR=%t/nexus-sandbox/rootlesskit
ExecStartPre=/usr/bin/mkdir -p %t/nexus-sandbox $MOUNT/docker
ExecStart=/usr/bin/dockerd-rootless.sh --config-file=%h/.config/docker/daemon.json --data-root=$MOUNT/docker --exec-root=%t/nexus-sandbox --host=unix://$SOCKET --pidfile=%t/nexus-sandbox/docker.pid
Delegate=yes
Type=simple
KillMode=mixed
Restart=on-failure
RestartSec=5s
LimitNOFILE=infinity
TasksMax=infinity

[Install]
WantedBy=default.target
EOF
}

# The socket lives outside the sandbox account's runtime directory (which is
# private to it) so the Deploy account can reach it, and only that account can.
render_tmpfiles() {
  cat <<EOF
d $SOCKET_DIR 0750 $SANDBOX_USER $SANDBOX_USER -
a+ $SOCKET_DIR - - - - user:$DEPLOY_USER:r-x
a+ $SOCKET_DIR - - - - default:user:$DEPLOY_USER:rw-
EOF
}

# nftables: the sandbox account may not open connections to this machine or to
# private networks. Without it a member's container reaches every service the host
# binds to a LAN address (a rootless daemon only blocks the host's loopback).
# DNS to the machine's resolvers is let through first.
render_nft() {
  local uid="$1" r
  cat <<EOF
table inet nexus_deploy_sandbox {
  chain output {
    type filter hook output priority 0; policy accept;
EOF
  while IFS= read -r r; do
    [ -n "$r" ] || continue
    printf '    meta skuid %s ip daddr %s udp dport 53 accept\n' "$uid" "$r"
    printf '    meta skuid %s ip daddr %s tcp dport 53 accept\n' "$uid" "$r"
  done <<<"$(resolvers)"
  cat <<EOF
    meta skuid $uid ip daddr { 0.0.0.0/8, 10.0.0.0/8, 100.64.0.0/10, 127.0.0.0/8, 169.254.0.0/16, 172.16.0.0/12, 192.168.0.0/16, 224.0.0.0/4 } drop
    meta skuid $uid ip6 daddr { ::1, fc00::/7, fe80::/10, ff00::/8 } drop
  }
}
EOF
}

render_nft_unit() {
  cat <<EOF
[Unit]
Description=Nexus Deploy sandbox egress filter
After=network-pre.target
Before=network.target

[Service]
Type=oneshot
RemainAfterExit=yes
ExecStart=/bin/sh -c '/usr/sbin/nft delete table inet nexus_deploy_sandbox 2>/dev/null; /usr/sbin/nft -f /etc/nexus-deploy-sandbox/egress.nft'
ExecStop=/bin/sh -c '/usr/sbin/nft delete table inet nexus_deploy_sandbox 2>/dev/null || true'

[Install]
WantedBy=multi-user.target
EOF
}

render_fstab_line() {
  printf '%s %s ext4 loop,nodev,nosuid,noatime 0 0\n' "$IMAGE" "$MOUNT"
}

render_env() {
  cat <<EOF
# Add to the environment Nexus Deploy starts with:
SANDBOX_DOCKER_HOST=unix://$SOCKET
SANDBOX_STORAGE_PATH=$MOUNT
SANDBOX_STORAGE_SIZE_GB=$SIZE_GB
EOF
}

UID_FOR_PRINT="$(id -u "$SANDBOX_USER" 2>/dev/null || echo 65000)"

if [ "$PRINT" -eq 1 ]; then
  for f in \
    "daemon.json:render_daemon_json" \
    "nexus-sandbox-docker.service:render_unit" \
    "tmpfiles:render_tmpfiles" \
    "egress.nft:render_nft $UID_FOR_PRINT" \
    "egress.service:render_nft_unit" \
    "fstab:render_fstab_line" \
    "env:render_env"; do
    printf '### %s\n' "${f%%:*}"
    ${f#*:}
  done
  exit 0
fi

# ── From here on it changes the machine ──────────────────────────────────────
[ "$(id -u)" -eq 0 ] || die "run as root: sudo bash sandbox/setup.sh --deploy-user $DEPLOY_USER"
[ "$(uname -s)" = "Linux" ] || die "Linux only"
id "$DEPLOY_USER" >/dev/null 2>&1 || die "no such account: $DEPLOY_USER"

for tool in newuidmap newgidmap rootlesskit dockerd-rootless.sh slirp4netns mkfs.ext4 losetup nft setfacl systemctl loginctl runuser; do
  command -v "$tool" >/dev/null 2>&1 || die "missing required tool: $tool"
done

# 1. The account, with subordinate uid/gid ranges.
if id "$SANDBOX_USER" >/dev/null 2>&1; then
  say "account $SANDBOX_USER exists"
else
  say "creating account $SANDBOX_USER"
  useradd --create-home --shell /usr/sbin/nologin "$SANDBOX_USER"
fi
SANDBOX_UID="$(id -u "$SANDBOX_USER")"
if ! grep -q "^$SANDBOX_USER:" /etc/subuid || ! grep -q "^$SANDBOX_USER:" /etc/subgid; then
  # First free block above every existing range.
  start="$(cat /etc/subuid /etc/subgid | awk -F: 'BEGIN{m=99999} {e=$2+$3; if (e>m) m=e} END{print m+1}')"
  say "granting $SANDBOX_USER subordinate ids $start-$((start + 65535))"
  usermod --add-subuids "$start-$((start + 65535))" --add-subgids "$start-$((start + 65535))" "$SANDBOX_USER"
fi
loginctl enable-linger "$SANDBOX_USER"

# 2. The storage image: a fixed size, reserved now, so the sandbox can never
#    grow past it and the rest of the node can never be starved by it.
mkdir -p "$(dirname "$IMAGE")" "$MOUNT"
if [ -e "$IMAGE" ]; then
  say "storage image $IMAGE exists; leaving it alone"
else
  disk_dir="$(dirname "$IMAGE")"
  read -r total avail < <(df -B1 --output=size,avail "$disk_dir" | tail -1)
  want=$((SIZE_GB * 1024 * 1024 * 1024))
  left=$((avail - want))
  if [ "$left" -lt 0 ]; then
    die "$disk_dir has only $((avail / 1073741824)) GB free; the image needs $SIZE_GB GB"
  fi
  left_pct=$((left * 100 / total))
  if [ "$left_pct" -lt "$MIN_FREE_PERCENT" ] && [ "$FORCE" -ne 1 ]; then
    die "a ${SIZE_GB} GB image would leave ${left_pct}% free on $disk_dir (minimum ${MIN_FREE_PERCENT}%). Choose a smaller --size-gb, a different --image, or pass --force."
  fi
  say "reserving ${SIZE_GB} GB at $IMAGE (leaves ${left_pct}% free)"
  : > "$IMAGE"
  chattr +C "$IMAGE" 2>/dev/null || true   # no copy-on-write for a loop image on btrfs
  fallocate -l "${SIZE_GB}G" "$IMAGE"
  chmod 600 "$IMAGE"
  mkfs.ext4 -q -F -L nexus-sandbox "$IMAGE"
fi
fstab_line="$(render_fstab_line)"
grep -qsF "$fstab_line" /etc/fstab || { say "adding the image to /etc/fstab"; printf '%s\n' "$fstab_line" >> /etc/fstab; }
systemctl daemon-reload
mountpoint -q "$MOUNT" || mount "$MOUNT"
chown "$SANDBOX_USER:$SANDBOX_USER" "$MOUNT"
chmod 700 "$MOUNT"

# 3. The socket directory, shared with the Deploy account only.
render_tmpfiles > /etc/tmpfiles.d/nexus-deploy-sandbox.conf
systemd-tmpfiles --create /etc/tmpfiles.d/nexus-deploy-sandbox.conf

# 4. Egress filter.
install -d -m 755 /etc/nexus-deploy-sandbox
render_nft "$SANDBOX_UID" > /etc/nexus-deploy-sandbox/egress.nft
nft -c -f /etc/nexus-deploy-sandbox/egress.nft || die "the generated egress rules do not parse"
render_nft_unit > /etc/systemd/system/nexus-deploy-sandbox-egress.service
systemctl daemon-reload
systemctl enable nexus-deploy-sandbox-egress.service >/dev/null 2>&1
systemctl restart nexus-deploy-sandbox-egress.service

# 5. The daemon, as a user service of the sandbox account.
home="$(getent passwd "$SANDBOX_USER" | cut -d: -f6)"
install -d -o "$SANDBOX_USER" -g "$SANDBOX_USER" -m 700 "$home/.config" "$home/.config/docker" "$home/.config/systemd" "$home/.config/systemd/user"
render_daemon_json | install -o "$SANDBOX_USER" -g "$SANDBOX_USER" -m 600 /dev/stdin "$home/.config/docker/daemon.json"
render_unit | install -o "$SANDBOX_USER" -g "$SANDBOX_USER" -m 600 /dev/stdin "$home/.config/systemd/user/nexus-sandbox-docker.service"
systemctl start "user@$SANDBOX_UID.service"
user_env=(env "XDG_RUNTIME_DIR=/run/user/$SANDBOX_UID" "DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/$SANDBOX_UID/bus")
runuser -u "$SANDBOX_USER" -- "${user_env[@]}" systemctl --user daemon-reload
runuser -u "$SANDBOX_USER" -- "${user_env[@]}" systemctl --user enable nexus-sandbox-docker.service >/dev/null 2>&1
runuser -u "$SANDBOX_USER" -- "${user_env[@]}" systemctl --user restart nexus-sandbox-docker.service

say "waiting for the daemon"
for _ in $(seq 1 40); do [ -S "$SOCKET" ] && break; sleep 0.5; done
[ -S "$SOCKET" ] || die "the daemon did not come up; see: sudo -u $SANDBOX_USER XDG_RUNTIME_DIR=/run/user/$SANDBOX_UID journalctl --user -u nexus-sandbox-docker"
chmod 660 "$SOCKET" 2>/dev/null || true

say "done."
echo
render_env
echo
say "next: as $DEPLOY_USER, run the checks:  cd apps/Nexus-Deploy && npm run doctor"
````

**Create `sandbox/README.md`:**

````markdown
# The Nexus Deploy sandbox

Every member's app is built and run by one **rootless Docker daemon** that belongs to a
dedicated account, lives on its own size-capped filesystem, and sits behind its own
network filter. Deploy talks to that daemon only; with `SANDBOX_DOCKER_HOST` empty it
refuses to build or run anything rather than fall back to the host's Docker.

## Set it up (once, by the operator)

```bash
cd apps/Nexus-Deploy
bash sandbox/setup.sh --print --deploy-user "$USER"      # read what it will do; changes nothing
sudo bash sandbox/setup.sh --deploy-user "$USER" --size-gb 12
```

Then put the three `SANDBOX_*` lines it prints into the environment Deploy starts with,
and run `npm run doctor`. Running the setup again repairs and re-applies it; it never
shrinks or reformats an existing storage image.

### What it costs

The only resource the sandbox takes is **disk, reserved up front**: `--size-gb` gigabytes
in one file (default `/var/lib/nexus-deploy-sandbox/storage.img`). That is the whole
point: a runaway build can fill that file and fail, but it cannot fill the disk the rest
of the node runs on. The script refuses if the image would leave less than 15% of its
disk free (`--min-free-percent`); `--force` overrides that, and only the operator should.

### What it changes

| What | Where |
|---|---|
| An account `nexus-deploy` (login disabled), with a subordinate uid/gid range and lingering | `/etc/passwd`, `/etc/subuid`, `/etc/subgid` |
| The storage image and its mount | `--image`, `--mount`, one line in `/etc/fstab` |
| A socket directory shared with the Deploy account only | `/run/nexus-deploy-sandbox/`, `/etc/tmpfiles.d/nexus-deploy-sandbox.conf` |
| The daemon, as a user service of that account | `~nexus-deploy/.config/systemd/user/nexus-sandbox-docker.service`, `~nexus-deploy/.config/docker/daemon.json` |
| The egress filter | `/etc/nexus-deploy-sandbox/egress.nft`, `nexus-deploy-sandbox-egress.service` |

### The egress filter

A rootless daemon already keeps containers off the host's own loopback. It does **not**
keep them off the host's LAN address, so a container could reach every service this
machine binds to all interfaces. The filter drops the sandbox account's connections to
loopback, private and link-local ranges, except DNS to the machine's own resolvers.
If the machine's resolvers change, run the setup again; `npm run doctor` says so.

## Check it

`npm run doctor` proves each property from inside a container: rootless, log caps,
no capabilities, its own filesystem, no route to the host's loopback or LAN, and working
DNS. It exits 1 on any failure.

## Take it out

```bash
sudo systemctl disable --now nexus-deploy-sandbox-egress.service
sudo rm -f /etc/systemd/system/nexus-deploy-sandbox-egress.service /etc/tmpfiles.d/nexus-deploy-sandbox.conf
sudo rm -rf /etc/nexus-deploy-sandbox
sudo runuser -u nexus-deploy -- env XDG_RUNTIME_DIR=/run/user/$(id -u nexus-deploy) systemctl --user disable --now nexus-sandbox-docker.service
sudo umount /var/lib/nexus-deploy-sandbox/storage
sudo sed -i '\#/var/lib/nexus-deploy-sandbox/storage #d' /etc/fstab
sudo rm -rf /var/lib/nexus-deploy-sandbox
sudo userdel -r nexus-deploy
```
````

**Modify `.env.example`:**

````diff
--- a/.env.example
+++ b/.env.example
@@ -41,10 +41,15 @@ APP_PORT_RANGE=20000-29999
 
 # The rootless sandbox Docker daemon. REQUIRED: with this empty, builds and
 # containers refuse to run rather than fall back to the host's Docker.
-SANDBOX_DOCKER_HOST=unix:///run/user/1001/docker.sock
+# sandbox/setup.sh prints these three lines when it finishes.
+SANDBOX_DOCKER_HOST=unix:///run/nexus-deploy-sandbox/docker.sock
 
 # Mount point of the sandbox's size-capped storage (builds pause when it fills)
-SANDBOX_STORAGE_PATH=/var/lib/nexus-deploy-sandbox
+SANDBOX_STORAGE_PATH=/var/lib/nexus-deploy-sandbox/storage
+
+# The size the storage image was created with; `npm run doctor` fails if the
+# mounted filesystem is bigger than this.
+SANDBOX_STORAGE_SIZE_GB=12
 
 # === Ingress (how live apps are announced) ===
 # Nexus Cloud's internal address. Empty means this node has no ingress to tell.
````

- [ ] **Step 4: Run the checks**

```bash
cd apps/Nexus-Deploy
export ESBUILD_BINARY_PATH=/tmp/nexus-deploy-tools/esbuild
/tmp/nexus-deploy-tools/biome check tests/sandbox-setup.test.ts tests/egress-rules.test.ts
node node_modules/typescript/bin/tsc -p tsconfig.json --noEmit
node node_modules/typescript/bin/tsc -p tsconfig.test.json
node --import tsx --test tests/*.test.ts
```

Expected: Biome reports no errors for these files; no type errors; **233 tests, all passing**.

Also read what the script would install, as the operator will:

```bash
cd apps/Nexus-Deploy
bash sandbox/setup.sh --print --deploy-user "$USER"
```

Expected: seven sections (`daemon.json`, `nexus-sandbox-docker.service`, `tmpfiles`, `egress.nft`, `egress.service`, `fstab`, `env`). The unit contains `DOCKERD_ROOTLESS_ROOTLESSKIT_DISABLE_HOST_LOOPBACK=true`; `egress.nft` names your machine's DNS resolvers in `accept` rules before the `drop` rules.

- [ ] **Step 5: Commit**

```bash
cd apps/Nexus-Deploy
git add tests/sandbox-setup.test.ts tests/egress-rules.test.ts sandbox/setup.sh sandbox/README.md .env.example
git commit \
  -m "feat(sandbox): one setup script for the rootless daemon, capped storage and egress filter" \
  -m "setup.sh installs a dedicated account, a size-capped storage image, a rootless
daemon with capped logs and no host loopback, a socket shared with the Deploy
account only, and an nftables filter that keeps the sandbox off this machine and
the LAN. --print shows every file without root." \
  -m "Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>"
```

### Task 2: `npm run doctor`: prove the sandbox from inside a container

Ten checks, each a function of an injectable context so they are tested without Docker or a disk. They prove the properties from the inside: a container is started with the real isolation flags and asked what it can do, and a listener the doctor opens on the host (on loopback, then on every LAN address) shows whether a container can reach it. A check whose prerequisite failed is skipped, not failed. To keep the probes honest, `isolationArgs` is extracted from Plan 1's `buildRunArgs` without changing its output, so the doctor and real containers use the same flags. Verified live against a throwaway rootless daemon: it passed the eight checks that daemon satisfied and failed the two it could not (its own filesystem, the egress filter); a second daemon with the host loopback enabled failed the loopback check.

**Repository:** `apps/Nexus-Deploy` (its own git repository). Paths are relative to it.

**Files:**

- Create: `tests/doctor.test.ts`
- Modify: `src/lib/run-args.ts`
- Create: `src/doctor/types.ts`
- Create: `src/doctor/checks.ts`
- Create: `src/doctor/run.ts`
- Create: `src/doctor/context.ts`
- Create: `src/doctor.ts`
- Modify: `package.json`

**Interfaces:**

- Consumes: `ContainerLimits` and `containerLimits` (Plan 1, `src/lib/safeguards.ts`), `childEnv` (Plan 1, `src/lib/docker.ts`), `config.sandboxDockerHost` and `config.sandboxStoragePath`.
- Produces: `isolationArgs(limits)`, `Check`, `CheckResult`, `Ctx`, `CHECKS`, `runChecks`, `summarize`, `formatReport`, `exitCode`, `realContext`, and the `npm run doctor` script (`--json` for machine output; exit 1 on any failure).

- [ ] **Step 1: Write the tests**

**Create `tests/doctor.test.ts`:**

````typescript
import assert from "node:assert/strict";
import { test } from "node:test";
import { CHECKS } from "../src/doctor/checks.js";
import { exitCode, formatReport, runChecks, summarize } from "../src/doctor/run.js";
import type { CheckResult, Ctx, DockerResult, Listener } from "../src/doctor/types.js";

const ok = (stdout = ""): DockerResult => ({ code: 0, stdout, stderr: "" });

const GOOD_LOG = JSON.stringify({
  Type: "local",
  Config: { "max-size": "10m", "max-file": "2", compress: "false" },
});

/**
 * A node where everything is right. `reach` lists the targets a container could
 * connect to, which is how the fake lets a test open a hole.
 */
function fakeCtx(over: Partial<Ctx> = {}, reach: string[] = []): Ctx & { calls: string[][] } {
  const calls: string[][] = [];
  let lastTarget = "";
  const accepted = new Map<number, number>();
  let nextPort = 40000;
  const ctx: Ctx & { calls: string[][] } = {
    calls,
    sandboxHost: "unix:///run/nexus-deploy-sandbox/docker.sock",
    storagePath: "/var/lib/nexus-deploy-sandbox/storage",
    expectedStorageGb: 12,
    limits: { memory: "512m", cpus: "0.5", pids: 256 },
    probeImage: "busybox:1.36",
    async docker(args) {
      calls.push(args);
      const joined = args.join(" ");
      if (args[0] === "info" && joined.includes("{{json .}}"))
        return ok('{"ServerVersion":"29.8.2"}');
      if (args[0] === "info") return ok('["name=seccomp,profile=builtin","name=rootless"]');
      if (args[0] === "image") return ok();
      if (args[0] === "create" || args[0] === "rm") return ok();
      if (args[0] === "inspect") return ok(GOOD_LOG);
      if (joined.includes("/proc/self/status"))
        return ok("CapEff:\t0000000000000000\nNoNewPrivs:\t1\n");
      if (joined.includes("nslookup")) return ok("2\n");
      const nc = /nc -w 3 (\S+) (\d+)/.exec(joined);
      if (nc?.[1] && nc[2]) {
        lastTarget = nc[1];
        if (reach.includes(lastTarget)) {
          const port = Number(nc[2]);
          accepted.set(port, (accepted.get(port) ?? 0) + 1);
        }
        return ok();
      }
      return ok();
    },
    fileExists: () => true,
    device: (p) => (p.endsWith("/..") ? 1 : 2),
    fsUsage: () => ({ totalBytes: 12 * 1024 ** 3, freeBytes: 11 * 1024 ** 3 }),
    hostAddresses: () => ["192.168.0.10"],
    async listen(): Promise<Listener> {
      const port = nextPort++;
      return { port, accepted: () => accepted.get(port) ?? 0, close() {} };
    },
    ...over,
  };
  return ctx;
}

const byId = (results: CheckResult[], id: string) => {
  const r = results.find((x) => x.id === id);
  assert.ok(r, `no result for ${id}`);
  return r;
};

test("a correctly set-up node passes every check", async () => {
  const results = await runChecks(fakeCtx());
  assert.deepEqual(
    results.map((r) => r.status),
    results.map(() => "pass"),
  );
  assert.equal(results.length, CHECKS.length);
  assert.equal(exitCode(results), 0);
});

test("an unset sandbox is a failure that skips everything which needs it", async () => {
  const results = await runChecks(fakeCtx({ sandboxHost: "" }));
  assert.equal(byId(results, "sandbox-configured").status, "fail");
  for (const id of [
    "sandbox-daemon",
    "sandbox-rootless",
    "sandbox-probe-image",
    "sandbox-log-caps",
    "sandbox-hardening",
    "sandbox-no-host-loopback",
    "sandbox-no-lan",
    "sandbox-dns",
  ]) {
    assert.equal(byId(results, id).status, "skip", id);
  }
  assert.equal(exitCode(results), 1);
});

test("the host's own Docker daemon is never accepted as the sandbox", async () => {
  for (const path of ["/var/run/docker.sock", "/run/docker.sock"]) {
    const results = await runChecks(fakeCtx({ sandboxHost: `unix://${path}` }));
    const r = byId(results, "sandbox-configured");
    assert.equal(r.status, "fail");
    assert.match(r.detail, /host's own Docker daemon/);
  }
});

test("a missing socket, a dead daemon and a root daemon are each reported", async () => {
  const noSocket = await runChecks(fakeCtx({ fileExists: () => false }));
  assert.match(byId(noSocket, "sandbox-configured").detail, /does not exist/);

  const dead = await runChecks(
    fakeCtx({
      docker: async () => ({ code: 1, stdout: "", stderr: "Cannot connect to the Docker daemon" }),
    }),
  );
  assert.equal(byId(dead, "sandbox-daemon").status, "fail");
  assert.equal(byId(dead, "sandbox-rootless").status, "skip");

  const base = fakeCtx();
  const rootDaemon = await runChecks({
    ...base,
    docker: async (args, t) =>
      args[0] === "info" && !args.join(" ").includes("{{json .}}")
        ? ok('["name=seccomp,profile=builtin"]')
        : base.docker(args, t),
  });
  assert.equal(byId(rootDaemon, "sandbox-rootless").status, "fail");
});

test("uncapped logs and weakened containers are failures", async () => {
  const base = fakeCtx();
  const uncapped = await runChecks({
    ...base,
    docker: async (args, t) =>
      args[0] === "inspect"
        ? ok(JSON.stringify({ Type: "json-file", Config: {} }))
        : base.docker(args, t),
  });
  assert.equal(byId(uncapped, "sandbox-log-caps").status, "fail");

  for (const status of [
    "CapEff:\t00000000a80425fb\nNoNewPrivs:\t1\n",
    "CapEff:\t0000000000000000\nNoNewPrivs:\t0\n",
  ]) {
    const weak = await runChecks({
      ...base,
      docker: async (args, t) =>
        args.join(" ").includes("/proc/self/status") ? ok(status) : base.docker(args, t),
    });
    assert.equal(byId(weak, "sandbox-hardening").status, "fail", status);
  }
});

test("the probes run with the real isolation flags", async () => {
  const ctx = fakeCtx();
  await runChecks(ctx);
  const probeRuns = ctx.calls.filter((a) => a[0] === "run");
  assert.ok(probeRuns.length >= 3);
  for (const args of probeRuns) {
    for (const flag of [
      "--cap-drop",
      "ALL",
      "--security-opt",
      "no-new-privileges",
      "--pids-limit",
      "256",
    ]) {
      assert.ok(args.includes(flag), `${flag} in ${args.join(" ")}`);
    }
  }
});

test("storage must be its own filesystem, no bigger than its cap, and not nearly full", async () => {
  const shared = await runChecks(fakeCtx({ device: () => 7 }));
  assert.equal(byId(shared, "sandbox-storage").status, "fail");
  assert.match(byId(shared, "sandbox-storage").detail, /same filesystem as its parent/);

  const missing = await runChecks(fakeCtx({ device: () => null }));
  assert.equal(byId(missing, "sandbox-storage").status, "fail");

  const unset = await runChecks(fakeCtx({ storagePath: "" }));
  assert.equal(byId(unset, "sandbox-storage").status, "fail");

  const tooBig = await runChecks(
    fakeCtx({ fsUsage: () => ({ totalBytes: 40 * 1024 ** 3, freeBytes: 30 * 1024 ** 3 }) }),
  );
  assert.equal(byId(tooBig, "sandbox-storage").status, "fail");

  const nearlyFull = await runChecks(
    fakeCtx({ fsUsage: () => ({ totalBytes: 12 * 1024 ** 3, freeBytes: 1 * 1024 ** 3 }) }),
  );
  assert.equal(byId(nearlyFull, "sandbox-storage").status, "warn");
  assert.equal(exitCode(nearlyFull), 0, "a warning does not fail the doctor");
});

test("a container that reaches the host's loopback is a failure", async () => {
  const results = await runChecks(fakeCtx({}, ["10.0.2.2"]));
  const r = byId(results, "sandbox-no-host-loopback");
  assert.equal(r.status, "fail");
  assert.match(r.fix ?? "", /DISABLE_HOST_LOOPBACK/);
});

test("a container that reaches the machine through a LAN address is a failure that names it", async () => {
  const results = await runChecks(
    fakeCtx({ hostAddresses: () => ["192.168.0.10", "172.20.0.1"] }, ["172.20.0.1"]),
  );
  const r = byId(results, "sandbox-no-lan");
  assert.equal(r.status, "fail");
  assert.match(r.detail, /172\.20\.0\.1/);
  assert.ok(!r.detail.includes("192.168.0.10"));
  assert.match(r.fix ?? "", /egress filter/);
});

test("a machine with no LAN address cannot be tested and says so", async () => {
  const results = await runChecks(fakeCtx({ hostAddresses: () => [] }));
  assert.equal(byId(results, "sandbox-no-lan").status, "warn");
});

test("a sandbox that cannot resolve names is a failure, because builds would break", async () => {
  const base = fakeCtx();
  const results = await runChecks({
    ...base,
    docker: async (args, t) =>
      args.join(" ").includes("nslookup") ? ok("0\n") : base.docker(args, t),
  });
  assert.equal(byId(results, "sandbox-dns").status, "fail");
});

test("a check that throws is a failure of that check, not of the run", async () => {
  const base = fakeCtx();
  const results = await runChecks({
    ...base,
    docker: async (args, t) => {
      if (args[0] === "inspect") throw new Error("boom");
      return base.docker(args, t);
    },
  });
  assert.match(byId(results, "sandbox-log-caps").detail, /boom/);
  assert.equal(byId(results, "sandbox-dns").status, "pass");
});

test("the report names each result and ends with a tally; fixes are shown for failures only", async () => {
  const results = await runChecks(fakeCtx({ device: () => 7 }));
  const text = formatReport(results);
  assert.match(text, /FAIL {2}The sandbox has its own size-capped filesystem/);
  assert.match(text, /fix: Run sandbox\/setup\.sh again/);
  assert.match(text, /9 passed, 1 failed, 0 warnings, 0 skipped/);
  assert.deepEqual(summarize(results), { passed: 9, warnings: 0, failed: 1, skipped: 0 });
  assert.ok(!formatReport(await runChecks(fakeCtx())).includes("fix:"));
});
````

- [ ] **Step 2: Run them and watch them fail**

```bash
cd apps/Nexus-Deploy
export ESBUILD_BINARY_PATH=/tmp/nexus-deploy-tools/esbuild
node --import tsx --test tests/doctor.test.ts
```

Expected: FAIL — `Cannot find module '../src/doctor/checks.js'`.

- [ ] **Step 3: Implement**

**Modify `src/lib/run-args.ts`:**

````diff
--- a/src/lib/run-args.ts
+++ b/src/lib/run-args.ts
@@ -33,34 +33,28 @@ export function validateVolumePath(path: unknown): string {
   return path;
 }
 
-export function buildRunArgs(spec: RunSpec): string[] {
-  if (!NAME_RE.test(spec.name)) throw new Error(`unsafe container name: ${spec.name}`);
-  if (spec.volume) {
-    if (!VOLUME_NAME_RE.test(spec.volume.name))
-      throw new Error(`unsafe volume name: ${spec.volume.name}`);
-    validateVolumePath(spec.volume.path);
-  }
+/**
+ * The flags that make a container safe to run a stranger's code: no capabilities
+ * and no way to gain any, hard CPU, memory and process caps, and capped logs. A
+ * function of its own so `doctor` proves the sandbox with the very flags real
+ * containers get, not a copy that could drift.
+ */
+export function isolationArgs(limits: ContainerLimits): string[] {
   return [
-    "run",
-    "-d",
-    "--name",
-    spec.name,
-    "--restart",
-    "unless-stopped",
     // No capabilities, and no way to acquire any (setuid binaries included).
     "--cap-drop",
     "ALL",
     "--security-opt",
     "no-new-privileges",
     "--pids-limit",
-    String(spec.limits.pids),
+    String(limits.pids),
     // Swap equal to memory means no swap at all.
     "--memory",
-    spec.limits.memory,
+    limits.memory,
     "--memory-swap",
-    spec.limits.memory,
+    limits.memory,
     "--cpus",
-    spec.limits.cpus,
+    limits.cpus,
     // Docker's default driver keeps logs forever. These are capped, and the
     // charter promises we keep nothing longer than we need.
     "--log-driver",
@@ -71,6 +65,24 @@ export function buildRunArgs(spec: RunSpec): string[] {
     "max-file=2",
     "--log-opt",
     "compress=false",
+  ];
+}
+
+export function buildRunArgs(spec: RunSpec): string[] {
+  if (!NAME_RE.test(spec.name)) throw new Error(`unsafe container name: ${spec.name}`);
+  if (spec.volume) {
+    if (!VOLUME_NAME_RE.test(spec.volume.name))
+      throw new Error(`unsafe volume name: ${spec.volume.name}`);
+    validateVolumePath(spec.volume.path);
+  }
+  return [
+    "run",
+    "-d",
+    "--name",
+    spec.name,
+    "--restart",
+    "unless-stopped",
+    ...isolationArgs(spec.limits),
     // Reachable from the ingress on loopback only; never on a public interface.
     "-p",
     `127.0.0.1:${spec.hostPort}:${spec.appPort}`,
````

**Create `src/doctor/types.ts`:**

````typescript
import type { ContainerLimits } from "../lib/safeguards.js";

export type Status = "pass" | "warn" | "fail" | "skip";

export type CheckResult = {
  id: string;
  title: string;
  status: Status;
  detail: string;
  /** What to do about a warning or failure. */
  fix?: string;
};

export type DockerResult = { code: number; stdout: string; stderr: string };

/** A TCP listener the doctor opens, to see whether a container can reach it. */
export type Listener = {
  port: number;
  /** How many connections it has accepted so far. */
  accepted(): number;
  close(): void;
};

/**
 * Everything a check needs from the outside world, so each check is a plain
 * function of this and can be tested against a fake without Docker or a disk.
 */
export type Ctx = {
  /** SANDBOX_DOCKER_HOST, e.g. unix:///run/nexus-deploy-sandbox/docker.sock. Empty when unset. */
  sandboxHost: string;
  /** SANDBOX_STORAGE_PATH. Empty when unset. */
  storagePath: string;
  /** SANDBOX_STORAGE_SIZE_GB, or 0 when unknown. */
  expectedStorageGb: number;
  limits: ContainerLimits;
  /** A tiny image the probes run; pulled once if absent. */
  probeImage: string;
  /** Run the docker CLI against the sandbox daemon, and nothing else. */
  docker(args: string[], timeoutMs?: number): Promise<DockerResult>;
  fileExists(path: string): boolean;
  /** The device number of the filesystem holding `path`, or null if it does not exist. */
  device(path: string): number | null;
  fsUsage(path: string): { totalBytes: number; freeBytes: number } | null;
  /** The host's non-loopback IPv4 addresses. */
  hostAddresses(): string[];
  listen(host: string): Promise<Listener>;
};

export type Check = {
  id: string;
  title: string;
  /** Ids that must have passed for this one to mean anything; otherwise it is skipped. */
  needs?: string[];
  run(ctx: Ctx): Promise<Omit<CheckResult, "id" | "title">>;
};
````

**Create `src/doctor/checks.ts`:**

````typescript
import { isolationArgs } from "../lib/run-args.js";
import type { Check, Ctx } from "./types.js";

/** Paths where the host's own Docker daemon listens. The sandbox must never be one of them. */
const HOST_DAEMON_SOCKETS = new Set(["/var/run/docker.sock", "/run/docker.sock"]);

/** How a slirp4netns container addresses the host's loopback (disabled in the sandbox). */
const SLIRP_HOST_LOOPBACK = "10.0.2.2";

const GB = 1024 * 1024 * 1024;

const pass = (detail: string) => ({ status: "pass" as const, detail });
const fail = (detail: string, fix?: string) => ({
  status: "fail" as const,
  detail,
  ...(fix ? { fix } : {}),
});
const warn = (detail: string, fix?: string) => ({
  status: "warn" as const,
  detail,
  ...(fix ? { fix } : {}),
});

const RERUN_SETUP = "Run sandbox/setup.sh again (see apps/Nexus-Deploy/sandbox/setup.sh --help).";

function socketPathOf(host: string): string | null {
  return host.startsWith("unix://") ? host.slice("unix://".length) : null;
}

/** A container that runs `script` under the real isolation flags and is removed afterwards. */
async function probe(ctx: Ctx, script: string, extra: string[] = []) {
  return ctx.docker(
    ["run", "--rm", ...isolationArgs(ctx.limits), ...extra, ctx.probeImage, "sh", "-c", script],
    60_000,
  );
}

/** Can a container reach `host:port`, judged by whether our listener saw it connect? */
async function containerReaches(ctx: Ctx, listenOn: string, targetHost: string): Promise<boolean> {
  const listener = await ctx.listen(listenOn);
  try {
    await probe(ctx, `echo hello | nc -w 3 ${targetHost} ${listener.port} >/dev/null 2>&1; true`);
    return listener.accepted() > 0;
  } finally {
    listener.close();
  }
}

export const CHECKS: Check[] = [
  {
    id: "sandbox-configured",
    title: "The sandbox daemon is configured",
    async run(ctx) {
      if (!ctx.sandboxHost) {
        return fail(
          "SANDBOX_DOCKER_HOST is empty, so Deploy refuses to build or run anything.",
          "Set SANDBOX_DOCKER_HOST to the socket printed by sandbox/setup.sh.",
        );
      }
      const socket = socketPathOf(ctx.sandboxHost);
      if (socket && HOST_DAEMON_SOCKETS.has(socket)) {
        return fail(
          `SANDBOX_DOCKER_HOST points at the host's own Docker daemon (${socket}).`,
          "Point it at the sandbox daemon, never the host's.",
        );
      }
      if (socket && !ctx.fileExists(socket)) {
        return fail(`The socket ${socket} does not exist.`, RERUN_SETUP);
      }
      return pass(ctx.sandboxHost);
    },
  },
  {
    id: "sandbox-daemon",
    title: "The sandbox daemon answers",
    needs: ["sandbox-configured"],
    async run(ctx) {
      const res = await ctx.docker(["info", "--format", "{{json .}}"], 20_000);
      if (res.code !== 0) {
        return fail(`docker info failed: ${res.stderr.trim().split("\n")[0] ?? ""}`, RERUN_SETUP);
      }
      try {
        const info = JSON.parse(res.stdout) as { ServerVersion?: string };
        return pass(`Docker ${info.ServerVersion ?? "(unknown version)"}`);
      } catch {
        return fail("docker info gave an unreadable answer.");
      }
    },
  },
  {
    id: "sandbox-rootless",
    title: "The daemon is rootless",
    needs: ["sandbox-daemon"],
    async run(ctx) {
      const res = await ctx.docker(["info", "--format", "{{json .SecurityOptions}}"], 20_000);
      const options = res.stdout;
      return res.code === 0 && options.includes("name=rootless")
        ? pass("name=rootless is among the daemon's security options")
        : fail(
            "The daemon is not rootless: a container escape would be root on this machine.",
            RERUN_SETUP,
          );
    },
  },
  {
    id: "sandbox-probe-image",
    title: "The probe image is available",
    needs: ["sandbox-daemon"],
    async run(ctx) {
      const have = await ctx.docker(["image", "inspect", ctx.probeImage], 20_000);
      if (have.code === 0) return pass(ctx.probeImage);
      const pulled = await ctx.docker(["pull", "-q", ctx.probeImage], 120_000);
      return pulled.code === 0
        ? pass(`${ctx.probeImage} (pulled)`)
        : fail(
            `Could not pull ${ctx.probeImage}: ${pulled.stderr.trim().split("\n")[0] ?? ""}`,
            "The sandbox needs outbound access to a registry; check DNS and the egress filter.",
          );
    },
  },
  {
    id: "sandbox-log-caps",
    title: "Container logs are size-capped",
    needs: ["sandbox-probe-image"],
    async run(ctx) {
      const name = `nexus-doctor-${process.pid}-${Date.now()}`;
      const made = await ctx.docker(
        ["create", "--name", name, ...isolationArgs(ctx.limits), ctx.probeImage, "true"],
        30_000,
      );
      if (made.code !== 0) return fail(`Could not create a test container: ${made.stderr.trim()}`);
      try {
        const res = await ctx.docker(
          ["inspect", "--format", "{{json .HostConfig.LogConfig}}", name],
          20_000,
        );
        const cfg = JSON.parse(res.stdout) as { Type?: string; Config?: Record<string, string> };
        const c = cfg.Config ?? {};
        const ok =
          cfg.Type === "local" &&
          c["max-size"] === "10m" &&
          c["max-file"] === "2" &&
          c["compress"] === "false";
        return ok
          ? pass("local driver, 10m x 2 files, no compression")
          : fail(
              `Logs are not capped as the Charter requires (${res.stdout.trim()}).`,
              "The run flags carry the caps; if this fails the daemon is overriding them.",
            );
      } finally {
        await ctx.docker(["rm", "-f", name], 20_000);
      }
    },
  },
  {
    id: "sandbox-hardening",
    title: "Containers get no capabilities and cannot gain any",
    needs: ["sandbox-probe-image"],
    async run(ctx) {
      const res = await probe(ctx, "grep -E '^(CapEff|NoNewPrivs):' /proc/self/status");
      const capEff = /CapEff:\s*([0-9a-f]+)/.exec(res.stdout)?.[1];
      const noNewPrivs = /NoNewPrivs:\s*(\d)/.exec(res.stdout)?.[1];
      if (res.code !== 0 || capEff === undefined || noNewPrivs === undefined) {
        return fail(`Could not read the container's privileges: ${res.stderr.trim()}`);
      }
      return /^0+$/.test(capEff) && noNewPrivs === "1"
        ? pass(`CapEff=${capEff}, NoNewPrivs=1`)
        : fail(`CapEff=${capEff}, NoNewPrivs=${noNewPrivs}: the flags are not taking effect.`);
    },
  },
  {
    id: "sandbox-storage",
    title: "The sandbox has its own size-capped filesystem",
    async run(ctx) {
      if (!ctx.storagePath) {
        return fail(
          "SANDBOX_STORAGE_PATH is empty.",
          "Set it to the mount printed by sandbox/setup.sh.",
        );
      }
      const own = ctx.device(ctx.storagePath);
      const parent = ctx.device(`${ctx.storagePath}/..`);
      if (own === null) return fail(`${ctx.storagePath} does not exist.`, RERUN_SETUP);
      if (own === parent) {
        return fail(
          `${ctx.storagePath} is on the same filesystem as its parent, so a runaway build can fill that disk.`,
          RERUN_SETUP,
        );
      }
      const usage = ctx.fsUsage(ctx.storagePath);
      if (!usage) return fail(`Could not read the size of ${ctx.storagePath}.`);
      const gb = usage.totalBytes / GB;
      const detail = `${gb.toFixed(1)} GB, ${((usage.freeBytes / usage.totalBytes) * 100).toFixed(0)}% free`;
      if (ctx.expectedStorageGb > 0 && gb > ctx.expectedStorageGb * 1.05) {
        return fail(
          `${detail}: larger than the configured ${ctx.expectedStorageGb} GB cap.`,
          RERUN_SETUP,
        );
      }
      return usage.freeBytes / usage.totalBytes < 0.15
        ? warn(
            `${detail}: builds will pause at 85% used.`,
            "Prune images or enlarge the storage image.",
          )
        : pass(detail);
    },
  },
  {
    id: "sandbox-no-host-loopback",
    title: "Containers cannot reach the host's loopback services",
    needs: ["sandbox-probe-image"],
    async run(ctx) {
      return (await containerReaches(ctx, "127.0.0.1", SLIRP_HOST_LOOPBACK))
        ? fail(
            "A container reached a service listening on the host's loopback (Auth, Cloud and the databases live there).",
            "The daemon must run with DOCKERD_ROOTLESS_ROOTLESSKIT_DISABLE_HOST_LOOPBACK=true. " +
              RERUN_SETUP,
          )
        : pass("a loopback listener on the host was unreachable from a container");
    },
  },
  {
    id: "sandbox-no-lan",
    title: "Containers cannot reach this machine or the LAN",
    needs: ["sandbox-probe-image"],
    async run(ctx) {
      const addresses = ctx.hostAddresses();
      if (addresses.length === 0)
        return warn("This machine has no non-loopback IPv4 address to test.");
      const reached: string[] = [];
      for (const address of addresses) {
        if (await containerReaches(ctx, "0.0.0.0", address)) reached.push(address);
      }
      return reached.length === 0
        ? pass(`unreachable via ${addresses.join(", ")}`)
        : fail(
            `A container reached a host service through ${reached.join(", ")}. Any service this machine binds to all interfaces is open to members' code.`,
            "The egress filter is missing or stale. " + RERUN_SETUP,
          );
    },
  },
  {
    id: "sandbox-dns",
    title: "Containers can still resolve names",
    needs: ["sandbox-probe-image"],
    async run(ctx) {
      const res = await probe(ctx, "nslookup example.com 2>&1 | grep -c Address");
      const count = Number(res.stdout.trim().split("\n").pop());
      return res.code === 0 && count > 0
        ? pass("example.com resolved from inside a container")
        : fail(
            "A container could not resolve a name, so builds will fail to fetch dependencies.",
            "The egress filter must allow the machine's resolvers; resolvers change. " +
              RERUN_SETUP,
          );
    },
  },
];
````

**Create `src/doctor/run.ts`:**

````typescript
import { CHECKS } from "./checks.js";
import type { Check, CheckResult, Ctx } from "./types.js";

/** Run every check in order. A check whose prerequisites did not pass is skipped, not failed. */
export async function runChecks(
  ctx: Ctx,
  checks: readonly Check[] = CHECKS,
): Promise<CheckResult[]> {
  const results: CheckResult[] = [];
  const passed = new Set<string>();
  for (const check of checks) {
    const unmet = (check.needs ?? []).filter((id) => !passed.has(id));
    if (unmet.length > 0) {
      results.push({
        id: check.id,
        title: check.title,
        status: "skip",
        detail: `needs: ${unmet.join(", ")}`,
      });
      continue;
    }
    let outcome: Awaited<ReturnType<Check["run"]>>;
    try {
      outcome = await check.run(ctx);
    } catch (error) {
      outcome = { status: "fail", detail: `the check itself failed: ${(error as Error).message}` };
    }
    results.push({ id: check.id, title: check.title, ...outcome });
    if (outcome.status === "pass") passed.add(check.id);
  }
  return results;
}

export function summarize(results: readonly CheckResult[]) {
  const count = (s: CheckResult["status"]) => results.filter((r) => r.status === s).length;
  return {
    passed: count("pass"),
    warnings: count("warn"),
    failed: count("fail"),
    skipped: count("skip"),
  };
}

const MARK = { pass: "PASS", warn: "WARN", fail: "FAIL", skip: "SKIP" } as const;

export function formatReport(results: readonly CheckResult[]): string {
  const lines = ["nexus-deploy doctor", ""];
  for (const r of results) {
    lines.push(`  ${MARK[r.status]}  ${r.title}`);
    lines.push(`        ${r.detail}`);
    if (r.fix && (r.status === "fail" || r.status === "warn")) lines.push(`        fix: ${r.fix}`);
  }
  const s = summarize(results);
  lines.push(
    "",
    `${s.passed} passed, ${s.failed} failed, ${s.warnings} warnings, ${s.skipped} skipped`,
  );
  return lines.join("\n");
}

export function exitCode(results: readonly CheckResult[]): number {
  return results.some((r) => r.status === "fail") ? 1 : 0;
}
````

**Create `src/doctor/context.ts`:**

````typescript
import { spawn } from "node:child_process";
import fs from "node:fs";
import net from "node:net";
import os from "node:os";
import { config } from "../lib/config.js";
import { childEnv } from "../lib/docker.js";
import type { Ctx, DockerResult, Listener } from "./types.js";

function run(cmd: string, args: string[], timeoutMs: number): Promise<DockerResult> {
  return new Promise((resolve) => {
    const proc = spawn(cmd, args, {
      stdio: ["ignore", "pipe", "pipe"],
      // The same forced environment real builds get: the sandbox daemon, nothing else.
      env: childEnv(cmd, config.sandboxDockerHost),
    });
    let stdout = "";
    let stderr = "";
    proc.stdout.on("data", (b: Buffer) => {
      stdout += b.toString();
    });
    proc.stderr.on("data", (b: Buffer) => {
      stderr += b.toString();
    });
    const timer = setTimeout(() => proc.kill("SIGKILL"), timeoutMs);
    proc.on("error", (error) => {
      clearTimeout(timer);
      resolve({ code: 127, stdout, stderr: `${stderr}${error.message}` });
    });
    proc.on("close", (code) => {
      clearTimeout(timer);
      resolve({ code: code ?? 1, stdout, stderr });
    });
  });
}

function listen(host: string): Promise<Listener> {
  return new Promise((resolve, reject) => {
    let accepted = 0;
    const server = net.createServer((socket) => {
      accepted++;
      socket.destroy();
    });
    server.on("error", reject);
    server.listen(0, host, () => {
      resolve({
        port: (server.address() as net.AddressInfo).port,
        accepted: () => accepted,
        close: () => server.close(),
      });
    });
  });
}

export function realContext(limits: Ctx["limits"]): Ctx {
  return {
    sandboxHost: config.sandboxDockerHost,
    storagePath: config.sandboxStoragePath,
    expectedStorageGb: Number(process.env["SANDBOX_STORAGE_SIZE_GB"] ?? 0),
    limits,
    probeImage: process.env["DOCTOR_PROBE_IMAGE"] ?? "busybox:1.36",
    docker: (args, timeoutMs = 30_000) => run("docker", args, timeoutMs),
    fileExists: (p) => fs.existsSync(p),
    device: (p) => {
      try {
        return fs.statSync(p).dev;
      } catch {
        return null;
      }
    },
    fsUsage: (p) => {
      try {
        const s = fs.statfsSync(p);
        return { totalBytes: s.blocks * s.bsize, freeBytes: s.bavail * s.bsize };
      } catch {
        return null;
      }
    },
    hostAddresses: () =>
      Object.values(os.networkInterfaces())
        .flat()
        .filter((i): i is os.NetworkInterfaceInfo => Boolean(i))
        .filter((i) => i.family === "IPv4" && !i.internal)
        .map((i) => i.address),
    listen,
  };
}
````

**Create `src/doctor.ts`:**

````typescript
/**
 * `npm run doctor` — is this node able to run members' apps safely?
 *
 * Static and fast: it looks at the sandbox, not at any app. Exit status 1 if
 * anything failed; warnings do not fail it. `--json` prints the results as JSON.
 */
import { realContext } from "./doctor/context.js";
import { exitCode, formatReport, runChecks, summarize } from "./doctor/run.js";
import { config } from "./lib/config.js";
import { containerLimits } from "./lib/safeguards.js";

const results = await runChecks(realContext(containerLimits({}, config.limits)));
if (process.argv.includes("--json")) {
  console.log(
    JSON.stringify({ ok: exitCode(results) === 0, summary: summarize(results), results }, null, 2),
  );
} else {
  console.log(formatReport(results));
}
process.exit(exitCode(results));
````

**Modify `package.json`:**

````diff
--- a/package.json
+++ b/package.json
@@ -12,6 +12,7 @@
     "build:server": "tsc -p tsconfig.json",
     "start": "node dist/index.js",
     "test": "node --import tsx --test tests/*.test.ts",
+    "doctor": "node --import tsx src/doctor.ts",
     "typecheck": "tsc -p tsconfig.json --noEmit",
     "typecheck:tests": "tsc -p tsconfig.test.json",
     "lint": "biome check src web/src",
````

- [ ] **Step 4: Run the checks**

```bash
cd apps/Nexus-Deploy
export ESBUILD_BINARY_PATH=/tmp/nexus-deploy-tools/esbuild
/tmp/nexus-deploy-tools/biome check src/doctor.ts src/doctor/types.ts src/doctor/checks.ts src/doctor/run.ts src/doctor/context.ts src/lib/run-args.ts tests/doctor.test.ts
node node_modules/typescript/bin/tsc -p tsconfig.json --noEmit
node node_modules/typescript/bin/tsc -p tsconfig.test.json
node --import tsx --test tests/*.test.ts
```

Expected: Biome reports no errors for these files; no type errors; **246 tests, all passing**.

- [ ] **Step 5: Commit**

```bash
cd apps/Nexus-Deploy
git add tests/doctor.test.ts src/lib/run-args.ts src/doctor/types.ts src/doctor/checks.ts src/doctor/run.ts src/doctor/context.ts src/doctor.ts package.json
git commit \
  -m "feat(doctor): prove the sandbox from inside a container" \
  -m "Ten checks run with the real isolation flags: rootless, capped logs, no
capabilities, its own filesystem, no route to the host's loopback or LAN, and
working DNS. isolationArgs is extracted from buildRunArgs unchanged." \
  -m "Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>"
git push origin main
```

### Task 3: Bring the sandbox up on this node (operator)

**This task is for the node's operator. It needs `sudo`, which an agent does not have, and it spends disk, so it is a decision, not a step to run on autopilot.** Nothing in Tasks 1–2 installed anything; this is where the machine changes, and where the properties the tests could only check on a stand-in are proved on the real thing.

**Files:** none changed in the repository.

- [ ] **Step 1: Decide the storage size and place**

The image is reserved up front and the script refuses to leave less than 15% of a disk free. This node is already under that line on both disks:

| Disk | Size | Free | Free % | Note |
|---|---|---|---|---|
| `/` (btrfs, NVMe) | 237 GB | 35 GB | 14.8% | default home of the image; any image needs `--force` |
| `Data1` (`fuseblk`) | 902 GB | 51 GB | 5.6% | slower, and holds production; not recommended |

Pick one: (a) make room first, then use the default. The host Docker daemon holds **34 GB of build cache (17.6 GB reclaimable)** and **10.7 GB of reclaimable images**; `docker builder prune` and `docker image prune -a` are the commands, and only you should run them. (b) accept the reservation with `--force` and a small image (`--size-gb 8`). (c) move the image with `--image /path/on/another/disk.img`. A smaller image means less room for members' builds; builds pause at 85% used and the hourly prune reclaims space.

- [ ] **Step 2: Read what will be installed**

```bash
cd apps/Nexus-Deploy
bash sandbox/setup.sh --print --deploy-user "$USER" --size-gb 12
```

Expected: the seven sections described in Task 1. Check that your machine's resolvers appear in `egress.nft`.

- [ ] **Step 3: Run the setup**

```bash
cd apps/Nexus-Deploy
sudo bash sandbox/setup.sh --deploy-user "$USER" --size-gb 12      # add --force if you chose (b)
```

Expected: it creates the account, reserves the image, mounts it, installs the egress filter, starts the daemon, and prints three lines beginning `SANDBOX_`. It is safe to run again.

- [ ] **Step 4: Run the doctor as the account that runs Deploy**

```bash
cd apps/Nexus-Deploy
export SANDBOX_DOCKER_HOST=unix:///run/nexus-deploy-sandbox/docker.sock
export SANDBOX_STORAGE_PATH=/var/lib/nexus-deploy-sandbox/storage
export SANDBOX_STORAGE_SIZE_GB=12
npm run doctor
```

Expected: ten lines beginning `PASS` and `10 passed, 0 failed, 0 warnings, 0 skipped`. Running it as the Deploy account is itself the proof that the socket is shared correctly. If a line fails, its `fix:` line says what to do; the likely ones are:

| Failing line | Meaning |
|---|---|
| The sandbox daemon is configured / answers | The socket is missing or this account cannot open it (the ACL on `/run/nexus-deploy-sandbox/`) |
| The sandbox has its own size-capped filesystem | The image is not mounted at `SANDBOX_STORAGE_PATH` |
| Containers cannot reach this machine or the LAN | The egress filter is not loaded (`systemctl status nexus-deploy-sandbox-egress`) |
| Containers can still resolve names | A resolver the filter does not know; run the setup again |

- [ ] **Step 5: Prove the doctor can fail on the real thing**

```bash
sudo systemctl stop nexus-deploy-sandbox-egress.service
cd apps/Nexus-Deploy && npm run doctor | grep -E "FAIL|passed"
sudo systemctl start nexus-deploy-sandbox-egress.service
npm run doctor | grep passed
```

Expected: with the filter stopped, exactly one `FAIL` (`Containers cannot reach this machine or the LAN`) and `9 passed, 1 failed`; after starting it again, `10 passed, 0 failed`.

- [ ] **Step 6: Record the result**

Note the doctor's output and the size you chose in the Deploy repository's `sandbox/README.md` under a heading "This node", and commit it. Plan 4 starts from a sandbox that passes.
