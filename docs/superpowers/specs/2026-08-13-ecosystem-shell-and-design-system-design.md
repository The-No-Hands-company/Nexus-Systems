# The ecosystem shell and design system

**Date:** 2026-08-13
**Status:** approved design, ready for an implementation plan

## Goal

Make the ecosystem feel like one product instead of a ring of separate sites, by
building the two things every app depends on: a shared design language, and a
shell that hosts the apps.

## Why this, and why now

The ecosystem has 89 apps under `apps/` — 30 built, 57 empty shells (verified
twice in `docs/ecosystem-audit-june-2026.md`, June 2026) — heading toward the
several hundred the founder intends. Every one of those apps will need to look
and behave like part of the same thing. Building 85 more before there is a
design language means 85 more things to retrofit.

Two of the questions that prompted this work turn out to be **already answered
in the project's own documents, and simply never built**:

- `docs/noname.md` states plainly: *"Nexus Cloud is the canonical host-shell
  reference."* It defines shell regions (`app-header`, `app-sidebar`,
  `app-content`, `app-utility-rail`), says tool content mounts only inside
  `app-content`, and specifies a canonical token file at
  `tokens/nexus.tokens.json` with a complete schema — colour, typography,
  space, radius, shadow, motion, z-index.
- The locked blueprint defines Nexus-Cloud as *"Orchestrator / sovereign cloud
  platform (control plane, scheduling, federation)"*.

Together: Cloud is meant to be the front door you sign into that contains all
the apps — the `google.com` of the ecosystem — *and* the thing that runs them.

What exists instead is a service registry, a routing table, and a portal that
displays infrastructure. Verified absent: `tokens/nexus.tokens.json`, any
shared UI package, any `artifacts/ui-fingerprint/`. The doctrine is entirely
unimplemented. That gap is the whole of this project.

## What actually gets embedded

Measured, not assumed. Of the surfaces reachable today:

| Surface | State | In this project |
|---|---|---|
| **Chat** (`chat.tnhc.dev`) | full React SPA | embedded |
| **Draw** (`draw.tnhc.dev`) | full React SPA | embedded, and first |
| **Cloud console** (`cloud.tnhc.dev`) | 98 KB inline HTML | rebuilt as shell-native views |
| **Hosting** (`hosting.tnhc.dev`) | 2 KB inline placeholder | launcher tile only — nothing to embed yet |

So the first slice embeds two apps, not four. That is a smaller claim than it
first appeared, and an honest one: two real interfaces is enough to prove the
contract, and the other two do not have interfaces to prove it with.

## Decisions

Each was chosen against alternatives; the rejected ones are recorded because
the reasons will resurface.

### Apps render in a frame inside the shell

The dual-mode principle — every tool runs standalone *or* orchestrated — means
apps must stay independently deployable. The ecosystem is polyglot by design:
C++ (Nexus-Modeling), Rust (Nexus, Phantom), Python (Nexuslang, Wiki),
TypeScript. **Nothing that requires apps to be JavaScript modules compiled into
a host can work here.**

So `app-content` holds an iframe pointing at the app's own public URL.

*Rejected — chrome injected at the proxy:* the proxy would have to parse and
rewrite HTML from five stacks, and any app doing client-side routing or owning
its own `<head>` fights it. Fragile in proportion to how many apps exist.

*Rejected — launcher only, apps as full pages:* simplest and closest to what
Google actually does, but the shell can then never host anything persistent
across apps, which forecloses global search and a docked chat later.

### The shell grows out of Nexus-Dashboard; Cloud sheds its UI

Nexus-Dashboard is small, tested, built with Vite/React/Tailwind, already the
authenticated front door, and already has the launcher grid, request-access,
claim, account and admin surfaces. Cloud's UI is a single 98 KB inline HTML
file with no build step.

The shell therefore grows out of the Dashboard. Cloud keeps the registry,
routes, orchestration and Systems API and loses its frontend entirely.
"Nexus Cloud" comes to mean shell plus control plane, the way "Google Cloud"
means console plus backend.

This contradicts a literal reading of *"Nexus Cloud is the canonical
host-shell reference"* — the shell will live in a different directory than the
sentence implies. The intent is honoured; the file layout is not. That is the
right trade against rewriting a working codebase into a 98 KB HTML file.

*Rejected — rebuild Cloud's portal:* effectively a rewrite, and it puts a large
frontend inside a service that should be an orchestrator.

*Rejected — a third, dedicated shell app:* cleanest boundaries, but rebuilds
working auth surfaces for no functional gain.

### The shell stays at `app.tnhc.dev` for now

The apex belongs to the shell eventually — that is the Google model and the
founder's instinct. But `tnhc.dev` is currently a static site on Cloudflare
Pages *specifically* so it survives this machine being off, and the
`/login`, `/signup`, `/claim` redirects already make the apex behave like a
front door. Moving the apex onto the tunnel trades away outage resilience and
is its own decision, made deliberately rather than as a side effect of this
work.

Related, and settled: **hiding subdomains is not a security measure.** Every
TLS certificate is published to Certificate Transparency logs, so
`chat.tnhc.dev` is enumerable whether or not it is ever linked. Google, the
stated model, does not do single-origin either — `mail.google.com` and
`drive.google.com` are public. What makes Google feel like one product is one
account, one launcher, one design language, consistent behaviour. That is what
this project builds. What protects the hosts is the login gate, which exists.

## Architecture

### Token pipeline

Source of truth: **`tokens/nexus.tokens.json`**, transcribing the schema
already written in `docs/noname.md` — nothing invented.

One generator, two outputs, so they cannot drift:

| Output | Consumer | Why |
|---|---|---|
| `nexus-tokens.css` | any app, any language | CSS custom properties on `:root`, linked with one tag. The universal floor — works for Cloud's hand-written HTML and any future Rust or Python frontend. |
| `nexus-tailwind-preset.js` | the React apps | the same tokens as a Tailwind theme, so `bg-canvas` and `text-primary` resolve to token values. The Dashboard, Chat's web client and Deploy's console are all Vite + React + Tailwind with no shared config today; each adopts it by adding one line. |

Both are generated. Hand-editing either is the bug, and the drift test below
exists to catch it.

Home: a new `packages/nexus-design`, beside the existing `packages/*`.

**No React component library in this project.** The doctrine's mandatory
primitives — button, input, select, modal, toast, tabs, table, command palette
— are real and needed, but primitives designed before real screens use them
come out wrong. The shell plus two embedded apps will tell us what the
primitives should be, and what they should not be.

### The shell

Regions, per the doctrine:

- **`app-header`** — wordmark, account menu. Global search is a later addition;
  the region exists for it.
- **`app-sidebar`** — the launcher: apps from Cloud's registry with health,
  which the Dashboard already consumes via `/api/apps`.
- **`app-content`** — the mounted app, or a shell-native view (Cloud's console,
  account, admin).
- **`app-utility-rail`** — named and reserved, not built. Better an empty named
  region than invented uses for it.

Shell routing: `/a/:appId` mounts an app. Shell-native views keep their own
paths (`/account`, `/admin`).

### The embed contract

An app is embeddable when it satisfies three things:

1. **Responds to `?embed=1`** by suppressing its own header and sidebar,
   rendering only content. Crude, language-agnostic, and degrades safely: an app
   that ignores it still works, just with doubled chrome.
2. **Permits framing by the shell** — see below.
3. **Links `nexus-tokens.css`**, so it inherits the palette even where it has
   not adopted the Tailwind preset.

Authentication needs no mechanism. The session cookie is `.tnhc.dev`-scoped and
the frame is same-site, so the proxy gates and identifies a framed request
exactly as it does a direct one. No token passing, no postMessage auth
handshake.

### Framing headers

Measured on the live hosts, 2026-08-13. All three need changes, in three
different directions:

| Host | Sends today | Effect | Change |
|---|---|---|---|
| `chat.tnhc.dev` | `X-Frame-Options: DENY` | embedding blocked outright | drop XFO; set `frame-ancestors` |
| `hosting.tnhc.dev` | `frame-ancestors 'self'` | blocks the shell, a different origin | widen when it has a UI worth embedding |
| `draw.tnhc.dev` | *nothing* | **any site on the internet can frame it** | add `frame-ancestors` |

Target for every embeddable app:

```
Content-Security-Policy: ...; frame-ancestors 'self' https://app.tnhc.dev
```

and no `X-Frame-Options`, which CSP obsoletes and which conflicts with it.

Note this *tightens* security overall: Draw currently has no framing protection
at all, and gains it here. Chat's Caddyfile also currently pairs
`frame-ancestors *` in its CSP with `X-Frame-Options: DENY` — two directives
disagreeing with each other; this replaces both with one correct value.

### Deep links: a stated limitation

The shell owns `/a/:appId`; the app owns its inner routing inside the frame.
`app.tnhc.dev/a/chat` opens Chat, but a link to a *specific channel* will not
survive as a shell URL in this version.

Making it work needs a postMessage route-sync contract implemented on both
sides. That contract should be designed against three real embedded apps rather
than guessed at now. It is a known gap, not an oversight.

## Out of scope

Named so they are not quietly absorbed:

- The React primitive library (deliberately deferred, see above).
- Deep-link route sync between shell and app.
- Moving the shell to the apex.
- Cloud as orchestrator — the other half of Cloud's identity, and its own
  project.
- The remaining 85 apps. Only what runs today is in scope.
- **Embedding Hosting.** Measured 2026-08-13, `hosting.tnhc.dev` serves a 2 KB
  inline page: one `<style>` block, no scripts, no control panel. There is no
  interface there to embed, and framing a placeholder achieves nothing. It gets
  a launcher tile like any other app; embedding waits until it has a UI. Same
  for Deploy, whose console exists but is not reachable as its own host.
- UI fingerprints, screenshot matrices, scoring and the Nit CI gate. The
  doctrine specifies all of it; none of it is useful before there is a design
  system to measure against.

## Verification

- **Token drift:** a test asserting every key in `nexus.tokens.json` appears in
  the generated CSS. Drift between source and output is the failure this
  pipeline exists to prevent, so it is the test that matters most.
- **No hardcoded colour on shell surfaces:** the doctrine's "token bypass" rule,
  enforced where it is cheap. Extending it across apps comes later.
- **Shell behaviour:** launcher renders the registry, `/a/:appId` mounts the
  right frame, an unknown appId fails visibly rather than blankly.
- **Embed contract, per app:** `?embed=1` suppresses chrome; the app loads
  inside the shell, signed in, with exactly one set of chrome visible. Applies
  to Draw and Chat, the two apps with real interfaces today.
- **Framing headers:** each embeddable host permits the shell and refuses an
  unrelated origin. Both directions asserted — the second is the security half.

## Build order

Each step is independently useful and independently verifiable.

1. `packages/nexus-design` — tokens, generator, CSS and Tailwind preset, drift test.
2. Shell chrome and launcher; apps still link out. The visual language becomes
   visible and reactable before anything commits to it.
3. Framing headers, then embed **Draw** first — it has no sidebar of its own, so
   it is the smallest embed contract to satisfy.
4. Embed **Chat**.
5. Cloud's console rebuilt as shell-native views; `status.html` retired and
   Cloud left frontend-free.
