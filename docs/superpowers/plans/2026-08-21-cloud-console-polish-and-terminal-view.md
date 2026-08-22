# Cloud Console Polish and Shell-Native Terminal Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Finish the existing shell-native Cloud Console, then deliver a founder/admin-only multi-tab `/terminal` backed by Nexus-Terminal's audited host PTYs.

**Architecture:** Keep Cloud as a read-only Dashboard-native console. Add one fixed same-origin Dashboard WebSocket relay to loopback Nexus-Terminal, with independent founder/admin checks at Dashboard and Terminal. Render isolated xterm.js sessions as tabs with a full-width canvas, status footer, and on-demand details drawer.

**Tech Stack:** Bun 1.3.12, strict TypeScript, React 18, React Router, Vitest/Testing Library, xterm.js 5.5, SQLite, Bun WebSockets.

## Global Constraints

- Finish Cloud Console polish before Terminal UI work.
- Add no Cloud mutations or generic proxy capability.
- Restrict Terminal to exactly `founder` and `admin` at Dashboard and Nexus-Terminal.
- Keep Nexus-Terminal loopback-only and disabled unless `NEXUS_TERMINAL_ENABLED=true`.
- Expose exactly `/api/terminal/attach`; never accept a client-selected upstream.
- Keep Cloud keys, Auth cookies, and bearer tokens out of frontend code and application logs.
- Preserve the deliberate input audit and document that it records secrets typed into the shell.
- Support multiple tabs now; do not persist or reattach PTYs after reload.
- Use initial PTY dimensions only; do not claim live server-side resize.
- Preserve existing user edits in Dashboard dependency files and `apps/Nexus-Hosting`.

## File Structure

- `frontend/src/pages/cloud/*`: Cloud rendering and state semantics.
- `apps/Nexus-Terminal/src/auth.ts`: authoritative Terminal role predicate.
- `apps/Nexus-Dashboard/src/auth.ts`: shared Dashboard Auth lookup and admin-role predicate.
- `apps/Nexus-Dashboard/src/terminal.ts`: fixed-upstream WebSocket relay only.
- `apps/Nexus-Dashboard/src/server.ts`: HTTP/upgrade routing and role-aware app list.
- `frontend/src/pages/terminal/session.ts`: one xterm/WebSocket session controller.
- `frontend/src/pages/terminal/TerminalView.tsx`: tabs and Hybrid 1 layout.
- `frontend/src/pages/terminal/TerminalAccess.tsx`: permission gate with no socket side effect.

---

### Task 1: Finish Cloud Console contract and UI polish

**Files:**
- Modify: `apps/Nexus-Dashboard/frontend/src/api.ts`
- Modify: `apps/Nexus-Dashboard/frontend/src/pages/cloud/CloudOverview.tsx`
- Modify: `apps/Nexus-Dashboard/frontend/src/pages/cloud/CloudTools.tsx`
- Modify: `apps/Nexus-Dashboard/frontend/src/pages/cloud/CloudFederation.tsx`
- Modify: `apps/Nexus-Dashboard/frontend/src/pages/cloud/CloudIdentity.tsx`
- Modify: `apps/Nexus-Dashboard/frontend/src/pages/cloud/CloudApi.tsx`
- Modify: `apps/Nexus-Dashboard/frontend/src/pages/cloud/CloudNav.tsx`
- Test: `apps/Nexus-Dashboard/frontend/src/pages/cloud/*.test.tsx`

**Interfaces:**
- Consumes: current Cloud status, tools, endpoints, peers, and identity wire payloads.
- Produces: existing `cloudStatus`, `cloudTrust`, `cloudTools`, `cloudFederationPeers`, `cloudIdentity`, and `cloudEndpoints` helpers with accurate types.

- [ ] **Step 1: Add failing real-payload and semantic-state tests**

Use current fields and assert visible semantics:

