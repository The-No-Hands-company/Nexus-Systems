# Nexus Deploy — Foundation (sub-project 1)

**Status:** design, awaiting review · **Date:** 2026-11-06
**Scope:** make the existing `apps/Nexus-Deploy` genuinely usable by invited ecosystem members on the reference node (this one), and portable to any node by configuration.
**Deploy sources in this slice:** a Git repository (clone and build) and a Docker image (pull and run).
**Not in scope:** the Railway-style feature leap (multi-service projects, managed databases, templates), the CLI and the GitHub App integration — later sub-projects, mapped in Appendix A.

---

## 1. Goal and success criterion

An invited member signs in at `https://deploy.tnhc.dev`, connects a repository, pushes, and gets a live HTTPS app at `https://<app>.tnhc.dev`. `https://deploy.tnhc.dev/<app>` is the short link that opens it.

**This node is the reference implementation.** It must demonstrate delivery and function end to end. Once it does, every other node can do the same from its own configuration. The proof is the conformance check in §8: `verify` passing on this node is the acceptance gate for this sub-project, and the same command is the gate for any other node.

## 2. Decisions

| # | Decision | Why |
|---|---|---|
| 1 | **Invited ecosystem members** may deploy | Needs per-user isolation and caps, not full hostile-code defences. Public/anonymous deploys are a later tier. |
| 2 | **Foundation first, on this node** | Nothing else works until apps are reachable, isolated and attributable. |
| 3 | **Apps live on `tnhc.dev`** (not a separate apps domain) | Chosen by the operator. Risk and mitigation in §6; moving to a separate domain later is a config change. |
| 4 | **Each app on its own origin: `<app>.tnhc.dev`** | One level deep, so Cloudflare's free certificate covers it, and apps work at `/`. |
| 5 | **`deploy.tnhc.dev/<app>` is a redirect, not a proxy path** | Serving apps under a path makes every app, and the Deploy dashboard/API, one browser origin: any app's script could use a visitor's session to read their env vars or control their deployments, and could read other apps' storage. Redirecting keeps the memorable URL and preserves isolation. |
| 6 | **Rootless sandbox daemon under a dedicated unprivileged user** | A container escape lands somewhere with nothing valuable, instead of in root or in the operator's account. |
| 7 | **No billing, plans or paywalls; safeguards allowed** | Caps protect the node and apply equally. `AGENTS.md` is reworded accordingly (§9). |
| 8 | **Docker-image deploys are in this slice** | Matches Railway's "deploy from Docker image"; the sandbox pulls instead of building, so it is small, and it lets the conformance canary run without a builder. CLI, templates and the GitHub App stay later (Appendix A). |

## 3. Current state (verified 2026-11-06)

- `apps/Nexus-Deploy` is a standalone git repository (submodule), v0.5.0, about 2,900 lines: Express 5 + TypeScript server, React/Vite dashboard, encrypted JSON store, Docker CLI wrapper, Nixpacks/Dockerfile builds, GitHub webhooks (HMAC), WebSocket/SSE logs, rollback, status sync. Sign-in already goes through Nexus-Auth.
- `deploy/production/deploy.sh` already starts it on port 8096 when `dist/index.js` exists. **It is not running**: nothing listens on 8096, `deploy.tnhc.dev` answers 404 (the proxy falls through to Hosting), and `web/dist` is empty — the dashboard has never been built.
- Its compose file assumes Traefik with Let's Encrypt on ports 80/443. This node is behind a token-managed Cloudflare tunnel and the ecosystem proxy (port 8080); that model does not apply here.
- The node has Docker 29.8 (rootful), `git`, and the rootless prerequisites (`newuidmap`, `rootlesskit`, `dockerd-rootless.sh`, `slirp4netns`, a subuid range, cgroup2 delegation of cpu/io/memory/pids). It has **no** Nixpacks or equivalent builder.
- The proxy (`deploy/production/proxy.ts`) already isolates member-owned Hosting sites: it deletes `Cookie` and `Authorization` from requests to them and drops any `Set-Cookie` that names a `Domain`. Unknown `*.tnhc.dev` hosts fall through to Hosting's site-proxy.
- Hosting already serves member sites at `<site>.tnhc.dev`, so that namespace is shared (§5.6).

