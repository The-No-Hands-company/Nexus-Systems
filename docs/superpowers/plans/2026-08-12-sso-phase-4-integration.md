# SSO Phase 4 — Integration and Hardening Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the single sign-on real — nexus-chat identifies users from the ecosystem identity token instead of its own login, its own login is deleted, the LAN bypass is closed, and the gate is switched on.

**Architecture:** nexus-chat gains a JWKS client that verifies the RS256 `X-Nexus-Identity` header Nexus-Auth signs, and provisions a local user row on first sight keyed by the Nexus account UUID. Its own `/auth/login`, `/auth/register` and `/auth/refresh` are removed. Services rebind to loopback so the proxy is the only way in. Only then does `requiresAuth` flip to true.

**Tech Stack:** Rust (axum, `jsonwebtoken` 9.3 — RS256 supported), Bun (Cloud registry data change).

**Spec:** `docs/superpowers/specs/2026-08-11-single-sign-on-and-front-door-design.md`
**Depends on:** Phases 1–3, all merged and live.

## Global Constraints

- **This is the phase where a mistake exposes accounts.** Every task ends green, and the gate is not switched on until the last one.
- **Order is load-bearing.** Ports must be closed *before* the gate is trusted, and identity must work *before* the local login is deleted — otherwise there is a window with no way in at all.
- **`aud` must be checked on every identity token.** Without it a token minted for one app is replayable against another; Phase 2 mints per-host tokens precisely so this check is possible.
- **Do not weaken `nexus-common`'s AnyPool conventions** — UUIDs bound as strings with `$n::uuid` casts, hand-written `FromRow<AnyRow>`, no BYTEA. See [[nexus-chat-never-compiled]].
- Rust: `cargo check --workspace` and `cargo test --workspace --lib` green before every commit.

---

## Verified starting facts

Established by reading the running system, not assumed:

| Fact | Evidence |
|---|---|
| Auth publishes RSA/RS256 JWKS with `kid`, `n`, `e` | `GET /api/v1/auth/oauth/jwks` returns one key, `kid=lGp0cFzl…` |
| `jsonwebtoken = "9.3"` is already a workspace dep | `apps/Nexus/Cargo.toml:47`; supports RS256 |
| Nothing verifies RS256 in nexus-chat yet | no `Algorithm::RS256` anywhere under `crates/` |
| nexus-chat authenticates with its **own** HS256 JWT | `middleware.rs:163` `auth_middleware`, `config.auth.jwt_secret` |
| It has its own login surface | `routes/auth.rs`: `/auth/login`, `/auth/register`, `/auth/refresh` |
| It binds every port to `0.0.0.0` | `crates/nexus-server/src/main.rs:373` — hardcoded, not configurable |
| Its user table is empty | `select count(*) from users` → 0, so no migration |
| `AuthContext` carries user_id, username, is_bot, session_id, two_fa_verified, email_verified, flags | `middleware.rs:78` |

---

### Task 1: Bind services to loopback

The gate only works if the proxy is the only way in. nexus-chat currently listens on `0.0.0.0:8180/8181/8182`, so anyone on the LAN reaches it ungated **today** — this is a live hole, and it is cheap to close.

**Files:**
- Modify: `apps/Nexus/crates/nexus-server/src/main.rs`
- Modify: `deploy/production/nexus-chat.env.example`

**Interfaces:**
- Produces: `NEXUS__SERVER__HOST` honoured for bind address, default `127.0.0.1`

- [ ] **Step 1: Find the hardcoded bind**

`main.rs:373` reads:

```rust
let host: std::net::IpAddr = "0.0.0.0".parse()?;
```

- [ ] **Step 2: Make it configurable, defaulting to loopback**

```rust
    // Loopback by default: the ecosystem proxy is the only intended client, and
    // the login gate lives there. Binding 0.0.0.0 let anyone on the LAN reach
    // the API, gateway and voice ports directly, bypassing authentication
    // entirely. Override with NEXUS__SERVER__HOST=0.0.0.0 only when something
    // off-box genuinely must connect.
    let host: std::net::IpAddr = config.server.host.parse().unwrap_or_else(|_| {
        tracing::warn!(host = %config.server.host, "invalid NEXUS__SERVER__HOST — falling back to 127.0.0.1");
        std::net::IpAddr::from([127, 0, 0, 1])
    });
```

