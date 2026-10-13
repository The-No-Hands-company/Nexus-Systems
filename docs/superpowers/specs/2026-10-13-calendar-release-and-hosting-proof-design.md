# Calendar Release and Hosting Publish Proof

## Goal

Prove two distinct product paths on shared Nexus infrastructure:

1. Release the first-party Nexus-Calendar service with Nexus-Deploy, confirm its live health through Nexus-Cloud, expose it through the selected production ingress path, and roll back to the prior release.
2. Publish a static website as a user-owned site through Nexus-Hosting.

The app-service path and user-site path share routing and infrastructure, while retaining separate identities, permissions, and user experiences.

## Decision

Use Nexus-Calendar as the first-party service. For this proof, “production ingress path” means Cloudflare Tunnel into the existing production proxy, with route intent read from Nexus-Cloud. The separately implemented Nexus-Tunnel app is not in that request path; checks and documentation must say so clearly. The proof must not claim Nexus-Tunnel handled public traffic.

All integration runs are local or explicitly staged. They use a unique Calendar registration identity and a fresh temporary SQLite database, and never replace `nexus-calendar`, alter live DNS, restart production services, or write to the production Cloud registry. The Hosting proof uses a disposable test user/site and local test services.

## Current constraints and resulting changes

- Calendar currently hard-codes the Cloud tool ID, `calendar.tnhc.dev`, and a healthy state. Make service identity, route, and public URL configurable with production-compatible defaults. Health sent to Cloud must be based on an HTTP probe of Calendar’s own `/health`; a failed probe must never be reported as healthy.
- Nexus-Deploy clones and builds only a repository root. Add a validated project build context (relative directory, default `.`) so a first-party app inside the Nexus-Systems monorepo can be built using its own Dockerfile. The integration fixture must use a local Git repository and Docker; it must not require a push or remote GitHub access.
- Cloud’s registry and route API already accept service registrations and heartbeats. Verify that a unique staged identity appears in Cloud’s real route response and that Cloud reflects health transitions.
- The production proxy already maps Cloud routes to an upstream and proxies requests. Exercise that same route-building and request-handling code with a temporary route table and a disposable local Calendar upstream. Do not start or mutate the production proxy or Cloudflare connector.
- Nexus-Hosting already has a site upload/deploy API and browser E2E tests. Add an isolated integration check that creates a temporary user and static site, uploads a small fixture, publishes it, and fetches the resulting site through the Hosting serving path. Clean up the fixture afterward.

## Lifecycle behavior

The Calendar service release check creates a temporary Git repository from the Calendar service source and Dockerfile, creates a Nexus-Deploy project with the scoped build context and disposable configuration, and triggers a deployment through the Deploy API. It waits for the deployment to become live, probes `/health`, and confirms Cloud’s registered route points at the deployed upstream and reports the probed health. It then deploys a second known version, proves the second version is exposed and healthy, invokes Nexus-Deploy rollback to the first image, and confirms the first version is exposed again. The check always removes its temporary project/container, route registration, database, and Git fixture, including on failure.

The integration test must be opt-in and refuse to run unless all target URLs are loopback or explicitly marked disposable. It must use a unique service ID, isolated Deploy data directory, isolated Cloud state, isolated Tunnel/proxy state as applicable, and an isolated Docker network. It must never fall back to production URLs or credentials.

Cloud status must be derived from confirmed observations: successful Calendar `/health` probe means `healthy`; a non-2xx response, timeout, or invalid health payload means `degraded`; missed heartbeat expiry remains Cloud’s existing `offline` state. `/api/v1/status` and `/health` must agree on service identity and readiness. Registration payloads keep production defaults unchanged.

The Hosting check is a separate test path and must not create a Nexus-Deploy project. It verifies site ownership and serving after publish, then removes the disposable account/site.

## Test and acceptance criteria

- Nexus-Deploy project API persists and validates a relative build context, defaults existing projects to `.`, and passes that context to the build engine without permitting paths outside the cloned repository.
- Nexus-Calendar registration supports an isolated identity and route; Cloud health changes only after a successful, valid local health probe.
- A disposable integration run proves Deploy build, Cloud registration and health, production proxy exposure semantics, version change, and rollback to the first version.
- Negative lifecycle cases prove an unhealthy service is not reported healthy and a failed or interrupted deployment does not leave a false live status.
- The Hosting integration check publishes and fetches a fixture website using a disposable user/site and cleans up reliably.
- Existing Calendar, Cloud, Deploy, proxy, Tunnel (if touched), and Hosting test gates pass.
- No live service, public domain, production database, production Cloud registry, or public Hosting account is modified by verification.

## Out of scope

- Replacing the production manual deployment of Calendar with Nexus-Deploy.
- Switching public Calendar traffic to the separate Nexus-Tunnel app.
- Changing Hosting’s public onboarding, billing, domain, or federation behavior.
- Running a public production deployment or changing DNS.
