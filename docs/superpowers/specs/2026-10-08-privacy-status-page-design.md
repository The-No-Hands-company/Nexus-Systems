# Privacy Architecture, Part 2: Public Privacy Status + Honest Charter — Design

**Date:** 2026-10-08
**Status:** Approved in conversation, awaiting written-spec review
**Programme:** Privacy Architecture (1 Zero retention — done → **2 Privacy status page + Charter wording** → 3 End-to-end encryption at rest → 4 Third parties out of the path → 5 Compulsion resistance)

## Goal

Every privacy sentence TNHC publishes is true today, each promise shows its real status, and the daily proof (the privacy canary) is visible to anyone.

## Why

Part 1 made "we never store your IP address, and we keep no logs about you" true and provable. The Charter (handbook `charter.md`, rendered at tnhc.dev/charter) still contains sentences the code does not keep:

- "We collect no data of any kind" — services keep account details, sign-in events (30 days), waitlist emails until invited, and the content users store.
- "You can export your data at any time, in open formats" — only Chat has an export.
- "You can move to your own node at any time" — self-hosting exists; moving an account does not.
- "When you delete your account, your data is deleted" — admin-only deletion; Chat's self-delete is a soft delete.
- "Nothing locks you in. You can take your data and leave" — export is not built.
- "We never ask for donations" — tnhc.dev has a "Support the build" section and a donate button.

## Decisions

| Topic | Decision |
|---|---|
| Canary result publication | Live public endpoint `https://status.tnhc.dev/privacy.json` served by the ecosystem proxy |
| Unbuilt promises | Kept as commitments, each with its honest current status ("Today: …") |
| Donations | Remove the asking section and the waitlist donate button; one plain "Donate" link in the site footer |

## Design

### 1. Privacy status page

Source of truth: new `privacy.md` in the handbook repo (CC BY 4.0). Rendered at `https://tnhc.dev/privacy` by tnhc.dev's existing `scripts/build-markdown-page.py`, pinned to a handbook commit, exactly as the Charter is.

Sections:

**Promise table** — columns `Promise | Status | How we know`; statuses `Done`, `Partial`, `Not done`:

| Promise | Status | How we know |
|---|---|---|
| We never store your IP address | Done | Daily privacy check (live result above); CI privacy guard on every commit |
| We keep no logs about you; technical logs live at most 24 hours | Done — a few containers keep small technical logs that are continuously overwritten rather than time-limited | Log configuration published in our repository |
| Your sign-in history is visible only to you and deleted after 30 days | Done. Encrypted with your own key: Not done | Account page; Auth source |
| Hosted sites get page-view counts only | Done | Hosting source; database schema |
| Export your data in open formats | Partial — Chat only | Chat data-export endpoint |
| Deleting your account deletes your data | Partial — an administrator can delete accounts; Chat's self-delete blanks the account but keeps messages | — |
| Move your account to your own node | Not done — you can run your own node | — |
| Your stored data is encrypted with your own key | Not done | Privacy Architecture Part 3 |

**What we keep, and why** — account details (to sign you in); sign-in events, 30 days (so you can spot someone else in your account); waitlist email address, until you are invited; the mail, messages and files you choose to store (that is the service).

**What others can see** — Cloudflare: all web traffic to TNHC passes through Cloudflare, which decrypts it at its edge and can see your IP address and what you send; Resend: outgoing mail to addresses outside Nexus; Cloudflare Email Routing: incoming mail from outside. Each is stated as a known limit with a pointer to Part 4 of the privacy plan (onion access, then the Phantom Protocol).

**Live result box** — rendered by the page component above the Markdown (section 2).

### 2. Live privacy-check result

Route: the ecosystem proxy (`deploy/production/proxy.ts`) answers `status.tnhc.dev` itself (fixed branch, before route lookup and the gate, like the email-ingress branch). Only `GET /privacy.json` (and `HEAD`, `OPTIONS`) is served; every other path → 404; other methods → 405.

Behaviour: read `/tmp/nexus-production/privacy-canary.json` (path overridable by `PRIVACY_CANARY_RESULT` for tests) and respond with exactly:

