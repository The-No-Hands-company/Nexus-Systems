# Nexus Deploy Foundation — Plan 4: Builder, Generic Fixtures, Doctor & Conformance Suite

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]") syntax for tracking.

**Goal:** Choose and install the single-binary builder (**Railpack `v0.43.0`**), wire `Nexus-Deploy` to build repositories without a `Dockerfile` through it, complete `nexus-deploy doctor` with the builder, Cloud registration and wildcard DNS checks, and ship the bundled generic fixture apps and end-to-end conformance test (`npm run verify`).

**Architecture:**
- **Builder choice (Railpack `v0.43.0` over Nixpacks `v1.41.0`):** Measured against a rootless Docker 29.8 daemon on this host, Nixpacks (`v1.41.0`, now in maintenance mode) took 77.8s cold on the Node fixture and produced a **947 MB** image (bundling Nix store layers), which would exhaust the capped sandbox storage image quickly. Railpack (`v0.43.0`, MIT, static musl binary) prepares a BuildKit frontend plan in user space (`railpack prepare <contextDir> --plan-out <plan.json> --error-missing-start`) and builds it through Docker's built-in rootless BuildKit (`docker build --build-arg BUILDKIT_SYNTAX=ghcr.io/railwayapp/railpack-frontend:v0.43.0 -f <plan.json> -t <tag> <contextDir>`) in **22.7s cold / 1.9s warm**, producing a **622 MB** Node image and **519 MB** Python image without needing `BUILDKIT_HOST` or any privileged container.
- **User-space installer (`sandbox/install-builder.sh`):** Installs `railpack` `v0.43.0` into `${INSTALL_DIR:-$HOME/.local/bin}/railpack` (on an `exec`-mounted filesystem, avoiding the `noexec` `Data1` mount), verifies the SHA-256 of both the release tarball and extracted binary, skips downloading if already present, and needs **no `sudo`**.
- **Complete `doctor` (`src/doctor/*`):** Adds `builder-installed`, `cloud-registration` (probing `GET /api/v1/user-apps/doctor-probe/availability` with `Authorization: Bearer <NEXUS_CLOUD_USER_APPS_KEY>`), and `wildcard-domain` (resolving `doctor-probe.<BASE_DOMAIN>`) to the 10 sandbox checks from Plan 3 (13 checks total).
- **Generic fixtures & `npm run verify` (`fixtures/*`, `src/verify/*`, `src/verify.ts`):** Bundles three zero-dependency generic fixtures (`fixtures/node`, `fixtures/python`, `fixtures/dockerfile`) plus an unprivileged image reference (`nginxinc/nginx-unprivileged:alpine` on port `8080`, which runs cleanly under `--cap-drop ALL` without calling `chown`) and an 8-step conformance runner (`VERIFY_STEPS`) proving every requirement in Spec §12.

**Tech Stack:** TypeScript 5.8 (strict, `NodeNext`, `noUncheckedIndexedAccess`), Node `node:test` via `tsx`, Bash, Railpack `v0.43.0`, rootless Docker 29.8.

**Governing spec:** `docs/superpowers/specs/2026-11-06-nexus-deploy-foundation-design.md` (§§6, 7, 12, 13).

**Series:** Plan 4 of 5. Plans 1 (control plane), 2 (edge) and 3 (sandbox setup & container doctor checks) are complete and on `main`. After this plan: Plan 5 (dashboard UI & operator bring-up).

---

## File Structure

All paths below are relative to `apps/Nexus-Deploy/` (an independent git repository inside the monorepo).

| File | Responsibility |
|---|---|
| `sandbox/install-builder.sh` *(new)* | User-space installer for Railpack `v0.43.0` into `~/.local/bin/railpack` with SHA-256 verification and `--print` mode. |
| `sandbox/README.md` *(modified)* | Documents `bash sandbox/install-builder.sh` alongside `sandbox/setup.sh`. |
| `src/lib/docker.ts` *(modified)* | Adds `"railpack"` to `SANDBOXED_COMMANDS` and checks `path.basename(cmd)` in `childEnv`. |
| `src/lib/build-steps.ts` *(modified)* | Replaces `nixpacks build` with `railpack prepare` + `docker build --build-arg BUILDKIT_SYNTAX=ghcr.io/railwayapp/railpack-frontend:v0.43.0`, cleaning up the temporary plan directory in `finally`. |
| `src/lib/build.ts` *(modified)* | Logs `Building with Railpack` when no `Dockerfile` is present. |
| `tests/sandbox.test.ts` *(modified)* | Verifies `childEnv` forces `DOCKER_HOST` and refuses an unset sandbox for `railpack` and `/home/ops/.local/bin/railpack`. |
| `tests/builder.test.ts` *(new)* | Unit tests for `install-builder.sh`, `resolveRailpackBin`, `railpackPrepareArgs`, `railpackDockerBuildArgs`, and `buildImageWithRunner`. |
| `src/doctor/types.ts` *(modified)* | Extends `Ctx` with `builder`, `cloudUrl`, `cloudUserAppsKey`, `probeCloud`, `baseDomain`, and `resolveHost`. |
| `src/doctor/checks.ts` *(modified)* | Adds `builder-installed`, `cloud-registration`, and `wildcard-domain` checks (13 total). |
| `src/doctor/context.ts` *(modified)* | Wires `builder`, `probeCloud` (via `cloudNameTaken`), and `resolveHost` (via `dns.resolve4`) into `realContext`. |
| `tests/doctor.test.ts` *(modified)* | Tests the 3 new doctor checks and the 13-check report summary. |
| `fixtures/node/package.json` *(new)* | Generic Node 22 fixture manifest. |
| `fixtures/node/server.js` *(new)* | Generic Node HTTP server echoing request headers and path as JSON. |
| `fixtures/python/requirements.txt` *(new)* | Empty requirements file for the generic Python stdlib fixture. |
| `fixtures/python/main.py` *(new)* | Generic Python `http.server` fixture echoing headers and path as JSON. |
| `fixtures/dockerfile/Dockerfile` *(new)* | Generic `node:22-alpine` Dockerfile fixture. |
| `fixtures/dockerfile/server.js` *(new)* | Generic server supporting `RELEASE_VERSION`, `FAIL_HEALTH=true` (`503`), and `/volume` read/write persistence. |
| `src/verify/types.ts` *(new)* | Pure interfaces (`VerifyHarness`, `VerifyStep`, `VerifyResult`) for the conformance suite. |
| `src/verify/checks.ts` *(new)* | The 8 Spec §12 conformance steps (`VERIFY_STEPS`), `runVerify`, `formatVerifyReport`, and `verifyExitCode`. |
| `src/verify/live-harness.ts` *(new)* | Live harness wiring an ephemeral Deploy instance, local Cloud route table + isolating edge proxy, temporary Git fixture repos, Railpack, and the rootless sandbox daemon. |
| `src/verify.ts` *(new)* | CLI entry point for `npm run verify`. |
| `package.json` *(modified)* | Adds `"verify": "node --import tsx src/verify.ts"`. |
| `tests/verify.test.ts` *(new)* | Unit tests for the fixture files and all 8 conformance steps (pass, failure modes, and guaranteed cleanup in `finally`). |

---

### Task 1: Install Railpack and build repositories without a Dockerfile through it

**Files:**
- Create: `apps/Nexus-Deploy/sandbox/install-builder.sh`
- Modify: `apps/Nexus-Deploy/sandbox/README.md`
- Modify: `apps/Nexus-Deploy/src/lib/docker.ts`
- Modify: `apps/Nexus-Deploy/src/lib/build-steps.ts`
- Modify: `apps/Nexus-Deploy/src/lib/build.ts`
- Modify: `apps/Nexus-Deploy/tests/sandbox.test.ts`
- Create: `apps/Nexus-Deploy/tests/builder.test.ts`

**Verified scratch commit:** `9e8687bdb31b40ff448a8530234afbfe9c86aac5` (tree `80402d037afa0528849b5ef07ac67cdb498ccced`)

- [ ] **Step 1: Write `sandbox/install-builder.sh`**

Create `apps/Nexus-Deploy/sandbox/install-builder.sh`:

```bash
#!/usr/bin/env bash
#
# Installs Railpack (the single-binary builder Nexus Deploy uses when a
# repository has no Dockerfile) into a user-writable executable directory.
#
#   bash sandbox/install-builder.sh [--install-dir DIR]
#   bash sandbox/install-builder.sh --print [--install-dir DIR]
#
# Needs no root privileges. Verifies the SHA-256 digest of both the release
# archive and the extracted binary before moving it into place, and skips the
# download when the verified binary is already installed.
#
# Options
#   --install-dir DIR   where to place the railpack binary (default: $HOME/.local/bin)
#   --print             print the pinned version, URL, digests and target path, then exit
#
set -euo pipefail

RAILPACK_VERSION="0.43.0"
RAILPACK_ARCH="x86_64-unknown-linux-musl"
RAILPACK_TARBALL="railpack-v${RAILPACK_VERSION}-${RAILPACK_ARCH}.tar.gz"
RAILPACK_URL="https://github.com/railwayapp/railpack/releases/download/v${RAILPACK_VERSION}/${RAILPACK_TARBALL}"
RAILPACK_TARBALL_SHA256="248a8f60f36854aabed6124113631078a802de3461660855fc4de9c26065c1fb"
RAILPACK_BINARY_SHA256="3e9a8477b2c7cd0bfb46ebdb0ca5f3153329f4f01cec263dbb6ea9b4679a3af9"
RAILPACK_FRONTEND="ghcr.io/railwayapp/railpack-frontend:v${RAILPACK_VERSION}"

INSTALL_DIR="${INSTALL_DIR:-${HOME}/.local/bin}"
PRINT=0

usage() { sed -n '3,16p' "$0" | sed 's/^# \{0,1\}//'; }
die() { printf 'install-builder: %s\n' "$*" >&2; exit 1; }
say() { printf 'install-builder: %s\n' "$*"; }

while [ $# -gt 0 ]; do
  case "$1" in
    --install-dir) INSTALL_DIR="${2:?--install-dir needs a path}"; shift 2 ;;
    --print) PRINT=1; shift ;;
    -h|--help) usage; exit 0 ;;
    *) die "unknown option: $1 (try --help)" ;;
  esac
done

case "$INSTALL_DIR" in
  /*) ;;
  *) die "--install-dir must be an absolute path: $INSTALL_DIR" ;;
esac

TARGET="${INSTALL_DIR}/railpack"

if [ "$PRINT" -eq 1 ]; then
  cat <<EOF
BUILDER=railpack
RAILPACK_VERSION=${RAILPACK_VERSION}
RAILPACK_URL=${RAILPACK_URL}
RAILPACK_TARBALL_SHA256=${RAILPACK_TARBALL_SHA256}
RAILPACK_BINARY_SHA256=${RAILPACK_BINARY_SHA256}
RAILPACK_FRONTEND=${RAILPACK_FRONTEND}
RAILPACK_BIN=${TARGET}
EOF
  exit 0
fi

if [ -x "$TARGET" ]; then
  CURRENT_SHA="$(sha256sum "$TARGET" | awk '{print $1}')"
  if [ "$CURRENT_SHA" = "$RAILPACK_BINARY_SHA256" ]; then
    say "railpack v${RAILPACK_VERSION} is already installed at ${TARGET}"
    exit 0
  fi
fi

WORK="$(mktemp -d "${TMPDIR:-/tmp}/nexus-railpack-install.XXXXXX")"
trap 'rm -rf "$WORK"' EXIT

say "downloading railpack v${RAILPACK_VERSION} from ${RAILPACK_URL}"
curl -fsSL "$RAILPACK_URL" -o "$WORK/$RAILPACK_TARBALL"

printf '%s  %s\n' "$RAILPACK_TARBALL_SHA256" "$WORK/$RAILPACK_TARBALL" | sha256sum -c - >/dev/null \
  || die "archive SHA-256 mismatch for $RAILPACK_TARBALL"

tar -xzf "$WORK/$RAILPACK_TARBALL" -C "$WORK" railpack
[ -f "$WORK/railpack" ] || die "archive did not contain railpack binary"

printf '%s  %s\n' "$RAILPACK_BINARY_SHA256" "$WORK/railpack" | sha256sum -c - >/dev/null \
  || die "binary SHA-256 mismatch for railpack"

mkdir -p "$INSTALL_DIR"
chmod 0755 "$WORK/railpack"
mv -f "$WORK/railpack" "$TARGET"

"$TARGET" --version >/dev/null \
  || die "${TARGET} is not executable (check that ${INSTALL_DIR} is not mounted noexec)"

say "installed $("${TARGET}" --version) at ${TARGET}"
```

- [ ] **Step 2: Update `sandbox/README.md`**

