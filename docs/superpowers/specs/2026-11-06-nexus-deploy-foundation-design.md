# Nexus Deploy — Foundation (sub-project 1)

**Status:** design v2, awaiting review · **Date:** 2026-11-06
**v2 note:** revised after reviewing the TNHC Charter, the project's governing notes and the existing code. v1 had treated Nexus AI as the app Deploy must run and had designed a private route table; both were wrong (§1, §6).

---

## 1. Purpose

> "TNHC offers, for free, the kind of software and services others charge for — self-hosted, federated and open to everyone." — TNHC Charter

**Nexus Deploy is TNHC's free, open-source alternative to paid deployment platforms** such as Railway, Render, Heroku and Fly: anyone with an account can deploy their own application from a Git repository or a container image and get a live HTTPS URL. It is a general service. Nexus apps are users of it like any other; none is special, and nothing in the product is shaped around one.

This slice makes the existing `apps/Nexus-Deploy` genuinely usable by invited ecosystem members on the reference node (this machine), and portable to any other node by configuration. Later sub-projects close the rest of the gap to Railway-class platforms (Appendix A).

**Success criterion.** `verify` (§12) passes on this node, and the same bring-up and `verify` pass on a disposable second node (§11). That is the proof of delivery and of portability.

## 2. Binding constraints

These come from the Charter and the project's governing notes. They are requirements, not preferences.

| Constraint | Source | What it means for Deploy |
|---|---|---|
| **Free; no billing, plans or paywall.** The only recurring cost is the domain (about 12.20 USD/yr). | Charter "Money"; self-sufficiency principle; hosting constraints | Every dependency is audited (§3). The software contains no billing. Operators of their own nodes may charge outside it; the Charter allows that. |
| **Tell the owner whenever a cost or cap exists or could arise.** Every design lists its third-party touchpoints. | Self-sufficiency principle | §3 lists them, including free-tier caps. |
| **Open source, AGPL-3.0 for applications.** | Charter "Ownership and licences" | `LICENSE` is AGPL-3.0, but `README.md` says "MIT". The README is false and is fixed (§10). |
| **No IP addresses stored, no logs about people; services keep only what they need.** A daily canary searches logs, containers, databases and sign-in files for a marked address. | Charter "Your data"; privacy programme | §8. |
| **Best-effort availability, no uptime guarantee; federation answers availability.** | Charter "Availability" | The dashboard says so plainly. Nothing hard-assumes one node. |
| **Behave like the major clouds** (release lifecycle, health, rollback, routing). Only differences: free, and the Zajfan standard. | Cloud industry-parity direction | Blue/green releases (§7); routes through Cloud's registry (§6). |
| **Builds share a disk with production.** A build once filled the disk and killed `cloud.tnhc.dev`. | Build-volume note | Hard storage cap and disk guard (§9). |
| **Free public multi-tenancy on one machine makes abuse control load-bearing.** | Hosting constraints | Safeguards and isolation (§9). |
| **Reuse what exists; Cloud's `/api/v1/routes` is the single routing authority.** | Hosting constraints; deploy topology | No private route table. |
| **No dependence on another service unless unavoidable.** GitHub is a convenience, not a requirement. | Self-sufficiency principle | Any Git URL works; any OCI registry works. |
| **No second always-on machine exists.** Second-node work must be provable on this machine. | Parity direction | The portability proof uses a disposable node (§11). |
| **Built to the Zajfan standard**; AI co-authorship recorded in commits. | Charter "How we build" | New code is scored with `zajfan-standard/rubrics/universal_rubric.md` and `typescript_audit_sheet.md`. |

## 3. Decisions and cost audit

### Decisions

