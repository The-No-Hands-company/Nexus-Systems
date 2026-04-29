# Nexus Ecosystem Connection Plan

## Scope baseline (current workspace)

The current active app set discovered under `apps/` is:

- Nexus
- Nexus-AI
- Nexus-Cloud
- Nexus-Computer
- Nexus-Deploy
- Nexus-Forge
- Nexus-Hosting
- Nexus-Network
- Nexus-Vault
- Nexusclaw
- Phantom

Each of these should support two operating patterns:

- Standalone mode (`docker compose up` or local dev command)
- Orchestrated mode (embedded in Nexus-Cloud via Systems API contracts)

## What is now canonical

Nexus-Cloud topology and app map now represent all current apps as embedded modules in Cloud orchestration terms.

Nexus-AI is modeled as a shared runtime consumed by:

- Nexus-Cloud
- Nexus
- Nexus-Computer
- Nexus-Forge
- Nexus-Deploy
- Nexus-Hosting
- Nexusclaw
- Nexus-Network (for routed AI transport)

## Required contract for every embedded app

Every app should expose and/or consume these minimum integration capabilities:

1. Tool registration
2. Health heartbeat
3. Upstream route declaration
4. Address/exposure/domain lifecycle
5. Service-to-service auth (bearer now, signed requests next)

## Phase rollout

### Phase 1 (now): Canonical topology alignment

- Complete: Cloud topology graph and ecosystem map document aligned to embedded-by-default model.
- Complete: AI cross-tool mesh expressed in canonical connection graph.

### Phase 2: Runtime registration hardening

- Add per-app startup registration call to `POST /api/v1/tools`.
- Add heartbeat loop to `POST /api/v1/tools/:toolId/heartbeat`.
- Include `upstreamUrl`, capability tags, and health state from each app.

### Phase 3: Exposure and routing integration

- Each app requests website/server addresses through `POST /api/v1/addresses`.
- Cloud-managed route table (`/api/v1/routes`) becomes the single source for proxy config.
- Domain verification flow is used for app-level custom domains.

### Phase 4: AI service mesh normalization

- Standardize AI invocation envelope across consuming apps.
- Add per-app AI policy profiles (allowed tools/models, guardrail levels, timeout budgets).
- Persist audit events for AI-driven actions in Cloud audit trail.

### Phase 5: Trust and identity unification

- Route all service credentials through Nexus-Vault-issued identities.
- Move from basic bearer patterns to signed service identities for internal calls.

## Immediate next implementation targets

1. Nexus-AI: add Cloud registration + heartbeat client and explicit capability tags.
2. Nexus-Deploy: wire AI-backed deployment assistant endpoint using Nexus-AI upstream.
3. Nexus-Hosting: consume Cloud route table directly for exposure/proxy sync.
4. Nexus-Computer: register edge nodes as tool instances through Systems API.

## Success checks

- `GET /api/v1/topology` shows all current apps and AI mesh links.
- `GET /api/v1/tools` includes all running embedded apps.
- `GET /api/v1/routes` returns active domain-to-upstream entries for exposed apps.
- `GET /api/v1/status` healthy tool count tracks real runtime state.
