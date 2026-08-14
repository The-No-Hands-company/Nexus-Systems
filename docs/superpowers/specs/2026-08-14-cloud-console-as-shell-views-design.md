# Cloud's console as shell-native views

**Date:** 2026-08-14
**Status:** design, ready for an implementation plan

## Goal

Finish what the shell project started: move Cloud's operator console into the
shell as native views, and leave Cloud a control plane with no frontend.

## Why now

The shell exists, three apps render in the ecosystem palette, and `/account`
and `/admin` are already shell-native. Cloud is the last surface that is still
its own website — a 99 KB inline HTML file with its own layout, its own
palette aliases, and its own idea of what an app launcher is.

The earlier plan tried to delete `status.html` outright and was refused,
because the shell had nowhere to put the console. It does now.

## What is actually in there

Measured, not assumed. `public/status.html` is 2,433 lines, of which 1,746 are
one `<script>`. The eight `view-*` sections are near-empty markup (~330 bytes
each); every view is rendered by JavaScript from API data. So this is not a
markup port — it is eight fetch-and-render functions, six of which are worth
keeping.

| View | Loader | Backing data | Disposition |
|---|---|---|---|
| `view-dashboard` | `loadDashboard` + `renderAuditCard`, `renderInternalServicesCard`, `renderPulseFeed`, `renderTrustCard` | `/api/v1/audit`, service state | **port** |
| `view-tools` | `loadToolsView` | `/api/v1/endpoints` | **port** |
| `view-users` | `loadUsersView` | `/api/v1/users` | **port** |
| `view-federation` | `loadFederationView` | `/v1/federation/peers` | **port** |
| `view-identity` | `loadIdentityView` | `/v1/federation/identity` | **port** |
| `view-api` | `loadApiView` | `/api/v1/endpoints` | **port** |
| `view-launcher` | `loadLauncher` + `renderEcosystemGrid` | Cloud's tool registry | **delete** |
| `view-app-frame` | — | — | **delete** |

### Two of the eight views are the shell

`view-launcher` and `view-app-frame` are an app launcher and an iframe that
mounts an app into a content area. That is precisely what `app.tnhc.dev` now
does. Cloud's console contains a second, older implementation of the shell,
and keeping it would mean maintaining the duplication this whole line of work
exists to remove.

They are deleted rather than ported.

### One thing worth salvaging, deliberately not now

`renderPinnedGrid`, `renderContinueGrid` and `renderOpenApps` implement pinned
apps, continue-where-you-left-off, and open-app tracking. Those are exactly the
"richer home" ideas rejected earlier in favour of keeping `/` a plain grid.

They are **deleted with the launcher**, and recorded here so the next person
knows the ecosystem once had them and where to find them in git history. Moving
them into the shell's home is a product decision that was already made the
other way; reversing it silently, in a migration, would be the wrong way to
make it.

## Architecture

### Where the views live

`apps/Nexus-Dashboard/frontend/src/pages/cloud/`, routed under `/cloud/*` and
wrapped in `ShellView` — the same wrapper `/account` and `/admin` now use. They
are shell-native pages, not a framed app.

The shell's launcher currently links Cloud to `https://cloud.tnhc.dev` as an
external app. That entry changes to point at `/cloud`, so Cloud stops being
"an app you frame" and becomes part of the shell, which is what a control plane
should be.

### How the browser reaches Cloud's API

It does not, directly. The Dashboard server already has a Cloud client
(`apps/Nexus-Dashboard/src/cloud.ts`) that holds `NEXUS_CLOUD_API_KEY` and
talks to `NEXUS_CLOUD_URL`. The browser calls the Dashboard, the Dashboard
calls Cloud:

```
browser → app.tnhc.dev/api/cloud/<path> → Dashboard server → 127.0.0.1:8787 → Cloud
                                          (adds X-API-Key)
```

**The API key must never reach the browser.** Cloud's console today is served
by Cloud itself on a same-origin session, which is why it could call the API
directly; a shell-native view cannot, and must not be given a key to do it.

The proxy is deliberately an allow-list of specific paths rather than a
catch-all `/api/cloud/*` passthrough. A blanket proxy would hand any signed-in
user the full control-plane API with the operator's key attached, including
mutating routes.

### Authentication

Cloud's `checkAuth`, `/api/v1/auth/login` and its login screen go away. The
shell is reached through the SSO gate, so a user viewing `/cloud/*` is already
authenticated. Cloud's own auth endpoints stay for direct API use; the console
stops using them.

**Admin-only surfaces stay admin-only.** `view-users` and the mutating parts of
tools management must remain restricted to the founder/admin role, enforced at
the Dashboard proxy, not by hiding UI.

## Out of scope

- Cloud's orchestration and scheduling — the other half of its identity.
- Any new operator capability. This migration ports what exists; it is not the
  moment to design better views.
- The `--orange` quarantine chip and other residual hardcoded colour in
  `status.html`, which dies with the file.

## Verification

- Each ported view renders the same data as its `status.html` predecessor,
  checked against the same live endpoint.
- The Dashboard proxy rejects a path outside its allow-list.
- The proxy never returns the API key, and the key appears nowhere in the
  built frontend bundle.
- A non-admin session cannot reach the users view's data through the proxy.
- `cloud.tnhc.dev/` returns the JSON service pointer, not HTML, once
  `status.html` is deleted.
- All six public hosts still serve.

## Build order

Each step leaves the system working.

1. The Dashboard proxy, with its allow-list and role check. No UI yet.
2. `/cloud` overview + tools, the two most-used views, routed in the shell.
3. Users, federation, identity, API views.
4. Point the launcher entry at `/cloud`; delete `status.html`, `handleDashboard`
   and the `/nexus-tokens.css` route that only existed to style it. Cloud
   returns the JSON pointer the earlier plan wanted, now that it is true.