| # | Decision | Why |
|---|---|---|
| 1 | **Invited ecosystem members** may deploy | Needs per-user isolation and caps, not full hostile-code defences. Public deploys are a later tier. |
| 2 | **Foundation first, on this node** | Nothing else works until apps are reachable, isolated and attributable. |
| 3 | **Apps live on `tnhc.dev`** | Chosen by the operator; consistent with the 2026-10-13 audit decision (O-1) that user-deployed sites share the apex and are protected at the proxy. A separate apps domain stays a config change (§11). |
| 4 | **Each app on its own origin: `<app>.tnhc.dev`** | One level deep, covered by Cloudflare's free certificate, and apps work at `/`. |
| 5 | **`deploy.tnhc.dev/<app>` is a redirect, not a proxy path** | Serving apps under a path would make every app and the Deploy dashboard/API one browser origin, so any app's script could use a visitor's session to read env vars or control deployments. |
| 6 | **Rootless sandbox daemon under a dedicated unprivileged user, on a size-capped filesystem** | A container escape lands somewhere with nothing valuable; a runaway build cannot fill the production disk. |
| 7 | **No billing, plans or paywalls; safeguards allowed** | Caps protect the node and apply equally. |
| 8 | **Sources: Git repository and Docker image** | Matches Railway's two core methods. The CLI, templates and GitHub App are later (Appendix A). |
| 9 | **General-purpose product** | See §1. |
| 10 | **Routes live in Cloud's registry** | Cloud is the single routing authority; no parallel table. |
| 11 | **Releases are blue/green** | Cloud-parity; made cheap by per-app port allocation. |

### Third-party touchpoints and cost

| Dependency | Cost | Cap or risk |
|---|---|---|
| Cloudflare DNS, Tunnel, free Universal SSL | free | Universal SSL covers one wildcard level only, hence `<app>.tnhc.dev`. Advanced Certificate Manager (paid) is deliberately not used. Cloudflare is already a tolerated, named dependency in the Charter, with a stated aim of removing it. |
| Cloudflare free-plan terms for the traffic apps carry | free | Large media or file transfer through the tunnel may be restricted. **To verify (§14).** |
| Docker Engine (rootless), `git`, Nixpacks-class builder | free, open source | |
| GitHub clones and webhooks | free | Optional convenience; any Git URL works. |
| Docker Hub / GHCR pulls | free | **Docker Hub rate-limits anonymous pulls per IP.** If members pull often, an open-source pull-through cache can be added. Not needed in this slice. |
| Compute, storage, bandwidth | the node operator's own | No billing anywhere. |
| Custom domains (later) | free tier, then a quota | Cloudflare for SaaS custom hostnames: see the existing custom-domains design before building. Out of scope here. |

## 4. Current state (verified 2026-11-06)

- `apps/Nexus-Deploy` is a standalone repository (submodule), v0.5.0, about 2,900 lines: Express 5 + TypeScript, React/Vite dashboard, AES-GCM encrypted JSON store, Docker CLI wrapper, Nixpacks/Dockerfile builds, GitHub webhooks (HMAC), WebSocket/SSE logs, rollback, status sync. Sign-in goes through Nexus-Auth.
- **Release lifecycle:** "live" is recorded only after a health probe passes, and a rejected release restores the previous image. It is **not zero-downtime**: the old container is replaced in place (the code says blue/green needs a second port).
- **Not hardened:** run flags are limited to optional memory/CPU limits and a network. No capability drop, `no-new-privileges`, pids limit or log limits; container logs use Docker's default driver, which keeps them forever and unbounded.
- **Health probes** reach the container by its Docker network IP, which a rootless daemon does not expose to the host.
- **Cloning** is a bare `git clone <url>`: private repositories cannot be deployed.
- **Privacy:** the code never reads a visitor address (no `req.ip` or forwarding headers).
- **Not running:** nothing listens on 8096, `deploy.tnhc.dev` answers 404 (the proxy falls through to Hosting), and `web/dist` is empty — the dashboard has never been built. `deploy.sh` would start it if `dist/index.js` exists.
- Its compose file assumes Traefik with Let's Encrypt on ports 80/443; this node is behind a token-managed Cloudflare tunnel and the ecosystem proxy (port 8080), and the `*.tnhc.dev` wildcard already routes to that proxy.
- Node tooling: Docker 29.8 (rootful), `git`, and the rootless prerequisites (`newuidmap`, `rootlesskit`, `dockerd-rootless.sh`, `slirp4netns`, a subuid range, cgroup2 delegation of cpu/io/memory/pids). **No** Nixpacks-class builder.
- The proxy already isolates user-deployed Hosting sites: it deletes `Cookie` and `Authorization` from requests to them, drops any `Set-Cookie` that names a `Domain`, and strips visitor-address headers, replacing them with a rotating opaque client tag. Unknown `*.tnhc.dev` hosts fall through to Hosting's site-proxy, which shares the `<name>.tnhc.dev` namespace.

## 5. Architecture

Seven units, each with one purpose.

