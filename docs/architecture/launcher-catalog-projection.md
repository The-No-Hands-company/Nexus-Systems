# Launcher and catalog projection

This document is the contract for **which apps appear in the Dashboard launcher,
and why**. It complements `frontend-load-and-app-delivery.md` (which covers *how*
an app opens once launched) by specifying *whether* an app is offered at all.

New contributors reach for this when "my app isn't showing in the grid" or "a
half-finished app is showing when it shouldn't."

## The launcher is a fail-closed projection

An app is offered in the launcher only when it is unambiguously a finished,
reachable, user-facing app. The predicate is `isLauncherApp`
(`apps/Nexus-Dashboard/frontend/src/shell/catalog.ts`); an entry must satisfy
*all* of:

| Field | Required value | Why |
| --- | --- | --- |
| `surface` | `user-app` | Admin consoles, platform services and developer tools are not everyday apps. |
| `releaseState` | `released` | A `staged` app is not finished; it must not be offered. See below. |
| `maturity` | `stable` | Preview/experimental builds are not launcher-ready. |
| `health` | `healthy` | A tile that leads to a down app is worse than no tile. |
| `reviewStatus` | `approved` | Pending/deferred/rejected entries have not cleared catalog review. |
| `visibility` | `default` or `discoverable` | `restricted`/`hidden` are deliberately kept out. |
| `path` | a safe relative path | Guards against fabricated or unsafe destinations. |
| destination | shell-native, or a safe `publicUrl` | An app with no reachable destination cannot be launched. |

Anything missing or malformed fails the predicate rather than slipping through —
the projection is fail-closed on purpose.

## `released` only — front and back must agree

`isLauncherApp` requires `releaseState: "released"`; it does **not** accept
`staged`. This mirrors the server's `isReleasedUserApp`
(`apps/Nexus-Dashboard/src/apps.ts`), which only ever sends released user apps to
the client. The two must stay in agreement: if the frontend admitted `staged`
while the server filtered it out, the launcher's contents would depend on which
path an app happened to arrive by.

Promoting an app from `staged` to `released` is a deliberate catalog decision
made in Nexus-Cloud's seed (`apps/Nexus-Cloud/src/catalog/seed.ts`, bumping
`CATALOG_SEED_VERSION`) or through the catalog approval API — not something the
dashboard decides.

## Two sources of truth: the Cloud registry and the pins

The launcher's app list is **data from two sources**, merged in `listApps`
(`apps/Nexus-Dashboard/frontend/src/api.ts`):

1. **The Cloud registry**, via the dashboard server's `/api/v1/apps`. This is the
   normal path: an app appears by registering with Nexus-Cloud, and a second
   node's apps appear with no change to the dashboard. Health here is **derived
   from heartbeats** — Cloud runs a sweep (`applyHeartbeatExpiry`, ~90s
   threshold) that marks any tool whose heartbeat has lapsed as `offline`, which
   then fails `isLauncherApp`.

2. **`SHELL_NATIVE_APPS`** — a small pinned set in the frontend. These are
   first-party apps that must appear regardless of how (or whether) Cloud
   classifies them. A pin **wins over a registry entry with the same id or path**,
   so the pin's name, description, delivery and health are authoritative.

### When to pin

Pin an app in `SHELL_NATIVE_APPS` only when both are true:

- It is **first-party** and its availability is not in question (it is served at
  a fixed, known URL, e.g. `chat.tnhc.dev`), and
- Relying on the Cloud registry would wrongly drop it — typically because
  **nothing heartbeats it** (the chat case: the Rust API heartbeats a tool named
  `nexus`, not `nexus-chat`, so the registry entry goes stale and the sweep marks
  it offline).

Pinning is a pragmatic override, not the default. An app that genuinely
heartbeats Cloud should rely on the registry path so its health is real. Prefer
fixing the heartbeat at the source over pinning when that is feasible.

### Pin side effects to know

Because a pin overrides a same-id/same-path registry entry, the pinned values
(name, description, delivery) replace whatever the registry says. Tests that mock
the registry must account for this: a mocked app whose id or path collides with a
pin is replaced by the pin, and the pins are always prepended to the result.

## Display names are prefix-stripped

`listApps` runs `dashboardName`, which strips a leading `Nexus ` / `Nexus-` from
the *display name* only (ids and slugs are untouched). Inside the dashboard the
user is already in Nexus, so "Nexus Chat" shows as "Chat". Anything asserting or
matching a display name must use the stripped form.

## Where each rule lives

| Concern | File |
| --- | --- |
| Launcher predicate | `apps/Nexus-Dashboard/frontend/src/shell/catalog.ts` (`isLauncherApp`) |
| Server-side release filter | `apps/Nexus-Dashboard/src/apps.ts` (`isReleasedUserApp`) |
| Merge of registry + pins, name stripping | `apps/Nexus-Dashboard/frontend/src/api.ts` (`listApps`, `SHELL_NATIVE_APPS`, `dashboardName`) |
| Catalog placement + release seed | `apps/Nexus-Cloud/src/catalog/seed.ts` |
| Heartbeat health sweep | `apps/Nexus-Cloud/src/server.ts` (`applyHeartbeatExpiry`) |

See `frontend-load-and-app-delivery.md` for how a launched app is delivered
(shell-native, external, framed) once it passes this projection.