## 4. Architecture

Seven units, each with one purpose and a defined interface.

1. **Control plane** — the existing Deploy server. Owns projects, deployments, sign-in, API and dashboard. Changes: per-user ownership, member list, config-driven URL patterns, name registry, safeguards, route-table endpoint.
2. **Builder** — for Git sources: clone and build with a time limit and resource caps, inside the sandbox (never on the host). Dockerfile builds remain; a Nixpacks-class builder is added so repositories without a Dockerfile build. For image sources there is no build: the sandbox pulls the image (§5.8).
3. **Sandbox runtime** — a separate rootless Docker daemon owned by a dedicated user. Containers run with all capabilities dropped, `no-new-privileges`, CPU/memory/pids caps and no host mounts. Deploy talks only to this daemon; it never falls back to the host daemon.
4. **Router** — an extension of the ecosystem proxy: a new `app` route kind that sends `<slug>.tnhc.dev` to the container, applying the existing cookie isolation.
5. **Short link** — `deploy.tnhc.dev/<slug>` redirects to the app's origin.
6. **Name registry** — the single authority on who owns `<slug>`.
7. **Conformance check** — `doctor` and `verify` (§8).

### Request flow

push → GitHub webhook (HMAC) → control plane creates a deployment → builder builds in the sandbox → container starts → **live only when its health check passes** (existing behaviour) → route table updated → proxy serves `<slug>.tnhc.dev`.

## 5. Interfaces and data

### 5.1 Members
Deploy keeps its own member list: `{ authUserId, role: "owner" | "member", status: "active" | "suspended", grantedBy, grantedAt }`. Authentication stays with Nexus-Auth; there is no separate signup. An operator (Auth role founder/admin) grants access in a Deploy admin page. The first founder is seeded as `owner` on first boot. A request from an authenticated account with no active member record gets 403 with a clear "ask an operator for access" page.

### 5.2 Ownership
Every project gets `ownerId`. Members see and act on only their own projects; owners/operators see all. On first boot after upgrade, existing projects are assigned to the seeded owner. All project, deployment, log, env and stats routes enforce ownership.

### 5.3 Storage
The existing AES-GCM encrypted JSON store is kept (writes via `writeDb`), extended with `members`, `ownerId`, and the name registry. It is adequate at invited-member scale; moving to SQLite is deferred to sub-project 2.

### 5.4 Route table (control plane → router)
`GET /internal/routes` on the control plane, loopback-only, authenticated with a shared secret header. Returns `{ version, routes: [{ host, upstream, projectId, slug }] }` for **live** deployments only. `upstream` is `http://127.0.0.1:<hostPort>`; the sandbox publishes each app on a loopback port allocated from a configured range. The proxy polls every 15 s (with `ETag`), and keeps serving the last known table for up to 5 minutes if Deploy is unreachable; a dead upstream returns 502.

### 5.5 Router precedence
`ecosystem fixed names` → `Deploy apps (route table)` → `Hosting sites (existing fallthrough)`. Deploy apps therefore can never shadow an ecosystem host, and unknown hosts still reach Hosting.

### 5.6 Name registry
A slug must match `^[a-z][a-z0-9-]{1,38}[a-z0-9]$`. It is rejected if reserved (every ecosystem host name, plus dashboard paths such as `api`, `assets`, `health`, `internal`), already owned by another Deploy project, or already used by a Hosting site. The Hosting check calls Hosting's internal API and **fails closed**: if Hosting cannot be reached, creation is refused. A slug is released only when its project is deleted.

### 5.7 Short link
On host `deploy.tnhc.dev`, a single-segment path that is not a dashboard route and matches a live slug returns a `302` to `https://<slug>.tnhc.dev/`. Otherwise the request goes to the control plane (which renders a not-found page).

### 5.8 Deploy sources
A project has exactly one source: `{ type: "git", repo, branch }` or `{ type: "image", image }`. Everything downstream — ownership, slug, caps, route, health gate, rollback — is identical for both.