1. **Control plane** — the existing Deploy server: projects, deployments, sign-in, API, dashboard. Changes: per-user ownership, member list, config-driven URL patterns, safeguards, blue/green release, publication of live apps to Cloud.
2. **Builder** — Git sources: clone and build with a time limit and resource caps inside the sandbox, never on the host. Dockerfile builds stay; a Nixpacks-class builder is added for repositories without one. Image sources are pulled, not built (§7).
3. **Sandbox runtime** — a separate rootless Docker daemon owned by a dedicated user, with its data root on a size-capped filesystem. Containers run with all capabilities dropped, `no-new-privileges`, CPU/memory/pids caps, capped logs and no host mounts. Deploy talks only to this daemon and never falls back to the host daemon.
4. **Router** — the ecosystem proxy, extended so that routes Cloud marks as user-deployed get the same isolation as Hosting sites (§6).
5. **Short link** — `deploy.tnhc.dev/<slug>` redirects to the app's origin.
6. **Name registry** — Cloud's registry is the authority on who owns `<slug>`; Deploy also checks Hosting (§6.3).
7. **Conformance check** — `doctor` and `verify` (§12).

**Request flow.** push → webhook (HMAC) → deployment created → builder builds in the sandbox → new container starts on a free loopback port → health probe passes → Cloud route switches to the new port → old container drains and stops. A failed build or failed probe leaves the previous release serving.

## 6. Routing, names and the short link

### 6.1 Cloud is the routing authority
Deploy publishes each live app to Nexus Cloud's registry, as Draw and Calendar already register: id `deploy-<slug>`, `publicUrl https://<slug>.tnhc.dev`, `upstreamUrl http://127.0.0.1:<hostPort>`, health from Deploy's own probe, heartbeating while alive. The proxy resolves hosts from Cloud's routes exactly as it does for every other app. If Deploy stops, heartbeats stop and Cloud marks the route offline.

**User-app isolation flag.** Routes created for deployed apps carry a flag that makes the proxy apply user-site isolation (no `Cookie`/`Authorization` forwarded, `Domain` cookies dropped, address headers and client tag stripped). The flag (`userDeployed`) is set by Cloud only through its `/api/v1/user-apps/:slug` surface, which is guarded by its own credential (`NEXUS_CLOUD_USER_APPS_KEY`), not by the node API key that every ecosystem service holds. The flag is carried over from the previous record and never read from a payload, exactly like `requiresAuth`, and the generic registration refuses the `deploy-` id prefix, so an app can neither choose nor clear it. Cloud accepts only `http://127.0.0.1:<port>` upstreams inside `NEXUS_CLOUD_USER_APPS_PORTS` (default `20000-29999`), so a leaked credential cannot aim a public hostname at Auth, Cloud or a database.

### 6.2 Precedence
`ecosystem fixed names` → `Cloud routes (including user-deployed apps)` → `Hosting sites (existing fallthrough)`. Deploy apps therefore never shadow an ecosystem host, and unknown hosts still reach Hosting.

### 6.3 Name registry
A slug matches `^[a-z][a-z0-9-]{1,38}[a-z0-9]$`. It is refused if reserved (every ecosystem host, plus dashboard paths such as `api`, `assets`, `health`, `internal`), already registered in Cloud, or already used by a Hosting site. The Hosting check asks Hosting's site-proxy by `Host` header (Hosting has no availability endpoint): its own "No site found" 404 means the name is free, anything else means a site is there, and an unreachable Hosting or a 5xx **fails closed**. Known limitation: the site-proxy returns the same 404 when its own database errors, so a Hosting database outage can make a taken name look free; because Cloud routes take precedence over Hosting (§6.2), the failure mode is a member's app shadowing a Hosting site, never exposing its content. An explicit Hosting endpoint is the follow-up that removes it. A slug is released when its project is deleted.

### 6.4 Short link
On host `deploy.tnhc.dev`, a single-segment path that is not a dashboard route and matches a live slug returns `302` to `https://<slug>.tnhc.dev/`. Otherwise the request goes to the control plane, which renders a not-found page.

## 7. Projects, sources and releases

### 7.1 Members and ownership
Authentication stays with Nexus-Auth; there is no separate signup. Deploy keeps a member list `{ authUserId, role: "owner" | "member", status: "active" | "suspended", grantedBy, grantedAt }`; an operator (Auth founder/admin) grants access in a Deploy admin page, and the first founder is seeded as `owner`. An authenticated account with no active record gets a 403 page saying to ask an operator. Every project has `ownerId`; members see and act on only their own, operators see all, and existing projects are assigned to the seeded owner on first boot. All project, deployment, log, env and stats routes enforce ownership.