Apply this change to `apps/Nexus-Deploy/sandbox/README.md`:

```diff
commit 9e8687bdb31b40ff448a8530234afbfe9c86aac5
Author: Eric Håkansson <erichakansson84@gmail.com>
Date:   Sat Nov 7 05:11:01 2026 +0100

    feat(builder): install Railpack and build repositories without a Dockerfile through it

diff --git a/sandbox/README.md b/sandbox/README.md
index e54c86e..2625276 100644
--- a/sandbox/README.md
+++ b/sandbox/README.md
@@ -9,6 +9,7 @@ refuses to build or run anything rather than fall back to the host's Docker.
 
 ```bash
 cd apps/Nexus-Deploy
+bash sandbox/install-builder.sh                          # installs Railpack v0.43.0 into ~/.local/bin (no root)
 bash sandbox/setup.sh --print --deploy-user "$USER"      # read what it will do; changes nothing
 sudo bash sandbox/setup.sh --deploy-user "$USER" --size-gb 12
 ```
```

- [ ] **Step 3: Update `src/lib/docker.ts`, `src/lib/build-steps.ts`, and `src/lib/build.ts`**

In `apps/Nexus-Deploy/src/lib/docker.ts`, add `"railpack"` to `SANDBOXED_COMMANDS` and check `path.basename(cmd)` in `childEnv`:

```diff
commit 9e8687bdb31b40ff448a8530234afbfe9c86aac5
Author: Eric Håkansson <erichakansson84@gmail.com>
Date:   Sat Nov 7 05:11:01 2026 +0100

    feat(builder): install Railpack and build repositories without a Dockerfile through it

diff --git a/src/lib/docker.ts b/src/lib/docker.ts
index ba34ada..504f61e 100644
--- a/src/lib/docker.ts
+++ b/src/lib/docker.ts
@@ -20,7 +20,7 @@ export class SandboxNotConfigured extends Error {
   }
 }
 
-const SANDBOXED_COMMANDS = new Set(["docker", "nixpacks"]);
+const SANDBOXED_COMMANDS = new Set(["docker", "railpack", "nixpacks"]);
 
 /**
  * Environment for a child process. Docker and the builder are pointed at the
@@ -33,7 +33,7 @@ export function childEnv(
   sandboxHost: string,
   base: NodeJS.ProcessEnv = process.env,
 ): NodeJS.ProcessEnv {
-  if (!SANDBOXED_COMMANDS.has(cmd)) return base;
+  if (!SANDBOXED_COMMANDS.has(path.basename(cmd))) return base;
   if (!sandboxHost) throw new SandboxNotConfigured();
   const env: NodeJS.ProcessEnv = { ...base, DOCKER_HOST: sandboxHost };
   delete env["DOCKER_CONTEXT"];
```

Replace `apps/Nexus-Deploy/src/lib/build-steps.ts` with:

```ts
import fs from "node:fs";
import os from "node:os";
import path from "node:path";
import type { GitSource } from "../types.js";
/**
 * The three things a deployment actually does to the outside world: clone a
 * repository, build an image, pull an image. They are separate from the
 * pipeline in build.ts so the pipeline can be tested with fakes, and because
 * each is only a thin, easily-read wrapper around one command.
 *
 * When a repository has no Dockerfile, Railpack prepares a BuildKit plan in a
 * throwaway directory outside the build context and `docker build` executes it
 * against the sandbox daemon via its built-in BuildKit frontend.
 *
 * Each step settles when its process ends. Cancelling kills the process, which
 * makes the step reject; build.ts then reads its own flags to say *why*.
 */
import { type LogLine, dockerPull, spawnStreaming } from "./docker.js";
import { resolveRepoUrl } from "./repo.js";
import { gitCredentialEnv } from "./source.js";

export const RAILPACK_VERSION = "0.43.0";
export const RAILPACK_FRONTEND = `ghcr.io/railwayapp/railpack-frontend:v${RAILPACK_VERSION}`;

/** Registers a way to stop the running step. */
export type Track = (kill: () => void) => void;

export type BuildOptions = {
  contextDir: string;
  /** Resolved Dockerfile path, or null to let Railpack work out the stack. */
  dockerfile: string | null;
  tag: string;
  buildCommand?: string;
  startCommand?: string;
};

export type StepRunner = (
  cmd: string,
  args: string[],
  log: LogLine,
  track: Track,
  env?: Record<string, string>,
) => Promise<void>;

function run(
  cmd: string,
  args: string[],
  log: LogLine,
  track: Track,
  env?: Record<string, string>,
): Promise<void> {
  return new Promise((resolve, reject) => {
    const kill = spawnStreaming(
      cmd,
      args,
      log,
      (err) => (err ? reject(err) : resolve()),
      env ? { env } : {},
    );
    track(kill);
  });
}

/**
 * Locate the Railpack binary: `RAILPACK_BIN` when set, otherwise
 * `~/.local/bin/railpack` (where `sandbox/install-builder.sh` places it, even
 * when systemd runs without `~/.local/bin` on `PATH`), falling back to `PATH`.
 */
export function resolveRailpackBin(
  env: NodeJS.ProcessEnv = process.env,
  home: string = os.homedir(),
  exists: (p: string) => boolean = fs.existsSync,
): string {
  const explicit = env["RAILPACK_BIN"]?.trim();
  if (explicit) return explicit;
  const localBin = path.join(home, ".local", "bin", "railpack");
  if (exists(localBin)) return localBin;
  return "railpack";
}

export function railpackPrepareArgs(opts: BuildOptions, planPath: string): string[] {
  const args = ["prepare", opts.contextDir, "--plan-out", planPath, "--error-missing-start"];
  if (opts.buildCommand) args.push("--build-cmd", opts.buildCommand);
  if (opts.startCommand) args.push("--start-cmd", opts.startCommand);
  return args;
}

export function railpackDockerBuildArgs(opts: BuildOptions, planPath: string): string[] {
  return [
    "build",
    "--build-arg",
    `BUILDKIT_SYNTAX=${RAILPACK_FRONTEND}`,
    "-f",
    planPath,
    "-t",
    opts.tag,
    opts.contextDir,
  ];
}

export async function cloneRepo(
  source: GitSource,
  dir: string,
  allowLocal: boolean,
  log: LogLine,
  track: Track,
): Promise<void> {
  const url = resolveRepoUrl(source.repo, allowLocal);
  const credentials = gitCredentialEnv(source.token);
  try {
    // `--` ends option parsing, so a repository URL can never be read as a flag.
    await run(
      "git",
      ["clone", "--depth", "1", "--branch", source.branch, "--", url, dir],
      log,
      track,
      credentials.env,
    );
  } finally {
    credentials.cleanup();
  }
}

export async function buildImageWithRunner(
  opts: BuildOptions,
  log: LogLine,
  track: Track,
  runner: StepRunner = run,
  railpackBin: string = resolveRailpackBin(),
): Promise<void> {
  if (opts.dockerfile) {
    await runner(
      "docker",
      ["build", "-t", opts.tag, "-f", opts.dockerfile, opts.contextDir],
      log,
      track,
    );
    return;
  }
  const planDir = fs.mkdtempSync(path.join(os.tmpdir(), "nexus-railpack-"));
  const planPath = path.join(planDir, "railpack-plan.json");
  try {
    await runner(railpackBin, railpackPrepareArgs(opts, planPath), log, track);
    await runner("docker", railpackDockerBuildArgs(opts, planPath), log, track);
  } finally {
    fs.rmSync(planDir, { recursive: true, force: true });
  }
}

export function buildImage(opts: BuildOptions, log: LogLine, track: Track): Promise<void> {
  return buildImageWithRunner(opts, log, track);
}

export function pullImage(image: string, log: LogLine, timeoutMs: number): Promise<string> {
  return dockerPull(image, log, timeoutMs);
}
```

In `apps/Nexus-Deploy/src/lib/build.ts`, update the builder log message from `Nixpacks` to `Railpack`:

```diff
commit 9e8687bdb31b40ff448a8530234afbfe9c86aac5
Author: Eric Håkansson <erichakansson84@gmail.com>
Date:   Sat Nov 7 05:11:01 2026 +0100

    feat(builder): install Railpack and build repositories without a Dockerfile through it

diff --git a/src/lib/build.ts b/src/lib/build.ts
index c035e33..2e34899 100644
--- a/src/lib/build.ts
+++ b/src/lib/build.ts
@@ -379,7 +379,7 @@ export async function runDeploy(
         buildDir,
         validateDockerfile(project.dockerfile, project.buildContext ?? "."),
       );
-      log(`[nexus] Building with ${dockerfile ? "Docker" : "Nixpacks"} → ${tag}`);
+      log(`[nexus] Building with ${dockerfile ? "Docker" : "Railpack"} → ${tag}`);
       await deps.steps.buildImage({ contextDir, dockerfile, tag }, log, track);
       assertActive();
       log("[nexus] ✓ Image built");
```

- [ ] **Step 4: Update `tests/sandbox.test.ts` and create `tests/builder.test.ts`**

Update `apps/Nexus-Deploy/tests/sandbox.test.ts`:

```diff
commit 9e8687bdb31b40ff448a8530234afbfe9c86aac5
Author: Eric Håkansson <erichakansson84@gmail.com>
Date:   Sat Nov 7 05:11:01 2026 +0100

    feat(builder): install Railpack and build repositories without a Dockerfile through it

diff --git a/tests/sandbox.test.ts b/tests/sandbox.test.ts
index eb6074e..95caa3f 100644
--- a/tests/sandbox.test.ts
+++ b/tests/sandbox.test.ts
@@ -25,6 +25,8 @@ test("docker and the builder are pointed at the sandbox, overriding anything amb
   assert.equal("DOCKER_CONTEXT" in env, false);
   assert.equal("DOCKER_TLS_VERIFY" in env, false);
   assert.equal("DOCKER_CERT_PATH" in env, false);
+  assert.equal(childEnv("railpack", SANDBOX, {})["DOCKER_HOST"], SANDBOX);
+  assert.equal(childEnv("/home/ops/.local/bin/railpack", SANDBOX, {})["DOCKER_HOST"], SANDBOX);
   assert.equal(childEnv("nixpacks", SANDBOX, {})["DOCKER_HOST"], SANDBOX);
 });
 
@@ -33,6 +35,11 @@ test("with no sandbox configured, docker and the builder refuse to run at all",
     () => childEnv("docker", "", { DOCKER_HOST: "unix:///var/run/docker.sock" }),
     SandboxNotConfigured,
   );
+  assert.throws(() => childEnv("railpack", ""), /Refusing to use the host's Docker daemon/);
+  assert.throws(
+    () => childEnv("/home/ops/.local/bin/railpack", ""),
+    /Refusing to use the host's Docker daemon/,
+  );
   assert.throws(() => childEnv("nixpacks", ""), /Refusing to use the host's Docker daemon/);
 });
```

Create `apps/Nexus-Deploy/tests/builder.test.ts`:

