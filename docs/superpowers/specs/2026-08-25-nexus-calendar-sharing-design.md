# Nexus Calendar Personal and Shared Events Design

**Date:** 2026-08-25
**Status:** Approved for implementation planning

## Goal

Make Nexus Calendar a reliable first-class web application with one frontend
served through both the standalone `calendar.tnhc.dev` surface and the canonical
shell path `/calendar`.
Events are private to their owner by default, can be shared explicitly with
Nexus users, and can be published through revocable public read-only links for
people outside the Nexus network.

## Scope

This increment covers event ownership, internal event sharing, public event
links, authorization, API validation, the shell-integrated proxied-app delivery
pattern, error states, migration of existing data, and automated/runtime
verification.

Named multi-event calendars, guest accounts, email invitations, CalDAV,
recurrence expansion, reminders, public calendar feeds, and migration of every
existing framed Nexus application are out of scope. The schema and service
boundaries must not prevent named shared calendars from being added later. The
registry contract introduced here must support later app-by-app frame removal.

## Architecture

Nexus Calendar remains the authority for events, sharing, and its web frontend.
Nexus Dashboard remains the same-origin authenticated gateway for the canonical
`/calendar` route, but it does not own or duplicate Calendar UI components. The
standalone front door serves the same built Calendar application at
`calendar.tnhc.dev`.

Every authenticated Calendar request carries an identity asserted by a trusted
front door. Browser-provided identity headers are always discarded. Dashboard
looks up the caller through Nexus Auth and writes the trusted subject to the
upstream request. The standalone front door must obtain the same authenticated
subject from the SSO layer. The Calendar service trusts identity headers only
from its loopback-only upstream path; its API stays bound to loopback in
production.

Public sharing uses a separate, narrow unauthenticated surface. A random bearer
token resolves to one published event representation. The route cannot list or
search events and exposes no authenticated API behavior.

Calendar produces one frontend artifact. Dashboard serves or reverse-proxies
that artifact beneath `/calendar`, including client-side fallback and assets,
without an iframe. The Calendar build uses a base-path-safe asset strategy and
runtime configuration for its API base, public base, and shell context. Direct
subdomain access serves the identical artifact at `/`. Shell context adds the
shared Nexus navigation package; standalone context uses the same application
core with standalone chrome. No Calendar feature code lives in Dashboard.

## Registry and Delivery Contract

Application discovery separates navigation identity from deployment location:

- `path` is the canonical in-shell route and is always relative;
- `publicUrl` is the optional direct HTTPS origin for bookmarks, integrations,
  and standalone access;
- `delivery` is `shell-native`, `proxied-app`, `framed`, or `external`.

`shell-native` is reserved for views genuinely owned by Dashboard, such as the
account and control-plane console. `proxied-app` is the target for product apps:
the app owns one frontend while Nexus exposes it through its shell path without
an iframe. `framed` remains a temporary compatibility state for apps not yet
migrated. `external` is reserved for destinations Nexus cannot integrate.

Launcher navigation always uses `path`. Calendar registers:

```text
id=nexus-calendar
path=/calendar
publicUrl=https://calendar.tnhc.dev
delivery=proxied-app
```

This change updates the shared registry types and Calendar entry. It may map
existing applications onto explicit delivery values when required for type
compatibility, but it does not migrate their rendering in this increment.

## Data Model and Migration

The `events` table gains an immutable `owner_subject` column. New records always
take this value from the trusted request identity, never from JSON input.

An `event_shares` table records:

- event ID;
- grantee Nexus subject;
- permission: `viewer` or `editor`;
- creator and timestamps;
- a uniqueness constraint on event ID plus grantee subject.

A `public_event_shares` table records one revocable public share per event:

- event ID;
- a cryptographically random token or token digest;
- creation and optional revocation timestamps;
- uniqueness constraints preventing token reuse.

Existing events have no defensible inferred owner. Deployment therefore
requires a configured legacy owner subject. Migration fails closed when legacy
rows exist and that subject is absent. Existing events are never made globally
visible as a migration shortcut.

## Authorization Rules

- Owners can read, update, delete, share, unshare, publish, and revoke an event.
- Internal viewers can read the event only.
- Internal editors can read and update event content.
- Editors cannot delete events, change ownership, or manage sharing.
- Users without an applicable grant receive `404` for event-specific reads and
  mutations so the API does not disclose event existence.
- Authenticated event listings return owned events plus events shared with the
  caller, without duplicates.
- Public tokens grant read-only access to their single published event.
- Revoked, malformed, or unknown public tokens return `404`.
- Public responses include title, start/end time, all-day state, location, and
  description. They exclude owner subject, internal share records, audit data,
  and database metadata.

Public URLs are bearer capabilities. Tokens must contain at least 128 bits of
entropy, must not appear in ordinary logs, and must be replaceable and
revocable. Public pages must send a restrictive referrer policy and must not
load third-party resources that could receive the URL.

