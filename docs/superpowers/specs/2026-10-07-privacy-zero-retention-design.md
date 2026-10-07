# Privacy Architecture, Part 1: Zero Retention — Design

**Date:** 2026-10-07
**Status:** Approved in conversation, awaiting written-spec review
**Programme:** Privacy Architecture (1 Zero retention → 2 Privacy status page + Charter wording → 3 End-to-end encryption at rest → 4 Third parties out of the path → 5 Compulsion resistance)

## Goal

Make this sentence true, structurally and provably, for everything TNHC runs:

> We never store your IP address, and we keep no logs about you.

"Structurally" means apps never receive a visitor's IP in the first place. "Provably" means a daily end-to-end test shows that a marked address is stored nowhere.

## What exists today (measured 2026-10-07)

| Where | Stored |
|---|---|
| Nexus-Auth | `auth_audit_log` rows with `ip` and `user_agent` for every auth event; sessions carry `ipAddress` / `userAgent` |
| Chat (`apps/Nexus`) | `ip_address` columns in the sessions table (initial schema) and `instance_audit_log`; IP reads in ~27 route/middleware files |
| Nexus-Hosting | Per-hit records on every hosted site (path, referrer, unsalted 16-hex `ipHash`), `site_analytics` roll-ups with unique-IP counts and top referrers, live SSE hit feed (`hostRouter.ts:83 recordHit`) |
| Caddy front doors (chat, draw, calendar) | Access logs with the client address (headers already deleted) |
| Nexus-Email | Standard `Received:` headers; submission path may write the submitting user's IP into outgoing mail |
| Supabase, live (`supabase-*`, reached at `auth.tnhc.dev` catch-all → `172.17.0.1:8000`) | GoTrue `auth.audit_log_entries.ip_address`; gateway access logs |
| Supabase, local dev copy (`*_tnhc-community-local`) | Runs `analytics` + `vector` log collection |
| tnhc.dev waitlist (Cloudflare Worker + D1) | Email addresses |
| Native service logs (`/tmp/nexus-production`, tmpfs) | Unbounded until reboot |
| Docker containers (cloudflared, Supabase gateway, Hosting, MinIO) | `json-file` logs with no rotation — kept on disk forever |

Routes that bypass the ecosystem proxy today: `auth.tnhc.dev` non-login paths (→ Supabase gateway) and `storage.tnhc.dev` (→ MinIO :9010).

## Decisions

| Topic | Decision |
|---|---|
| Operational logs | Technical events only — never an IP, email address, username, account ID, message content, or full URL with query string. Kept at most 24 hours. |
| Sign-in security history | Events only (what + when + random device ID), no IP and no user agent, kept 30 days, visible to the user who can sign a device out. Upgraded in Part 3 to encryption with the user's own key. |
| Abuse protection | Forgetful tags: `HMAC(secret, client IP)` where the secret lives only in proxy memory and is replaced every 24 hours; short-window counters in memory; plus per-account slow-down. Cloudflare's edge still absorbs floods. |
| Hosted-site statistics | Page + day + view count only. No IP, hash, referrer, unique-visitor count or live feed. |
| Email | Mail submitted by Nexus users never carries the user's IP in its headers. `Received:` headers added by outside servers are untouched. Mail-server logs carry no addresses or IPs. |
| Supabase | Same rules as Nexus: IPs wiped and blanked on every new auth-log entry; gateway access logs off; dev copy's log collection stopped. |
| Waitlist | An email address is deleted when that person is invited. |
| Approach | Choke point at the front door + cleanup of stored data + automatic guard. |

## Design

### 1. The front door

The ecosystem proxy (`deploy/production/proxy.ts`, :8080) is the only path from the tunnel to any service.

For every request it:

1. Reads the client address from `CF-Connecting-IP` (absent → fixed tag `unknown`).
2. Computes `tag = base64url(HMAC-SHA256(secret, address))[0..22]`. `secret` is 32 random bytes generated in memory at start-up and replaced every 24 hours; it is never written to disk or logged.
3. Deletes every address- or location-bearing header: `CF-Connecting-IP`, `X-Forwarded-For`, `X-Real-IP`, `True-Client-IP`, `CF-IPCountry`, `CF-Ray`, `Forwarded`, `X-Client-IP`, `CF-Visitor`, `CF-EW-Via`, `CDN-Loop`.
4. Sets `X-Nexus-Client-Tag: <tag>` and forwards.

If header stripping throws, the request is refused (HTTP 500), never forwarded with the original headers. The proxy's own log prints no request line.

