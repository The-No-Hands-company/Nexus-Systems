# Public IPA Contract Migration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make `/ipa` the only public API prefix, reject public `/api` at the production gate, and prevent the Dashboard from loading indefinitely when identity resolution stalls.

**Architecture:** `deploy/production/proxy.ts` is the sole public ingress contract adapter. It will reject client `/api` requests before routing and translate fixed `/ipa` public paths to existing private `/api` upstream paths. Dashboard browser clients will use same-origin `/ipa` routes; its server continues to use fixed private upstream paths and remains responsible for caller identity injection.

**Tech Stack:** Bun 1.3, TypeScript (strict), Bun test, Vitest, React, Rust/Axum Mail API, Cloudflare Tunnel.

**Spec:** `docs/superpowers/specs/2026-11-01-public-ipa-contract-design.md`

## Global Constraints

- Public `/api/...` requests return `404`; do not redirect or leave a compatibility alias.
- Private loopback service-to-service routes may retain `/api/...`.
- Do not route from a client-supplied upstream host or path beyond an explicit allow-list.
- Do not log cookies, authorization headers, identity tokens, message contents, or mailbox identifiers.
- Browser clients receive no service credentials or caller identity headers.
- Every behavior change starts with a failing test and is committed after its focused checks pass.
- Run Nit and staging validation before production release; production release additionally requires Founder approval and a changelog entry.

---

### Task 1: Establish the public-prefix ingress contract

**Files:**
- Modify: `deploy/production/proxy.ts`
- Modify: `deploy/production/tests/proxy.test.ts`
- Modify: `deploy/production/tests/terminal-public-hop.test.ts`
- Modify: `deploy/production/gate.ts`
- Modify: `deploy/production/tests/gate.test.ts`

**Interfaces:**
- Produces `publicIpaPath(pathname: string): string | null` in `proxy.ts`.
- `publicIpaPath("/ipa/v1/apps")` returns `"/api/v1/apps"`.
- `publicIpaPath("/api/v1/apps")` returns `null`.
- The existing `gate(request, target)` remains the authentication decision point and receives the original public request.

- [ ] **Step 1: Write the failing proxy contract tests**

Add focused tests that execute `handleRequest()` with a mocked `fetch`:

```ts
it("translates the public IPA prefix before forwarding to the fixed API upstream", async () => {
  const response = await handleRequest(new Request("http://api.tnhc.dev/ipa/v1/apps"));
  expect(response.status).toBe(200);
  expect(seenUrl).toBe("http://127.0.0.1:3036/api/v1/apps");
});

it("rejects the deprecated public API prefix before forwarding", async () => {
  const response = await handleRequest(new Request("http://api.tnhc.dev/api/v1/apps"));
  expect(response.status).toBe(404);
  expect(fetch).not.toHaveBeenCalled();
});
```

Add equivalent tests for `app.tnhc.dev/ipa/terminal/attach` and confirm
`/api/terminal/attach` no longer reaches the credential-bearing WebSocket
route.

- [ ] **Step 2: Run the ingress tests and verify they fail**

Run:

```bash
cd deploy/production
bun test tests/proxy.test.ts tests/gate.test.ts tests/terminal-public-hop.test.ts
```

Expected: the `/ipa` forwarding and public `/api` rejection assertions fail
because the proxy currently forwards the browser path unchanged.

- [ ] **Step 3: Implement one fixed public-prefix translation helper**

In `proxy.ts`, add:

```ts
export function publicIpaPath(pathname: string): string | null {
  if (!pathname.startsWith("/ipa/")) return null;
  return `/api/${pathname.slice("/ipa/".length)}`;
}
```

Before calling an upstream, reject requests whose path starts with `/api/`
using `new Response("Not found", { status: 404 })`. For valid `/ipa/` paths,
replace only the upstream URL pathname with `publicIpaPath(url.pathname)`;
preserve the query string, method, and existing fixed-host restrictions.

Change `DASHBOARD_TERMINAL_PATH` to `"/ipa/terminal/attach"` and ensure its
fixed Dashboard upstream receives `"/api/terminal/attach"` rather than a
browser-provided path.

Do not translate `/health`, `/health/live`, `/health/ready`, `/share/...`,
or `/internal/v1/cloudflare-email`; they are not public API contracts.

- [ ] **Step 4: Run the ingress tests and verify they pass**

