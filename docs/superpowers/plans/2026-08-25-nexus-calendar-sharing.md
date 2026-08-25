# Nexus Calendar Sharing and Proxied-App Delivery Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Deliver private-by-default Calendar events, explicit Nexus-user sharing, revocable public read-only links, and one Calendar frontend served at both `/calendar` and `calendar.tnhc.dev` without an iframe.

**Architecture:** Nexus Calendar owns its database, authorization, API, and sole frontend artifact. Nexus Cloud and Dashboard expose an explicit `path`/`publicUrl`/`delivery` registry contract; Dashboard proxies the Calendar document and API at the canonical path while Calendar’s direct origin provides authenticated app access plus a narrow public-token route.

**Tech Stack:** Bun 1.3.12, strict TypeScript 5.8, `bun:sqlite`, React 18, Vite 5, Tailwind 4, Bun test, Vitest/jsdom, Caddy, Nexus Auth identity JWTs.

## Global Constraints

- Events are private to their trusted `ownerSubject` by default.
- Internal permissions are exactly `viewer` and `editor`; only owners manage sharing or delete events.
- Public links are read-only, independently revocable bearer capabilities with at least 128 bits of entropy.
- Public event responses contain only title, start/end, all-day state, location, and description.
- The Calendar backend remains loopback-only in production; browser identity headers are never trusted.
- One Calendar frontend artifact must serve both `/` and `/calendar`; Dashboard must contain no Calendar feature implementation and no Calendar iframe.
- `path` is the canonical shell route, `publicUrl` is the direct origin, and `delivery` is `shell-native`, `proxied-app`, `framed`, or `external`.
- Existing user changes and `apps/Nexus-Hosting` submodule state are out of scope and must be preserved.
- Each behavior change starts with a failing focused test and ends with its focused gate plus `git diff --check`.

---

## File Map

- `apps/Nexus-Cloud/src/systems-api/types.ts`, `service.ts`, `registry.ts`, `store.ts`: persist and return delivery metadata.
- `apps/Nexus-Dashboard/src/apps.ts` and `frontend/src/api.ts`: normalize registry entries and make launcher routing independent of deployment origin.
- `apps/Nexus-Calendar/src/calendar-engine.ts`: SQLite schema, migration, transactions, ownership and sharing persistence only.
- `apps/Nexus-Calendar/src/auth.ts`: extract trusted caller subjects from proxy identity JWTs or the loopback Dashboard hop.
- `apps/Nexus-Calendar/src/validation.ts`: allow-listed event/range/share validation.
- `apps/Nexus-Calendar/src/server.ts`: HTTP routing and authorization orchestration.
- `apps/Nexus-Calendar/frontend/src/calendar-api.ts`: typed HTTP client and stable error mapping.
- `apps/Nexus-Calendar/frontend/src/CalendarApp.tsx`: authenticated month/event/sharing experience.
- `apps/Nexus-Calendar/frontend/src/PublicEventPage.tsx`: isolated public read-only event view.
- `apps/Nexus-Calendar/frontend/src/runtime.ts`: `/` versus `/calendar` base-path and shell-context resolution.
- `packages/nexus-app-shell/`: small shared shell header/navigation primitive consumed by Dashboard and Calendar.
- `apps/Nexus-Dashboard/src/calendar-proxy.ts`: allow-listed document/assets/API proxy for the canonical shell path.
- `deploy/production/gate.ts`, `nexus-calendar.Caddyfile`, `deploy.sh`: narrow public-token exception and dual-base production delivery.

---

### Task 1: Explicit Registry Delivery Contract

**Files:**
- Modify: `apps/Nexus-Cloud/src/systems-api/types.ts`
- Modify: `apps/Nexus-Cloud/src/systems-api/service.ts`
- Modify: `apps/Nexus-Cloud/src/systems-api/registry.ts`
- Modify: `apps/Nexus-Cloud/src/systems-api/store.ts`
- Modify: `apps/Nexus-Cloud/src/test/registry.service.test.ts`
- Modify: `apps/Nexus-Calendar/src/contracts.ts`
- Modify: `apps/Nexus-Dashboard/src/apps.ts`
- Modify: `apps/Nexus-Dashboard/frontend/src/api.ts`
- Modify: `apps/Nexus-Dashboard/tests/apps.test.ts`

