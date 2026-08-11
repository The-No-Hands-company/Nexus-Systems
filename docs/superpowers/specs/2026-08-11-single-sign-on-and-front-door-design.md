# Single sign-on and the ecosystem front door

**Status:** design approved 2026-08-11, not implemented
**Scope:** one account for the whole ecosystem, a public way to get one, and one
place to reach every app from.

---

## Problem

A visitor to `tnhc.dev` cannot get an account, and there is no single place to
reach anything. Each app is found only by knowing its subdomain, and apps carry
their own logins — nexus-chat ships its own users table, its own JWT and its own
password hashing, entirely separate from Nexus-Auth. The ecosystem cannot be
introduced to the public in this state.

### What already exists

Better than it appears from the outside:

- **Nexus-Auth is a real OIDC provider.** `oauth/authorize`, `/token`, `/jwks`,
  `/userinfo`, plus sessions, token issue/validate, API keys and a hosted login
  page ("Sign in — Nexus Systems").
- **Its session cookie is already scoped to `.tnhc.dev`** (leading dot,
  deliberately) so one login can carry across every subdomain.
- **Cloud's registry already routes** `domain → upstream` for every app and
  tracks health, nodes and peers.

### What is actually missing

1. **No public registration.** Accounts are seeded (founder/operator) or
   admin-created via `POST /api/v1/auth/users`. A stranger has no path in.
2. **No front door.** `Nexus-Account`, `Nexus-Dashboard`, `Nexus-Portal` and
   `Nexus-Home` exist as directories — all four are empty ghost scaffolds.
3. **Apps do not use the SSO.** nexus-chat authenticates independently.
4. **The apex is disconnected.** `tnhc.dev` is a static Cloudflare Pages site
   that never reaches the tunnel; `tnhc.dev/login` returns the marketing SPA
   catch-all, not a login form.

So this project **finishes** an SSO that is mostly built, and gives it a door.

### Governing constraints

- **Self-contained.** No third-party service, no recurring cost beyond the
  ~$12.20/yr domain. If it can be built here, it is built here.
- **No flow may hard-depend on outbound email.** Self-hosting SMTP is easy;
  deliverability from a residential IP is not (Spamhaus PBL, port 25 blocks,
  provider reputation). Internal Nexus→Nexus mail is fine; reaching a stranger's
  gmail.com is not reliable. Email is an enhancement, never a dependency.
- **Do not assume one node.** Federation across opted-in user nodes is the
  answer to availability, not rented cloud capacity.

---

## Decisions

| # | Decision | Chosen |
|---|---|---|
| 1 | Where signup and the dashboard live | Static apex; product at `app.tnhc.dev` |
| 2 | What sits behind login | Gate the **tools**, not published output |
| 3 | How accounts are created | Invite-only/waitlist, flag-flip to open later |
| 4 | Account recovery | Recovery codes now, TOTP soon, operator reset always |
| 5 | How apps learn the user | Proxy gate + short-lived signed identity JWT |

**1. Static apex, product at `app.tnhc.dev`.** Cloudflare Pages cannot reach the
tunnel, so the marketing page cannot host a form that talks to Nexus-Auth
without cross-origin complexity. Keeping the apex static also keeps the public
face of the company online when the node is not — which matters while there is
one node. The apex gains two links, "Sign in" and "Request access", both to
`app.tnhc.dev`.

**2. Gate the tools, not the output.** Every Nexus app requires login. Anything
a user *publishes* through Hosting stays public, as does the marketing site.
Gating published sites would leave Hosting with no purpose. A later per-site
visibility toggle (public / Nexus-accounts-only) is explicitly out of day-one
scope; `sites` already carries `visibility` and `password_hash`, so the schema
is half-ready when it is wanted.

**3. Invite-only for launch.** One machine, no federation yet, so uncontrolled
growth is a liability. The data model and flow are identical to open signup —
open registration is an auto-approve policy check, not a redesign. It also means
launch does not block on email infrastructure.

**4. Recovery codes.** The only option that is both self-contained and
self-service. TOTP later shares the same backup-code mechanism. Operator reset
stays forever as a backstop.

**5. Proxy gate + signed identity JWT.** Keeps one integration pattern across
120 apps without the forgeable-header trap of plain proxy headers, and without
the per-app cost of making every app a full OIDC client.

---

## Architecture

One gate, at the proxy. Every hostname already passes through it, so apps cannot
accidentally ship unauthenticated and app #121 costs nothing.

```
browser → Cloudflare edge → tunnel → proxy:8080
                                       │
                                       ├─ 1. resolve host → upstream   (Cloud route table, exists)
                                       ├─ 2. does this host require auth?  (NEW: policy field on route)
                                       ├─ 3. no  → forward as-is        (published sites, marketing, auth)
                                       └─ 4. yes → validate session cookie
                                                    │
                                              invalid → 302 auth.tnhc.dev/login?redirect_uri=<original>
                                                    │
                                                valid → attach signed identity JWT → forward upstream
                                                                                        │
                                                                            app verifies via JWKS
```