Run:

```bash
cd deploy/production
bun test tests/proxy.test.ts tests/gate.test.ts tests/terminal-public-hop.test.ts
```

Expected: all selected tests pass, including original identity-header and
credential-forwarding protections.

- [ ] **Step 5: Commit the ingress contract**

```bash
git add deploy/production/proxy.ts deploy/production/gate.ts \
  deploy/production/tests/proxy.test.ts deploy/production/tests/gate.test.ts \
  deploy/production/tests/terminal-public-hop.test.ts
git commit -m "feat: require IPA public API routes"
```

### Task 2: Move the Nexus API public facade to `/ipa`

**Files:**
- Modify: `apps/Nexus-API/src/server.ts`
- Modify: `apps/Nexus-API/src/api-contract.ts`
- Modify: `apps/Nexus-API/src/openapi.ts`
- Modify: `apps/Nexus-API/tests/server.test.ts`
- Modify: `apps/Nexus-API/tests/contract-parity.test.ts`
- Modify: `apps/Nexus-API/tests/public-api.test.ts`

**Interfaces:**
- Consumes the gate translation from Task 1.
- Private handler routes continue to match `/api/v1/...`.
- Public contract documents advertise only `/ipa/v1/...`.
- Produces a public OpenAPI server URL with `/ipa/v1` paths and no `/api/v1`
  path entries.

- [ ] **Step 1: Write failing public-contract tests**

Add tests that assert:

```ts
expect(buildApiContract("https://api.tnhc.dev")).toMatchObject({
  baseUrl: "https://api.tnhc.dev/ipa/v1",
});
expect(buildOpenApiDocument(contract).paths).toHaveProperty("/ipa/v1/me");
expect(buildOpenApiDocument(contract).paths).not.toHaveProperty("/api/v1/me");
```

Keep the direct server tests on `/api/v1/...` and state in their names that
they validate the private loopback contract.

- [ ] **Step 2: Run the Nexus API tests and verify they fail**

Run:

```bash
cd apps/Nexus-API
bun test tests/server.test.ts tests/contract-parity.test.ts tests/public-api.test.ts
```

Expected: public-document assertions fail because generated documents currently
publish `/api/v1`.

- [ ] **Step 3: Implement public-document prefix generation**

Add a public-prefix constant in `api-contract.ts`:

```ts
export const PUBLIC_API_PREFIX = "/ipa/v1";
```

Use it only while building external contract and OpenAPI URLs. Do not change
the `createApiServer()` private route matcher or calls to Auth, Cloud, Mail,
and Calendar; the production proxy translates public `/ipa` to those private
paths.

- [ ] **Step 4: Run the Nexus API quality gate**

Run:

```bash
cd apps/Nexus-API
bun run check && bun test
```

Expected: all API checks and tests pass.

- [ ] **Step 5: Commit the public facade documentation**

```bash
git add apps/Nexus-API/src apps/Nexus-API/tests
git commit -m "feat: publish IPA gateway contract"
```

### Task 3: Make Dashboard browser API calls and terminal routing use `/ipa`

**Files:**
- Modify: `apps/Nexus-Dashboard/frontend/src/api.ts`
- Modify: `apps/Nexus-Dashboard/frontend/src/api.test.ts`
- Modify: `apps/Nexus-Dashboard/frontend/src/pages/Home.test.tsx`
- Modify: `apps/Nexus-Dashboard/src/server.ts`
- Modify: `apps/Nexus-Dashboard/tests/server.test.ts`
- Modify: `apps/Nexus-Dashboard/tests/terminal-proxy.test.ts`

**Interfaces:**
- Browser-facing `platformRequest()` calls `/ipa/v1/me` and `/ipa/v1/apps`.
- Browser-facing Mail, Calendar, Cloud, notification, and terminal calls use
  their existing or migrated `/ipa/...` public routes.
- Dashboard internal proxy calls remain fixed `/api/...` loopback requests.

- [ ] **Step 1: Write failing Dashboard client tests**

Add exact URL assertions:

```ts
expect(mockFetch).toHaveBeenCalledWith(
  "/ipa/v1/me",
  expect.objectContaining({ credentials: "include" }),
);
expect(mockFetch).toHaveBeenCalledWith(
  "/ipa/v1/apps",
  expect.objectContaining({ credentials: "include" }),
);
```