### 7.2 Storage
The encrypted JSON store is kept (writes via `writeDb`), extended with members, ownership and the project definitions. It is adequate at invited-member scale; SQLite is deferred to sub-project 2.

### 7.3 Deploy sources
A project has exactly one source: `{ type: "git", repo, branch }` or `{ type: "image", image }`. Ownership, slug, caps, route, health gate and rollback are identical for both.

- **Git:** any `http(s)` Git URL (or `owner/repo` shorthand for GitHub). Webhook-triggered or manual. The built image is tagged `nexus/{project}:{deploymentId8}`. **Private repositories** use an optional per-project read-only token: encrypted at rest, supplied to `git` through an askpass helper in the process environment, never in the clone URL, on a command line, in logs or in any API response; logs redact it.
- **Image:** any OCI registry reference (`[registry/]repository[:tag]` or `@sha256:` digest), validated (no whitespace or shell metacharacters; never run through a shell). The sandbox pulls with a 10-minute timeout and the **resolved digest** is recorded on the deployment, so rollback restores the exact bytes even if a tag moves. A redeploy re-pulls the tag. Public registries only; private-registry credentials are out of scope.
- The hourly prune removes images by reference, not by label. The sandbox daemon is dedicated to Deploy, so every image in it is Deploy's; the prune keeps the images that a running, rollback or recent release still references.
- Images may run as root *inside* the container; the rootless daemon makes that an unprivileged user on the host.

### 7.4 Blue/green release
1. Start the new container on a free loopback port from the configured range.
2. Probe it on that port (`127.0.0.1:<hostPort>`; the Docker network IP is not reachable from the host under a rootless daemon).
3. If healthy, switch the Cloud route to the new port, let the old container drain for a short, configurable period, then stop it.
4. If unhealthy, remove the new container, record the failure, and leave the previous release serving — no outage.

**Projects with a persistent volume** release stop-then-start by default (a short outage), because two containers writing one volume at once (SQLite, for example) can corrupt data. The dashboard says which mode a project uses. Rollback uses the same path with the recorded image or digest.

## 8. Security and privacy

### 8.1 Isolation
- **Sessions.** For user-deployed routes the proxy removes `Cookie` and `Authorization` from requests and drops `Set-Cookie` with a `Domain`. The 2026-10-15 fix for the same-site finding (O-1) already blocks `Sec-Fetch-Site: same-site` mutations to ecosystem app routes and deliberately does not apply to user-deployed sites; deployed apps keep that status.
- **Residual risk (accepted, as for Hosting sites today).** An app's browser-side JavaScript can plant a `.tnhc.dev` cookie. Invite-only access bounds who can do it; the real fix is a separate apps domain, a config change (§11). The Auth session cookie is `HttpOnly` (verified in `apps/Nexus-Auth/src/server.ts`), so an app's JavaScript cannot read the session.
- **Sandbox boundary.** Builds and runtime happen only in the rootless daemon. Containers must not reach host loopback services (Auth, Cloud, Postgres, Redis); `verify` tests this. Containers can reach the operator's LAN, a documented limitation of this slice.
- **Secrets.** Env vars stay encrypted at rest and are never returned to another member. Webhook HMAC is unchanged.
- **Fail closed.** Sandbox daemon down → deploys fail with a clear status and the host daemon is never used. Hosting unreachable → name creation refused.

### 8.2 Privacy (Charter "Your data")
- **Visitor addresses never reach Deploy or deployed apps.** The proxy strips address headers; for user-deployed routes it also strips the opaque client tag, so apps receive nothing about the visitor. `verify` sends a marked address and asserts the app never sees it.
- **Logs are capped.** Containers use the `local` log driver with size caps (10 MB × 2 files, compression off), set on the sandbox daemon so it covers every container, in place of Docker's default unbounded logs. No access logging anywhere in Deploy. Build logs are kept for the project's recent deployments only, are deletable by the owner, and are removed with the project.
- **The audit log** records the account id, action and time — no address, no user agent — and is retained 30 days, matching sign-in history.
- **Guards.** `scripts/check-privacy.sh` (which scans every submodule) must pass for Nexus-Deploy. Whether and how the daily canary covers user-deployed containers is decided in planning, and any public privacy sentence about them is checked against the code before it is written (seven of twelve first drafts of such sentences were overclaims).
- **Your data is yours.** An owner can export a project's definition (source, settings, env) as JSON, and deleting a project removes its container, volume, images, logs, route and slug.