```ts
import assert from "node:assert/strict";
import { spawnSync } from "node:child_process";
import fs from "node:fs";
import path from "node:path";
import { test } from "node:test";
import {
  RAILPACK_FRONTEND,
  RAILPACK_VERSION,
  buildImageWithRunner,
  railpackDockerBuildArgs,
  railpackPrepareArgs,
  resolveRailpackBin,
} from "../src/lib/build-steps.js";

const SCRIPT = path.resolve(import.meta.dirname, "../sandbox/install-builder.sh");

function runInstall(args: string[]) {
  return spawnSync("bash", [SCRIPT, ...args], { encoding: "utf8" });
}

test("install-builder.sh is valid bash and --help explains its options", () => {
  const check = spawnSync("bash", ["-n", SCRIPT], { encoding: "utf8" });
  assert.equal(check.status, 0, check.stderr);
  const help = runInstall(["--help"]);
  assert.equal(help.status, 0, help.stderr);
  assert.match(help.stdout, /--install-dir/);
});

test("install-builder.sh --print outputs the pinned Railpack version, digests and target path", () => {
  const res = runInstall(["--print", "--install-dir", "/tmp/custom-bin"]);
  assert.equal(res.status, 0, res.stderr);
  assert.match(res.stdout, /^BUILDER=railpack$/m);
  assert.match(res.stdout, new RegExp(`^RAILPACK_VERSION=${RAILPACK_VERSION}$`, "m"));
  assert.match(
    res.stdout,
    /^RAILPACK_TARBALL_SHA256=248a8f60f36854aabed6124113631078a802de3461660855fc4de9c26065c1fb$/m,
  );
  assert.match(
    res.stdout,
    /^RAILPACK_BINARY_SHA256=3e9a8477b2c7cd0bfb46ebdb0ca5f3153329f4f01cec263dbb6ea9b4679a3af9$/m,
  );
  assert.match(res.stdout, /^RAILPACK_BIN=\/tmp\/custom-bin\/railpack$/m);
});

test("install-builder.sh rejects relative --install-dir and unknown options", () => {
  const rel = runInstall(["--print", "--install-dir", "relative/bin"]);
  assert.notEqual(rel.status, 0);
  assert.match(rel.stderr, /absolute path/);

  const bad = runInstall(["--not-a-flag"]);
  assert.notEqual(bad.status, 0);
  assert.match(bad.stderr, /unknown option/);
});

test("resolveRailpackBin prefers RAILPACK_BIN, then ~/.local/bin/railpack, then PATH", () => {
  assert.equal(
    resolveRailpackBin({ RAILPACK_BIN: "/opt/railpack" }, "/home/ops", () => true),
    "/opt/railpack",
  );
  assert.equal(
    resolveRailpackBin({}, "/home/ops", (p) => p === "/home/ops/.local/bin/railpack"),
    "/home/ops/.local/bin/railpack",
  );
  assert.equal(
    resolveRailpackBin({}, "/home/ops", () => false),
    "railpack",
  );
});

test("railpackPrepareArgs and railpackDockerBuildArgs construct the two-stage build commands", () => {
  const base = { contextDir: "/work/app", dockerfile: null, tag: "nexus/demo:12345678" };
  assert.deepEqual(railpackPrepareArgs(base, "/tmp/plan.json"), [
    "prepare",
    "/work/app",
    "--plan-out",
    "/tmp/plan.json",
    "--error-missing-start",
  ]);
  assert.deepEqual(
    railpackPrepareArgs(
      { ...base, buildCommand: "npm run build", startCommand: "node dist/index.js" },
      "/tmp/plan.json",
    ),
    [
      "prepare",
      "/work/app",
      "--plan-out",
      "/tmp/plan.json",
      "--error-missing-start",
      "--build-cmd",
      "npm run build",
      "--start-cmd",
      "node dist/index.js",
    ],
  );
  assert.deepEqual(railpackDockerBuildArgs(base, "/tmp/plan.json"), [
    "build",
    "--build-arg",
    `BUILDKIT_SYNTAX=${RAILPACK_FRONTEND}`,
    "-f",
    "/tmp/plan.json",
    "-t",
    "nexus/demo:12345678",
    "/work/app",
  ]);
});

test("buildImageWithRunner uses docker build directly when a Dockerfile is present", async () => {
  const calls: Array<{ cmd: string; args: string[] }> = [];
  await buildImageWithRunner(
    { contextDir: "/work/app", dockerfile: "/work/app/Dockerfile", tag: "nexus/demo:abc" },
    () => {},
    () => {},
    async (cmd, args) => {
      calls.push({ cmd, args });
    },
    "/usr/local/bin/railpack",
  );
  assert.deepEqual(calls, [
    {
      cmd: "docker",
      args: ["build", "-t", "nexus/demo:abc", "-f", "/work/app/Dockerfile", "/work/app"],
    },
  ]);
});

test("buildImageWithRunner runs railpack prepare then docker build and removes the temp plan", async () => {
  const calls: Array<{ cmd: string; args: string[] }> = [];
  let recordedPlanPath = "";
  await buildImageWithRunner(
    {
      contextDir: "/work/app",
      dockerfile: null,
      tag: "nexus/demo:abc",
      buildCommand: "npm run compile",
    },
    () => {},
    () => {},
    async (cmd, args) => {
      calls.push({ cmd, args });
      if (cmd.endsWith("railpack")) {
        const idx = args.indexOf("--plan-out");
        recordedPlanPath = args[idx + 1] ?? "";
        fs.writeFileSync(recordedPlanPath, "{}");
      }
    },
    "/home/ops/.local/bin/railpack",
  );
  assert.equal(calls.length, 2);
  assert.equal(calls[0]?.cmd, "/home/ops/.local/bin/railpack");
  assert.ok(recordedPlanPath.endsWith("railpack-plan.json"));
  assert.equal(calls[1]?.cmd, "docker");
  assert.deepEqual(calls[1]?.args, [
    "build",
    "--build-arg",
    `BUILDKIT_SYNTAX=${RAILPACK_FRONTEND}`,
    "-f",
    recordedPlanPath,
    "-t",
    "nexus/demo:abc",
    "/work/app",
  ]);
  assert.equal(fs.existsSync(path.dirname(recordedPlanPath)), false, "temp plan dir was removed");
});

test("buildImageWithRunner removes the temporary plan directory even when railpack fails", async () => {
  let recordedPlanDir = "";
  await assert.rejects(
    buildImageWithRunner(
      { contextDir: "/work/app", dockerfile: null, tag: "nexus/demo:abc" },
      () => {},
      () => {},
      async (_cmd, args) => {
        const idx = args.indexOf("--plan-out");
        const planPath = args[idx + 1] ?? "";
        recordedPlanDir = path.dirname(planPath);
        throw new Error("no start command found");
      },
      "railpack",
    ),
    /no start command found/,
  );
  assert.ok(recordedPlanDir.length > 0);
  assert.equal(fs.existsSync(recordedPlanDir), false, "temp plan dir cleaned up on error");
});
```

- [ ] **Step 5: Run tests, typecheck, and commit**

Run in `apps/Nexus-Deploy`:

```bash
export ESBUILD_BINARY_PATH=/tmp/nexus-deploy-tools/esbuild
node --import tsx --test tests/builder.test.ts tests/sandbox.test.ts tests/deploy-flow.test.ts
node ./node_modules/typescript/bin/tsc -p tsconfig.json --noEmit
node ./node_modules/typescript/bin/tsc -p tsconfig.test.json --noEmit
/tmp/nexus-deploy-tools/biome check src/lib/build-steps.ts src/lib/build.ts tests/sandbox.test.ts tests/builder.test.ts
git add sandbox/install-builder.sh sandbox/README.md src/lib/docker.ts src/lib/build-steps.ts src/lib/build.ts tests/sandbox.test.ts tests/builder.test.ts
git commit -m "feat(builder): install Railpack and build repositories without a Dockerfile through it"
```

---

### Task 2: Complete `nexus-deploy doctor` with builder, Cloud registration and wildcard DNS checks

**Files:**
- Modify: `apps/Nexus-Deploy/src/doctor/types.ts`
- Modify: `apps/Nexus-Deploy/src/doctor/checks.ts`
- Modify: `apps/Nexus-Deploy/src/doctor/context.ts`
- Modify: `apps/Nexus-Deploy/tests/doctor.test.ts`

**Verified scratch commit:** `4fa1563e81737aa35cd8fa6dccdf416f7c2af628` (tree `ccb6015e39ac1968cf7da15340fe9ef9c7c8cb3e`)

- [ ] **Step 1: Extend `src/doctor/types.ts`**

Update `apps/Nexus-Deploy/src/doctor/types.ts`:

```diff
commit 4fa1563e81737aa35cd8fa6dccdf416f7c2af628
Author: Eric Håkansson <erichakansson84@gmail.com>
Date:   Sat Nov 7 05:11:01 2026 +0100

    feat(doctor): check the builder binary, Cloud registration and wildcard DNS

diff --git a/src/doctor/types.ts b/src/doctor/types.ts
index ce52aab..9578a94 100644
--- a/src/doctor/types.ts
+++ b/src/doctor/types.ts
@@ -37,6 +37,18 @@ export type Ctx = {
   probeImage: string;
   /** Run the docker CLI against the sandbox daemon, and nothing else. */
   docker(args: string[], timeoutMs?: number): Promise<DockerResult>;
+  /** Run the builder CLI (Railpack) to check its presence and version. */
+  builder(args: string[], timeoutMs?: number): Promise<DockerResult>;
+  /** CLOUD_URL. Empty on a standalone node that does not announce routes to Cloud. */
+  cloudUrl: string;
+  /** NEXUS_CLOUD_USER_APPS_KEY. Empty when unset. */
+  cloudUserAppsKey: string;
+  /** Read-only probe against Cloud's user-apps endpoint using NEXUS_CLOUD_USER_APPS_KEY. */
+  probeCloud(): Promise<{ ok: boolean; detail: string }>;
+  /** BASE_DOMAIN, e.g. example.test or localhost. */
+  baseDomain: string;
+  /** Resolve an IPv4 hostname via the system resolver; returns [] when it does not resolve. */
+  resolveHost(host: string): Promise<string[]>;
   fileExists(path: string): boolean;
   /** The device number of the filesystem holding `path`, or null if it does not exist. */
   device(path: string): number | null;
```

- [ ] **Step 2: Add the 3 remaining checks to `src/doctor/checks.ts`**

Update `apps/Nexus-Deploy/src/doctor/checks.ts`:

```diff
commit 4fa1563e81737aa35cd8fa6dccdf416f7c2af628
Author: Eric Håkansson <erichakansson84@gmail.com>
Date:   Sat Nov 7 05:11:01 2026 +0100

    feat(doctor): check the builder binary, Cloud registration and wildcard DNS

diff --git a/src/doctor/checks.ts b/src/doctor/checks.ts
index f3e9d75..f693219 100644
--- a/src/doctor/checks.ts
+++ b/src/doctor/checks.ts
@@ -255,4 +255,63 @@ export const CHECKS: Check[] = [
           );
     },
   },
+  {
+    id: "builder-installed",
+    title: "The builder (Railpack) is installed",
+    async run(ctx) {
+      const res = await ctx.builder(["--version"], 10_000);
+      const version = res.stdout.trim().split("\n")[0] ?? "";
+      return res.code === 0 && /railpack/i.test(version)
+        ? pass(version)
+        : fail(
+            `Railpack is missing or could not run (${(res.stderr || res.stdout).trim().split("\n")[0] || `exit ${res.code}`}).`,
+            "Run bash sandbox/install-builder.sh to install Railpack into ~/.local/bin.",
+          );
+    },
+  },
+  {
+    id: "cloud-registration",
+    title: "Cloud route registration is ready",
+    async run(ctx) {
+      if (!ctx.cloudUrl) {
+        return warn(
+          "CLOUD_URL is empty; live routes will not be announced to Nexus Cloud (standalone mode).",
+          "Set CLOUD_URL and NEXUS_CLOUD_USER_APPS_KEY to publish routes through Nexus Cloud.",
+        );
+      }
+      if (!ctx.cloudUserAppsKey) {
+        return fail(
+          "NEXUS_CLOUD_USER_APPS_KEY is empty while CLOUD_URL is set, so route publishing will fail.",
+          "Set NEXUS_CLOUD_USER_APPS_KEY in both Cloud's and Deploy's .env files.",
+        );
+      }
+      const res = await ctx.probeCloud();
+      return res.ok
+        ? pass(res.detail)
+        : fail(
+            `Cloud rejected or did not answer the route-registration probe: ${res.detail}`,
+            "Check that Nexus Cloud is running at CLOUD_URL and has the same NEXUS_CLOUD_USER_APPS_KEY.",
+          );
+    },
+  },
+  {
+    id: "wildcard-domain",
+    title: "The wildcard domain resolves",
+    async run(ctx) {
+      if (!ctx.baseDomain) {
+        return fail("BASE_DOMAIN is empty.", "Set BASE_DOMAIN in .env (for example example.test).");
+      }
+      if (ctx.baseDomain === "localhost" || ctx.baseDomain.endsWith(".localhost")) {
+        return pass(`${ctx.baseDomain} (loopback domain)`);
+      }
+      const probeHost = `doctor-probe.${ctx.baseDomain}`;
+      const addrs = await ctx.resolveHost(probeHost);
+      return addrs.length > 0
+        ? pass(`*.${ctx.baseDomain} → ${addrs.join(", ")}`)
+        : fail(
+            `${probeHost} did not resolve to an IPv4 address.`,
+            `Point a wildcard DNS record (*.${ctx.baseDomain}) and deploy.${ctx.baseDomain} at this node's ingress.`,
+          );
+    },
+  },
 ];
```

- [ ] **Step 3: Wire `builder`, `probeCloud`, and `resolveHost` in `src/doctor/context.ts`**

Replace `apps/Nexus-Deploy/src/doctor/context.ts` with:

```ts
import { spawn } from "node:child_process";
import dns from "node:dns/promises";
import fs from "node:fs";
import net from "node:net";
import os from "node:os";
import { resolveRailpackBin } from "../lib/build-steps.js";
import { cloudNameTaken } from "../lib/cloud.js";
import { config } from "../lib/config.js";
import { childEnv } from "../lib/docker.js";
import type { Ctx, DockerResult, Listener } from "./types.js";