```tsx
stubFetch({ "/api/cloud/status": () => jsonResponse({
  mode: "standalone", toolCount: 86, healthyToolCount: 36,
  exposedToolCount: 7, trust: { peers: { total: 0 }, nodes: { total: 0 } },
}) });
expect(await screen.findByText("86 registered")).toBeTruthy();
expect(screen.getByRole("navigation", { name: "Cloud console" })).toBeTruthy();
```

For every view assert loading, ready, empty, and unavailable. Assert semantic tables/headings, textual health in addition to colored dots, and tool-specific accessible link names.

- [ ] **Step 2: Verify the new tests fail**

Run `cd apps/Nexus-Dashboard/frontend && bun run test -- src/pages/cloud`.

Expected: new contract or semantic assertions fail while existing tests remain diagnostic.

- [ ] **Step 3: Make minimal component corrections**

Keep translation in `api.ts`. Use `role="status"` for loading, `role="alert"` for errors, `<th scope="col">`, `overflow-x-auto`, keyboard-visible controls, and:

```tsx
<a aria-label={`Open ${t.name}`} href={url} target="_blank" rel="noreferrer">
  Open <span aria-hidden="true">↗</span>
</a>
```

Retain the existing Nexus tokens, `max-w-5xl`, and optional-card degradation.

- [ ] **Step 4: Run Cloud gates**

Run `cd apps/Nexus-Dashboard/frontend && bun run test -- src/pages/cloud && bun run check`.

Expected: both commands exit 0.

- [ ] **Step 5: Commit**

```bash
git add apps/Nexus-Dashboard/frontend/src/api.ts apps/Nexus-Dashboard/frontend/src/pages/cloud
git diff --cached --check
git commit -m "fix(dashboard): finish cloud console polish"
```

---

### Task 2: Enforce founder/admin inside Nexus-Terminal

**Files:**
- Modify: `apps/Nexus-Terminal/src/auth.ts`
- Modify: `apps/Nexus-Terminal/src/server.ts`
- Modify: `apps/Nexus-Terminal/tests/attach.test.ts`
- Modify: `docs/TERMINAL-SECURITY.md`

**Interfaces:**
- Consumes: `callerIdentity(req): Promise<Caller | null>`.
- Produces: `canUseTerminal(caller: Caller | null): caller is Caller`.

- [ ] **Step 1: Write failing role-matrix tests**

Run a fake Auth server keyed by cookie. Assert `founder` and `admin` reach upgrade handling, while `member`, `operator`, missing role, missing session, and Auth outage fail. Ordinary roles must receive `{ error: "forbidden" }` with 403 for attach and audit.

```ts
for (const role of ["member", "operator", ""]) {
  const res = await fetch(`${base}/api/v1/terminal/attach`, {
    headers: { cookie: `role=${role}` },
  });
  expect(res.status).toBe(403);
}
```

- [ ] **Step 2: Verify the guard test fails**

Run `cd apps/Nexus-Terminal && bun test tests/attach.test.ts`.

Expected: authenticated ordinary roles are not yet rejected.

- [ ] **Step 3: Add and apply the predicate**

```ts
const TERMINAL_ROLES = new Set(["founder", "admin"]);
export function canUseTerminal(caller: Caller | null): caller is Caller {
  return !!caller?.role && TERMINAL_ROLES.has(caller.role);
}
```

Return 401 for null, then 403 for a disallowed role before capacity checks or upgrade. Apply the same sequence to audit.
Also make `Bun.serve` use `hostname: process.env.NEXUS_BIND_HOST || "127.0.0.1"`; the production loopback setting must be enforced by the service rather than merely documented by deploy configuration.

- [ ] **Step 4: Correct the security documentation**

State the exact founder/admin rule and add: “Every input byte is audited; secrets typed interactively can be recorded in the audit database.”

- [ ] **Step 5: Run and commit**