## API Behavior

Authenticated event CRUD remains under `/api/v1/calendar/events`. All endpoints
require a trusted caller identity.

Sharing adds these owner-only endpoints:

- `GET /api/v1/calendar/events/:eventId/shares` lists internal grants and the
  active public-share state;
- `PUT /api/v1/calendar/events/:eventId/shares/:subject` creates or replaces a
  viewer/editor grant;
- `DELETE /api/v1/calendar/events/:eventId/shares/:subject` removes a grant;
- `POST /api/v1/calendar/events/:eventId/public-share` creates or replaces the
  event's public link;
- `DELETE /api/v1/calendar/events/:eventId/public-share` revokes it;
- `GET /api/v1/calendar/public/:token` returns the filtered public event without
  requiring an authenticated identity.

The public web page is served at `/share/:token` on the direct Calendar origin
and reads only the matching `/api/v1/calendar/public/:token` resource. The
Dashboard origin does not need an unauthenticated public route.

Mutation inputs use allow-listed fields. They reject unknown fields, malformed
timestamps, an end at or before the start, invalid permission names, blank
titles, and invalid identifiers with clear `400` responses. Authentication
failures return `401`; authorization is concealed with `404`; upstream
unavailability remains `503` at Dashboard.

Range queries use interval overlap (`event.start < requestedEnd` and
`event.end > requestedStart`) so multi-day and boundary-crossing events appear
in every relevant month. Query ranges are validated and bounded.

## Web Experience

The month view displays owned and shared events. Shared events have a visible,
accessible indicator and permission state. Viewer-only events do not expose edit
or delete controls.

Clicking `+` without a selected day selects today and opens the form. Clicking a
day selects that day. Create, update, delete, and sharing actions show progress,
preserve user input on failure, and show actionable errors. Initial loading,
empty results, offline service, validation failures, and authorization failures
are distinct states; API failures never silently become an empty calendar.

Owners can open a sharing panel to grant a Nexus subject viewer/editor access,
remove grants, create a public link, copy it, and revoke it. The public page is a
small read-only event view and contains no navigation into authenticated
Calendar data.

Both entry points use the same behavior and responsive layout from the same
build artifact. Dashboard no longer imports a Calendar page component. The
proxied application renders shared Nexus navigation when running under the shell
path; the direct Calendar origin renders its standalone chrome.

## Error Handling and Operations

Database writes that modify an event and its sharing state use transactions.
Unique conflicts are idempotent where appropriate and otherwise return a stable
conflict response. Calendar closes its SQLite handle during shutdown.

Health reporting distinguishes service liveness from database readiness. The
Dashboard health probe remains bounded. Proxy routes use method and path
allow-lists, forward only necessary headers and bodies, and return stable error
envelopes without leaking upstream details.

Deployment builds the single Calendar frontend artifact before starting either
front door, sets the legacy owner for migrations, keeps the private API on
loopback, and exposes only authenticated application routes plus the dedicated
public share route. Cache rules distinguish immutable hashed assets from the
uncached application shell at both base paths.

## Verification

Backend tests cover:

- migration and legacy-owner failure behavior;
- owner isolation and non-enumerability;
- viewer/editor/owner permission matrices;
- spoofed identity rejection at the Dashboard boundary;
- public token entropy, filtering, access, replacement, and revocation;
- invalid payloads and time ranges;
- overlapping and multi-day range queries;
- transaction and persistence behavior.

Frontend tests cover:

- `+` selecting today and opening the form;
- loading, empty, offline, validation, and mutation-failure states;
- input preservation on failed saves;
- permission-aware controls;
- internal sharing and public-link creation/revocation flows;
- identical Calendar behavior under `/` and `/calendar` base paths;
- runtime API-base selection and shell/standalone chrome;
- absence of an iframe and absence of duplicated Calendar feature code in
  Dashboard.

Integration verification includes Calendar and Dashboard type checks/tests, the
single production Calendar build, registry delivery tests, both front-door
routes, the per-app `check.sh` gates, and an authenticated browser smoke test.
The smoke test creates a private event,
proves a second Nexus identity cannot see it, grants that identity access,
creates and opens a public link without a Nexus session, revokes it, and proves
the link no longer resolves.

## Success Criteria

- Calendar is visible and usable from both intended web surfaces.
- `/calendar` and `calendar.tnhc.dev` execute the same Calendar frontend without
  an iframe or Dashboard-owned feature duplicate.
- Discovery exposes distinct `path`, `publicUrl`, and `delivery` fields and the
  launcher consistently navigates through `path`.
- Personal events are isolated by default.
- Explicit Nexus-user sharing enforces viewer/editor permissions.
- External users can view only one deliberately published event through a
  revocable link.
- No browser-controlled identity value influences authorization.
- UI failures are visible and recoverable.
- Automated suites, production builds, and the browser smoke test pass.