Update terminal tests to issue `"/ipa/terminal/attach"` externally and assert
the Dashboard still opens its private `"/api/v1/terminal/attach"` upstream
connection.

- [ ] **Step 2: Run the Dashboard focused tests and verify they fail**

Run:

```bash
cd apps/Nexus-Dashboard
ESBUILD_BINARY_PATH=/tmp/nexus-dashboard-esbuild \
  bun frontend/node_modules/vitest/vitest.mjs run \
  frontend/src/api.test.ts frontend/src/pages/Home.test.tsx \
  tests/server.test.ts tests/terminal-proxy.test.ts
```

Expected: API client and terminal assertions fail because the browser contract
still includes `/api`.

- [ ] **Step 3: Implement only public URL changes**

Change browser URL literals in `frontend/src/api.ts` from `/api/v1/...` to
`/ipa/v1/...`. In `src/server.ts`, add the public `/ipa/v1/` dispatch entries
that map to the existing Dashboard handlers, while leaving its Auth, Mail,
Cloud, Hosting, Calendar, and terminal upstream URLs as explicit private
`/api/...` values.

Do not change `MAIL_PREFIX`, `CALENDAR_PREFIX`, or `NOTIFICATIONS_PREFIX`;
they are already public `/ipa` boundaries.

- [ ] **Step 4: Run focused Dashboard tests and type-check**

Run:

```bash
cd apps/Nexus-Dashboard
ESBUILD_BINARY_PATH=/tmp/nexus-dashboard-esbuild \
  bun frontend/node_modules/vitest/vitest.mjs run \
  frontend/src/api.test.ts frontend/src/pages/Home.test.tsx \
  frontend/src/pages/mail/MailList.test.tsx frontend/src/pages/mail/MailRead.test.tsx \
  tests/server.test.ts tests/mail-proxy.test.ts tests/terminal-proxy.test.ts
bun frontend/node_modules/typescript/bin/tsc --noEmit -p frontend/tsconfig.json
```

Expected: focused tests and TypeScript check pass.

- [ ] **Step 5: Commit the Dashboard public-contract migration**

```bash
git add apps/Nexus-Dashboard/frontend/src/api.ts \
  apps/Nexus-Dashboard/frontend/src/api.test.ts \
  apps/Nexus-Dashboard/frontend/src/pages/Home.test.tsx \
  apps/Nexus-Dashboard/src/server.ts apps/Nexus-Dashboard/tests
git commit -m "feat: migrate dashboard public routes to IPA"
```

### Task 4: Prevent indefinite Dashboard loading and add safe Mail request correlation

**Files:**
- Modify: `apps/Nexus-Dashboard/frontend/src/api.ts`
- Modify: `apps/Nexus-Dashboard/frontend/src/api.test.ts`
- Modify: `apps/Nexus-Dashboard/frontend/src/pages/Home.tsx`
- Modify: `apps/Nexus-Dashboard/frontend/src/pages/Home.test.tsx`
- Modify: `apps/Nexus-Dashboard/src/server.ts`
- Modify: `apps/Nexus-Dashboard/tests/mail-proxy.test.ts`

**Interfaces:**
- `platformRequest<T>(path, init)` rejects with `ApiError("timeout")` when the
  identity or catalog request exceeds the configured client timeout.
- `Home` transitions out of `user === undefined` after timeout.
- Mail proxy responses include `x-request-id`; logs contain only request ID,
  route category, method, and response status.

- [ ] **Step 1: Write failing timeout and correlation tests**

Use fake timers and a fetch promise that never resolves:

```ts
it("turns an identity timeout into a recoverable signed-out home page", async () => {
  mockFetch.mockImplementation(() => new Promise<Response>(() => {}));
  render(<Home />);
  await vi.advanceTimersByTimeAsync(10_000);
  expect(screen.queryByText("Loading…")).not.toBeInTheDocument();
  expect(screen.getByRole("link", { name: "Sign in" })).toBeVisible();
});
```

Add a Mail proxy test that receives `x-request-id`, while asserting the proxy
log payload contains no request cookie, `authorization`, `x-nexus-subject`,
message ID, or message body.

- [ ] **Step 2: Run the regression tests and verify they fail**

Run:

```bash
cd apps/Nexus-Dashboard
ESBUILD_BINARY_PATH=/tmp/nexus-dashboard-esbuild \
  bun frontend/node_modules/vitest/vitest.mjs run \
  frontend/src/api.test.ts frontend/src/pages/Home.test.tsx tests/mail-proxy.test.ts
```

Expected: the Home test remains on `Loading…` and the Mail response lacks a
correlation ID.

- [ ] **Step 3: Implement the bounded recovery and diagnostics**

Define `const BROWSER_REQUEST_TIMEOUT_MS = 10_000` in `frontend/src/api.ts`.
Pass an abort signal composed with any caller-supplied signal to `fetch`; map
an abort caused by the timeout to:

```ts
throw new ApiError("timeout", "The server took too long to respond. Please try again.");
```

Keep `me()` returning `null` for this error so `Home` reaches `SignedOut`.

In `src/server.ts`, generate a `crypto.randomUUID()` per Mail proxy request,
set it as `x-request-id` on every Mail proxy response, and log only:

```ts
console.info("[mail-proxy]", { requestId, method: req.method, status: res.status });
```

Do not include `rest`, search parameters, headers, body, subject, or user ID.

- [ ] **Step 4: Run Dashboard validation**

Run:

```bash
cd apps/Nexus-Dashboard
ESBUILD_BINARY_PATH=/tmp/nexus-dashboard-esbuild \
  bun frontend/node_modules/vitest/vitest.mjs run \
  frontend/src/api.test.ts frontend/src/pages/Home.test.tsx \
  frontend/src/pages/mail/MailList.test.tsx frontend/src/pages/mail/MailRead.test.tsx \
  tests/mail-proxy.test.ts
bun frontend/node_modules/typescript/bin/tsc --noEmit -p frontend/tsconfig.json
```

Expected: all selected tests and type-check pass.

- [ ] **Step 5: Commit Dashboard resiliency diagnostics**

```bash
git add apps/Nexus-Dashboard/frontend/src/api.ts \
  apps/Nexus-Dashboard/frontend/src/api.test.ts \
  apps/Nexus-Dashboard/frontend/src/pages/Home.tsx \
  apps/Nexus-Dashboard/frontend/src/pages/Home.test.tsx \
  apps/Nexus-Dashboard/src/server.ts apps/Nexus-Dashboard/tests/mail-proxy.test.ts
git commit -m "fix: recover dashboard identity timeouts"
```

### Task 5: Migrate public documentation, probes, and deployment checks

**Files:**
- Modify: `ARCHITECTURE.md`
- Modify: `docs/ECOSYSTEM_CONNECTION_PLAN.md`
- Modify: `docs/LOCAL-DEV.md`
- Modify: `VERSION_MATRIX.md`
- Modify: `deploy/production/deploy.sh`
- Modify: `deploy/production/tests/start-contract.test.sh`
- Modify: `governance/CHANGELOG.md`

**Interfaces:**
- Public documentation shows `/ipa/v1/...`.
- Private loopback examples are labelled `private` and may show `/api/v1/...`.
- Deployment probes use public `/ipa` paths when crossing the gate.

- [ ] **Step 1: Write failing shell assertions for public documentation**

In `start-contract.test.sh`, add:

```bash
assert_contains "/ipa/v1/" "$public_contract_docs" "Public route documentation must use IPA"
assert_not_contains "https://api.tnhc.dev/api/" "$public_contract_docs" "Deprecated public API URL leaked into docs"
```

Build `public_contract_docs` from the four public documentation files above.

- [ ] **Step 2: Run the documentation contract test and verify it fails**

Run:

```bash
cd deploy/production
bash tests/start-contract.test.sh
```

Expected: the public documentation assertion fails because `/api/v1` is
currently documented as public.

- [ ] **Step 3: Update documentation and probes**

Change external examples to `/ipa/v1/...`. Retain private URLs only where the
text explicitly identifies them as loopback/internal. Update deployment
public-hop probes to request `/ipa`; do not alter the loopback health probes.
Add a dated breaking-change entry to `governance/CHANGELOG.md` naming the
removed public `/api` contract and the replacement `/ipa` contract.

- [ ] **Step 4: Run documentation and deployment contract tests**

Run:

```bash
cd deploy/production
bash tests/start-contract.test.sh
bun test tests/proxy.test.ts tests/gate.test.ts tests/terminal-public-hop.test.ts
```

Expected: the documentation contract and selected deployment tests pass.