**Interfaces:**
- Produces: `SystemsApiDelivery = "shell-native" | "proxied-app" | "framed" | "external"`.
- Produces: `AppEntry { path: string; publicUrl?: string; delivery: SystemsApiDelivery }`; remove overloaded `url` after compatibility normalization.
- Calendar registration: `{ path: "/calendar", publicUrl: "https://calendar.tnhc.dev", delivery: "proxied-app" }`.

- [ ] **Step 1: Write failing Cloud persistence tests**

Add a registration/reload case that registers Calendar, asserts the three fields, serializes state, reloads it, and asserts they survive unchanged. Add malformed persisted values that fall back to `framed` for absolute public URLs and `shell-native` for relative-only entries.

```ts
expect(reloaded).toMatchObject({
  path: "/calendar",
  publicUrl: "https://calendar.tnhc.dev",
  delivery: "proxied-app",
});
```

- [ ] **Step 2: Run the focused Cloud test and verify RED**

Run: `cd apps/Nexus-Cloud && bun test src/test/registry.service.test.ts`

Expected: type/assertion failure because `path` and `delivery` do not exist.

- [ ] **Step 3: Implement and persist the contract**

Add exact sanitizers for relative paths and the four delivery literals. Accept these fields during registration, copy them in `buildTool`, and include them in `sanitizeTool`. Reject non-relative `path`; do not infer `proxied-app` from a URL.

```ts
export type SystemsApiDelivery = "shell-native" | "proxied-app" | "framed" | "external";
// Registration fields:
path?: string;
publicUrl?: string;
delivery?: SystemsApiDelivery;
```

- [ ] **Step 4: Write failing Dashboard normalization tests**

Assert Calendar remains `proxied-app`, launcher destination is `/calendar`, direct origin remains `publicUrl`, Cloud is `shell-native`, and a legacy absolute-URL record normalizes to `framed` without changing its shell path.

- [ ] **Step 5: Implement Dashboard normalization**

Replace `url`-based delivery decisions with `delivery`. Keep a `WireAppEntry` compatibility adapter for old servers, but all returned `AppEntry` values must contain explicit delivery and separate public URL.

```ts
export type AppEntry = {
  id: string; name: string; description: string;
  path: string; publicUrl?: string; delivery: SystemsApiDelivery;
  health: "healthy" | "offline";
};
```

- [ ] **Step 6: Run focused tests and commit**

Run: `cd apps/Nexus-Cloud && bun test src/test/registry.service.test.ts && cd ../Nexus-Dashboard && bun test tests/apps.test.ts`

Commit: `feat(registry): separate app paths from delivery origins`

---

### Task 2: Owned Event Schema and Fail-Closed Migration

**Files:**
- Modify: `apps/Nexus-Calendar/src/calendar-engine.ts`
- Create: `apps/Nexus-Calendar/tests/calendar-engine.test.ts`
- Modify: `apps/Nexus-Calendar/src/server.ts`
- Modify: `apps/Nexus-Calendar/README.md`

**Interfaces:**
- Produces: `CalEvent` with `ownerSubject`, `access: "owner" | "editor" | "viewer"`.
- Produces: `new CalendarEngine(path, { legacyOwnerSubject?: string })`.
- Produces: `close(): void` and transaction-backed migration.

- [ ] **Step 1: Write migration tests**

Create a temporary legacy SQLite database with the current `events` columns. Assert startup fails with `legacy_owner_required` when rows exist and no owner is configured; assert `NEXUS_CALENDAR_LEGACY_OWNER_SUBJECT=usr-founder` backfills all rows; assert a fresh empty database needs no legacy owner.

- [ ] **Step 2: Run the engine test and verify RED**

Run: `cd apps/Nexus-Calendar && bun test tests/calendar-engine.test.ts`

Expected: constructor signature/schema assertions fail.

- [ ] **Step 3: Implement schema versioning and ownership**