```bash
cd apps/Nexus-Terminal
bun run check
bun test
cd ../..
git add apps/Nexus-Terminal/src/auth.ts apps/Nexus-Terminal/src/server.ts apps/Nexus-Terminal/tests/attach.test.ts docs/TERMINAL-SECURITY.md
git diff --cached --check
git commit -m "fix(terminal): restrict host shells to admins"
```

---

### Task 3: Add the role-aware native Terminal entry

**Files:**
- Modify: `apps/Nexus-Dashboard/src/apps.ts`
- Create: `apps/Nexus-Dashboard/src/auth.ts`
- Modify: `apps/Nexus-Dashboard/src/server.ts`
- Modify: `apps/Nexus-Dashboard/tests/apps.test.ts`
- Modify: `apps/Nexus-Dashboard/tests/server.test.ts`

**Interfaces:**
- Consumes: Auth `/api/v1/auth/me` and `NEXUS_TERMINAL_URL` (default `http://127.0.0.1:3110`).
- Produces: `callerIdentity(req): Promise<Caller | null>`, `isAdminRole(role): boolean`, and `shellNativeEntries({ mailHealthy, terminalHealthy, includeTerminal }): AppEntry[]`.

- [ ] **Step 1: Write failing entry and authorization tests**

```ts
expect(shellNativeEntries({
  mailHealthy: true, terminalHealthy: true, includeTerminal: true,
})).toContainEqual({
  id: "nexus-terminal", name: "Nexus Terminal",
  description: "Audited host shell for Nexus operators",
  url: "/terminal", path: "/terminal", health: "healthy",
});
```

Assert founder/admin `/api/apps` includes Terminal, others do not, offline health is truthful, and a Cloud Terminal record never duplicates or externalizes it.

- [ ] **Step 2: Verify focused tests fail**

Run `cd apps/Nexus-Dashboard && bun test tests/apps.test.ts tests/server.test.ts`.

- [ ] **Step 3: Implement authorized merging and bounded health**

```ts
async function terminalReachable(): Promise<boolean> {
  try {
    return (await fetch(`${terminalUrl()}/health`, {
      signal: AbortSignal.timeout(2000),
    })).ok;
  } catch { return false; }
}
```

Make `fetchApps(req)` obtain the caller, include/probe Terminal only for allowed roles, and pass `req` from `/api/apps`. Add `terminal` to reserved routes. Filter a Cloud `nexus-terminal` record for every non-admin and let the native entry win for admins.

Extract Dashboard's existing `Caller`, `ADMIN_ROLES`, and `callerIdentity` implementation into `src/auth.ts`; export `isAdminRole(role: string | null): boolean`. Update mail, issues, Cloud admin proxy, and app-list callers to import that one implementation so the later relay cannot drift to a different role rule.

- [ ] **Step 4: Run and commit**

```bash
cd apps/Nexus-Dashboard
bun test tests/apps.test.ts tests/server.test.ts
bun run check
cd ../..
git add apps/Nexus-Dashboard/src/apps.ts apps/Nexus-Dashboard/src/auth.ts apps/Nexus-Dashboard/src/server.ts apps/Nexus-Dashboard/tests/apps.test.ts apps/Nexus-Dashboard/tests/server.test.ts
git diff --cached --check
git commit -m "feat(dashboard): add admin terminal entry"
```

---

### Task 4: Build the fixed WebSocket relay

**Files:**
- Create: `apps/Nexus-Dashboard/src/terminal.ts`
- Create: `apps/Nexus-Dashboard/tests/terminal-proxy.test.ts`
- Modify: `apps/Nexus-Dashboard/src/server.ts`

**Interfaces:**
- Consumes: `callerIdentity` and `isAdminRole` from `src/auth.ts`.
- Produces: `TerminalSocketData`, `authorizeTerminalUpgrade(req)`, and `terminalWebSocketHandlers`.

- [ ] **Step 1: Write failing authorization and relay tests**

Test zero upstream connections for signed-out/member calls, fixed URL construction for founder/admin, cookie forwarding upstream only, pre-open buffering, text/binary relay, close propagation, and `1011` on upstream failure.

