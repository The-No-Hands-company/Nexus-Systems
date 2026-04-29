# P0 Implementation Queue

Generated: 2026-04-27
Scope: First 10 P0 execution units in locked order
Source: Ecosystem phase rollout board and current apps workspace state

## Goal

This queue turns the P0 wave into an implementation sequence.

Each entry defines:

- why it is in this position
- the minimum viable contract required before moving on
- explicit outputs that unblock the next apps in the queue

## Priority Lock

This queue is locked for wave execution.

- Do not re-rank items during active implementation.
- If a blocker appears, open a short blocker note and continue with the next unblocked item.
- Only change ordering when one of these conditions is true:
	- a hard dependency was discovered that invalidates current order
	- a security risk requires immediate reprioritization
	- the founder explicitly overrides the queue

## Architecture Boundary Decision (Locked)

- `Nexus-Systems-API` is a Cloud-internal platform stream hosted inside `Nexus-Cloud`.
- `Nexus-Tunnel`, `Nexus-Guardian`, and `Nexus-Edge` are standalone apps that also integrate with `Nexus-Cloud`.
- The `apps/Nexus-Systems-API` directory is maintained as contract/spec/SDK docs, not a standalone runtime service.

## Execution Rule

Do not try to fully complete each app before starting the next one.

For this queue, "done" means the app has:

1. a runnable local dev entrypoint
2. a health/status surface
3. a minimal documented API or integration contract
4. Nexus-Cloud registration compatibility where relevant
5. at least one smoke test or contract test

## Locked Queue

## 1. Nexus-Cloud

Reason for position:
The rest of the ecosystem depends on Cloud as the control plane, registry, topology source, routing authority, and Systems API host.

Minimal MVP contract:

- Expose the Systems API, topology, route, status, and compliance endpoints
- Support tool registration, heartbeat, exposure, and route listing
- Persist registry state safely
- Provide a documented local dev startup path
- Pass existing contract and route tests

Must output before next app:

- stable local Systems API base URL
- registration contract for tools
- heartbeat contract for tools
- health/status contract for dependent services

## 2. Nexus-Auth

Reason for position:
Identity and service authentication become painful if delayed. Auth should land before large numbers of app APIs start drifting.

Minimal MVP contract:

- Local identity service with one seed admin/user flow
- Service-issued token or API-key validation path for internal apps
- Minimal user/session model
- Health endpoint and auth readiness endpoint
- Integration notes for Cloud and future apps

Must output before next app:

- token validation contract
- service auth contract for internal app calls
- user identity shape shared across ecosystem apps

## 3. Nexus-Vault

Reason for position:
Secrets and signing should exist before the ecosystem starts wiring app-to-app credentials, deploy tokens, and internal secrets by hand.

Minimal MVP contract:

- Secure local secret store abstraction
- Read/write API for service secrets in dev mode
- Token/signing placeholder flow for internal services
- Health endpoint and capability declaration
- Documentation for secret naming conventions

Must output before next app:

- secret retrieval contract for internal apps
- signing/key-management stub contract
- documented secret naming scheme

## 4. Nexus-Guardian

Reason for position:
Guardian is already conceptually central to policy and exposure decisions. It should become a first-class service before more public surfaces go live.

Minimal MVP contract:

- Decision model for approve, deny, suspend, quarantine
- API surface for evaluating and recording decisions
- Audit trail emission for each decision
- Health endpoint and policy readiness state
- Initial policy rules for exposures and sensitive actions

Must output before next app:

- decision contract reused by Cloud and Tunnel
- operator action API contract
- policy event format for audit/monitoring

## 5. Nexus-Tunnel

Reason for position:
Public exposure is a hard dependency for the broader ecosystem. Tunnel should become the sovereign exposure path before many apps request internet-facing routes.

Minimal MVP contract:

- Local tunnel/session manager stub
- Public URL issuance flow compatible with Nexus-Cloud
- Route registration/renewal handshake with Cloud
- Health endpoint and connection status endpoint
- Guardian-aware exposure checks