Use `PRAGMA user_version` and one immediate transaction. Add `owner_subject TEXT NOT NULL` by rebuilding the table when necessary, create indexes on `(owner_subject, start_time, end_time)`, and never choose an implicit owner for existing rows.

```ts
constructor(path = ":memory:", options: { legacyOwnerSubject?: string } = {})
createEvent(ownerSubject: string, input: EventCreate): CalEvent
listEvents(callerSubject: string, range: EventRange): CalEvent[]
```

- [ ] **Step 4: Add shutdown verification**

Test that `close()` closes the SQLite handle and that `createServer().close()` stops heartbeat, closes the engine, then stops Bun.

- [ ] **Step 5: Run and commit**

Run: `cd apps/Nexus-Calendar && bun test tests/calendar-engine.test.ts tests/server.test.ts`

Commit: `feat(calendar): migrate events to private ownership`

---

### Task 3: Trusted Identity, Validation, and Owner CRUD

**Files:**
- Create: `apps/Nexus-Calendar/src/auth.ts`
- Create: `apps/Nexus-Calendar/src/validation.ts`
- Modify: `apps/Nexus-Calendar/src/calendar-engine.ts`
- Modify: `apps/Nexus-Calendar/src/server.ts`
- Modify: `apps/Nexus-Calendar/tests/server.test.ts`
- Create: `apps/Nexus-Calendar/tests/auth.test.ts`
- Create: `apps/Nexus-Calendar/tests/validation.test.ts`

**Interfaces:**
- Produces: `resolveCaller(req): Promise<{ subject: string } | null>`.
- Produces: `parseEventCreate`, `parseEventPatch`, `parseRange`, each returning `{ ok: true, value } | { ok: false, error }`.
- Engine CRUD signatures take `callerSubject` and enforce access in SQL.

- [ ] **Step 1: Write failing identity-boundary tests**

Prove a browser-supplied `x-nexus-subject` is ignored unless the request carries the private Dashboard hop marker configured by `NEXUS_CALENDAR_DASHBOARD_SECRET`. Prove a valid `x-nexus-identity` JWT with the Calendar audience resolves its `sub`, while wrong audience, expiry, algorithm, and signature fail.

- [ ] **Step 2: Implement identity resolution**

Reuse the repository’s JWKS/RS256 verification pattern, cache keys with a bounded TTL, require audience `NEXUS_CALENDAR_JWT_AUDIENCE` (default `calendar.tnhc.dev`), and compare the Dashboard secret with constant-time byte comparison.

```ts
export async function resolveCaller(req: Request): Promise<{ subject: string } | null>;
```

- [ ] **Step 3: Write validation and authorization tests**

Cover blank/oversized titles, unknown fields, malformed timestamps, `end <= start`, oversized query windows, invalid IDs, owner-only patch/delete, and cross-user list/get returning no event or `404`.

- [ ] **Step 4: Implement minimal owner CRUD**

Create events with the resolved subject only. Use interval overlap:

```sql
WHERE start_time < :requested_end AND end_time > :requested_start
```

Return `401` without identity, `400` for invalid input, and `404` for inaccessible IDs.

```ts
type ValidationResult<T> = { ok: true; value: T } | { ok: false; error: string };
export function parseEventCreate(value: unknown): ValidationResult<EventCreate>;
export function parseEventPatch(value: unknown): ValidationResult<EventPatch>;
export function parseRange(url: URL): ValidationResult<EventRange>;
```

- [ ] **Step 5: Run and commit**

Run: `cd apps/Nexus-Calendar && bun test tests/auth.test.ts tests/validation.test.ts tests/server.test.ts`

Commit: `feat(calendar): enforce owner-scoped event access`

---

### Task 4: Internal Viewer and Editor Sharing

**Files:**
- Modify: `apps/Nexus-Calendar/src/calendar-engine.ts`
- Modify: `apps/Nexus-Calendar/src/validation.ts`
- Modify: `apps/Nexus-Calendar/src/server.ts`
- Create: `apps/Nexus-Calendar/tests/sharing.test.ts`