- **Git:** as today (webhook-triggered or manual deploy); the built image is tagged `nexus/{project}:{deploymentId8}`.
- **Image:** the control plane validates the reference (`[registry/]repository[:tag]` or `@sha256:` digest; no whitespace or shell metacharacters; never executed through a shell). The sandbox pulls it with a 10-minute timeout, and the **resolved digest** is recorded on the deployment, so rollback restores the exact bytes even if a mutable tag moves. There is no webhook; a redeploy re-pulls the tag. Only public registries are supported (private-registry credentials are out of scope).
- Pulled and built images carry the label `nexus.managed=true` so the existing hourly prune covers both. The prune is extended from `nexus/*` names to that label.
- An image runs with the same hardening and caps as a built app. It may run as root *inside* the container; the rootless daemon makes that an unprivileged user on the host. The project's listening port is configured as today.
- In `verify` mode only, an image that exists locally in the sandbox daemon is accepted without a registry pull.

## 6. Security model

- **Session isolation.** For `app` routes the proxy reuses the Hosting-site isolation: no `Cookie` or `Authorization` forwarded; any `Set-Cookie` with a `Domain` attribute dropped. The conformance canary proves it.
- **Residual risk (accepted).** Because apps share the `tnhc.dev` parent domain, an app's browser-side JavaScript can set a `.tnhc.dev` cookie. Hosting member sites have the same exposure today. Invite-only access bounds who can do it. The real fix is a separate apps domain, which the URL-pattern config allows later. **To verify in implementation:** that the Auth session cookie carries `HttpOnly`, so page script cannot read it.
- **Sandbox boundary.** Builds and runtime happen only in the rootless daemon. Containers must not reach host loopback services (Auth 4310, Cloud 8787, Postgres, Redis); this is a `verify` test, not an assumption. Containers *can* reach the operator's LAN — a documented limitation of this slice.
- **Secrets.** Env vars stay encrypted at rest and are never returned to another member. Webhook HMAC is unchanged. The `/internal/routes` secret is generated by `deploy.sh`.
- **Fail closed.** Sandbox daemon down → deploys fail with a clear status and the host daemon is never used. Hosting unreachable → name creation refused.

## 7. Safeguards (defaults, operator-adjustable, equal for everyone)

| Scope | Default |
|---|---|
| Container | 512 MB memory, no swap; 0.5 CPU; 256 processes |
| Member | 5 projects; 1 concurrent build; 6 builds/hour; 10-minute build timeout |
| Operator | suspend a project or member; audit log of deploys, suspensions and grants |

Limits are shown in the dashboard. They are not tiers.

## 8. Conformance check — the proof of delivery

`npm run doctor` (static, fast): sandbox daemon reachable and rootless; hardening flags applied; builder present; `deploy.<domain>` and the app wildcard resolve and route to the node; route-table endpoint responds; web UI built.

`npm run verify` (end to end): deploys a bundled canary app from a local fixture, then asserts —
1. build succeeds and the app goes live (health passes);
2. HTTPS request to `https://<canary>.<domain>/` returns 200;
3. a request carrying a `Cookie` header reaches the app with **no** cookie and no `Authorization`;
4. the app cannot connect to host loopback services (Auth, Cloud);
5. the memory cap is enforced;
6. `deploy.<domain>/<canary>` redirects to the app;
7. a second deploy followed by rollback restores the first;
8. an **image-source** canary (a locally loaded image, no registry or internet needed) goes live, is routed, and passes checks 2–4 as well;
9. cleanup removes the containers, routes and slugs.

Output is a human summary plus JSON; exit code non-zero on any failure. A passing run on this node is the acceptance criterion for this sub-project.

## 9. Bring-up on this node

1. **Sandbox setup script** (`deploy/production/`), run by the operator with `sudo` (the assistant cannot): creates the `nexus-deploy` user, subuid/subgid range, lingering, and a systemd user unit for the rootless daemon. Idempotent; checked by `doctor`.
2. **Builder** installed under the sandbox user, not machine-wide.
3. **`deploy.sh`**: points Deploy at the sandbox socket; builds the dashboard if absent; seeds the first owner; generates the route-table secret.
4. **Proxy**: fixed route for `deploy.tnhc.dev` → Deploy; `app` route kind; short-link redirect; reserved names. The Cloudflare `*.tnhc.dev` wildcard already routes to the proxy (Hosting depends on it); confirm during implementation.
5. **Cloud registration** so Deploy appears in the launcher, and the dashboard moved onto the shared tokens per `docs/THEME-GUIDE.md` (it currently has its own terminal-dark stylesheet).
6. **`apps/Nexus-Deploy/AGENTS.md`**: replace "Never add billing, limits, or feature gates" with "Never add billing, plans or paywalls; safeguards protect the node and apply equally."