Bypass routes are closed: the tunnel ingress for `auth.tnhc.dev` (catch-all to the Supabase gateway) and `storage.tnhc.dev` (MinIO) is pointed at the proxy, and the proxy gains fixed upstreams for both, with the same stripping. Presigned MinIO URLs are signed over the request's `Host`, which the proxy normally rewrites to the upstream: the storage route therefore forwards the original `Host` (`storage.tnhc.dev`) unchanged, together with path and query, and a test performs a real presigned upload and download through the proxy.

This ships first, so no new addresses arrive while stored ones are cleaned.

### 2. Cleaning stored data

Each change: wipe existing values → switch code to the tag → drop the column/field.

- **Auth:** `auth_audit_log` keeps `event`, `user_id`, `actor_id`, `device_id`, `detail`, `created_at`; `ip` and `user_agent` are nulled then dropped; rows older than 30 days are deleted by a scheduled purge. Sessions lose `ipAddress` / `userAgent` and gain a random `deviceId` minted at sign-in. Wrong-password limits use the tag (short window, in memory) plus per-account back-off. The Dashboard Account page gains "Your recent activity" (the user's own events) with "Sign this device out".
- **Chat:** `ip_address` columns in sessions and `instance_audit_log` nulled then dropped (Postgres and SQLite-lite migrations); rate limiting and any audit writes use `X-Nexus-Client-Tag`.
- **Hosting:** per-hit table and live SSE feed removed; new `site_page_views(site_id, path, day, views)` incremented per request; existing per-hit and roll-up data deleted; owner charts read the new table.
- **Email:** the submission path omits the client address from the `Received:` header it writes; daemon and API logs drop addresses and IPs.
- **Supabase (live):** existing `auth.audit_log_entries.ip_address` set to `''`; a `BEFORE INSERT` trigger blanks it on every new row; gateway access logging disabled.
- **Supabase (dev copy):** `analytics` and `vector` containers stopped and disabled.
- **Waitlist:** the invite path deletes the address from D1.

### 3. Logs and the 24-hour rule

- **Native services:** a systemd **user** timer (no root) rotates each `/tmp/nexus-production/*.log` every 12 hours with copy-truncate, keeping one previous file — nothing older than 24 hours. Installed by `deploy.sh`.
- **Caddy:** per-request access logs removed from the three Caddyfiles; error logs kept.
- **Docker:** per-container `logging:` in the compose files — `driver: none` for cloudflared and the Supabase gateway; `driver: local` with `max-size: 5m`, `max-file: 1` for the rest. Recreating each container drops its old permanent log file. Each recreate is a short restart; the operator chooses the moment.
- **Backups and dumps:** found, listed for the operator, then cleaned the same way or deleted.

Containers with a size-capped log are described publicly as "technical logs only, continuously overwritten", not as "24 hours".

### 4. Proof and guard

- **Static guard** `scripts/check-privacy.sh` (CI job `edge`, beside the licence check): fails if any file outside the proxy's stripping module reads the headers listed in §1, or if a migration adds an `ip`/`ip_address`/`ip_hash`/`user_agent` column.
- **Live canary**, daily: Cloudflare overwrites `CF-Connecting-IP` on public traffic, so the canary sends its requests to the proxy on loopback (`127.0.0.1:8080`) with `Host` set to each public hostname (auth, app, cloud, chat, hosting, storage, a hosted site) and `CF-Connecting-IP` set to a unique address from the documentation ranges (192.0.2.0/24, 198.51.100.0/24, 203.0.113.0/24) — exercising everything TNHC runs from the proxy inward — then searches every database, native log, container log and stored mail for that exact value. Any hit fails, naming the location. The latest result is published in Part 2.
- **Tests:** proxy (headers stripped; storage route keeps the original `Host` and a presigned upload succeeds; tag stable within a day, changes after rotation; refuse on strip failure); Auth (no address stored; per-account back-off; 30-day purge; device sign-out); Hosting (counts only); Email (no client address in submitted mail headers); each migration (values gone, columns gone).

## Out of scope

- Public privacy status page and Charter rewording — Part 2.
- Encryption with the user's own keys, including the encrypted sign-in history and moving Chat's Phantom secret keys off the server — Part 3.
- Cloudflare and Resend seeing traffic and mail; onion access; Phantom transport — Part 4.
- Legal review of Swedish retention duties; transparency report / warrant canary — Part 5.

## Known limits, stated publicly in Part 2

- Cloudflare sees client addresses and decrypted traffic before it reaches TNHC.
- The proxy holds addresses in memory for the life of a connection.
- Size-capped container logs are not strictly time-limited.