Confirm `ServerConfig.host` exists (`nexus-common/src/config.rs:152` has `pub host: String`) and that its default is `127.0.0.1`; if the default is `0.0.0.0`, change the default too — the env file already sets it explicitly.

- [ ] **Step 3: Verify the ports actually close**

```bash
cd apps/Nexus && cargo build --bin nexus
# restart via deploy.sh, then:
ss -ltn | grep -E ':818[0-2]'
```

Expected: `127.0.0.1:8180` etc., **not** `0.0.0.0:8180`. Then confirm the site still works through the proxy: `curl -s -o /dev/null -w '%{http_code}' https://chat.tnhc.dev/` → 200.

- [ ] **Step 4: Audit the other services**

```bash
ss -ltnp | grep -vE '127\.0\.0\.1|\[::1\]'
```

The ecosystem proxy (8080), Hosting's site-proxy (8090) and MinIO (9010) are **deliberately** wide — cloudflared runs in a bridge container and reaches them over the LAN address (documented in `hosting.compose.yml`). Everything else should be loopback. Record anything that is not, and why.

- [ ] **Step 5: Commit**

```bash
git add apps/Nexus/crates/nexus-server/src/main.rs deploy/production/nexus-chat.env.example
git commit -m "fix(chat): bind to loopback so the proxy is the only way in

The API, gateway and voice ports were hardcoded to 0.0.0.0, so anyone on the
LAN reached them directly and skipped the login gate entirely — the gate only
means anything if the proxy is the sole route in. Host is now configurable and
defaults to 127.0.0.1; an unparseable value warns and falls back to loopback
rather than silently listening everywhere."
```

---

#### Task 1 outcome — bind audit, 2026-08-12

nexus-chat was not alone. Every Bun service bound all interfaces too, because
`Bun.serve` defaults to that when `hostname` is omitted — so 4310 (auth), 8787
(cloud), 3132 (dashboard), 3075 (draw), 3109 (team-chat) and Caddy's 8095 front
door were all reachable from the LAN without passing the gate. All six now bind
`127.0.0.1`, overridable with `NEXUS_BIND_HOST`. Verified from the host's own
LAN address: every one refuses, and only 8080 answers.

Caddy needs the `bind 127.0.0.1` directive, **not** a `127.0.0.1:8095` site
address. In Caddy a site address is also a host matcher, so that form serves
only requests whose Host is literally `127.0.0.1` and answers real traffic
(`Host: chat.tnhc.dev`) with 400. This was caught in verification, not review.

Still wide, deliberately:

| Port | What | Why |
|------|------|-----|
| 8080 | ecosystem proxy | cloudflared is on a bridge network and reaches it by LAN address. This is the intended single way in. |
| 8090 | Hosting site-proxy | published by container; serves hosted-site bytes |
| 8788 | Hosting app | published by container |
| 9010 | MinIO | published by container; browsers/CLIs fetch bytes directly |

**Open finding — blocks gating the *rest* of the ecosystem, not Task 6.**
(Corrected 2026-08-12: Task 6 gates chat only, which this does not affect.)
The tunnel's ingress does not send everything
through the proxy. Current rules:

```
cloud|chat|auth|*.tnhc.dev  ->  192.168.0.179:8080   (proxy — gated)
hosting.tnhc.dev            ->  192.168.0.179:8788   (bypasses the proxy)
storage.tnhc.dev            ->  192.168.0.179:9010   (bypasses the proxy)
```

So `hosting.tnhc.dev` and `storage.tnhc.dev` reach their origins **from the
public internet** without ever traversing the gate. Flipping `requiresAuth:
true` in Task 6 would leave Hosting's control plane — site deploys, tokens —
ungated while everything else was locked. Binding those two to loopback is not
the fix on its own; the ingress rules have to move to 8080 first, and the proxy
needs routes for both hostnames. Storage additionally needs a decision: it
serves public site assets, so gating it as-is would break every hosted site.

Resolve in Task 6, before the flip.

---

### Task 2: JWKS client and identity verification

**Files:**
- Create: `apps/Nexus/crates/nexus-api/src/identity.rs`
- Modify: `apps/Nexus/crates/nexus-api/Cargo.toml` (add `reqwest` if absent)
- Test: inline `#[cfg(test)] mod tests` in `identity.rs`