```ts
const result = await authorizeTerminalUpgrade(founderRequest);
expect(result.ok).toBe(true);
if (result.ok) {
  expect(result.data.upstreamUrl).toBe(
    "ws://127.0.0.1:3110/api/v1/terminal/attach?cols=120&rows=40",
  );
}
```

- [ ] **Step 2: Verify the module is absent**

Run `cd apps/Nexus-Dashboard && bun test tests/terminal-proxy.test.ts`.

- [ ] **Step 3: Implement the narrow relay**

```ts
export type TerminalSocketData = { upstreamUrl: string; cookie: string };
export type UpgradeDecision =
  | { ok: true; data: TerminalSocketData }
  | { ok: false; response: Response };
```

Require exact path, WebSocket headers, caller, allowed role, non-empty cookie, and numeric dimensions clamped to Terminal bounds. Build upstream only from server `NEXUS_TERMINAL_URL`. Pair sockets in a `WeakMap`, queue early frames, forward cookie only in upstream constructor headers, and relay close/error without logging credentials.

- [ ] **Step 4: Wire Bun upgrade handling**

Allow `handleRequest` to receive `server.upgrade`. Route exact attach before ordinary `/api` handling. Start `Bun.serve<TerminalSocketData>` with `terminalWebSocketHandlers`.

- [ ] **Step 5: Run and commit**

```bash
cd apps/Nexus-Dashboard
bun test tests/terminal-proxy.test.ts tests/server.test.ts
bun run check
cd ../..
git add apps/Nexus-Dashboard/src/terminal.ts apps/Nexus-Dashboard/src/server.ts apps/Nexus-Dashboard/tests/terminal-proxy.test.ts
git diff --cached --check
git commit -m "feat(dashboard): relay terminal websocket"
```

---

### Task 5: Implement one isolated xterm session controller

**Files:**
- Create: `apps/Nexus-Dashboard/frontend/src/pages/terminal/session.ts`
- Create: `apps/Nexus-Dashboard/frontend/src/pages/terminal/session.test.ts`
- Modify: `apps/Nexus-Dashboard/frontend/package.json`
- Modify: root `bun.lock` only if `bun install` changes it for xterm dependencies

**Interfaces:**
- Produces: `createTerminalSession(opts): TerminalSessionController`.

- [ ] **Step 1: Write failing controller tests with injected factories**

```ts
export type SessionState = "connecting" | "connected" | "disconnected"
  | "refused" | "disabled" | "unavailable" | "limit";
export type TerminalSessionController = {
  id: string; startedAt: number;
  mount(element: HTMLElement): void;
  focus(): void; fit(): void; dispose(): void;
};
```

Assert initial dimension query, xterm input to only its socket, socket data to `terminal.write`, close-code mapping, renderer-only `fit`, and exactly-once disposal.

- [ ] **Step 2: Verify missing controller**

Run `cd apps/Nexus-Dashboard/frontend && bun run test -- src/pages/terminal/session.test.ts`.

- [ ] **Step 3: Implement controller**

Create one `Terminal` and `FitAddon`. Mount/open/fit first, then use:

```ts
const protocol = window.location.protocol === "https:" ? "wss:" : "ws:";
const url = `${protocol}//${window.location.host}/api/terminal/attach?cols=${terminal.cols}&rows=${terminal.rows}`;
```

Forward xterm `onData` only while connected and socket messages through `terminal.write`. `fit()` sends no resize frame. `dispose()` closes socket and disposes subscriptions/xterm once.

- [ ] **Step 4: Resolve dependencies and run**

Preserve user-declared `@xterm/xterm@^5.5.0` and `@xterm/addon-fit@^0.10.0`. Run `bun install`, inspect lock changes, and do not stage the user-owned pnpm files.

Run `cd apps/Nexus-Dashboard/frontend && bun run test -- src/pages/terminal/session.test.ts && bun run check`.

- [ ] **Step 5: Commit**

```bash
git add apps/Nexus-Dashboard/frontend/src/pages/terminal/session.ts apps/Nexus-Dashboard/frontend/src/pages/terminal/session.test.ts apps/Nexus-Dashboard/frontend/package.json bun.lock
git diff --cached --check
git commit -m "feat(dashboard): add terminal session controller"
```

---

### Task 6: Build Hybrid 1 with multiple tabs

**Files:**
- Create: `apps/Nexus-Dashboard/frontend/src/pages/terminal/TerminalView.tsx`
- Create: `apps/Nexus-Dashboard/frontend/src/pages/terminal/TerminalView.test.tsx`
- Create: `apps/Nexus-Dashboard/frontend/src/pages/terminal/TerminalAccess.tsx`
- Create: `apps/Nexus-Dashboard/frontend/src/pages/terminal/TerminalAccess.test.tsx`
- Modify: `apps/Nexus-Dashboard/frontend/src/App.tsx`
- Modify: `apps/Nexus-Dashboard/frontend/src/App.test.tsx`
- Modify: `apps/Nexus-Dashboard/frontend/src/index.css`

**Interfaces:**
- Consumes: `createTerminalSession`, `Me`, and `isAdmin`.
- Produces: shell-native `/terminal`, multi-tab PTYs, footer, and details drawer.

- [ ] **Step 1: Write failing access and route tests**

For member, assert “Terminal access required” and zero controller calls. For founder/admin, assert one automatic controller. Add `/terminal` to the shell-native routing matrix.

- [ ] **Step 2: Write failing tab lifecycle tests**

Assert `+` creates a second controller, switching focuses without disposal, `×` disposes only that tab, last-close shows `New session`, Details opens `role="dialog"`, Disconnect removes active session, and unmount disposes all exactly once.

- [ ] **Step 3: Verify missing UI**

Run `cd apps/Nexus-Dashboard/frontend && bun run test -- src/pages/terminal src/App.test.tsx`.

- [ ] **Step 4: Implement access gate and route**

Track `me()` as loading/ready separately so initial null never attaches or prematurely denies. Route inside `ShellView`:

```tsx
<Route path="/terminal" element={
  <ShellView state={appsState} user={user}>
    <TerminalAccess userState={userState} />
  </ShellView>
} />
```

- [ ] **Step 5: Implement Hybrid 1**

Store controllers in a ref-backed map. Render accessible `tablist`, `tab`, and `tabpanel`; keep inactive terminal DOM mounted but hidden. Full-width active canvas uses `min-h-0 flex-1`. Footer shows state text, `Audited`, and elapsed time. Details opens `aside role="dialog" aria-label="Terminal session details"`. Import xterm CSS and scope terminal overrides.

- [ ] **Step 6: Run and commit**

```bash
cd apps/Nexus-Dashboard/frontend
bun run test -- src/pages/terminal src/App.test.tsx
bun run test
bun run check
bun run build
cd ../../../
git add apps/Nexus-Dashboard/frontend/src/pages/terminal apps/Nexus-Dashboard/frontend/src/App.tsx apps/Nexus-Dashboard/frontend/src/App.test.tsx apps/Nexus-Dashboard/frontend/src/index.css
git diff --cached --check
git commit -m "feat(dashboard): build multi-tab terminal view"
```

---

### Task 7: Add production startup and integration smoke test

**Files:**
- Modify: `deploy/production/deploy.sh`
- Modify: `deploy/production/tests/processes.test.sh`
- Modify: `apps/Nexus-Terminal/README.md`
- Modify: `docs/LOCAL-DEV.md`
- Modify: `apps/Nexus-Dashboard/tests/terminal-proxy.test.ts`

**Interfaces:**
- Consumes: Terminal `127.0.0.1:3110`, Auth `:4310`, Cloud `:8787`, Dashboard `:3132`.
- Produces: repeatable Terminal-before-Dashboard startup and a real relay smoke test.

- [ ] **Step 1: Write failing production assertions**

Assert Terminal starts before Dashboard, loopback URLs are passed, the enable flag is explicit, Dashboard receives `NEXUS_TERMINAL_URL=http://127.0.0.1:3110`, and deploy health checks Terminal.