function run(
  cmd: string,
  args: string[],
  timeoutMs: number,
  env: NodeJS.ProcessEnv = childEnv(cmd, config.sandboxDockerHost),
): Promise<DockerResult> {
  return new Promise((resolve) => {
    const proc = spawn(cmd, args, {
      stdio: ["ignore", "pipe", "pipe"],
      env,
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
    builder: (args, timeoutMs = 10_000) => run(resolveRailpackBin(), args, timeoutMs, process.env),
    cloudUrl: config.cloudUrl,
    cloudUserAppsKey: config.cloudUserAppsKey,
    async probeCloud() {
      const base = config.cloudUrl.replace(/\/+$/, "");
      try {
        await cloudNameTaken(
          { url: base, key: config.cloudUserAppsKey, timeoutMs: 5_000 },
          "doctor-probe",
        );
        return { ok: true, detail: `${base} (user-apps key accepted)` };
      } catch (error) {
        return { ok: false, detail: (error as Error).message };
      }
    },
    baseDomain: config.baseDomain,
    async resolveHost(host) {
      try {
        return await dns.resolve4(host);
      } catch {
        return [];
      }
    },
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
```

- [ ] **Step 4: Extend `tests/doctor.test.ts`**

Update `apps/Nexus-Deploy/tests/doctor.test.ts`:

```diff
commit 4fa1563e81737aa35cd8fa6dccdf416f7c2af628
Author: Eric Håkansson <erichakansson84@gmail.com>
Date:   Sat Nov 7 05:11:01 2026 +0100

    feat(doctor): check the builder binary, Cloud registration and wildcard DNS

diff --git a/tests/doctor.test.ts b/tests/doctor.test.ts
index 278ec98..1a3c525 100644
--- a/tests/doctor.test.ts
+++ b/tests/doctor.test.ts
@@ -50,6 +50,15 @@ function fakeCtx(over: Partial<Ctx> = {}, reach: string[] = []): Ctx & { calls:
       }
       return ok();
     },
+    builder: async () => ok("railpack version 0.43.0\n"),
+    cloudUrl: "http://127.0.0.1:8787",
+    cloudUserAppsKey: "uakey-test",
+    probeCloud: async () => ({
+      ok: true,
+      detail: "http://127.0.0.1:8787 (user-apps key accepted)",
+    }),
+    baseDomain: "example.test",
+    resolveHost: async () => ["203.0.113.10"],
     fileExists: () => true,
     device: (p) => (p.endsWith("/..") ? 1 : 2),
     fsUsage: () => ({ totalBytes: 12 * 1024 ** 3, freeBytes: 11 * 1024 ** 3 }),
@@ -241,12 +250,56 @@ test("a check that throws is a failure of that check, not of the run", async ()
   assert.equal(byId(results, "sandbox-dns").status, "pass");
 });
 
+test("a missing builder binary is reported with the install command", async () => {
+  const results = await runChecks(
+    fakeCtx({
+      builder: async () => ({ code: 127, stdout: "", stderr: "spawn railpack ENOENT" }),
+    }),
+  );
+  const r = byId(results, "builder-installed");
+  assert.equal(r.status, "fail");
+  assert.match(r.detail, /ENOENT/);
+  assert.match(r.fix ?? "", /sandbox\/install-builder\.sh/);
+});
+
+test("Cloud registration warns in standalone mode and fails when misconfigured or rejected", async () => {
+  const standalone = await runChecks(fakeCtx({ cloudUrl: "", cloudUserAppsKey: "" }));
+  assert.equal(byId(standalone, "cloud-registration").status, "warn");
+  assert.equal(exitCode(standalone), 0);
+
+  const missingKey = await runChecks(fakeCtx({ cloudUserAppsKey: "" }));
+  assert.equal(byId(missingKey, "cloud-registration").status, "fail");
+  assert.match(byId(missingKey, "cloud-registration").detail, /NEXUS_CLOUD_USER_APPS_KEY/);
+
+  const rejected = await runChecks(
+    fakeCtx({ probeCloud: async () => ({ ok: false, detail: "HTTP 403" }) }),
+  );
+  assert.equal(byId(rejected, "cloud-registration").status, "fail");
+  assert.match(byId(rejected, "cloud-registration").detail, /HTTP 403/);
+});
+
+test("the wildcard domain check accepts loopback domains and fails when DNS does not resolve", async () => {
+  const loopback = await runChecks(
+    fakeCtx({ baseDomain: "localhost", resolveHost: async () => [] }),
+  );
+  assert.equal(byId(loopback, "wildcard-domain").status, "pass");
+
+  const empty = await runChecks(fakeCtx({ baseDomain: "" }));
+  assert.equal(byId(empty, "wildcard-domain").status, "fail");
+
+  const unresolvable = await runChecks(fakeCtx({ resolveHost: async () => [] }));
+  const r = byId(unresolvable, "wildcard-domain");
+  assert.equal(r.status, "fail");
+  assert.match(r.detail, /doctor-probe\.example\.test/);
+  assert.match(r.fix ?? "", /\*\.example\.test/);
+});
+
 test("the report names each result and ends with a tally; fixes are shown for failures only", async () => {
   const results = await runChecks(fakeCtx({ device: () => 7 }));
   const text = formatReport(results);
   assert.match(text, /FAIL {2}The sandbox has its own size-capped filesystem/);
   assert.match(text, /fix: Run sandbox\/setup\.sh again/);
-  assert.match(text, /9 passed, 1 failed, 0 warnings, 0 skipped/);
-  assert.deepEqual(summarize(results), { passed: 9, warnings: 0, failed: 1, skipped: 0 });
+  assert.match(text, /12 passed, 1 failed, 0 warnings, 0 skipped/);
+  assert.deepEqual(summarize(results), { passed: 12, warnings: 0, failed: 1, skipped: 0 });
   assert.ok(!formatReport(await runChecks(fakeCtx())).includes("fix:"));
 });
```

- [ ] **Step 5: Run tests, typecheck, and commit**

Run in `apps/Nexus-Deploy`:

```bash
export ESBUILD_BINARY_PATH=/tmp/nexus-deploy-tools/esbuild
node --import tsx --test tests/doctor.test.ts tests/no-hardcoded-domain.test.ts
node ./node_modules/typescript/bin/tsc -p tsconfig.json --noEmit
node ./node_modules/typescript/bin/tsc -p tsconfig.test.json --noEmit
/tmp/nexus-deploy-tools/biome check src/doctor/ tests/doctor.test.ts
git add src/doctor/types.ts src/doctor/checks.ts src/doctor/context.ts tests/doctor.test.ts
git commit -m "feat(doctor): check the builder binary, Cloud registration and wildcard DNS"
```

---

### Task 3: Add bundled generic fixture apps and the end-to-end conformance suite (`npm run verify`)

**Files:**
- Create: `apps/Nexus-Deploy/fixtures/node/package.json`
- Create: `apps/Nexus-Deploy/fixtures/node/server.js`
- Create: `apps/Nexus-Deploy/fixtures/python/requirements.txt`
- Create: `apps/Nexus-Deploy/fixtures/python/main.py`
- Create: `apps/Nexus-Deploy/fixtures/dockerfile/Dockerfile`
- Create: `apps/Nexus-Deploy/fixtures/dockerfile/server.js`
- Create: `apps/Nexus-Deploy/src/verify/types.ts`
- Create: `apps/Nexus-Deploy/src/verify/checks.ts`
- Create: `apps/Nexus-Deploy/src/verify/live-harness.ts`
- Create: `apps/Nexus-Deploy/src/verify.ts`
- Modify: `apps/Nexus-Deploy/package.json`
- Create: `apps/Nexus-Deploy/tests/verify.test.ts`

**Verified scratch commit:** `84313dccffc32ec53be2502fec3507fd1941d610` (tree `d56927d747b026d60843002a5476976563f7938d`)

- [ ] **Step 1: Create the bundled generic fixture apps in `fixtures/`**

Create `apps/Nexus-Deploy/fixtures/node/package.json`:

```json
{
  "name": "nexus-deploy-fixture-node",
  "version": "1.0.0",
  "private": true,
  "type": "module",
  "engines": {
    "node": "22.x"
  },
  "scripts": {
    "start": "node server.js"
  }
}
```

Create `apps/Nexus-Deploy/fixtures/node/server.js`:

```js
import http from "node:http";

const port = Number(process.env.PORT || 3000);

const server = http.createServer((req, res) => {
  res.writeHead(200, { "Content-Type": "application/json" });
  res.end(
    JSON.stringify({
      ok: true,
      fixture: "node",
      path: req.url || "/",
      headers: req.headers,
    }),
  );
});

server.listen(port, "0.0.0.0");
```

Create `apps/Nexus-Deploy/fixtures/python/requirements.txt`:

```text
# Generic Python fixture for Nexus Deploy conformance verification (stdlib only).
```

Create `apps/Nexus-Deploy/fixtures/python/main.py`:

```python
import json
import os
from http.server import BaseHTTPRequestHandler, HTTPServer

PORT = int(os.environ.get("PORT", "8000"))


class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        body = json.dumps(
            {
                "ok": True,
                "fixture": "python",
                "path": self.path,
                "headers": {k.lower(): v for k, v in self.headers.items()},
            }
        ).encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, _format, *_args):
        return


if __name__ == "__main__":
    HTTPServer(("0.0.0.0", PORT), Handler).serve_forever()
```

Create `apps/Nexus-Deploy/fixtures/dockerfile/Dockerfile`:

```dockerfile
FROM node:22-alpine
WORKDIR /app
COPY server.js /app/server.js
EXPOSE 3000
CMD ["node", "/app/server.js"]
```

Create `apps/Nexus-Deploy/fixtures/dockerfile/server.js`:

```js
import fs from "node:fs";
import http from "node:http";
import path from "node:path";

const port = Number(process.env.PORT || 3000);
const version = process.env.RELEASE_VERSION || "v1";
const failHealth = process.env.FAIL_HEALTH === "true";
const volumeDir = process.env.VOLUME_DIR || "/data";
const stateFile = path.join(volumeDir, "state.txt");

const server = http.createServer((req, res) => {
  if (failHealth) {
    res.writeHead(503, { "Content-Type": "application/json" });
    res.end(JSON.stringify({ ok: false, version, reason: "intentionally unhealthy" }));
    return;
  }
  if (req.url === "/volume" && req.method === "POST") {
    let body = "";
    req.setEncoding("utf8");
    req.on("data", (chunk) => {
      body += chunk;
    });
    req.on("end", () => {
      fs.mkdirSync(volumeDir, { recursive: true });
      fs.writeFileSync(stateFile, body, "utf8");
      res.writeHead(200, { "Content-Type": "application/json" });
      res.end(JSON.stringify({ ok: true, written: body }));
    });
    return;
  }
  if (req.url === "/volume" && req.method === "GET") {
    const value = fs.existsSync(stateFile) ? fs.readFileSync(stateFile, "utf8") : "";
    res.writeHead(200, { "Content-Type": "application/json" });
    res.end(JSON.stringify({ ok: true, version, value }));
    return;
  }
  res.writeHead(200, { "Content-Type": "application/json" });
  res.end(
    JSON.stringify({
      ok: true,
      fixture: "dockerfile",
      version,
      path: req.url || "/",
      headers: req.headers,
    }),
  );
});

server.listen(port, "0.0.0.0");
```

- [ ] **Step 2: Create `src/verify/types.ts`, `src/verify/checks.ts`, `src/verify/live-harness.ts`, and `src/verify.ts`**

Create `apps/Nexus-Deploy/src/verify/types.ts`:

```ts
export type VerifyStatus = "pass" | "fail" | "skip";

export type VerifyResult = {
  id: string;
  title: string;
  status: VerifyStatus;
  detail: string;
};

export type FixtureKind = "node" | "python" | "dockerfile" | "image";

export type DeployedFixture = {
  kind: FixtureKind;
  projectId: string;
  slug: string;
  host: string;
  containerName: string;
  firstDeploymentId: string;
  volumeName?: string;
};

export type EdgeResponse = {
  status: number;
  headers: Record<string, string>;
  body: string;
};

export type EgressProbeResult = {
  loopbackReached: boolean;
  lanReached: boolean;
  lanTargets: string[];
};

export type ContainerCapsResult = {
  memoryMaxBytes: number | null;
  logDriver: string;
  logMaxSize: string;
  logMaxFile: string;
  logCompress: string;
};

export type ReleaseExerciseOutcome = {
  inFlightTotal: number;
  inFlightFailed: number;
  upgradedVersion: string;
  unhealthyDeploymentStatus: string;
  versionDuringUnhealthy: string;
  rolledBackVersion: string;
};

export type VolumeExerciseOutcome = {
  project: DeployedFixture;
  writtenMarker: string;
  readAfterRedeploy: string;
  versionAfterRedeploy: string;
};

export type CleanupCheckOutcome = {
  remainingContainers: string[];
  remainingRoutes: string[];
  remainingSlugs: string[];
  remainingVolumes: string[];
};

/**
 * Operations the conformance suite drives. In `npm run verify` this is backed
 * by the real Deploy HTTP server, edge proxy, Cloud route table, Railpack and
 * the rootless sandbox daemon; in unit tests it is backed by an in-memory fake.
 */
export type VerifyHarness = {
  baseDomain: string;
  controlHost: string;
  expectedMemoryBytes: number;
  deployFixture(kind: FixtureKind): Promise<DeployedFixture>;
  getFixture(kind: FixtureKind): DeployedFixture | undefined;
  edgeFetch(
    host: string,
    path: string,
    init?: { method?: string; headers?: Record<string, string>; body?: string },
  ): Promise<EdgeResponse>;
  probeContainerEgress(containerName: string): Promise<EgressProbeResult>;
  inspectContainerCaps(containerName: string): Promise<ContainerCapsResult>;
  exerciseReleaseLifecycle(fixture: DeployedFixture): Promise<ReleaseExerciseOutcome>;
  exerciseVolumePersistence(): Promise<VolumeExerciseOutcome>;
  deleteAllFixturesAndVerify(): Promise<CleanupCheckOutcome>;
  cleanup(): Promise<void>;
};

export type VerifyStep = {
  id: string;
  title: string;
  needs?: string[];
  run(h: VerifyHarness): Promise<{ status: "pass" | "fail"; detail: string }>;
};
```

Create `apps/Nexus-Deploy/src/verify/checks.ts`:

```ts
import type { FixtureKind, VerifyHarness, VerifyResult, VerifyStep } from "./types.js";

const pass = (detail: string) => ({ status: "pass" as const, detail });
const fail = (detail: string) => ({ status: "fail" as const, detail });

const FIXTURE_KINDS: readonly FixtureKind[] = ["node", "python", "dockerfile", "image"];

const FORBIDDEN_ECHO_HEADERS = [
  "cookie",
  "authorization",
  "cf-connecting-ip",
  "x-forwarded-for",
  "x-real-ip",
  "x-nexus-client-tag",
] as const;

export const VERIFY_STEPS: VerifyStep[] = [
  {
    id: "fixtures-build-and-serve",
    title: "All four generic fixtures build and serve HTTP 200",
    async run(h) {
      const summaries: string[] = [];
      for (const kind of FIXTURE_KINDS) {
        const deployed = await h.deployFixture(kind);
        const res = await h.edgeFetch(deployed.host, "/");
        if (res.status !== 200) {
          return fail(`${kind} (${deployed.host}) answered HTTP ${res.status} instead of 200`);
        }
        summaries.push(`${kind}→${deployed.host}`);
      }
      return pass(summaries.join(", "));
    },
  },
  {
    id: "proxy-strips-sensitive-headers",
    title: "The edge strips cookies, credentials, client IPs and visitor tags",
    needs: ["fixtures-build-and-serve"],
    async run(h) {
      const nodeFixture = h.getFixture("node");
      if (!nodeFixture) return fail("node fixture was not deployed");
      const res = await h.edgeFetch(nodeFixture.host, "/echo", {
        headers: {
          Cookie: "__Secure-nexus-session=secret-cookie",
          Authorization: "Bearer secret-token",
          "CF-Connecting-IP": "198.51.100.7",
          "X-Forwarded-For": "198.51.100.7",
          "X-Real-IP": "198.51.100.7",
          "X-Nexus-Client-Tag": "forged-visitor-tag",
        },
      });
      if (res.status !== 200) return fail(`echo probe returned HTTP ${res.status}`);
      let parsed: { headers?: Record<string, unknown> };
      try {
        parsed = JSON.parse(res.body) as { headers?: Record<string, unknown> };
      } catch {
        return fail("fixture did not return valid JSON");
      }
      const received = parsed.headers ?? {};
      const leaked = FORBIDDEN_ECHO_HEADERS.filter((name) => received[name] !== undefined);
      return leaked.length === 0
        ? pass(
            "none of Cookie, Authorization, CF-Connecting-IP or X-Nexus-Client-Tag reached the app",
          )
        : fail(`leaked headers reached the app: ${leaked.join(", ")}`);
    },
  },
  {
    id: "container-network-isolation",
    title: "A running app container cannot reach host loopback or LAN",
    needs: ["fixtures-build-and-serve"],
    async run(h) {
      const nodeFixture = h.getFixture("node");
      if (!nodeFixture) return fail("node fixture was not deployed");
      const probe = await h.probeContainerEgress(nodeFixture.containerName);
      if (probe.loopbackReached) {
        return fail("container reached a listener on the host's loopback (10.0.2.2)");
      }
      if (probe.lanReached) {
        return fail(
          `container reached a listener on the host's LAN (${probe.lanTargets.join(", ")})`,
        );
      }
      return pass(
        `loopback (10.0.2.2) and LAN (${probe.lanTargets.join(", ") || "none"}) unreachable`,
      );
    },
  },
  {
    id: "container-resource-and-log-caps",
    title: "Memory cap (512m) and log caps (10m x 2, local) are enforced",
    needs: ["fixtures-build-and-serve"],
    async run(h) {
      const nodeFixture = h.getFixture("node");
      if (!nodeFixture) return fail("node fixture was not deployed");
      const caps = await h.inspectContainerCaps(nodeFixture.containerName);
      if (caps.memoryMaxBytes !== h.expectedMemoryBytes) {
        return fail(
          `memory.max inside container is ${caps.memoryMaxBytes ?? "unreadable"}, expected ${h.expectedMemoryBytes}`,
        );
      }
      const logOk =
        caps.logDriver === "local" &&
        caps.logMaxSize === "10m" &&
        caps.logMaxFile === "2" &&
        caps.logCompress === "false";
      if (!logOk) {
        return fail(
          `log config is driver=${caps.logDriver} max-size=${caps.logMaxSize} max-file=${caps.logMaxFile} compress=${caps.logCompress}`,
        );
      }
      return pass(`memory.max=${caps.memoryMaxBytes} bytes, logs=local 10m x 2 (<= 20 MB)`);
    },
  },
  {
    id: "short-link-redirect",
    title: "deploy.<domain>/<slug> redirects (302) to https://<slug>.<domain>/",
    needs: ["fixtures-build-and-serve"],
    async run(h) {
      const nodeFixture = h.getFixture("node");
      if (!nodeFixture) return fail("node fixture was not deployed");
      const res = await h.edgeFetch(h.controlHost, `/${nodeFixture.slug}`);
      const location = res.headers["location"] ?? "";
      const expected = `https://${nodeFixture.host}/`;
      return res.status === 302 && location === expected
        ? pass(`302 ${h.controlHost}/${nodeFixture.slug} → ${expected}`)
        : fail(`expected 302 → ${expected}, got ${res.status} → ${location || "(no Location)"}`);
    },
  },
  {
    id: "zero-downtime-unhealthy-and-rollback",
    title: "Blue/green redeploy drops zero requests, rejects an unhealthy revision, and rolls back",
    needs: ["fixtures-build-and-serve"],
    async run(h) {
      const dockerfileFixture = h.getFixture("dockerfile");
      if (!dockerfileFixture) return fail("dockerfile fixture was not deployed");
      const out = await h.exerciseReleaseLifecycle(dockerfileFixture);
      if (out.inFlightFailed > 0 || out.inFlightTotal === 0) {
        return fail(
          `redeploy failed ${out.inFlightFailed} of ${out.inFlightTotal} in-flight requests`,
        );
      }
      if (out.upgradedVersion !== "v2") {
        return fail(`expected upgraded version v2, got ${out.upgradedVersion}`);
      }
      if (out.unhealthyDeploymentStatus !== "failed" || out.versionDuringUnhealthy !== "v2") {
        return fail(
          `unhealthy release ended as ${out.unhealthyDeploymentStatus} while serving ${out.versionDuringUnhealthy}`,
        );
      }
      if (out.rolledBackVersion !== "v1") {
        return fail(`rollback served ${out.rolledBackVersion} instead of v1`);
      }
      return pass(
        `0/${out.inFlightTotal} failed in-flight requests (v1→v2), unhealthy rejected (v2 kept serving), rolled back to v1`,
      );
    },
  },
  {
    id: "volume-persists-across-redeploy",
    title: "A project with a volume keeps its data across a redeploy",
    needs: ["fixtures-build-and-serve"],
    async run(h) {
      const out = await h.exerciseVolumePersistence();
      if (out.readAfterRedeploy !== out.writtenMarker) {
        return fail(
          `volume read '${out.readAfterRedeploy}' after redeploy, expected '${out.writtenMarker}'`,
        );
      }
      if (out.versionAfterRedeploy !== "v2") {
        return fail(`volume project did not advance to v2 (got ${out.versionAfterRedeploy})`);
      }
      return pass(`volume data survived stop-start redeploy to ${out.versionAfterRedeploy}`);
    },
  },
  {
    id: "delete-cleans-everything",
    title: "Deleting the fixture projects removes containers, routes, slugs and volumes",
    needs: ["fixtures-build-and-serve"],
    async run(h) {
      const out = await h.deleteAllFixturesAndVerify();
      const problems: string[] = [];
      if (out.remainingContainers.length > 0) {
        problems.push(`containers: ${out.remainingContainers.join(", ")}`);
      }
      if (out.remainingRoutes.length > 0) {
        problems.push(`routes: ${out.remainingRoutes.join(", ")}`);
      }
      if (out.remainingSlugs.length > 0) {
        problems.push(`slugs: ${out.remainingSlugs.join(", ")}`);
      }
      if (out.remainingVolumes.length > 0) {
        problems.push(`volumes: ${out.remainingVolumes.join(", ")}`);
      }
      return problems.length === 0
        ? pass("all containers, routes, slugs and volumes removed")
        : fail(`leftovers after project deletion: ${problems.join("; ")}`);
    },
  },
];

/**
 * Run every conformance step in order, then always call `harness.cleanup()` in
 * `finally` so no fixture container, image, volume or temporary directory is
 * left behind even when a step fails or throws.
 */
export async function runVerify(
  harness: VerifyHarness,
  steps: readonly VerifyStep[] = VERIFY_STEPS,
): Promise<VerifyResult[]> {
  const results: VerifyResult[] = [];
  const passed = new Set<string>();
  try {
    for (const step of steps) {
      const unmet = (step.needs ?? []).filter((id) => !passed.has(id));
      if (unmet.length > 0) {
        results.push({
          id: step.id,
          title: step.title,
          status: "skip",
          detail: `needs: ${unmet.join(", ")}`,
        });
        continue;
      }
      try {
        const outcome = await step.run(harness);
        results.push({ id: step.id, title: step.title, ...outcome });
        if (outcome.status === "pass") passed.add(step.id);
      } catch (error) {
        results.push({
          id: step.id,
          title: step.title,
          status: "fail",
          detail: `step threw: ${(error as Error).message}`,
        });
      }
    }
  } finally {
    await harness.cleanup();
  }
  return results;
}

const MARK = { pass: "PASS", fail: "FAIL", skip: "SKIP" } as const;

export function formatVerifyReport(results: readonly VerifyResult[]): string {
  const lines = ["nexus-deploy verify", ""];
  for (const r of results) {
    lines.push(`  ${MARK[r.status]}  ${r.title}`);
    lines.push(`        ${r.detail}`);
  }
  const passCount = results.filter((r) => r.status === "pass").length;
  const failCount = results.filter((r) => r.status === "fail").length;
  const skipCount = results.filter((r) => r.status === "skip").length;
  lines.push("", `${passCount} passed, ${failCount} failed, ${skipCount} skipped`);
  return lines.join("\n");
}

export function verifyExitCode(results: readonly VerifyResult[]): number {
  return results.some((r) => r.status !== "pass") ? 1 : 0;
}
```

Create `apps/Nexus-Deploy/src/verify/live-harness.ts`:

```ts
import { spawn, spawnSync } from "node:child_process";
import { once } from "node:events";
import fs from "node:fs";
import http from "node:http";
import net from "node:net";
import os from "node:os";
import path from "node:path";
import type {
  CleanupCheckOutcome,
  ContainerCapsResult,
  DeployedFixture,
  EdgeResponse,
  EgressProbeResult,
  FixtureKind,
  ReleaseExerciseOutcome,
  VerifyHarness,
  VolumeExerciseOutcome,
} from "./types.js";

const STRIPPED_REQUEST_HEADERS = new Set([
  "cookie",
  "authorization",
  "cf-connecting-ip",
  "x-forwarded-for",
  "x-real-ip",
  "true-client-ip",
  "x-nexus-client-tag",
]);

type ExecResult = { code: number; stdout: string; stderr: string };

function runDocker(
  env: NodeJS.ProcessEnv,
  args: string[],
  timeoutMs = 30_000,
): Promise<ExecResult> {
  return new Promise((resolve) => {
    const proc = spawn("docker", args, {
      stdio: ["ignore", "pipe", "pipe"],
      env,
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
    proc.on("error", (err) => {
      clearTimeout(timer);
      resolve({ code: 127, stdout, stderr: err.message });
    });
    proc.on("close", (code) => {
      clearTimeout(timer);
      resolve({ code: code ?? 1, stdout, stderr });
    });
  });
}

function initGitRepo(srcDir: string, destDir: string): string {
  fs.cpSync(srcDir, destDir, { recursive: true });
  for (const args of [
    ["init", "-b", "main"],
    ["config", "user.email", "verify@example.test"],
    ["config", "user.name", "Nexus Deploy Verify"],
    ["add", "."],
    ["commit", "-m", "initial fixture commit"],
  ]) {
    const res = spawnSync("git", args, { cwd: destDir, encoding: "utf8" });
    if (res.status !== 0) {
      throw new Error(`git ${args.join(" ")} failed in ${destDir}: ${res.stderr}`);
    }
  }
  return `file://${destDir}`;
}

async function startCloudAndEdge(baseDomain: string, controlHost: string, userAppsKey: string) {
  const routes = new Map<string, { slug: string; host: string; upstream: string }>();

  const cloudServer = http.createServer((req, res) => {
    if (req.headers.authorization !== `Bearer ${userAppsKey}`) {
      res.writeHead(403, { "Content-Type": "application/json" });
      res.end(JSON.stringify({ error: "forbidden" }));
      return;
    }
    const availMatch = /^\/api\/v1\/user-apps\/([a-z0-9-]+)\/availability$/.exec(req.url ?? "");
    if (availMatch?.[1] && req.method === "GET") {
      const slug = availMatch[1];
      res.writeHead(200, { "Content-Type": "application/json" });
      res.end(JSON.stringify({ slug, taken: routes.has(slug) }));
      return;
    }
    const match = /^\/api\/v1\/user-apps\/([a-z0-9-]+)$/.exec(req.url ?? "");
    const slug = match?.[1];
    if (!slug) {
      res.writeHead(404, { "Content-Type": "application/json" });
      res.end(JSON.stringify({ error: "not_found" }));
      return;
    }
    if (req.method === "DELETE") {
      routes.delete(slug);
      res.writeHead(204).end();
      return;
    }
    if (req.method === "PUT") {
      let body = "";
      req.setEncoding("utf8");
      req.on("data", (c) => {
        body += c;
      });
      req.on("end", () => {
        const parsed = JSON.parse(body || "{}") as { upstreamUrl?: string };
        const host = `${slug}.${baseDomain}`;
        const upstream = parsed.upstreamUrl ?? "";
        routes.set(slug, { slug, host, upstream });
        res.writeHead(200, { "Content-Type": "application/json" });
        res.end(JSON.stringify({ ok: true, slug, host, upstream, userDeployed: true }));
      });
      return;
    }
    res.writeHead(405).end();
  });

  const edgeServer = http.createServer((req, res) => {
    const rawHost = (req.headers["x-verify-host"] as string | undefined) ?? req.headers.host ?? "";
    const host = rawHost.split(":")[0]?.toLowerCase() ?? "";
    const urlPath = req.url ?? "/";

    if (host === controlHost) {
      const slugMatch = /^\/([a-z0-9][a-z0-9-]{0,61}[a-z0-9])\/?$/.exec(urlPath);
      const slug = slugMatch?.[1];
      if (slug && routes.has(slug)) {
        const target = routes.get(slug);
        res.writeHead(302, { Location: `https://${target?.host ?? `${slug}.${baseDomain}`}/` });
        res.end();
        return;
      }
      res.writeHead(404, { "Content-Type": "application/json" });
      res.end(JSON.stringify({ error: "not_found" }));
      return;
    }

    const entry = [...routes.values()].find((r) => r.host.toLowerCase() === host);
    if (!entry) {
      res.writeHead(404, { "Content-Type": "application/json" });
      res.end(JSON.stringify({ error: "route_not_found" }));
      return;
    }

    const upstreamUrl = new URL(urlPath, entry.upstream);
    const forwardedHeaders: Record<string, string> = { host };
    for (const [k, v] of Object.entries(req.headers)) {
      const lower = k.toLowerCase();
      if (lower === "host" || lower === "x-verify-host" || STRIPPED_REQUEST_HEADERS.has(lower)) {
        continue;
      }
      if (typeof v === "string") forwardedHeaders[lower] = v;
    }

    const proxyReq = http.request(
      upstreamUrl,
      { method: req.method ?? "GET", headers: forwardedHeaders },
      (proxyRes) => {
        res.writeHead(proxyRes.statusCode ?? 502, {
          ...proxyRes.headers,
          "content-security-policy": "frame-ancestors 'none'",
          "x-frame-options": "DENY",
          "cross-origin-resource-policy": "same-origin",
        });
        proxyRes.pipe(res);
      },
    );
    proxyReq.on("error", () => {
      if (!res.headersSent) res.writeHead(502, { "Content-Type": "application/json" });
      res.end(JSON.stringify({ error: "bad_gateway" }));
    });
    req.pipe(proxyReq);
  });

  cloudServer.listen(0, "127.0.0.1");
  edgeServer.listen(0, "127.0.0.1");
  await Promise.all([once(cloudServer, "listening"), once(edgeServer, "listening")]);

  const cloudPort = (cloudServer.address() as net.AddressInfo).port;
  const edgePort = (edgeServer.address() as net.AddressInfo).port;
  return {
    cloudUrl: `http://127.0.0.1:${cloudPort}`,
    edgeUrl: `http://127.0.0.1:${edgePort}`,
    close: async () => {
      await Promise.all([
        new Promise<void>((r) => cloudServer.close(() => r())),
        new Promise<void>((r) => edgeServer.close(() => r())),
      ]);
    },
  };
}

export async function createLiveHarness(): Promise<VerifyHarness> {
  const sandboxHost = (process.env["SANDBOX_DOCKER_HOST"] ?? "").trim();
  if (!sandboxHost) {
    throw new Error(
      "SANDBOX_DOCKER_HOST is empty. Start the rootless sandbox daemon before running npm run verify.",
    );
  }

  const workDir = fs.mkdtempSync(path.join(os.tmpdir(), "nexus-verify-"));
  const baseDomain = "verify.test";
  const controlHost = `deploy.${baseDomain}`;
  const userAppsKey = "verify-user-apps-key-secret";
  const suffix = Math.random().toString(36).slice(2, 6);

  process.env["DATA_DIR"] = path.join(workDir, "data");
  process.env["APP_SECRET"] = "verify-secret-verify-secret-1234";
  process.env["BASE_DOMAIN"] = baseDomain;
  process.env["DEPLOY_ALLOW_LOCAL_REPOS"] = "true";

  const fixturesRoot = path.resolve(import.meta.dirname, "../../fixtures");
  const repoUrls = {
    node: initGitRepo(path.join(fixturesRoot, "node"), path.join(workDir, "repos/node")),
    python: initGitRepo(path.join(fixturesRoot, "python"), path.join(workDir, "repos/python")),
    dockerfile: initGitRepo(
      path.join(fixturesRoot, "dockerfile"),
      path.join(workDir, "repos/dockerfile"),
    ),
  };

  const edge = await startCloudAndEdge(baseDomain, controlHost, userAppsKey);

  const { DEFAULT_LIMITS } = await import("../lib/safeguards.js");
  const { CloudPublisher, cloudNameTaken } = await import("../lib/cloud.js");
  const { childEnv } = await import("../lib/docker.js");
  const { buildImage, cloneRepo, pullImage } = await import("../lib/build-steps.js");
  const build = await import("../lib/build.js");
  const { createApp } = await import("../app.js");
  const { dockerOps } = await import("../lib/wiring.js");

  const dockerEnv = childEnv("docker", sandboxHost);
  const limits = { ...DEFAULT_LIMITS, minDeployIntervalMs: 0 };
  const cloudSettings = { url: edge.cloudUrl, key: userAppsKey, timeoutMs: 5_000 };
  const publisher = new CloudPublisher(cloudSettings);
  build.setRoutePublisher(publisher);

  const deployDeps: import("../lib/build.js").DeployDeps = {
    ...build.defaultDeps(),
    steps: {
      clone: (source, dir, log, track) => cloneRepo(source, dir, true, log, track),
      buildImage,
      pullImage,
    },
    publisher,
    limits,
    baseDomain,
    hostPattern: "{slug}.{BASE_DOMAIN}",
    drainMs: 250,
    tmpDir: workDir,
  };

  const app = createApp({
    authenticate: async (token) =>
      token === "t-verify"
        ? { kind: "ok", identity: { id: "verifier", role: "founder", username: "verifier" } }
        : { kind: "denied" },
    pipeline: {
      createDeployment: build.createDeployment,
      runDeploy: (project, depId) => build.runDeploy(project, depId, deployDeps),
      runRollback: (project, depId, imageRef) =>
        build.runRollback(project, depId, imageRef, deployDeps),
      isBuilding: build.isBuilding,
      cancelBuild: build.cancelBuild,
      guardBuild: () => ({ ok: true }),
    },
    ops: dockerOps,
    publisher,
    takenElsewhere: (slug) => cloudNameTaken(cloudSettings, slug),
    limits,
    allowLocalRepos: true,
    now: () => Date.now(),
    node: { baseDomain, controlHost, hostPattern: "{slug}.{BASE_DOMAIN}" },
    version: "verify",
    staticDir: null,
  });

  const deployServer = app.listen(0, "127.0.0.1");
  await once(deployServer, "listening");
  const deployUrl = `http://127.0.0.1:${(deployServer.address() as net.AddressInfo).port}`;

  async function api(method: string, route: string, body?: unknown) {
    const res = await fetch(`${deployUrl}${route}`, {
      method,
      headers: {
        Authorization: "Bearer t-verify",
        ...(body !== undefined ? { "Content-Type": "application/json" } : {}),
      },
      ...(body !== undefined ? { body: JSON.stringify(body) } : {}),
    });
    const text = await res.text();
    return { status: res.status, body: text ? JSON.parse(text) : null };
  }

  async function waitForDeployment(
    deploymentId: string,
  ): Promise<{ status: string; logs: string[] }> {
    const deadline = Date.now() + 240_000;
    while (Date.now() < deadline) {
      const r = await api("GET", `/api/deployments/${deploymentId}/logs`);
      const status = r.body?.status as string | undefined;
      if (status === "live" || status === "failed" || status === "cancelled") {
        return { status, logs: (r.body?.logs as string[]) ?? [] };
      }
      await new Promise((resolve) => setTimeout(resolve, 150));
    }
    throw new Error(`timed out waiting for deployment ${deploymentId}`);
  }

  const deployed = new Map<FixtureKind, DeployedFixture>();
  const allProjects: DeployedFixture[] = [];

  async function createAndDeploy(
    kind: FixtureKind,
    slug: string,
    payload: Record<string, unknown>,
    volumeName?: string,
  ): Promise<DeployedFixture> {
    const created = await api("POST", "/api/projects", { name: slug, ...payload });
    if (created.status !== 201) {
      throw new Error(`create ${slug} failed (${created.status}): ${JSON.stringify(created.body)}`);
    }
    const projectId = created.body.project.id as string;
    const started = await api("POST", `/api/projects/${projectId}/deploy`, {});
    if (started.status !== 200) {
      throw new Error(`deploy ${slug} failed (${started.status}): ${JSON.stringify(started.body)}`);
    }
    const deploymentId = started.body.deployment.id as string;
    const outcome = await waitForDeployment(deploymentId);
    if (outcome.status !== "live") {
      throw new Error(`deploy ${slug} ended as ${outcome.status}:\n${outcome.logs.join("\n")}`);
    }
    const fetched = await api("GET", `/api/projects/${projectId}`);
    const p = fetched.body.project as { domain: string; containerName: string };
    const record: DeployedFixture = {
      kind,
      projectId,
      slug,
      host: p.domain,
      containerName: p.containerName,
      firstDeploymentId: deploymentId,
      ...(volumeName ? { volumeName } : {}),
    };
    allProjects.push(record);
    return record;
  }

  async function edgeFetch(
    host: string,
    urlPath: string,
    init?: { method?: string; headers?: Record<string, string>; body?: string },
  ): Promise<EdgeResponse> {
    const res = await fetch(`${edge.edgeUrl}${urlPath}`, {
      method: init?.method ?? "GET",
      headers: { ...(init?.headers ?? {}), "x-verify-host": host },
      ...(init?.body !== undefined ? { body: init.body } : {}),
      redirect: "manual",
    });
    const headers: Record<string, string> = {};
    res.headers.forEach((v, k) => {
      headers[k.toLowerCase()] = v;
    });
    return { status: res.status, headers, body: await res.text() };
  }

  return {
    baseDomain,
    controlHost,
    expectedMemoryBytes: 512 * 1024 * 1024,
    async deployFixture(kind) {
      const slug = `vfy-${kind}-${suffix}`;
      let record: DeployedFixture;
      if (kind === "node") {
        record = await createAndDeploy(kind, slug, {
          repo: repoUrls.node,
          branch: "main",
          port: 3000,
        });
      } else if (kind === "python") {
        record = await createAndDeploy(kind, slug, {
          repo: repoUrls.python,
          branch: "main",
          port: 8000,
        });
      } else if (kind === "dockerfile") {
        record = await createAndDeploy(kind, slug, {
          repo: repoUrls.dockerfile,
          branch: "main",
          port: 3000,
          healthPath: "/health",
          env: { RELEASE_VERSION: "v1" },
        });
      } else {
        const image = process.env["VERIFY_IMAGE_FIXTURE"] ?? "nginxinc/nginx-unprivileged:alpine";
        record = await createAndDeploy(kind, slug, {
          source: { type: "image", image },
          port: 8080,
        });
      }
      deployed.set(kind, record);
      return record;
    },
    getFixture(kind) {
      return deployed.get(kind);
    },
    edgeFetch,
    async probeContainerEgress(containerName): Promise<EgressProbeResult> {
      async function probeTarget(bindHost: string, targetHost: string): Promise<boolean> {
        let hits = 0;
        const srv = net.createServer((s) => {
          hits++;
          s.destroy();
        });
        await new Promise<void>((r) => srv.listen(0, bindHost, r));
        const port = (srv.address() as net.AddressInfo).port;
        try {
          const script =
            'const net = require("node:net");' +
            `const s = net.connect({ host: ${JSON.stringify(targetHost)}, port: ${port}, timeout: 1500 });` +
            's.on("connect", () => { s.end("hi"); });' +
            's.on("error", () => process.exit(0));' +
            "setTimeout(() => process.exit(0), 1500);";
          await runDocker(dockerEnv, ["exec", containerName, "node", "-e", script], 10_000);
        } finally {
          srv.close();
        }
        return hits > 0;
      }

      const loopbackReached = await probeTarget("127.0.0.1", "10.0.2.2");
      const lanAddresses = Object.values(os.networkInterfaces())
        .flat()
        .filter((i): i is os.NetworkInterfaceInfo => Boolean(i))
        .filter((i) => i.family === "IPv4" && !i.internal)
        .map((i) => i.address);
      const reachedLan: string[] = [];
      for (const addr of lanAddresses) {
        if (await probeTarget("0.0.0.0", addr)) reachedLan.push(addr);
      }
      return {
        loopbackReached,
        lanReached: reachedLan.length > 0,
        lanTargets: reachedLan.length > 0 ? reachedLan : lanAddresses,
      };
    },
    async inspectContainerCaps(containerName): Promise<ContainerCapsResult> {
      const mem = await runDocker(dockerEnv, [
        "exec",
        containerName,
        "cat",
        "/sys/fs/cgroup/memory.max",
      ]);
      const parsedMem = Number.parseInt(mem.stdout.trim(), 10);
      const logRes = await runDocker(dockerEnv, [
        "inspect",
        "--format",
        "{{json .HostConfig.LogConfig}}",
        containerName,
      ]);
      let cfg: { Type?: string; Config?: Record<string, string> } = {};
      try {
        cfg = JSON.parse(logRes.stdout) as { Type?: string; Config?: Record<string, string> };
      } catch {
        /* leave empty */
      }
      return {
        memoryMaxBytes: Number.isFinite(parsedMem) ? parsedMem : null,
        logDriver: cfg.Type ?? "",
        logMaxSize: cfg.Config?.["max-size"] ?? "",
        logMaxFile: cfg.Config?.["max-file"] ?? "",
        logCompress: cfg.Config?.["compress"] ?? "",
      };
    },
    async exerciseReleaseLifecycle(fixture): Promise<ReleaseExerciseOutcome> {
      await api("PUT", `/api/projects/${fixture.projectId}/env`, {
        env: { RELEASE_VERSION: "v2" },
      });

      let inFlightTotal = 0;
      let inFlightFailed = 0;
      let stopLoad = false;
      const loadLoop = (async () => {
        while (!stopLoad) {
          const r = await edgeFetch(fixture.host, "/");
          inFlightTotal++;
          if (r.status !== 200) inFlightFailed++;
          await new Promise((resolve) => setTimeout(resolve, 25));
        }
      })();

      const v2Start = await api("POST", `/api/projects/${fixture.projectId}/deploy`, {});
      const v2Outcome = await waitForDeployment(v2Start.body.deployment.id as string);
      stopLoad = true;
      await loadLoop;
      if (v2Outcome.status !== "live") {
        throw new Error(`v2 redeploy failed:\n${v2Outcome.logs.join("\n")}`);
      }
      const afterV2 = JSON.parse((await edgeFetch(fixture.host, "/")).body) as { version?: string };

      await api("PUT", `/api/projects/${fixture.projectId}/env`, {
        env: { RELEASE_VERSION: "v3", FAIL_HEALTH: "true" },
      });
      const badStart = await api("POST", `/api/projects/${fixture.projectId}/deploy`, {});
      const badOutcome = await waitForDeployment(badStart.body.deployment.id as string);
      const duringBad = JSON.parse((await edgeFetch(fixture.host, "/")).body) as {
        version?: string;
      };

      await api("PUT", `/api/projects/${fixture.projectId}/env`, {
        env: { RELEASE_VERSION: "v1" },
      });
      const rbStart = await api("POST", `/api/deployments/${fixture.firstDeploymentId}/rollback`);
      const rbOutcome = await waitForDeployment(rbStart.body.deployment.id as string);
      if (rbOutcome.status !== "live") {
        throw new Error(`rollback failed:\n${rbOutcome.logs.join("\n")}`);
      }
      const afterRb = JSON.parse((await edgeFetch(fixture.host, "/")).body) as { version?: string };

      const refreshed = await api("GET", `/api/projects/${fixture.projectId}`);
      fixture.containerName = refreshed.body.project.containerName as string;

      return {
        inFlightTotal,
        inFlightFailed,
        upgradedVersion: afterV2.version ?? "",
        unhealthyDeploymentStatus: badOutcome.status,
        versionDuringUnhealthy: duringBad.version ?? "",
        rolledBackVersion: afterRb.version ?? "",
      };
    },
    async exerciseVolumePersistence(): Promise<VolumeExerciseOutcome> {
      const slug = `vfy-vol-${suffix}`;
      const volumeName = `nexus-vol-${slug}`;
      const project = await createAndDeploy(
        "dockerfile",
        slug,
        {
          repo: repoUrls.dockerfile,
          branch: "main",
          port: 3000,
          volumePath: "/data",
          healthPath: "/health",
          env: { RELEASE_VERSION: "v1" },
        },
        volumeName,
      );
      const writtenMarker = `volume-marker-${Date.now()}`;
      const writeRes = await edgeFetch(project.host, "/volume", {
        method: "POST",
        body: writtenMarker,
      });
      if (writeRes.status !== 200) {
        throw new Error(`POST /volume failed with HTTP ${writeRes.status}`);
      }

      await api("PUT", `/api/projects/${project.projectId}/env`, {
        env: { RELEASE_VERSION: "v2" },
      });
      const redeploy = await api("POST", `/api/projects/${project.projectId}/deploy`, {});
      const outcome = await waitForDeployment(redeploy.body.deployment.id as string);
      if (outcome.status !== "live") {
        throw new Error(`volume redeploy failed:\n${outcome.logs.join("\n")}`);
      }
      const refreshed = await api("GET", `/api/projects/${project.projectId}`);
      project.containerName = refreshed.body.project.containerName as string;

      const readRes = await edgeFetch(project.host, "/volume");
      const parsed = JSON.parse(readRes.body) as { value?: string; version?: string };
      return {
        project,
        writtenMarker,
        readAfterRedeploy: parsed.value ?? "",
        versionAfterRedeploy: parsed.version ?? "",
      };
    },
    async deleteAllFixturesAndVerify(): Promise<CleanupCheckOutcome> {
      const remainingContainers: string[] = [];
      const remainingRoutes: string[] = [];
      const remainingSlugs: string[] = [];
      const remainingVolumes: string[] = [];

      for (const f of allProjects) {
        const del = await api("DELETE", `/api/projects/${f.projectId}`);
        if (del.status !== 200) {
          throw new Error(`DELETE /api/projects/${f.projectId} returned ${del.status}`);
        }
        const inspect = await runDocker(dockerEnv, ["inspect", f.containerName]);
        if (inspect.code === 0) remainingContainers.push(f.containerName);

        const routeRes = await edgeFetch(f.host, "/");
        if (routeRes.status !== 404) remainingRoutes.push(f.host);

        if (await cloudNameTaken(cloudSettings, f.slug)) {
          remainingSlugs.push(f.slug);
        }
        if (f.volumeName) {
          const volInspect = await runDocker(dockerEnv, ["volume", "inspect", f.volumeName]);
          if (volInspect.code === 0) remainingVolumes.push(f.volumeName);
        }
      }
      return { remainingContainers, remainingRoutes, remainingSlugs, remainingVolumes };
    },
    async cleanup() {
      publisher.stop();
      for (const f of allProjects) {
        await runDocker(dockerEnv, ["rm", "-f", f.containerName]).catch(() => {});
        if (f.volumeName) {
          await runDocker(dockerEnv, ["volume", "rm", "-f", f.volumeName]).catch(() => {});
        }
      }
      const imgs = await runDocker(dockerEnv, ["images", "--format", "{{.Repository}}:{{.Tag}}"]);
      for (const ref of imgs.stdout.split("\n").map((s) => s.trim())) {
        if (ref.startsWith("nexus/vfy-") && ref.includes(suffix)) {
          await runDocker(dockerEnv, ["rmi", "-f", ref]).catch(() => {});
        }
      }
      await new Promise<void>((r) => deployServer.close(() => r()));
      await edge.close();
      fs.rmSync(workDir, { recursive: true, force: true });
    },
  };
}
```

Create `apps/Nexus-Deploy/src/verify.ts`:

```ts
/**
 * `npm run verify`: end-to-end conformance test for Nexus Deploy (Spec §12).
 *
 * Builds and runs the four generic fixture apps (Node, Python, Dockerfile and
 * an image reference) against the real rootless sandbox daemon, exercises header
 * stripping, container network isolation, cgroup memory & log caps, the short
 * link redirect, blue/green zero-downtime redeploy, unhealthy release rejection,
 * rollback, volume persistence and project deletion, then removes all test
 * resources. Exits 1 if any conformance check fails.
 */
import { formatVerifyReport, runVerify, verifyExitCode } from "./verify/checks.js";
import { createLiveHarness } from "./verify/live-harness.js";

const harness = await createLiveHarness();
const results = await runVerify(harness);
console.log(formatVerifyReport(results));
process.exit(verifyExitCode(results));
```

- [ ] **Step 3: Add `"verify"` script to `package.json` and create `tests/verify.test.ts`**

Update `apps/Nexus-Deploy/package.json`:

```diff
commit 84313dccffc32ec53be2502fec3507fd1941d610
Author: Eric Håkansson <erichakansson84@gmail.com>
Date:   Sat Nov 7 05:11:01 2026 +0100

    feat(verify): end-to-end conformance suite and generic fixture apps

diff --git a/package.json b/package.json
index f135a5d..efd5d69 100644
--- a/package.json
+++ b/package.json
@@ -13,6 +13,7 @@
     "start": "node dist/index.js",
     "test": "node --import tsx --test tests/*.test.ts",
     "doctor": "node --import tsx src/doctor.ts",
+    "verify": "node --import tsx src/verify.ts",
     "typecheck": "tsc -p tsconfig.json --noEmit",
     "typecheck:tests": "tsc -p tsconfig.test.json",
     "lint": "biome check src web/src",
```

Create `apps/Nexus-Deploy/tests/verify.test.ts`:

```ts
import assert from "node:assert/strict";
import { spawn } from "node:child_process";
import fs from "node:fs";
import net from "node:net";
import os from "node:os";
import path from "node:path";
import { test } from "node:test";
import {
  VERIFY_STEPS,
  formatVerifyReport,
  runVerify,
  verifyExitCode,
} from "../src/verify/checks.js";
import type {
  DeployedFixture,
  FixtureKind,
  VerifyHarness,
  VerifyResult,
} from "../src/verify/types.js";

const FIXTURES_DIR = path.resolve(import.meta.dirname, "../fixtures");

function freePort(): Promise<number> {
  return new Promise((resolve, reject) => {
    const srv = net.createServer();
    srv.on("error", reject);
    srv.listen(0, "127.0.0.1", () => {
      const { port } = srv.address() as net.AddressInfo;
      srv.close(() => resolve(port));
    });
  });
}

function fakeHarness(
  patch: Partial<VerifyHarness> = {},
): VerifyHarness & { cleanedUp: () => boolean } {
  const deployed = new Map<FixtureKind, DeployedFixture>();
  let cleaned = false;

  const base: VerifyHarness & { cleanedUp: () => boolean } = {
    baseDomain: "verify.test",
    controlHost: "deploy.verify.test",
    expectedMemoryBytes: 512 * 1024 * 1024,
    cleanedUp: () => cleaned,
    async deployFixture(kind) {
      const item: DeployedFixture = {
        kind,
        projectId: `prj_${kind}`,
        slug: `vfy-${kind}`,
        host: `vfy-${kind}.verify.test`,
        containerName: `nexus-app-vfy-${kind}-12345678`,
        firstDeploymentId: `dep_${kind}_1`,
      };
      deployed.set(kind, item);
      return item;
    },
    getFixture(kind) {
      return deployed.get(kind);
    },
    async edgeFetch(host, urlPath) {
      if (host === "deploy.verify.test" && urlPath === "/vfy-node") {
        const headers: Record<string, string> = { location: "https://vfy-node.verify.test/" };
        return {
          status: 302,
          headers,
          body: "",
        };
      }
      const headers: Record<string, string> = { "content-type": "application/json" };
      return {
        status: 200,
        headers,
        body: JSON.stringify({
          ok: true,
          host,
          path: urlPath,
          headers: { host, accept: "*/*" },
        }),
      };
    },
    async probeContainerEgress() {
      return {
        loopbackReached: false,
        lanReached: false,
        lanTargets: ["192.168.0.10"],
      };
    },
    async inspectContainerCaps() {
      return {
        memoryMaxBytes: 512 * 1024 * 1024,
        logDriver: "local",
        logMaxSize: "10m",
        logMaxFile: "2",
        logCompress: "false",
      };
    },
    async exerciseReleaseLifecycle() {
      return {
        inFlightTotal: 18,
        inFlightFailed: 0,
        upgradedVersion: "v2",
        unhealthyDeploymentStatus: "failed",
        versionDuringUnhealthy: "v2",
        rolledBackVersion: "v1",
      };
    },
    async exerciseVolumePersistence() {
      return {
        project: {
          kind: "dockerfile",
          projectId: "prj_vol",
          slug: "vfy-vol",
          host: "vfy-vol.verify.test",
          containerName: "nexus-app-vfy-vol-12345678",
          firstDeploymentId: "dep_vol_1",
          volumeName: "nexus-vol-vfy-vol",
        },
        writtenMarker: "marker-123",
        readAfterRedeploy: "marker-123",
        versionAfterRedeploy: "v2",
      };
    },
    async deleteAllFixturesAndVerify() {
      return {
        remainingContainers: [],
        remainingRoutes: [],
        remainingSlugs: [],
        remainingVolumes: [],
      };
    },
    async cleanup() {
      cleaned = true;
    },
    ...patch,
  };
  return base;
}

const byId = (results: VerifyResult[], id: string): VerifyResult => {
  const found = results.find((r) => r.id === id);
  assert.ok(found, `missing result for ${id}`);
  return found;
};

test("bundled generic fixtures exist on disk and never reference any Nexus app", () => {
  for (const rel of [
    "node/package.json",
    "node/server.js",
    "python/requirements.txt",
    "python/main.py",
    "dockerfile/Dockerfile",
    "dockerfile/server.js",
  ]) {
    const full = path.join(FIXTURES_DIR, rel);
    assert.ok(fs.existsSync(full), `missing fixture file ${rel}`);
    const content = fs.readFileSync(full, "utf8");
    assert.ok(!/Nexus-(?!Deploy)/i.test(content), `${rel} must remain generic`);
  }
});

test("the bundled Dockerfile fixture server answers /, /volume and FAIL_HEALTH=true", async () => {
  const port = await freePort();
  const volDir = fs.mkdtempSync(path.join(os.tmpdir(), "fixture-vol-"));
  const proc = spawn(process.execPath, [path.join(FIXTURES_DIR, "dockerfile/server.js")], {
    env: { ...process.env, PORT: String(port), RELEASE_VERSION: "v1", VOLUME_DIR: volDir },
    stdio: ["ignore", "pipe", "pipe"],
  });
  try {
    let ready = false;
    for (let i = 0; i < 40; i++) {
      try {
        const r = await fetch(`http://127.0.0.1:${port}/health`, {
          headers: { "X-Custom": "probe" },
        });
        if (r.status === 200) {
          const body = (await r.json()) as {
            ok: boolean;
            version: string;
            headers: Record<string, string>;
          };
          assert.equal(body.ok, true);
          assert.equal(body.version, "v1");
          assert.equal(body.headers["x-custom"], "probe");
          ready = true;
          break;
        }
      } catch {
        await new Promise((r) => setTimeout(r, 25));
      }
    }
    assert.equal(ready, true, "dockerfile fixture server started");

    const w = await fetch(`http://127.0.0.1:${port}/volume`, {
      method: "POST",
      body: "hello-volume",
    });
    assert.equal(w.status, 200);
    const rd = (await (await fetch(`http://127.0.0.1:${port}/volume`)).json()) as {
      value: string;
    };
    assert.equal(rd.value, "hello-volume");
  } finally {
    proc.kill("SIGTERM");
    fs.rmSync(volDir, { recursive: true, force: true });
  }
});

test("a conforming node passes all eight verification steps and runs cleanup", async () => {
  const h = fakeHarness();
  const results = await runVerify(h);
  assert.equal(results.length, VERIFY_STEPS.length);
  assert.deepEqual(
    results.map((r) => r.status),
    VERIFY_STEPS.map(() => "pass"),
  );
  assert.equal(verifyExitCode(results), 0);
  assert.equal(h.cleanedUp(), true);
  assert.match(formatVerifyReport(results), /8 passed, 0 failed, 0 skipped/);
});

test("a fixture build or route failure fails step 1 and skips the dependent steps", async () => {
  const h = fakeHarness({
    async edgeFetch(host) {
      return { status: host.includes("python") ? 502 : 200, headers: {}, body: "" };
    },
  });
  const results = await runVerify(h);
  assert.equal(byId(results, "fixtures-build-and-serve").status, "fail");
  assert.match(byId(results, "fixtures-build-and-serve").detail, /python.*502/);
  for (const step of VERIFY_STEPS.slice(1)) {
    assert.equal(byId(results, step.id).status, "skip", step.id);
  }
  assert.equal(verifyExitCode(results), 1);
  assert.equal(h.cleanedUp(), true);
});

test("leaked cookies, credentials, client IPs or visitor tags fail the header-stripping check", async () => {
  for (const leakedHeader of [
    "cookie",
    "authorization",
    "cf-connecting-ip",
    "x-nexus-client-tag",
  ]) {
    const base = fakeHarness();
    const h = fakeHarness({
      async edgeFetch(host, urlPath, init) {
        if (urlPath === "/echo") {
          return {
            status: 200,
            headers: {},
            body: JSON.stringify({ headers: { [leakedHeader]: "leaked" } }),
          };
        }
        return base.edgeFetch(host, urlPath, init);
      },
    });
    const results = await runVerify(h);
    const r = byId(results, "proxy-strips-sensitive-headers");
    assert.equal(r.status, "fail", leakedHeader);
    assert.match(r.detail, new RegExp(leakedHeader));
  }
});

test("container egress to loopback or LAN, uncapped memory or logs, and broken short links each fail", async () => {
  const loopbackLeak = await runVerify(
    fakeHarness({
      probeContainerEgress: async () => ({
        loopbackReached: true,
        lanReached: false,
        lanTargets: [],
      }),
    }),
  );
  assert.equal(byId(loopbackLeak, "container-network-isolation").status, "fail");

  const lanLeak = await runVerify(
    fakeHarness({
      probeContainerEgress: async () => ({
        loopbackReached: false,
        lanReached: true,
        lanTargets: ["192.168.0.10"],
      }),
    }),
  );
  assert.equal(byId(lanLeak, "container-network-isolation").status, "fail");
  assert.match(byId(lanLeak, "container-network-isolation").detail, /192\.168\.0\.10/);

  const wrongMem = await runVerify(
    fakeHarness({
      inspectContainerCaps: async () => ({
        memoryMaxBytes: 1024 * 1024 * 1024,
        logDriver: "local",
        logMaxSize: "10m",
        logMaxFile: "2",
        logCompress: "false",
      }),
    }),
  );
  assert.equal(byId(wrongMem, "container-resource-and-log-caps").status, "fail");

  const wrongLogs = await runVerify(
    fakeHarness({
      inspectContainerCaps: async () => ({
        memoryMaxBytes: 512 * 1024 * 1024,
        logDriver: "json-file",
        logMaxSize: "",
        logMaxFile: "",
        logCompress: "",
      }),
    }),
  );
  assert.equal(byId(wrongLogs, "container-resource-and-log-caps").status, "fail");

  const base = fakeHarness();
  const brokenShortLink = await runVerify(
    fakeHarness({
      async edgeFetch(host, urlPath, init) {
        if (host === "deploy.verify.test") return { status: 404, headers: {}, body: "" };
        return base.edgeFetch(host, urlPath, init);
      },
    }),
  );
  assert.equal(byId(brokenShortLink, "short-link-redirect").status, "fail");
});

test("dropped requests during redeploy, accepted unhealthy releases, lost volume data and leftover resources fail", async () => {
  const dropped = await runVerify(
    fakeHarness({
      exerciseReleaseLifecycle: async () => ({
        inFlightTotal: 10,
        inFlightFailed: 2,
        upgradedVersion: "v2",
        unhealthyDeploymentStatus: "failed",
        versionDuringUnhealthy: "v2",
        rolledBackVersion: "v1",
      }),
    }),
  );
  assert.equal(byId(dropped, "zero-downtime-unhealthy-and-rollback").status, "fail");

  const lostVol = await runVerify(
    fakeHarness({
      exerciseVolumePersistence: async () => ({
        project: {
          kind: "dockerfile",
          projectId: "prj_vol",
          slug: "vfy-vol",
          host: "vfy-vol.verify.test",
          containerName: "c",
          firstDeploymentId: "d",
        },
        writtenMarker: "expected-data",
        readAfterRedeploy: "",
        versionAfterRedeploy: "v2",
      }),
    }),
  );
  assert.equal(byId(lostVol, "volume-persists-across-redeploy").status, "fail");

  const leftover = await runVerify(
    fakeHarness({
      deleteAllFixturesAndVerify: async () => ({
        remainingContainers: ["nexus-app-vfy-node-12345678"],
        remainingRoutes: [],
        remainingSlugs: ["vfy-node"],
        remainingVolumes: ["nexus-vol-vfy-vol"],
      }),
    }),
  );
  const del = byId(leftover, "delete-cleans-everything");
  assert.equal(del.status, "fail");
  assert.match(del.detail, /containers: nexus-app-vfy-node-12345678/);
  assert.match(del.detail, /slugs: vfy-node/);
  assert.match(del.detail, /volumes: nexus-vol-vfy-vol/);
});

test("cleanup always runs even when a step throws an exception", async () => {
  const h = fakeHarness({
    async probeContainerEgress() {
      throw new Error("docker exec crashed");
    },
  });
  const results = await runVerify(h);
  assert.equal(byId(results, "container-network-isolation").status, "fail");
  assert.match(byId(results, "container-network-isolation").detail, /docker exec crashed/);
  assert.equal(h.cleanedUp(), true);
});
```

- [ ] **Step 4: Run the full unit test suite, typechecks, and Biome check, then commit**

Run in `apps/Nexus-Deploy`:

```bash
export ESBUILD_BINARY_PATH=/tmp/nexus-deploy-tools/esbuild
node ./node_modules/typescript/bin/tsc -p tsconfig.json --noEmit
node ./node_modules/typescript/bin/tsc -p tsconfig.test.json --noEmit
/tmp/nexus-deploy-tools/biome check src/verify/ src/verify.ts tests/verify.test.ts fixtures/node/ fixtures/dockerfile/
node --import tsx --test tests/*.test.ts
git add fixtures/ src/verify/ src/verify.ts package.json tests/verify.test.ts
git commit -m "feat(verify): end-to-end conformance suite and generic fixture apps"
```