**Interfaces:**
- Produces:
  - `pub struct IdentityClaims { pub sub: String, pub email: String, pub username: String, pub role: String, pub typ: String, pub aud: String, pub exp: usize }`
  - `pub struct JwksCache { … }` with `pub async fn verify(&self, token: &str, audience: &str) -> Result<IdentityClaims, IdentityError>`
  - `pub enum IdentityError { Malformed, UnknownKey, BadSignature, WrongAudience, Expired, WrongType, JwksUnavailable }`

- [ ] **Step 1: Write the failing tests**

Tests must not reach the network. Generate a keypair in-test, build a JWKS from it, sign tokens locally, and assert each rejection path:

```rust
#[cfg(test)]
mod tests {
    use super::*;

    // A fixed test keypair; the cache is seeded directly so no HTTP happens.
    fn seeded_cache() -> (JwksCache, EncodingKey) { /* build from a PEM fixture */ }

    #[tokio::test]
    async fn accepts_a_valid_token_for_this_audience() { /* … */ }

    #[tokio::test]
    async fn rejects_a_token_minted_for_another_audience() {
        // The replay case: a token for draw.tnhc.dev must not satisfy chat.
    }

    #[tokio::test]
    async fn rejects_an_expired_token() { /* … */ }

    #[tokio::test]
    async fn rejects_a_tampered_signature() {
        // Flip one byte of the payload; the signature must no longer verify.
    }

    #[tokio::test]
    async fn rejects_an_unknown_kid() { /* … */ }

    #[tokio::test]
    async fn rejects_a_non_identity_typ() {
        // Auth also mints service tokens with the same key; typ separates them,
        // and without this check a service token would authenticate as a user.
    }
}
```

- [ ] **Step 2: Run → FAIL.** `cd apps/Nexus && cargo test -p nexus-api identity`

- [ ] **Step 3: Implement**

`JwksCache` holds `HashMap<kid, DecodingKey>` behind an `RwLock`, refreshed from
`{NEXUS_AUTH_INTERNAL_URL}/api/v1/auth/oauth/jwks` with a TTL (default 1 hour)
and an on-demand refresh when an unknown `kid` appears — that is what makes key
rotation survivable without a restart. Build keys with
`DecodingKey::from_rsa_components(n, e)`. Validation sets
`Validation::new(Algorithm::RS256)`, `validate_exp = true`, `leeway = 0`, and
`set_audience(&[audience])`.

Reject `typ != "identity"` explicitly. Auth signs service tokens with the same
key, and only `typ` distinguishes them.

- [ ] **Step 4: Run → PASS**, `cargo check --workspace` clean.

- [ ] **Step 5: Commit** — `feat(chat): verify ecosystem identity tokens against Auth's JWKS`.

---

### Task 3: Identity middleware and first-sight provisioning

**Files:**
- Modify: `apps/Nexus/crates/nexus-api/src/middleware.rs`
- Modify: `apps/Nexus/crates/nexus-db/src/repository/users.rs` (provisioning helper)
- Test: repository test + middleware test

**Interfaces:**
- Consumes: `JwksCache::verify` (Task 2)
- Produces:
  - `pub async fn identity_middleware(request, next) -> Result<Response, NexusError>` — populates the existing `AuthContext`
  - `pub async fn provision_from_identity(pool, claims) -> Result<Uuid, sqlx::Error>` — insert-if-absent, keyed by the Nexus account UUID

- [ ] **Step 1: Write the failing tests**

- provisioning inserts a row on first sight and is idempotent on the second call
- the inserted `users.id` **equals** the token's `sub`, so every existing foreign key (`messages.author_id`, memberships) keeps working
- a request with no `X-Nexus-Identity` is rejected
- a request with a valid identity populates `AuthContext.user_id` with `sub`
- a client-supplied `Authorization: Bearer <old local jwt>` no longer authenticates

- [ ] **Step 2: Run → FAIL.**

- [ ] **Step 3: Implement.** `identity_middleware` reads the header, verifies it
against the cache with `audience = config.server.name`, provisions, and builds
`AuthContext { user_id: sub, username, is_bot: false, session_id: None,
two_fa_verified: true, email_verified: true, flags: … }`.

`session_id: None` is correct and worth stating: session revocation now lives
at the proxy and Auth, and identity tokens live ~120s. Keeping the local
session-table check would mean maintaining a second revocation system that
nothing writes to.

- [ ] **Step 4: Run → PASS**, workspace check clean.

