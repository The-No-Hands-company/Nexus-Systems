# App Catalog Release Policy

**Date:** 2026-10-18
**Status:** Design approved in conversation; implementation pending written-spec review

## Purpose

The Nexus dashboard should show only user-facing applications that are fully built, tested, integrated, approved, and currently reachable. The dashboard must communicate real product growth one verified app at a time. Internal platform services and development or operations tools must not be presented as ordinary apps.

This policy also reduces exposure from unfinished services. A service can remain registered for health, administration, federation, or release work without becoming launchable from the normal user directory.

## Goals

- Make Cloud the source of truth for catalog surface and release state.
- Keep unfinished, preview, retired, unregistered, malformed, and unhealthy apps out of the normal user launcher.
- Keep platform services available to the control plane and authorized administration without representing them as user apps.
- Preserve role-appropriate access to developer and operator tools.
- Make promotion a per-app, testable, auditable step.
- Keep the public signed-out app list consistent with the signed-in user launcher.

## Non-goals

- Replacing the existing Cloud registry or health probes.
- Removing platform services from the deployment or admin health inventory.
- Building separate admin and developer products in this change.
- Reorganizing shelves or user preference behavior beyond applying the release projection.

## Catalog contract

Each approved catalog record gains two explicit fields:

```ts
type CatalogSurface =
  | "user-app"
  | "developer-tool"
  | "admin-console"
  | "platform-service";

type CatalogReleaseState = "staged" | "released" | "retired";
```

`surface` describes who the record is for. `releaseState` describes whether the record is allowed to enter a launch projection. Existing fields retain their current meanings:

- `reviewStatus` records catalog review outcome.
- `maturity` records stable, preview, or experimental quality.
- `visibility` controls catalog hiding or restriction.
- `health` is runtime reachability and is evaluated when the projection is built.
- `delivery` and the route or public URL describe how a launch is performed.

Cloud validates both new fields. Unknown or missing values fail closed and do not become launchable records.

## Launcher projection

The normal Dashboard launcher and `/apps` directory use one shared projection predicate:

```text
surface == "user-app"
releaseState == "released"
reviewStatus == "approved"
maturity == "stable"
visibility != "hidden"
health == "healthy"
valid route or public URL
```

The predicate is applied after native shell entries and Cloud registry entries are merged. Native entries must carry the same metadata as registry entries; they are not an escape hatch around the policy.

An app that fails any condition is omitted from normal shelves, search, pinned fallback lists, and the public signed-out “running now” list. A released app that becomes offline remains visible to authorized health and release views, where its offline state is actionable, but it does not remain in the normal launcher.

Malformed Cloud responses continue to fail safely: the Dashboard keeps its valid native entries and never constructs a tile from an incomplete or unsafe record.

## Surface assignments

The initial assignments are:

### User apps

Mail, Calendar, Chat, Project, Design, Graphic, Photos, Video, and Draw are `user-app` records. Each is promoted independently after its release gates pass.

### Developer tools

Terminal, Forge, and Deploy are `developer-tool` records. They are not part of Everyday, Work, or Create shelves. Authorized power users and operators can reach them through a dedicated Develop or Operate surface. Terminal remains available for authorized users, but is not presented as a general user app.

### Admin consoles

Nexus Cloud is an `admin-console`. It remains reachable through explicit administrator navigation and its health/control pages, but it is not an app tile in the normal directory.

### Platform services

Nexus Auth, Network, and Vault are `platform-service` records. They remain infrastructure used by the platform and its administration surfaces. They are not launcher apps and are not listed on the public signed-out page.

## Release promotion

An app may move from `staged` to `released` only when all gates below are satisfied:

1. The app quality gate passes its tests, TypeScript check, and production build.
2. Cloud has a registered record with the intended route and delivery mode.
3. The runtime health probe is green.
4. Catalog metadata has an approved owner and `stable` maturity.
5. Dashboard contract tests confirm the intended surface and filtering behavior.
6. A signed-in smoke check confirms that the platform session reaches the app through its launch route.

The promotion is per app. A new app therefore appears as a verified addition rather than as a promise about an entire unfinished category.

## Data flow

1. Cloud registers services and stores approved catalog metadata.
2. Cloud exposes catalog records together with surface and release state.
3. Dashboard fetches the catalog and merges it with native Mail and role-gated Terminal entries.
4. Dashboard applies the shared launcher projection before building shelves, search results, pins, or the public app summary.
5. Admin and release views continue to read the broader inventory so operators can diagnose staged, offline, or platform records.

User pins, recents, shelf ordering, and audience preferences operate only on the already filtered launcher projection. They cannot promote an app or change its surface.

## Testing and verification

### Cloud

- Validate and round-trip `surface` and `releaseState` in catalog contract tests.
- Verify seed metadata classifies platform and developer services correctly.
- Verify staged, retired, preview, malformed, and unhealthy records are not launchable.
- Verify the registry and admin inventory still retain non-launchable records.

### Dashboard

- Test the projection predicate for every surface and release state.
- Test that native Mail is released user-app metadata and native Terminal is role-gated developer-tool metadata.
- Test that legacy records without the new fields fail closed.
- Test that Auth, Cloud, Network, and Vault cannot enter normal shelves through explicit or malformed payloads.
- Test that offline released apps remain in admin inventory but disappear from launcher, search, pins, and signed-out summaries.

### End-to-end gates

- Run the complete app quality matrix already established for Design, Graphic, Photos, Video, Draw, Chat, Auth, Calendar, Project, Dashboard, and Cloud.
- Run Dashboard and Cloud catalog contract suites.
- Run production builds and runtime health probes.
- Confirm the deployed dashboard exposes only the currently released, stable, healthy user apps.

## Rollout and recovery

The change is additive to the catalog schema and defaults unknown records to non-launchable. Existing records without the new fields will disappear from normal shelves until they are explicitly classified and released. This is intentional and fail-closed.

If a release classification is wrong, an administrator can move the record back to `staged` or `retired`; no client-side code change is required. If Cloud is unavailable, Dashboard continues to render only its validated native entries and does not invent registry apps.

## Review checklist

- [x] User-facing and platform responsibilities are separated.
- [x] Offline behavior is explicit.
- [x] Release gates are per app and testable.
- [x] Existing admin and health inventory remains available.
- [x] Unknown or malformed metadata fails closed.
- [x] User preferences cannot override release policy.
- [x] Scope is limited to catalog contract, projection, native metadata, and tests.