- [ ] **Step 5: Commit public migration documentation**

```bash
git add ARCHITECTURE.md docs/ECOSYSTEM_CONNECTION_PLAN.md docs/LOCAL-DEV.md \
  VERSION_MATRIX.md deploy/production/deploy.sh \
  deploy/production/tests/start-contract.test.sh governance/CHANGELOG.md
git commit -m "docs: publish IPA public API contract"
```

### Task 6: Build, stage, and release the public-contract migration

**Files:**
- Modify: `governance/CHANGELOG.md` only if staging reveals a release-note
  correction.
- No source change is permitted in this task without returning to its
  task-specific failing test.

**Interfaces:**
- Consumes all prior commits.
- Produces staging evidence that `/ipa` succeeds and public `/api` returns
  `404`.

- [ ] **Step 1: Run service quality gates**

Run:

```bash
cd apps/Nexus-API && bun run check && bun test
cd ../Nexus-Dashboard && \
  ESBUILD_BINARY_PATH=/tmp/nexus-dashboard-esbuild \
  bun frontend/node_modules/vitest/vitest.mjs run && \
  bun frontend/node_modules/typescript/bin/tsc --noEmit -p frontend/tsconfig.json
cd ../Nexus-Email && bash ./check.sh
```

Expected: every command exits zero. Record any pre-existing, unrelated
failure separately rather than weakening a check.

- [ ] **Step 2: Build release artifacts**

Run:

```bash
cd packages/nexus-design && bun run build
cd ../../apps/Nexus-Dashboard/frontend
ESBUILD_BINARY_PATH=/tmp/nexus-dashboard-esbuild bun node_modules/vite/bin/vite.js build
```

Expected: the Dashboard `dist/index.html` references a new hashed JavaScript
bundle.

- [ ] **Step 3: Deploy to staging and probe the public contract**

After staging uses the committed artifacts, run:

```bash
curl --fail-with-body --location https://staging-api.tnhc.dev/ipa/v1/apps
test "$(curl --silent --output /dev/null --write-out '%{http_code}' \
  https://staging-api.tnhc.dev/api/v1/apps)" = "404"
test "$(curl --silent --output /dev/null --write-out '%{http_code}' \
  https://staging-app.tnhc.dev/api/v1/me)" = "404"
```

For an authenticated staging session, move a real Inbox message to Trash and
record the returned `x-request-id`; verify its server-side diagnostic contains
only the request ID, method, route category, and status.

- [ ] **Step 4: Run Nit and obtain release approvals**

Run:

```bash
cd dhts/ecosystem-internal-testsuit
bun run audit
```

Expected: Nit passes. Attach the exact command result to the release record,
obtain staging approval, then obtain Founder production approval.

- [ ] **Step 5: Deploy production and verify**

Deploy the committed Dashboard artifact and production proxy together. Verify:

```bash
test "$(curl --silent --output /dev/null --write-out '%{http_code}' \
  https://api.tnhc.dev/api/v1/apps)" = "404"
curl --fail-with-body https://api.tnhc.dev/ipa/v1/apps
```

Verify the authenticated Dashboard reaches its home screen without a permanent
`Loading…` state and can move a real Inbox message to Trash. If rollback is
needed, restore the immediately previous proxy configuration and Dashboard
bundle together; do not introduce a public `/api` compatibility alias.

- [ ] **Step 6: Commit only any release-note correction**

```bash
git add governance/CHANGELOG.md
git commit -m "docs: record IPA contract release"
```

Skip this commit when the existing release note needs no correction.

## Plan Self-Review

- **Spec coverage:** Task 1 implements public `/api` rejection and fixed
  server-side translation. Task 2 publishes the external gateway contract.
  Task 3 migrates Dashboard browser consumers. Task 4 covers the indefinite
  loading state and safe Mail correlation. Task 5 updates public documentation
  and deployment probes. Task 6 supplies service gates, Nit, staging, Founder
  approval, production verification, and rollback.
- **Placeholder scan:** The plan contains no deferred implementation markers;
  every task names files, tests, commands, interfaces, and commit scope.
- **Type consistency:** `publicIpaPath()` is defined in Task 1 and used only by
  proxy forwarding. Public `/ipa/v1` contracts are generated in Task 2 and
  consumed by Task 3. Private `/api` upstreams remain explicitly server-side
  throughout.
