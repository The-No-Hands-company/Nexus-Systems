# Calendar release lifecycle check

Proves a first-party service can be released through Nexus-Deploy, confirmed
healthy by Nexus-Cloud, routed by the production proxy's own code, and rolled
back — and that Deploy's "live" means the app answers, not that a container
started.

```sh
cd projects/Nexus-Systems
bun run deploy/production/tests/calendar-release.lifecycle.ts
```

Needs Docker Engine, Bun, Node.js, Git, and installed dependencies for
Nexus-Deploy and Nexus-Cloud. It runs on the production machine safely (verified
2026-10-13): it starts its own Cloud and Deploy on free loopback ports with
temporary data directories, an in-process stub of Nexus-Auth, and a unique
Calendar identity; it only imports `proxy.ts` (whose server starts only under
`import.meta.main`, so :8080 is never touched); and the isolated Deploy's image
prune only sees `nexus/*` images, which production does not use. Everything it
creates — containers, images, Cloud registration, child processes, the fixture
repo, temporary databases — is removed in `finally`, pass or fail.

## What it proves, in order

1. **Release a** builds Calendar from a fixture repo laid out like the monorepo
   (repo-root context, `-f apps/Nexus-Calendar/Dockerfile`, because Calendar
   imports `packages/nexus-identity`). Deploy marks it live only after
   `/health` answers 2xx; Calendar reports itself to Cloud; Cloud confirms it
   healthy; an exposure request publishes a route; the production proxy's
   `buildRouteMap` + `handleRequest` serve version a through that route.
2. **Release b** replaces it; the same chain serves version b.
3. **A broken build** (invalid Dockerfile) fails and b keeps serving.
4. **A release that builds but exits on boot** is rejected by Deploy's health
   gate: the deployment is `failed`, b is started again, and the project stays
   `live` on b's image. A mutation run with the gate disabled fails at this step
   with "marked live", so this step really tests the gate.
5. **Rollback** to a's image serves a again.

## What it does not prove

- Cloudflare's public tunnel connector or DNS. The proxy is exercised in-process
  with real Cloud routes; nothing goes through tnhc.dev.
- Nexus-Tunnel. It is not in the public request path.
- Zero-downtime releases. Deploy replaces the container in place, so a rejected
  release costs the time to restart the previous image.

## Deploy settings it relies on

- `DEPLOY_ALLOW_LOCAL_REPOS=true` — the fixture is a local `file:` repo. Off by
  default, so production Deploy refuses `file:` repositories.
- Project fields `dockerfile` (relative to the repo root) and `healthPath`.
- `DEPLOY_HEALTH_TIMEOUT_MS` (30 s here; default 60 s).

## Hosting is a separate flow

User sites go through Nexus-Hosting, not Deploy. Its E2E runs against a loopback
disposable Hosting stack and a pre-seeded disposable user token:

```sh
cd projects/Nexus-Systems/apps/Nexus-Hosting
NH_BASE_URL=http://127.0.0.1:<hosting-port> NH_TEST_TOKEN=<disposable-user-token> pnpm exec playwright test e2e/deploy.spec.ts
```
