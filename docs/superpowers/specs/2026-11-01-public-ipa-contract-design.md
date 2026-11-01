# Public IPA Contract Migration

## Status

Approved for planning. This document defines the public contract migration only;
implementation requires an approved implementation plan.

## Goal

Make `/ipa` the only public application-programming interface prefix for Nexus
services. Public `/api` paths will be removed rather than redirected.

The rename is a naming convention, not a security boundary. Existing
authentication, authorization, input validation, rate limiting, loopback-only
binding, and gateway allow-lists remain required and are not weakened by this
work.

## Scope

The migration covers HTTP and WebSocket routes reachable from the internet
through Cloudflare Tunnel and `deploy/production/proxy.ts`, including:

- `api.tnhc.dev` gateway routes.
- Dashboard browser-facing proxy routes and frontend clients.
- Public service endpoints exposed by production host routing.
- Public endpoint documentation, tests, and deployment probes.

The migration does not rename private loopback service-to-service routes. Those
routes are unreachable from the internet and may retain `/api` while public
proxies translate the external `/ipa` contract to the private upstream path.
This keeps the public boundary explicit without creating an ecosystem-wide
internal breaking change.

## Contract

1. Public browser and API clients call `/ipa/...`.
2. The production gate/proxy forwards only explicitly allow-listed `/ipa/...`
   public routes to their intended upstreams.
3. Public `/api/...` requests return `404` and never redirect to `/ipa/...`.
4. Private upstream paths may remain `/api/...`; proxy translation is
   server-side and must not expose caller-controlled upstream destinations.
5. Health and discovery exceptions are explicit and documented rather than
   inherited accidentally from a broad prefix rule.
6. External WebSocket paths use `/ipa/...` where they are public API contracts.

## Dashboard and Mail

The Dashboard remains the browser's same-origin trust boundary:

- Browser Mail calls use `/ipa/mail/...` on `app.tnhc.dev`.
- Dashboard verifies the session, injects caller identity server-side, and
  forwards to the loopback Mail API.
- The Mail API remains loopback-only and continues to reject unavailable or
  mailbox-inaccessible messages with `404`.

The Dashboard home page must not remain indefinitely on `Loading…` when the
identity request stalls. Its identity request receives a bounded timeout and
the UI transitions to a recoverable signed-out or error state. This prevents a
failed upstream from leaving the user without a route to recovery.

Safe request-level diagnostics will identify the public route, proxied upstream
route, response status, and a generated request ID. They must not log session
cookies, authorization headers, identity tokens, message contents, or mailbox
identifiers. The request ID is returned to the browser on failures so a user
report can be correlated with server logs.

## Migration Order

1. Inventory all externally reachable HTTP and WebSocket route contracts.
2. Add failing contract tests that prove `/ipa` succeeds and public `/api`
   fails for each affected gateway/proxy boundary.
3. Update proxy allow-lists and external route mappings to accept `/ipa` only.
4. Update browser clients, external SDKs, public documentation, probes, and
   tests to `/ipa`.
5. Retain private upstream paths where appropriate, with fixed server-side
   translation from the public prefix.
6. Add Dashboard request timeouts and safe correlation diagnostics.
7. Build and validate every affected active service, then run Nit and staging
   approval before production release.
8. Publish release notes describing the breaking public contract change.

## Security Invariants

- The proxy never routes based on a client-supplied upstream host.
- Private services stay on loopback or protected internal networks.
- Browser clients never receive service credentials or caller identity headers.
- Mail identity remains derived from the authenticated session for each request.
- `/ipa` route names do not substitute for authorization checks.
- Public `/api` rejection applies before the request can reach an application.

## Validation

Before release, validation must demonstrate:

- Each public `/ipa` endpoint reaches the intended service with its existing
  authentication and authorization behavior.
- Equivalent public `/api` requests receive `404`.
- Private upstream API routes are not reachable from the public gate.
- Dashboard identity timeout exits the loading state.
- Mail Trash uses the Dashboard proxy and succeeds for an authenticated
  mailbox message without exposing its identity to the browser.
- Existing security, proxy, Mail, Dashboard, gateway, and integration tests
  pass, followed by Nit and staging approval.

## Rollback

Rollback restores the prior release bundle and proxy configuration as one
deployment. It does not expose a long-lived `/api` compatibility alias. Any
emergency restoration of `/api` requires an explicit incident decision and a
new release note.