```json
{ "status": "pass" | "fail" | "stale",
  "checkedAt": "<ISO time from the result>",
  "searched": { "databases": n, "containers": n, "logs": n, "files": n, "probes": n },
  "findings": n }
```

- `status` is `stale` when the file is missing, unreadable, unparsable, or `checkedAt` is more than 36 hours old; `checkedAt` and `searched` are then omitted, `findings` 0.
- Never included: the test address, finding locations, file paths, container or database names.
- Headers: `Content-Type: application/json`, `Cache-Control: public, max-age=300`, `Access-Control-Allow-Origin: https://tnhc.dev`, plus the proxy's standard security headers.
- The file path is a constant; no request input reaches the filesystem.

Page box states: pass → "✅ Verified <relative time>: searched N databases, N containers, N logs, N files — found nothing"; fail → "❌ The last check found a problem. We are investigating."; stale → "Not verified recently"; fetch error → "Status unavailable".

### 3. Charter wording

Replace the "Our promises" opening bullets and the "Your data" list in handbook `charter.md` with:

```markdown
- Everything we make is open source.
- Nothing is sold, and nothing is behind a paywall.
- Nothing locks you in: our software is yours to run yourself, and we are building the tools to take your data with you (see below).

### Your data

- **We never store your IP address, and we keep no logs about you.** Our services keep only what they need to work and stay secure — never for tracking, analytics or profiling. Exactly what we keep, and for how long, is listed on our [privacy status page](https://tnhc.dev/privacy).
- **An automated check proves this every day.** It sends a marked test address through every public service and searches everything we run for it. Its latest result is public on the [privacy status page](https://tnhc.dev/privacy).
- **You will be able to export all your data in open formats.** Today: Chat only.
- **Deleting your account will delete your data, everywhere.** Today: partial — see the [privacy status page](https://tnhc.dev/privacy).
- **You will be able to move your account to your own node.** Today: you can run your own node; moving an existing account is not built yet.
- **What others can see:** our web traffic passes through Cloudflare, which can see it, and mail passes through Resend and Cloudflare Email Routing. We say so plainly and are working to remove them.
```

The Phantom paragraph, Availability, licences, money, AI and governance sections are unchanged. The handbook README lists `privacy.md`.

### 4. Donations

tnhc.dev: remove the "Support the build" section (`Support.jsx`) from the landing page and the donate button in `Waitlist.jsx`; add a plain "Donate" link to the shared footer (`SiteFooterLinks.jsx`) pointing at the existing PayPal URL. No other copy asks for money.

### 5. Wiring

- tnhc.dev: route `/privacy` (page component = the Markdown page + live box), data module `frontend/src/data/privacy.js` generated by `webmaster-sync.sh`, sitemap entry, "Privacy" link in `SiteFooterLinks`.
- Charter links to `/privacy` (section 3).

## Testing

- Proxy route (bun test): pass / fail / stale (missing file, unparsable, older than 36 h) → exact field set; never contains the marker address, `found` locations or paths; 404 on other paths; 405 on POST; CORS header value; ungated (no 302 without a session).
- Generator: `privacy.md` renders with commit pinned (existing tests cover the generator; add a sync assertion that `privacy.js` exists).
- Live box: unit-tested state mapping (pass/fail/stale/unavailable) if tnhc.dev has a JS test harness; otherwise verified live in each state by pointing the component at fixtures during development and recorded in the report.
- Handbook `scripts/check-charter.sh` extended: `privacy.md` exists, Charter links to `https://tnhc.dev/privacy`, no "data of any kind" phrase remains.
- Live after deploy: `https://tnhc.dev/privacy` 200 with the live box showing "Verified …"; `https://status.tnhc.dev/privacy.json` returns the safe fields; no "Support the build" text in the live bundle; privacy canary still passes.

## Out of scope

Building export, self-service deletion and account migration; encryption with users' own keys (Part 3); removing Cloudflare/Resend from the path (Part 4); rewriting tnhc.dev's "zero human intervention" copy (Voice chapter).