**Interfaces:**
- Produces: `EventPermission = "viewer" | "editor"` and `EventShare`.
- Produces: list/upsert/delete share methods executed transactionally.
- Endpoints: `GET .../:eventId/shares`, `PUT/DELETE .../:eventId/shares/:subject`.

- [ ] **Step 1: Write the permission-matrix tests**

Use owner, editor, viewer, and stranger subjects. Assert viewer reads but cannot patch; editor reads/patches but cannot delete or share; owner can do all; stranger receives `404`; listings contain owned/shared events once.

- [ ] **Step 2: Run and verify RED**

Run: `cd apps/Nexus-Calendar && bun test tests/sharing.test.ts`

- [ ] **Step 3: Implement share persistence and SQL access joins**

Create `event_shares(event_id, grantee_subject, permission, created_by, created_at, updated_at)` with foreign key cascade and a composite primary key. Resolve access in the engine rather than trusting route-level prechecks.

```ts
type EventPermission = "viewer" | "editor";
upsertShare(owner: string, eventId: string, subject: string, permission: EventPermission): EventShare | undefined;
deleteShare(owner: string, eventId: string, subject: string): boolean;
```

- [ ] **Step 4: Implement owner-only share routes**

Allow-list only `permission` in PUT bodies. Prevent sharing with the owner, use idempotent upsert/delete semantics, and never return grants to non-owners.

```json
{ "permission": "viewer" }
```

- [ ] **Step 5: Run and commit**

Run: `cd apps/Nexus-Calendar && bun test tests/sharing.test.ts tests/server.test.ts`

Commit: `feat(calendar): add explicit user event sharing`

---

### Task 5: Revocable Public Read-Only Links

**Files:**
- Modify: `apps/Nexus-Calendar/src/calendar-engine.ts`
- Modify: `apps/Nexus-Calendar/src/server.ts`
- Create: `apps/Nexus-Calendar/tests/public-sharing.test.ts`
- Modify: `deploy/production/gate.ts`
- Modify: `deploy/production/tests/gate.test.ts`

**Interfaces:**
- Produces: `createPublicShare(owner, eventId): { token: string; publicPath: string }`.
- Produces: filtered `PublicEvent` without IDs tied to identity or sharing metadata.
- Public API: `GET /api/v1/calendar/public/:token`.

- [ ] **Step 1: Write failing token and filtering tests**

Assert generated tokens decode to at least 16 random bytes, replacement invalidates the previous token, revocation returns `404`, non-GET methods return `405`, and public JSON contains exactly the allowed keys.

- [ ] **Step 2: Implement hashed token storage**

Store `SHA-256(token)` rather than the bearer token, compare by digest, return the raw token only on creation, and exclude tokens/digests from logs and ordinary event queries.

```ts
const token = randomBytes(32).toString("base64url");
const tokenDigest = createHash("sha256").update(token).digest("hex");
```

- [ ] **Step 3: Write the production gate exception tests**

Assert only `GET https://calendar.tnhc.dev/api/v1/calendar/public/<valid-shape>` bypasses SSO. Other Calendar API paths, other methods, other hosts, missing tokens, encoded slashes, and query tricks remain gated.

- [ ] **Step 4: Implement the structural gate exception**

Add a host-, method-, and anchored-path-specific predicate before default denial. Do not add Calendar to `PUBLIC_HOSTS` or the general `PUBLIC_PATHS` set.

```ts
const PUBLIC_CALENDAR_EVENT = /^\/api\/v1\/calendar\/public\/[A-Za-z0-9_-]{43}$/;
```

- [ ] **Step 5: Run and commit**

Run: `cd apps/Nexus-Calendar && bun test tests/public-sharing.test.ts && cd ../../deploy/production && bun test tests/gate.test.ts`

Commit: `feat(calendar): add revocable public event links`

---

### Task 6: Hardened Dashboard Calendar Proxy

**Files:**
- Create: `apps/Nexus-Dashboard/src/calendar-proxy.ts`
- Modify: `apps/Nexus-Dashboard/src/server.ts`
- Create: `apps/Nexus-Dashboard/tests/calendar-proxy.test.ts`
- Modify: `deploy/production/deploy.sh`