## 10. Portability to other nodes

All node-specific values live in one config block, with no `tnhc.dev` literal in code (a test scans for it):

| Key | Meaning | This node |
|---|---|---|
| `BASE_DOMAIN` | node apex | `tnhc.dev` |
| `APP_HOST_PATTERN` | app origin | `{slug}.{BASE_DOMAIN}` |
| `CONTROL_HOST` | dashboard/short-link host | `deploy.tnhc.dev` |
| `SANDBOX_DOCKER_HOST` | rootless daemon socket | per sandbox user |
| `APP_PORT_RANGE` | loopback ports for apps | `20000-29999` |
| `CAP_*` | safeguards in §7 | defaults |

The router contract (§5.4) is ingress-agnostic: any proxy that can read the route table can serve apps. This slice ships the ecosystem-proxy implementation only.

## 11. Testing

- Unit: member authorization and ownership (a member cannot read or modify another's project, env or logs); name registry (reserved, duplicate, Hosting collision, fail-closed); safeguards (concurrency, hourly limit, timeout); config scan for hardcoded domains; image-reference validation (accepts tags and digests; rejects whitespace, shell metacharacters, empty and over-long references); source model (exactly one source per project); rollback restores the recorded digest.
- Proxy (`deploy/production/tests`): `app` route kind, precedence, cookie/authorization stripping, `Domain` cookie drop, short-link redirect, reserved paths, stale-table behaviour.
- End to end: `verify` (§8), run on this node.
- Existing tests (`build-context`, `health-gate`, `context-resolution`, `repo-and-dockerfile`) keep passing.

## 12. Verification tasks (resolve while planning, not assumptions)

1. Session cookie is `HttpOnly` (Nexus-Auth).
2. Rootless daemon blocks container access to host loopback (`--disable-host-loopback` default) on this node.
3. Rootless Docker behaves correctly under SELinux enforcing on Fedora.
4. Cloudflare wildcard `*.tnhc.dev` and `deploy.tnhc.dev` reach the proxy.
5. Hosting exposes (or can expose) an internal endpoint to check a name.
6. Which Nixpacks-class builder is available and maintained at implementation time.
7. Why Deploy is not currently running although `dist/index.js` exists.

## 13. Out of scope (later sub-projects)

Multi-service projects, managed databases and volumes, templates, private networking between services, env-variable references, custom domains, preview deploys, cron, teams, the CLI, the GitHub App integration (repo picker and automatic webhook install), private-registry credentials, multi-node scheduling and federation, SQLite migration, gVisor, a separate apps domain, and a single-hostname path mode for nodes without wildcard DNS.

---

## Appendix A — Railway deploy methods mapped to this project

| Railway method | Plan |
|---|---|
| Deploy from a GitHub repo (auto-detect stack, live URL) | **This slice** (Git source). Automatic webhook install and the repo picker need a GitHub App; each node operator registers their own, so it is its own sub-project. |
| Deploy from the CLI (`railway up`) | Later. Needs personal access tokens issued by Nexus-Auth (not available yet), then one API-client binary per the ecosystem's CLI doctrine. |
| Deploy from a Docker image | **This slice** (Image source). |
| Deploy from a template (Postgres, Redis, n8n, full stacks) | Later. Needs multi-service projects and managed databases (sub-project 2) before templates are meaningful. |

## Appendix B — Peer platforms and where Deploy sits

| Class | Examples | In the Nexus ecosystem |
|---|---|---|
| Static sites and frontends | Netlify, Vercel, Cloudflare Pages | **Nexus Hosting** already covers this. Deploy does not duplicate it. A later integration could publish a repo that builds to plain static files to Hosting instead of running a container. |
| Container apps with managed databases | Railway, Render, Back4app | **Nexus Deploy.** This slice is the foundation; databases arrive in sub-project 2. |
| Self-hosted all-in-one | Dokploy (Docker, Nixpacks, databases, real-time monitoring) | Closest open-source peer. Study its database and monitoring model when designing sub-project 2. |
