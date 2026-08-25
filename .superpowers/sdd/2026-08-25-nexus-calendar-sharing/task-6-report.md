# Task 6 — Hardened Dashboard Calendar Proxy

## Status contract

- **Commit:** pending `feat(dashboard): proxy calendar as a shell app`
- **Scope:** Dashboard Calendar proxy/server/tests and production deploy wiring only.
- **Frontend:** untouched; the existing Dashboard Calendar React route remains a temporary compatibility route for Task 8 to remove.

## Delivered

- Added `src/calendar-proxy.ts` with a fixed Calendar API resource/method allow-list, Auth-first subject resolution, browser identity/secret header stripping, a private Dashboard hop secret, 5-second abort timeouts, safe header forwarding, and stable `{ error: "calendar_unavailable" }` 503 responses.
- Mounted Calendar's own web artifact at `/calendar` before Dashboard's SPA fallback. It rejects traversal (including double-encoded traversal) and API-shaped web paths, marks proxied responses with `X-Nexus-Shell-Context: proxied-app`, makes HTML non-cacheable, and gives hashed assets immutable caching.
- Updated production startup to generate/load one gitignored 32-byte hex secret at `deploy/production/nexus-calendar-dashboard.secret`, validate it, and pass it only to Calendar and Dashboard. Dashboard also receives the loopback Calendar web URL.
- Preserved Dashboard's complete CSP on its no-build 503 page; this fixes the existing server test's security invariant.

## Exact TDD evidence

### RED

1. Initial focused test run:

   ```text
   error: Cannot find module '../src/calendar-proxy'
   0 pass
   1 fail
   ```

2. Added an encoded-traversal case after the initial proxy implementation:

   ```text
   Expected: 404
   Received: 200
   7 pass
   1 fail
   ```

3. The required server test exposed the no-build shell page's incomplete CSP:

   ```text
   Expected to contain: "script-src 'self'"
   Received: "frame-ancestors 'self'"
   ```

4. Production fixture red before directory-safe secret creation:

   ```text
   .../workspace/deploy/production/nexus-calendar-dashboard.secret: No such file or directory
   ```

### GREEN

Fresh required gate, run outside the filesystem sandbox because Bun mock-port binds otherwise return false `EADDRINUSE`:

```text
bun test tests/calendar-proxy.test.ts tests/server.test.ts
21 pass
0 fail
72 expect() calls
```

The same command chain also completed cleanly:

```text
git diff --check
bash -n ../../deploy/production/deploy.sh
cd ../../deploy/production && bash tests/processes.test.sh
PASS: production starts loopback Nexus-Terminal before Dashboard with an explicit safe-default enable switch
```

## Concern

`bun run check` cannot currently resolve the repository's `bun-types` dependency (`TS2688: Cannot find type definition file for 'bun-types'`), so the TypeScript check is environment-blocked rather than a reported source error. The focused Bun test gate compiles and passes.