**Interfaces:**
- Produces: `proxyCalendarApi(req, rest, search): Promise<Response>`.
- Produces: `proxyCalendarWeb(req, relativePath): Promise<Response>` for `/calendar`, nested routes, and hashed assets.
- Uses `NEXUS_CALENDAR_DASHBOARD_SECRET` on the private upstream hop.

- [ ] **Step 1: Write failing API proxy tests**

Prove authentication happens before upstream contact; inbound subject/secret headers are stripped; the Auth-derived subject and private hop secret are attached; GET/POST/PATCH/PUT/DELETE are forwarded only to the exact Calendar allow-list; bodies, queries, statuses, and safe response headers survive; failure is a stable `503`.

- [ ] **Step 2: Write failing web proxy tests**

Prove `/calendar`, `/calendar/`, client routes, and assets come from the Calendar web upstream; traversal and API confusion are rejected; HTML is uncached; hashed assets retain immutable caching; responses declare shell context without an iframe.

- [ ] **Step 3: Implement the isolated proxy module**

Read upstream URLs per call for test isolation. Use method/path allow-lists, bounded timeouts, explicit request/response headers, and no generic pass-through.

```ts
export async function proxyCalendarApi(req: Request, rest: string, search: string): Promise<Response>;
export async function proxyCalendarWeb(req: Request, relativePath: string): Promise<Response>;
```

- [ ] **Step 4: Wire server and deployment secrets**

Route Calendar requests before Dashboard’s SPA fallback. Generate/load one production secret, pass it to Dashboard and Calendar, and never expose it to frontend runtime configuration.

```text
NEXUS_CALENDAR_DASHBOARD_SECRET=<32-byte deployment secret>
NEXUS_CALENDAR_WEB_URL=http://127.0.0.1:8092
```

- [ ] **Step 5: Run and commit**

Run: `cd apps/Nexus-Dashboard && bun test tests/calendar-proxy.test.ts tests/server.test.ts`

Commit: `feat(dashboard): proxy calendar as a shell app`

---

### Task 7: Shared App Chrome and Base-Path Runtime

**Files:**
- Create: `packages/nexus-app-shell/package.json`
- Create: `packages/nexus-app-shell/src/index.tsx`
- Create: `packages/nexus-app-shell/src/AppHeader.tsx`
- Create: `packages/nexus-app-shell/src/AppHeader.test.tsx`
- Create: `packages/nexus-app-shell/vitest.config.ts`
- Modify: `apps/Nexus-Dashboard/frontend/src/shell/Shell.tsx`
- Modify: `apps/Nexus-Calendar/frontend/package.json`
- Create: `apps/Nexus-Calendar/frontend/src/runtime.ts`
- Create: `apps/Nexus-Calendar/frontend/src/runtime.test.ts`
- Modify: `apps/Nexus-Calendar/frontend/vite.config.ts`

**Interfaces:**
- Produces: `AppHeader({ homeHref, appName, userSlot, utilitySlot })` with ordinary anchor navigation that works across separately owned documents.
- Produces: `CalendarRuntime { basePath, apiBase, publicBase, shellContext, publicToken? }`.

- [ ] **Step 1: Write failing shared-header accessibility tests**

Assert a banner, Nexus home link, app name, keyboard-operable navigation, and slots. Keep the component fetch-free and router-independent.

- [ ] **Step 2: Extract the minimal shared primitive**

Move only common header markup and tokens; Dashboard retains its sidebar, routing, notifications, and role decisions. Build/package it through the existing Bun workspace.

```ts
export type AppHeaderProps = {
  homeHref: string; appName: string; userSlot?: ReactNode; utilitySlot?: ReactNode;
};
```

- [ ] **Step 3: Write base-path tests**

Cover direct `/`, direct `/share/token`, shell `/calendar`, shell `/calendar/month`, and injected runtime config. Assert assets are relative/base-safe and API bases are `/api/v1/calendar` direct versus `/ipa/calendar` through Dashboard.

- [ ] **Step 4: Implement runtime resolution and one relative build**

Use `base: "./"` for Vite assets and an explicit server-provided shell-context marker. Never infer authorization from the hostname or UI context.

