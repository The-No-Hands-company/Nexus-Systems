# User Custom Domains — Design Spec (Phase 2)

**Status:** Approved for build (design), 2026-08-10
**Scope:** Let a user attach their own domain (e.g. `alice.com`) to a Nexus-Hosting
site, entirely from the tnhc.dev dashboard, with automatic TLS — on the free
Cloudflare-tunnel architecture, at $0 for the first 100 domains.

---

## 1. Goal & principle

Every project already gets a free, unlimited, $0 `*.tnhc.dev` subdomain — that is
the core "free cloud for everyone" promise and it is untouched. **Custom domains
are an opt-in vanity upgrade.** The everyday-joe experience must be: add the
domain in the dashboard, add ~2 DNS records once at your registrar, done — no
self-hosting, no Cloudflare account, no cert wrangling.

**Cost (verified against Cloudflare docs, 2026-08-10):** Cloudflare for SaaS
includes **100 custom hostnames free** on the Free plan; additional hostnames are
**$0.10/hostname/month** (~$1.20/yr) pay-as-you-go up to 50,000. The included
certificate is free (custom certs are Enterprise-only and not needed). This is the
*only* managed option on a no-public-IP tunnel: any $0-unlimited alternative
requires the user to run their own Cloudflare, which defeats the ease-of-use goal.

Sources: developers.cloudflare.com/cloudflare-for-platforms/cloudflare-for-saas/plans/

## 2. Architecture — the data path once live

```
alice.com
  → Cloudflare edge          (Cloudflare for SaaS: issues+serves the cert, routes)
  → fallback origin ssl.tnhc.dev   (a proxied name already covered by *.tnhc.dev)
  → Cloudflare Tunnel a3fc7587…    (wildcard ingress → 192.168.0.179:8080)
  → ecosystem proxy :8080          (Host not an app route → Hosting default backend)
  → Hosting site-proxy :8090       (lookup_site: custom_domains → site)
  → MinIO object storage           (site files)
```

## 3. What ALREADY exists (confirmed in code — not to be rebuilt)

- **Serving a custom domain is done.** `crates/nexus-proxy/src/db.rs::lookup_site`
  resolves `sites.domain` first, then `custom_domains cd JOIN sites WHERE
  cd.status = 'verified'`. So the instant a `custom_domains` row is `verified`,
  the proxy serves that host. Verified end-to-end for `Host`-header dispatch.
- **The edge path is done.** `*.tnhc.dev` wildcard DNS + tunnel ingress + the
  ecosystem-proxy default-backend to Hosting (shipped this session).
- **Add-domain UX skeleton exists.** `routes/domains.ts`:
  `GET/POST /sites/:id/domains`, `POST /domains/:id/verify`, the `custom_domains`
  table with `verificationToken` / `status` (`pending|verified|failed`).
- **Cloud owns the CF credential + a DNS module.** `apps/Nexus-Cloud`
  (`cloudflare-dns.ts`, token `nexus-tunnel-dns-edit`), reachable from Hosting via
  the existing `nexusCloudClient.ts`.

## 4. What is NEW (the actual build)

### 4a. One-time operator setup (manual, in Cloudflare — ~10 min)
1. Enable **Cloudflare for SaaS** on the `tnhc.dev` zone.
2. Set **fallback origin** = `ssl.tnhc.dev` (already reaches the tunnel via the
   wildcard; no new tunnel route needed).
3. Add **`SSL and Certificates: Edit`** (Zone → tnhc.dev) to the existing
   `nexus-tunnel-dns-edit` account token — required by the Custom Hostnames API.

### 4b. Cloud — Cloudflare-for-SaaS custom-hostname capability
A new module beside `cloudflare-dns.ts` (e.g. `cloudflare-saas.ts`) plus one
endpoint, using the same token/zone config:
- `createCustomHostname(host)` → `POST /zones/{zone}/custom_hostnames`
  (`ssl.method = "txt"`, `type = "dv"`, `settings` default), returns the CF
  hostname id + the **DCV** + **routing** records to show the user. Enable **DCV
  Delegation** so renewals are automatic.
- `getCustomHostname(id)` → status (`pending` → `active`) + `ssl.status`.
- `deleteCustomHostname(id)` → for domain removal.
- Endpoint `POST /api/v1/dns/custom-hostname` (+ GET status, DELETE), `X-Api-Key`
  gated, mirroring the `dns/custom-domain` pattern. Pure helpers unit-tested;
  live-verified against the CF API (create → poll → delete) exactly like the
  cloudflare-dns work.

### 4c. Hosting — wire the domain flow to Cloud
- `POST /sites/:id/domains` also calls Cloud (`nexusCloudClient`) to create the CF
  custom hostname, stores the returned CF hostname id on `custom_domains`, and
  returns the **two records the user adds once**:
  - routing CNAME: `alice.com → ssl.tnhc.dev`
  - DCV-delegation CNAME: `_acme-challenge.alice.com → <…>.dcv.cloudflare.com`
- `POST /domains/:id/verify` becomes a **status poll of the CF hostname** (active +
  cert issued) rather than the local `_fh-verify` TXT check; on active → set
  `custom_domains.status = 'verified'` (which is exactly what the proxy already
  keys on). Keep a background re-poll so it flips without the user re-clicking.
- `DELETE /sites/:id/domains/:domain` → delete the CF hostname via Cloud + remove
  the row.

### 4d. Data model
Add to `custom_domains`: `cf_hostname_id` (text, nullable), `cf_ssl_status`
(text, nullable), `last_checked_at`. Migration via Drizzle (never `db push`).

## 5. Non-goals / cleanup
- **Keep `acme.ts`, gated off (decided).** Its Let's-Encrypt HTTP-01 path assumes
  a public-IP origin on port 80, impossible behind the tunnel. Disable it by
  default (`ACME_ENABLED` off); the code is **kept, not deleted** — it is the TLS
  path for future self-host deployments that do have a public IP. On this node
  Cloudflare issues/renews the cert.
- No apex-specific handling beyond what CF for SaaS + the user's registrar
  (CNAME flattening) already provide; documented, not coded.
- Org-owned domain auto-DNS (`nohands.company`) is **out** — dropped for cost.

## 6. Risks & testing
- **Testing gap (accepted):** full "a stranger's real domain loads" can't be
  rehearsed without a throwaway external domain, which we deliberately do not buy.
  Coverage: (a) unit tests on the pure CF-SaaS request/response mapping; (b) a
  **live API test** against Cloudflare (create a custom hostname for a
  placeholder host, read back its DCV records + status, then delete) — no real
  domain needed to exercise the integration; (c) the serving path is already
  proven (`lookup_site` + Host-header, verified this session). The only unrehearsed
  hop is Cloudflare's own edge serving a real external hostname — which is
  Cloudflare's responsibility, not our code.
- **Token scope:** needs the `SSL:Edit` addition (4a.3) before 4b can pass live
  tests — a you-side action.
- **Cost past 100 (decided): the platform absorbs it.** Custom domains beyond the
  free 100 cost ~$1.20/yr each and are **never passed to users** — users pay
  nothing, ever. This is a deliberate cost the operator carries to keep the
  "free cloud for everyone" promise whole.

## 7. Build order (summary; detailed plan separate)
1. Operator setup (you) + `SSL:Edit` on the token.
2. Cloud `cloudflare-saas.ts` + endpoint, unit + live-API tested.
3. `custom_domains` migration (cf fields).
4. Hosting: add-domain → Cloud create; verify → CF status poll; delete.
5. Retire `acme.ts` behind config.
6. End-to-end dry run at the API level; docs for the 2-record user step.