Must output before next app:

- public URL provisioning handshake
- tunnel session status contract
- route publication contract consumed by Cloud/Hosting

## 6. Nexus-Edge

Reason for position:
Edge orchestration sits close to Computer, Tunnel, and Network. Getting the minimal Edge contract in place early prevents later rewrites around device and locality assumptions.

Minimal MVP contract:

- Device registration model
- Heartbeat/status flow for edge nodes
- Minimal task dispatch placeholder
- Optional public exposure handoff to Tunnel
- Health endpoint and node inventory response

Must output before next app:

- device identity contract
- edge heartbeat contract
- edge task dispatch envelope

## 7. Nexus-Computer

Reason for position:
Computer is one of the major flagship apps and depends on Cloud, Auth, Vault, Edge, and Network conventions being at least minimally stable.

Minimal MVP contract:

- Local runtime shell with health endpoint
- Register with Cloud as a tool/service
- Basic remote task execution placeholder
- Identity-aware session model
- Minimal file/workspace awareness contract

Must output before next app:

- remote task execution API shape
- edge/device-aware session contract
- Computer capability declaration for AI and orchestration layers

## 8. Nexus-AI

Reason for position:
AI becomes much more useful once Cloud, Auth, Vault, and Computer are available. At this point it can plug into real runtime surfaces instead of mocked ones.

Minimal MVP contract:

- Agent runtime service with health endpoint
- Tool discovery via Systems API
- Minimal model routing or provider abstraction
- One internal workflow path that can call a registered tool
- Basic policy-aware execution guardrails

Must output before next app:

- tool invocation contract
- agent execution result schema
- model/provider abstraction contract

## 9. Nexus-Network

Reason for position:
Network should formalize connectivity, federation, and reachability after the control, auth, tunnel, edge, compute, and AI layers have minimal real traffic to connect.

Minimal MVP contract:

- Node/connectivity inventory view
- Reachability and peer status reporting
- Basic federation/routing metadata surface
- Health endpoint and network summary endpoint
- Integration with Cloud route and edge/tunnel state

Must output before next app:

- peer and connectivity contract
- routing metadata contract
- federation status shape for downstream apps

## 10. Nexus-Hosting

Reason for position:
Hosting should follow once sovereign public exposure, policy controls, and basic service identity are in place so hosted workloads inherit secure defaults.

Minimal MVP contract:

- Minimal site/app deployment surface for local dev
- Hosted workload registration with Cloud and Tunnel
- Basic certificate/domain workflow placeholder
- Health endpoint and hosted target inventory
- Compatibility with Guardian exposure policy checks

Must output before next wave:

- hosted workload registration contract
- publication contract aligned with Tunnel
- hosting capability declaration for later product apps

## Parallel Internal Stream: Nexus Systems API (Inside Cloud)

This stream runs in parallel while queue items 2 to 10 are executed.

Owner:

- `Nexus-Cloud` Systems API module (`src/systems-api/`)

Required outputs during P0:

- pinned v1 DTO and route contract documentation
- example payload set for registration, heartbeat, status, route, and compliance
- SDK/client skeleton in `apps/Nexus-Systems-API` docs scope
- compatibility notes for Auth, Vault, Guardian, Tunnel, and Edge integrations

## Cross-App MVP Standard

Every app in this queue should add the following before being considered wave-complete:

- `README.md` updated from scaffold placeholder to real scope
- one runnable dev command or script
- one `/health` or equivalent status endpoint
- one contract test or smoke test
- Nexus-Cloud registration if it is a service/app surface
- a short architecture note in `docs/`

## Recommended Next Queue After These 10

Once these ten reach MVP, the next best batch is:

1. Nexus-Monitor
2. Nexus-Compliance
3. Nexus-AI-Hub
4. Nexus-Files
5. Nexus-Search
6. Nexus-IDE
7. Nexus-API
8. Nexus-Testing
9. Nexus-Forge
10. Nexus-Systems-API SDK and spec hardening