```ts
export type CalendarRuntime = {
  basePath: "/" | "/calendar";
  apiBase: "/api/v1/calendar" | "/ipa/calendar";
  publicBase: string;
  shellContext: boolean;
  publicToken?: string;
};
```

- [ ] **Step 5: Run and commit**

Run: `cd packages/nexus-app-shell && bun test && cd ../../apps/Nexus-Calendar/frontend && bun run build && bun run test`

Commit: `feat(ui): add shared chrome for proxied apps`

---

### Task 8: One Calendar Frontend with Recoverable Errors

**Files:**
- Create: `apps/Nexus-Calendar/frontend/src/calendar-api.ts`
- Create: `apps/Nexus-Calendar/frontend/src/calendar-api.test.ts`
- Create: `apps/Nexus-Calendar/frontend/src/CalendarApp.tsx`
- Create: `apps/Nexus-Calendar/frontend/src/CalendarApp.test.tsx`
- Create: `apps/Nexus-Calendar/frontend/src/MonthGrid.tsx`
- Create: `apps/Nexus-Calendar/frontend/src/EventEditor.tsx`
- Create: `apps/Nexus-Calendar/frontend/src/EventDetails.tsx`
- Create: `apps/Nexus-Calendar/frontend/src/SharingPanel.tsx`
- Create: `apps/Nexus-Calendar/frontend/src/PublicEventPage.tsx`
- Create: `apps/Nexus-Calendar/frontend/src/PublicEventPage.test.tsx`
- Modify: `apps/Nexus-Calendar/frontend/src/main.tsx`
- Modify: `apps/Nexus-Calendar/frontend/src/index.css`
- Modify: `apps/Nexus-Calendar/frontend/package.json`
- Create: `apps/Nexus-Calendar/frontend/vitest.config.ts`
- Create: `apps/Nexus-Calendar/frontend/vitest.setup.ts`
- Delete: `apps/Nexus-Dashboard/frontend/src/pages/calendar/CalendarView.tsx`
- Modify: `apps/Nexus-Dashboard/frontend/src/App.tsx`
- Modify: `apps/Nexus-Dashboard/frontend/src/App.test.tsx`

**Interfaces:**
- Produces typed API methods for CRUD, share grants, public-link create/revoke, and public reads.
- Produces stable `CalendarApiError { reason, status }`.
- Produces authenticated `CalendarApp` and isolated `PublicEventPage` from the same entrypoint.

- [ ] **Step 1: Write API-client failure tests**

Cover thrown network errors, non-JSON failures, validation envelopes, `401`, `404`, and `503`. Assert no failure maps to `{ events: [] }`.

- [ ] **Step 2: Implement the typed client**

Check `Response.ok`, parse error envelopes defensively, use runtime API bases, and expose no raw upstream details.

```ts
export class CalendarApiError extends Error {
  constructor(readonly reason: string, readonly status: number) { super(reason); }
}
```

- [ ] **Step 3: Write authenticated UI tests**

Test loading, real empty state, offline state, retry, `+` selecting today, selected-day creation, input preservation after failed save, delete failure, multi-day rendering, owner/editor/viewer controls, grant management, link copy/create/revoke, and responsive accessible labels.

- [ ] **Step 4: Implement the minimal authenticated UI**

Implement `MonthGrid`, `EventEditor`, `EventDetails`, and `SharingPanel` as separate focused files. Keep orchestration state in `CalendarApp`; do not introduce a new state library.

```tsx
<CalendarApp runtime={runtime} api={calendarApi(runtime.apiBase)} />
```

- [ ] **Step 5: Write and implement public-page tests**

Assert only filtered fields render, unknown/revoked tokens show a neutral not-found state, no authenticated navigation/data loads occur, and the page sets `Referrer-Policy: no-referrer` through the front door.

```tsx
const page = runtime.publicToken
  ? <PublicEventPage token={runtime.publicToken} />
  : <CalendarApp runtime={runtime} />;
```

- [ ] **Step 6: Remove the Dashboard duplicate**