- [ ] **Step 5: Commit** — `feat(chat): authenticate from the ecosystem identity header`.

---

### Task 4: Delete nexus-chat's own login

Only after Task 3 proves identity works end to end — deleting first would leave no way in.

**Files:**
- Modify: `apps/Nexus/crates/nexus-api/src/routes/auth.rs`
- Modify: `apps/Nexus/crates/nexus-api/src/lib.rs` (router wiring)

- [ ] **Step 1** Remove `/auth/login`, `/auth/register`, `/auth/refresh` and any
password-hashing path they were the only caller of. Leave `/auth/logout` only
if it does something meaningful without a local session; otherwise remove it
too and let the dashboard handle sign-out.

- [ ] **Step 2** Swap `auth_middleware` for `identity_middleware` on every
protected router. `combined_auth_middleware` keeps its `Bot <token>` branch —
bots are service credentials, not ecosystem users, and are out of scope here.

- [ ] **Step 3** `cargo test --workspace --lib` green; delete tests that only
covered the removed routes, and say so in the commit rather than leaving them
skipped.

- [ ] **Step 4: Commit** — `feat(chat): delete the local login in favour of ecosystem SSO`.

Two credential systems is the problem this whole project exists to remove; a
disabled-but-present login is a second way in and a second thing to get wrong.

---

#### Task 4 outcome — WebSocket transport gap, 2026-08-12

Task 4's own work is verified: hitting the gateway (8181) and voice (8182)
directly returns 401 without an identity header and 101 with one, so both
WebSocket servers now authenticate from the proxy's header at the HTTP upgrade.

But a WebSocket cannot currently reach them through the public hostname, and
this **predates this task** — it is not caused by the SSO cutover:

| Hop | Behaviour | Cause |
|-----|-----------|-------|
| `proxy.ts` :8080 | cannot upgrade at all | forwards with `fetch()`, which cannot perform a WebSocket handshake, and strips `upgrade` from the response (commit `fd973538`, the tnhc.dev move) |
| Caddy :8095 `/gateway` | 400, with **or without** an identity header | the upgrade never reaches :8181; auth is not involved |

So realtime on chat.tnhc.dev was already broken before today; gating it did not
break it. Fixing it means teaching the ecosystem proxy to proxy WebSockets
(`Bun.serve` upgrade + a socket pump, not `fetch`) and repairing Caddy's
`/gateway` handle. That is its own piece of work with its own verification, and
it is a *transport* problem, not an authentication one.

---

### Task 5: Rotate the JWT secret

`apps/Nexus/.env` carries a 47-character `NEXUS__AUTH__JWT_SECRET` that predates
the domain and has been in a dev file throughout. After Task 4 it no longer
signs user sessions, but it still signs anything else that uses it, and it
should not survive into production use.

- [ ] **Step 1** Generate: `openssl rand -hex 32`.
- [ ] **Step 2** Replace the value in `apps/Nexus/.env` (gitignored — never commit it).
- [ ] **Step 3** Restart nexus-chat; confirm `JWT secret validated length=64` in the log.
- [ ] **Step 4** Confirm `https://chat.tnhc.dev/` still 200s and a signed-in user still resolves.

No commit — this is operator state, not source. Record that it was rotated.

---

### Task 6: Security review, then switch the gate on

**Nothing before this point changes who can reach what.** This task does.

- [ ] **Step 1: Review**, not a test run. Read, with the question "what would an attacker do":
  - `deploy/production/gate.ts` — auth-host allowlist, `redirect_uri` validation, the stale-cache window
  - `deploy/production/proxy.ts` — the `x-nexus-identity` strip, and that it happens on *every* path
  - `apps/Nexus-Auth/src/server.ts` — the identity-token endpoint's suspension check
  - `apps/Nexus/crates/nexus-api/src/identity.rs` — `aud`, `typ`, `exp`, unknown-`kid`
  - Confirm no service still listens off-loopback except the three documented exceptions

- [ ] **Step 2: Verify the whole path manually** before gating:
  request access → approve in the admin panel → claim → sign in → open Chat → confirm the app
  sees the right user.

- [ ] **Step 3: Gate chat only, first.** One route, so a mistake is one app:

```bash
K=$(sed -n 's/^NEXUS_CLOUD_API_KEY=//p' apps/Nexus-Cloud/.env | head -1 | tr -d '\r"')
curl -s -X PATCH -H "X-API-Key: $K" -H 'content-type: application/json' \
  -d '{"requiresAuth":true}' http://127.0.0.1:8787/api/v1/tools/nexus-chat
```

If Cloud has no field for it yet, add `requiresAuth` to the tool record and to
`listActiveRoutes`'s route output — the proxy already reads it (Phase 2, Task 4).

- [ ] **Step 4: Verify the gate bites.** Signed out, `https://chat.tnhc.dev/`
must 302 to `auth.tnhc.dev/login?redirect_uri=…`. Signed in, it must serve.
**Then check every other host still 200s** — `app`, `auth`, `draw`, `cloud`,
`hosting` must be unaffected.

- [ ] **Step 5: Roll back instantly if wrong.** `requiresAuth:false` on the same
route restores the previous behaviour; the proxy picks it up within its 60s
poll. Know this before switching, not after.

- [ ] **Step 6: Commit** whatever code the review changed, and record in the
logbook that the gate is live.

---

#### Task 6 outcome — the gate is live, 2026-08-12

`chat.tnhc.dev` is gated. Signed out it 302s to `auth.tnhc.dev` carrying the
address you asked for; signed in it serves. Every other host is untouched:
`app`, `draw`, `cloud`, `hosting` 200, `auth` 404 at `/` as it always has.

**Rollback**, known before it was needed rather than after:

```bash
K=$(sed -n 's/^NEXUS_CLOUD_API_KEY=//p' apps/Nexus-Cloud/.env | head -1 | tr -d '\r"')
curl -s -X PATCH -H "X-API-Key: $K" -H 'content-type: application/json' \
  -d '{"requiresAuth":false}' http://127.0.0.1:8787/api/v1/tools/nexus-chat
```

The proxy picks it up within its 60s poll. No restart, no deploy.

**The whole path, walked with a real new account** (created, then removed):
request access → refused a login while pending → operator saw it and approved →
a weak password was refused at claim → claimed → signed in → opened Chat, which
recognised them and provisioned their row automatically. That is the entire
thesis of this project working for someone who had no Chat account at all.

**Three defects the review found**, all in `gate.ts`, all fixed with tests:

1. **A malformed cookie 500'd every gated route.** `decodeURIComponent` throws
   on an escape like `%zz`, `readCookie` did not catch it, and `gate()` runs
   outside the proxy's try — so one header crashed the path every gated request
   takes. Confirmed live before the fix, confirmed 302 after.
2. **The stale-cache window outlived the tokens it cached.** `STALE_MS` was 15
   minutes against a 120-second token, so all but the first two minutes handed
   out tokens the app is guaranteed to reject. Now 100s, under the TTL.
3. **The identity cache was never pruned.** Not attacker-growable — entries are
   only written for sessions Auth accepted — but every (session, host) pair a
   real user visited stayed for the life of the process. Swept on write.

**What held up:** a forged `X-Nexus-Identity` is refused twice over — the gate
strips any inbound value before forwarding, and the app rejects the signature
independently. Verified both ways. The `AUTH_HOST` allowlist is structural, so
no route row can lock everyone out of signing in. Every Nexus port refuses from
this host's own LAN address; only the four documented exceptions listen wide.

**Known limitation, worth knowing before gating anything else:** only hosts with
a Cloud *route* can be gated. The wildcard and static-app fallbacks in
`proxy.ts` hardcode `requiresAuth = false`, so `draw.tnhc.dev` — served through
the Hosting fallback — has nowhere to carry the flag. Gating it means giving it
a real route first.

---

## Definition of done

- nexus-chat listens only on loopback; `ss -ltn` shows no `0.0.0.0:818x`.
- A signed-in ecosystem user is identified by nexus-chat with no local login.
- `/auth/login`, `/auth/register`, `/auth/refresh` are gone from the codebase.
- `cargo test --workspace --lib` green; Auth, dashboard and proxy suites green.
- `chat.tnhc.dev` redirects when signed out and serves when signed in.
- Every other host still 200s.

## Deliberately not in this plan

- Gating any app other than chat. Once chat is proven, the rest is one field per route.
- TOTP enrolment.
- The ecosystem bug-reporting system — it wants this identity and the dashboard's admin surface, so it is the natural next project with its own spec.
- Migrating existing chat accounts. There are none; that window closes the moment there are.