**The auth decision is data, not code.** Whether a host requires login is a field
on the route record in Cloud's registry — the same table that maps
`chat.tnhc.dev → 127.0.0.1:8095`. Tool routes default to requiring auth; the
Hosting wildcard serving published sites defaults to public. Decision 2 is
therefore one column, and the later per-site toggle plugs in there without
touching the proxy.

**Opaque cookie, derived token.** The `.tnhc.dev` session cookie stays the source
of truth so logout and suspension revoke for real. The proxy exchanges it for a
short-lived JWT that apps verify locally. Opaque cookie buys real revocation;
derived JWT buys verification with no callback.

**Session cache: 60s.** `cookie-hash → {identity JWT, expiry}` in proxy memory,
so a session costs at most one auth call per minute rather than one per request.
Accepted cost: revocation takes up to 60s to propagate. Tunable to 0 for instant
revocation at the price of a round trip per request.

**Federation-ready.** Policy and routes live in Cloud's registry, which already
models nodes and peers. A second node consumes the same data.

### Consequences

- **App ports bind to `127.0.0.1`.** The gate only works if the proxy is the only
  way in. nexus-chat currently listens on `0.0.0.0:8180/8181/8182` and is
  reachable ungated from the LAN today.
- **`auth.tnhc.dev` stays public.** It is what an unauthenticated user needs in
  order to authenticate.

---

## Accounts

The hard part of invite-only without email is **delivery**. Solved by handing the
secret over at request time rather than at approval time.

```
stranger fills the request form (email + handle + one line "what for")
        │
        └─► account row created, status = pending
            screen shows a CLAIM CODE once: "save this — you'll need it to finish"
        │
operator approves in the admin panel            status = approved
        │
user returns to app.tnhc.dev/claim whenever they like
        enters email + claim code  ──► sets password
                                   ──► shown 10 recovery codes, must confirm saved
        │
        └─► status = active, session issued, lands on the dashboard
```

Nothing is ever sent anywhere; the user polls instead of being notified. The
claim code proves the returning visitor made the request — without it, anyone who
guessed an approved email address could seize the account.

**Two entry paths, both email-free:**
- **Waitlist** — the public path from the landing page, as above.
- **Invite codes** — minted in the admin panel, handed over in whatever channel
  you already use. Skips `pending` and goes straight to setting a password.

**Status is a state machine:** `pending → approved → active`, plus `suspended`
and `rejected`. Open registration later = auto-approve on request.

**Storage extends Nexus-Auth's existing `users`** rather than adding a parallel
identity store — it already owns sessions, OIDC and API keys, and a second user
table recreates the problem being solved. Added: `status`, `handle`,
`claim_code_hash`, `approved_at`, `approved_by`; new `recovery_codes` (10 per
account, hashed, single-use, regenerable) and `invites`. Claim and recovery codes
are hashed at rest like API tokens, so a database leak yields nothing usable.

**Recovery:** codes shown exactly once at claim time behind a
must-tick confirmation. Single-use; using one logs in and forces a password
reset. The dashboard nags when few remain. TOTP later reuses this table.

**Stated plainly in the UI, not in a footnote:** a user who loses both password
and recovery codes has permanently lost the account, and the operator cannot
rescue them. That is the cost of holding no channel to them.

---

## Dashboard — `app.tnhc.dev`

**Built in `apps/Nexus-Dashboard`**, currently an empty ghost scaffold, served at
`app.tnhc.dev`. Named explicitly because `Nexus-Account`, `Nexus-Portal` and
`Nexus-Home` are equally empty scaffolds with plausible-sounding names; they stay
unused here, and picking one deliberately avoids the work landing in three
half-built places.

Three surfaces, nothing else.

**1. App grid.** Tiles for every running app, linking to its subdomain; the
`.tnhc.dev` cookie means clicking through lands already logged in. Rendered from
**Cloud's registry, never a hardcoded list** — it already knows every tool, its
address, upstream and health. A new app appears by registering; a second node's
apps appear with no code change; a down service shows as down instead of a dead
link.

**2. Account.** Password change, recovery-code view and regeneration, active
sessions with revoke. TOTP enrolment slots in here later.

**3. Admin** (operator only). Pending requests with approve/reject, mint invite
codes, list users, suspend.

**Roles stay at two: `user` and `admin`.** One account grants every app, so there
are no per-app entitlements, groups or permission matrix. Deliberate: easy to add
on real need, very hard to remove once speculatively built.

---

## App integration

The contract is one header:

```
X-Nexus-Identity: <jwt>
  sub    Nexus account UUID        aud  target host (chat.tnhc.dev)
  email  handle  role              exp  ~120s, iss auth.tnhc.dev
```

The app verifies the signature against `auth.tnhc.dev/.well-known/jwks.json`
(cached, key-id aware for rotation) and checks `aud` matches itself. The `aud`
check prevents a token minted for one app being replayed against another.

Integration is ~20 lines: fetch JWKS, cache, verify, read `sub`. Provided as a
small shared helper per language — one TypeScript, one Rust.

**Un-integrated apps stay safe.** An app ignoring the header is still gated by
the proxy; it simply cannot tell users apart. Rollout is incremental, not a flag
day across 120 apps.