Delete `CalendarView`, its import, and its explicit React route. Add a regression assertion that Dashboard has no Calendar feature component or iframe; `/calendar` is server-dispatched to the proxied application.

- [ ] **Step 7: Run and commit**

Run: `cd apps/Nexus-Calendar/frontend && bun run test && bun run build && cd ../../Nexus-Dashboard/frontend && bun run test && bun run build`

Commit: `feat(calendar): unify personal and shared calendar UI`

---

### Task 9: Production Front Door, Documentation, and Acceptance

**Files:**
- Modify: `deploy/production/nexus-calendar.Caddyfile`
- Modify: `deploy/production/deploy.sh`
- Modify: `deploy/production/tests/processes.test.sh`
- Create: `deploy/production/tests/calendar-routing.test.ts`
- Modify: `apps/Nexus-Calendar/check.sh`
- Modify: `apps/Nexus-Calendar/README.md`
- Modify: `docs/NEXUS-ECOSYSTEM.md`
- Modify: `ARCHITECTURE.md`
- Modify: `docs/LOCAL-DEV.md`
- Modify: `VERSION_MATRIX.md`

**Interfaces:**
- Production serves the same hashed build at `calendar.tnhc.dev/` and `app.tnhc.dev/calendar`.
- Only the direct-origin `/share/:token` page and public GET API bypass user authentication.

- [ ] **Step 1: Write failing routing/process tests**

Assert Calendar backend and web processes receive DB, legacy-owner, JWT audience, and Dashboard-hop settings; both base paths serve the same asset hashes; SPA fallback does not swallow APIs; public pages carry `no-referrer`; private APIs redirect or reject without a session.

- [ ] **Step 2: Implement Caddy and deploy routing**

Add explicit API, public API, assets, shell-path, SPA fallback, cache, CSP, referrer, and nosniff handlers. Ensure prefix stripping preserves Calendar client routes and does not turn arbitrary Dashboard paths into Calendar paths.

```caddyfile
handle /api/v1/calendar/public/* { reverse_proxy 127.0.0.1:3068 }
handle /api/v1/calendar/* { reverse_proxy 127.0.0.1:3068 }
```

- [ ] **Step 3: Strengthen the app gate and documentation**

Make `check.sh` run backend typecheck/tests plus frontend tests/build. Document local two-origin startup, migration configuration, sharing permissions, public-link risk/revocation, and the delivery contract. Change ecosystem status from scaffold to active only after gates pass.

- [ ] **Step 4: Run complete verification**

Run:

```bash
cd apps/Nexus-Calendar && ./check.sh
cd ../Nexus-Dashboard && bun run check && bun test tests/
cd frontend && bun run test && bun run build
cd ../../Nexus-Cloud && bun run check && bun test src/test/registry.service.test.ts
cd ../../deploy/production && bun test tests/gate.test.ts tests/calendar-routing.test.ts
bash tests/processes.test.sh
git diff --check
```

Expected: every command exits 0.

- [ ] **Step 5: Run the authenticated browser acceptance smoke**

With local Auth, Dashboard, Calendar, and both web front doors running:

1. Sign in as owner; create a private event from `/calendar` using `+` with no day selected.
2. Sign in as a second user; prove the event is absent.
3. Grant viewer access; prove it appears but has no edit/delete/share controls.
4. Upgrade to editor; prove content can change but ownership/sharing/deletion cannot.
5. Create a public link; open it in a clean browser context without Nexus cookies.
6. Revoke it; prove the same URL returns the neutral not-found page.
7. Open `calendar.tnhc.dev`; prove it uses the same asset hash and data.

- [ ] **Step 6: Commit acceptance artifacts**

Commit: `docs(calendar): document shared calendar operations`

---

## Final Review Checklist

- [ ] Every spec requirement maps to a task above.
- [ ] No Calendar UI implementation remains in Dashboard.
- [ ] No Calendar iframe exists.
- [ ] Registry persistence survives restart and legacy records normalize safely.
- [ ] Identity spoofing, cross-user access, and public-token enumeration tests pass.
- [ ] Both routes serve one artifact and visible errors never masquerade as empty data.
- [ ] All required app, frontend, deployment, and documentation gates pass.