- [ ] **Step 2: Verify production test fails**

Run `timeout 120s bash deploy/production/tests/processes.test.sh`.

- [ ] **Step 3: Add loopback startup**

Add `start_service "terminal"` before Dashboard with port/bind, Auth/Cloud URLs, Cloud API key, `NEXUS_NEXUS_TERMINAL_BASE_URL=http://127.0.0.1:3110`, and explicit `NEXUS_TERMINAL_ENABLED`. Pass the Terminal URL to Dashboard and add `/health` verification.

- [ ] **Step 4: Document behavior**

Document attach/audit endpoints, founder/admin restriction, enable switch, loopback-only deployment, multiple tabs, initial-size limitation, and input-audit warning. Add local Terminal + Dashboard startup to `docs/LOCAL-DEV.md`.

- [ ] **Step 5: Add real relay smoke coverage**

Start fake Auth, enabled Terminal, and Dashboard. Connect through Dashboard with a founder cookie, send `printf 'nexus-terminal-smoke\\n'\r`, and observe the marker. Open two sockets with distinct markers and prove isolation. Close both and bounded-retry Terminal `/health` until `activeShells` is zero.

- [ ] **Step 6: Run and commit**

```bash
cd apps/Nexus-Terminal && bun run check && bun test
cd ../Nexus-Dashboard && bun run check && bun test tests/
cd frontend && bun run test && bun run check && bun run build
cd ../../../
bun test deploy/production/tests/proxy.test.ts
timeout 120s bash deploy/production/tests/processes.test.sh
bash -n deploy/production/deploy.sh
git add deploy/production/deploy.sh deploy/production/tests/processes.test.sh apps/Nexus-Terminal/README.md docs/LOCAL-DEV.md apps/Nexus-Dashboard/tests/terminal-proxy.test.ts
git diff --cached --check
git commit -m "feat(terminal): integrate shell view in production"
```

---

### Task 8: Final regression and acceptance audit

**Files:**
- Verify only; modify a task-owned file only when a failing acceptance check exposes a defect.

**Interfaces:**
- Consumes: Tasks 1–7.
- Produces: evidence for every acceptance criterion without staging unrelated work.

- [ ] **Step 1: Run complete targeted regression**

```bash
cd apps/Nexus-Cloud && bun run check && bun test src
cd ../Nexus-Terminal && bun run check && bun test
cd ../Nexus-Dashboard && bun run check && bun test tests/
cd frontend && bun run test && bun run check && bun run build
cd ../../../
bun test deploy/production/tests/proxy.test.ts
timeout 120s bash deploy/production/tests/processes.test.sh
git diff --check
```

Expected: every command exits 0 and diff check prints nothing.

- [ ] **Step 2: Audit secrets and upstream construction**

```bash
rg -n "NEXUS_CLOUD_API_KEY|nexus_session=|NEXUS_TERMINAL_URL" apps/Nexus-Dashboard/frontend/dist
rg -n "new URL\(|upstream" apps/Nexus-Dashboard/src/terminal.ts
```

Expected: no transport secrets/internal URL appear in `dist`; relay upstream comes only from server configuration.

- [ ] **Step 3: Check worktree ownership**

Run `git status --short && git log -10 --oneline`.

Expected: task commits exist; pre-existing Hosting and pnpm changes remain untouched unless separately authorized.

- [ ] **Step 4: Commit only a proven final correction**

If the audit required a task-scoped fix, rerun its focused test, stage only named files, run `git diff --cached --check`, and commit `fix(terminal): close integration regression`. Otherwise create no empty commit.