### nexus-chat is the first case

Both `nexus_chat.users` and `nexus.users` currently hold **zero rows**, so there
is no migration, no account merge and no legacy credential to preserve.

- Its `users.id` becomes **the Nexus account UUID directly** — no mapping table,
  no extra column, no join. Existing foreign keys (`messages.author_id`,
  memberships, etc.) keep working untouched.
- Rows are **provisioned on first sight**: valid identity arrives, no local row,
  create one from the token claims.
- Its own `/auth/login`, registration and password paths are **removed, not
  bypassed**. A second way in is a second thing to get wrong.

Then Cloud, then Hosting, then anything new.

### Fixed in the same pass

- Bind nexus-chat (and audit the rest) to `127.0.0.1`.
- **Rotate Nexus-Auth's JWT secret.** It is 47 characters, from a dev `.env`
  predating the domain, and is now the key to every app. Fresh 64-byte value at
  cutover.

---

## Failure modes and security

**Nexus-Auth becomes a single point of failure.** Inherent to one credential; the
behaviour when it is down is the choice. **Serve stale:** cache entries carry two
ages — a **60s freshness TTL** used normally, and a **15-minute staleness limit**
that only applies when Auth is unreachable. In healthy operation an entry older
than 60s is revalidated. When Auth cannot be reached, an entry past 60s but under
15 minutes is honoured anyway rather than logging the user out, while new logins
are refused. Mid-session users survive an auth restart; nobody new gets in.
Precedent exists — the proxy already falls back to a stale route cache when Cloud
is unreachable, and logs that it did. Accepted cost: a revocation issued just
before an outage can lag by the staleness window.

**Four hard requirements:**

- **The auth host is allowlisted in code, not policy.** If `auth.tnhc.dev` were
  ever marked "requires auth" in the registry, logging in would require being
  logged in — an unrecoverable deadlock from one bad row.
- **`redirect_uri` is validated against `*.tnhc.dev`.** An unvalidated redirect
  turns the login page into a phishing tool that forwards users to an attacker
  *after* they authenticate. The most commonly botched part of an SSO flow.
- **Claim and recovery codes are the whole secret**, so: ~128 bits of entropy,
  hashed at rest, single-use, per-IP and per-account rate limits, lockout after
  repeated failures.
- **Cookie: `HttpOnly; Secure; SameSite=Lax; Domain=.tnhc.dev`.** Lax not Strict —
  Strict drops the cookie on cross-subdomain navigation, so clicking Chat from
  the dashboard would appear logged-out.

---

## Testing

Weighted to where a bug exposes accounts rather than breaks a feature.

- **Proxy gate decision table** (unit, exhaustive — this is the security
  boundary): public route passes; gated route without cookie redirects; gated
  route with valid cookie forwards *with a token attached*; auth host always
  public regardless of policy.
- **Token verification** (unit): valid accepted; wrong `aud` rejected; expired
  rejected; **tampered signature rejected**; unknown key-id rejected. These prove
  the forged-header hole is closed.
- **Account state machine** (integration, real DB): pending cannot log in;
  approved can claim exactly once; a claim code cannot be reused; a recovery code
  is single-use; suspended is refused everywhere.
- **One end-to-end pass:** request access → approve → claim → dashboard → click
  through to Chat and arrive authenticated.

Deliberately not chasing coverage on dashboard UI; it is tiles over an API that
is tested on its own.

**This is the first security-critical build in the ecosystem** — a mistake
exposes real accounts rather than breaking a feature. The proxy gate and token
verification get a genuine review pass before public launch, not just green
tests.

---

## Build order

Four phases. Each ends somewhere testable, and nothing is publicly reachable
until the last one.

1. **Identity.** Account state machine, claim codes, recovery codes, invites, and
   the admin endpoints to approve/reject/suspend — all inside Nexus-Auth. Ends
   with: an account can be requested, approved and claimed via API, proven by the
   state-machine tests. No UI yet.
2. **The gate.** Route policy field, proxy session validation, cache with the
   two-age behaviour, identity-token minting, `redirect_uri` validation, auth-host
   allowlist. Ends with: a gated host redirects when logged out and forwards a
   verifiable token when logged in, proven by the decision-table tests.
3. **The dashboard.** `Nexus-Dashboard` at `app.tnhc.dev` — request-access form,
   claim flow, app grid from Cloud's registry, account page, admin panel. Ends
   with: the end-to-end pass runs green.
4. **First integration and hardening.** nexus-chat consumes the token, its own
   login is deleted, ports rebind to `127.0.0.1`, the JWT secret is rotated, and
   the security review happens. Ends with: public launch is possible.

Phases 1 and 2 are independent enough to build in either order; 3 depends on
both; 4 depends on all.

## Out of scope

- Per-site visibility toggle for published Hosting sites (decision 2, later).
- TOTP (decision 4, later — designed to reuse the recovery-code table).
- Open self-service registration (decision 3 — a policy flag, not a redesign).
- Per-app entitlements, groups, permission matrices.
- Outbound email of any kind.
- Retrofitting apps beyond nexus-chat; the pattern is established, the long tail
  is ordinary follow-on work.