## 9. Safeguards and disk

| Scope | Default (operator-adjustable, equal for everyone) |
|---|---|
| Container | 512 MB memory, no swap; 0.5 CPU; 256 processes; capped logs |
| Member | 5 projects; 1 concurrent build; 6 builds per hour; 10-minute build timeout; 10-minute image pull |
| Operator | suspend a project or member; audit log |

**Disk.** The sandbox daemon's data root is a **fixed-size filesystem image** (default 40 GB), so the worst case of a runaway build is that Deploy's own builds fail, not that production loses its disk. Builds are also refused when the host volumes are below 15% free, matching the existing webmaster check (warn 15%, fail 8%), and the hourly prune removes unused images. The dashboard shows limits; they are not tiers. Do not relocate build directories to `/`, which shares a device with the OS.

## 10. Bring-up on this node

1. **Sandbox setup script**, run by the operator with `sudo` (the assistant cannot): creates the `nexus-deploy` user, subuid/subgid range, lingering, the size-capped storage image and mount, and a systemd user unit for the rootless daemon with log caps. Idempotent; checked by `doctor`.
2. **Builder** installed under the sandbox user, not machine-wide.
3. **`deploy.sh`**: points Deploy at the sandbox socket; builds the dashboard if absent; seeds the first owner; passes Cloud credentials. Deploy is started the way every other service is.
4. **Proxy and Cloud**: fixed route for `deploy.tnhc.dev`; the user-app flag and short-link redirect; reserved names. The `*.tnhc.dev` wildcard already reaches the proxy.
5. **Cloud registration** so Deploy appears in the launcher; the dashboard moves onto the shared tokens per `docs/THEME-GUIDE.md` (it has its own terminal-dark stylesheet).
6. **Documentation fixes:** `README.md` states MIT while `LICENSE` is AGPL-3.0 — correct the README. In `AGENTS.md`, replace "Never add billing, limits, or feature gates" with "Never add billing, plans or paywalls; safeguards protect the node and apply equally."

## 11. Portability and the second-node proof

All node-specific values live in one config block, with no `tnhc.dev` literal in code (a test scans for it):

| Key | Meaning | This node |
|---|---|---|
| `BASE_DOMAIN` | node apex | `tnhc.dev` |
| `APP_HOST_PATTERN` | app origin | `{slug}.{BASE_DOMAIN}` |
| `CONTROL_HOST` | dashboard and short-link host | `deploy.tnhc.dev` |
| `SANDBOX_DOCKER_HOST` | rootless daemon socket | per sandbox user |
| `SANDBOX_STORAGE_SIZE` | size of the storage image | `40G` |
| `APP_PORT_RANGE` | loopback ports for apps | `20000-29999` |
| `CAP_*` | safeguards in §9 | defaults |

The routing contract is Cloud's registry, which is ingress-agnostic. A node without Cloudflare serves the same routes through any proxy with free Let's Encrypt certificates (and may use two-level names via a DNS-01 wildcard). This slice ships only the ecosystem-proxy implementation.

**Second-node proof without second hardware.** Because no always-on second machine exists, the bring-up and `verify` also run on a **disposable node** — a VM or container on this machine with a different `BASE_DOMAIN` (a local test domain). Passing there shows nothing is hardcoded to this node.

## 12. Conformance check — the proof of delivery

`npm run doctor` (static, fast): sandbox daemon reachable, rootless, with log caps and the storage cap applied; hardening flags applied; builder present; `deploy.<domain>` and the app wildcard resolve and reach the node; Cloud registration works; web UI built.

`npm run verify` (end to end) uses **generic fixture apps bundled in the repository** — small Node, Python and Dockerfile servers that echo the request headers they receive, plus an image-source fixture loaded locally so no registry or internet is needed. For each fixture it asserts:
1. the build succeeds and the app goes live; `https://<slug>.<domain>/` returns 200;
2. a request carrying a `Cookie` and `Authorization` header reaches the app with neither, and with **no visitor-address header and no client tag**;
3. the app cannot reach host loopback services (Auth, Cloud);
4. the memory cap is enforced, and container logs are size-capped;
5. `deploy.<domain>/<slug>` redirects to the app;
6. a redeploy under continuous requests sees **no failed request** (blue/green), a deliberately unhealthy release is rejected with the old one still serving, and rollback restores the earlier release;
7. a project with a volume keeps its data across a redeploy;
8. cleanup removes containers, routes, slugs and volumes.

Output is a human summary plus JSON, with a non-zero exit on any failure. A passing `verify` on this node and on the disposable node is the acceptance criterion.

## 13. Testing

- Unit: member authorization and ownership (a member cannot read or modify another's project, env or logs); name registry (reserved, duplicate, Hosting collision, fail-closed); safeguards (concurrency, hourly limit, timeouts); config scan for hardcoded domains; image-reference validation (tags and digests accepted; whitespace, shell metacharacters, empty and over-long refused); one-source-per-project; rollback restores the recorded digest; private-repo token never in a URL, argument, log or API response; disk guard refuses builds when low.
- Proxy (`deploy/production/tests`): user-app isolation (cookie/authorization stripped, `Domain` cookie dropped, address headers and client tag stripped), precedence, short-link redirect, reserved paths.
- Cloud: the user-app flag cannot be set or cleared by a registrant.
- Existing Deploy tests (`build-context`, `health-gate`, `context-resolution`, `repo-and-dockerfile`) keep passing.
- Code review: new code is scored against the Zajfan universal rubric and the TypeScript audit sheet.

## 14. Verification tasks (resolve while planning, not assumptions)

1. ~~Auth session cookie is `HttpOnly`.~~ Verified (Plan 2).
2. The rootless daemon blocks container access to host loopback on this node, and runs correctly under SELinux enforcing on Fedora.
3. Cloudflare's current free-plan terms for the traffic apps will carry (the cost rule depends on it); and that `deploy.tnhc.dev` plus the wildcard reach the proxy end to end.
4. ~~How Cloud should model the user-app flag, and whether Hosting exposes an internal name-check endpoint.~~ Resolved (Plan 2): a dedicated credential and surface for the flag; Hosting is asked through its site-proxy.
5. Which Nixpacks-class builder is available and maintained at implementation time.
6. Why Deploy is not running although `dist/index.js` exists.
7. Fixed-size storage image: filesystem choice and mount approach that works on this Fedora node.
8. How the daily privacy canary should treat user-deployed containers, and what the privacy page may truthfully say about them.
9. That the private-repository token never appears in process listings, logs or API output (tested).
10. Blue/green with the sandbox's loopback port publishing: port allocation races and cleanup after a crash.

## 15. Out of scope (later sub-projects)

Multi-service projects, managed databases and volumes, templates, private networking between services, env-variable references, custom domains, preview deploys, cron, teams, the CLI, the GitHub App integration (repo picker and automatic webhook install), private-registry credentials, multi-node scheduling and federation, SQLite migration, gVisor, a separate apps domain, and a single-hostname path mode for nodes without wildcard DNS.

Managed databases deserve an early follow-up: most real backends want one. They are not required by the acceptance checks above, which is why they are not in this slice.

---

## Appendix A — Railway deploy methods mapped to this project

| Railway method | Plan |
|---|---|
| Deploy from a GitHub repo (auto-detect stack, live URL) | **This slice** (Git source). Automatic webhook install and the repo picker need a GitHub App; each node operator registers their own, so it is its own sub-project. |
| Deploy from the CLI (`railway up`) | Later. Needs personal access tokens issued by Nexus-Auth (not available yet), then one API-client binary per the ecosystem's CLI doctrine. |
| Deploy from a Docker image | **This slice** (Image source). |
| Deploy from a template (Postgres, Redis, n8n, full stacks) | Later. Needs multi-service projects and managed databases (sub-project 2) first. |

## Appendix B — Peer platforms and where Deploy sits

| Class | Examples | In the Nexus ecosystem |
|---|---|---|
| Static sites and frontends | Netlify, Vercel, Cloudflare Pages | **Nexus Hosting** already covers this. Deploy does not duplicate it. A later integration could publish a repo that builds to plain static files to Hosting instead of running a container. |
| Container apps with managed databases | Railway, Render, Back4app | **Nexus Deploy.** This slice is the foundation; databases arrive in sub-project 2. |
| Self-hosted all-in-one | Dokploy (Docker, Nixpacks, databases, real-time monitoring) | Closest open-source peer. Study its database and monitoring model when designing sub-project 2. |
