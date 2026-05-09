# P0 MVP Contract Checklists (Per App)

Generated: 2026-05-08
Scope: Locked queue apps 1 to 10
Intent: enforce queue outputs without redoing mature apps that already exceed MVP

How to use:
- mark only contract outputs required by the locked queue
- if an app is already beyond MVP, check only alignment items and record evidence path
- do not expand scope to full production hardening in this pass

## Cross-App Baseline (applies to every app)

- [ ] README reflects real scope (not scaffold placeholder)
- [ ] one runnable local dev command exists
- [ ] one health or status endpoint exists
- [ ] one smoke or contract test exists
- [ ] cloud registration compatibility exists where relevant
- [ ] short architecture note exists in app docs

## 1) Nexus-Cloud

Current maturity: in-progress

Queue-specific contract checklist:
- [ ] Systems API endpoint set exposed
- [ ] topology endpoint exposed
- [ ] route endpoint exposed
- [ ] status endpoint exposed
- [ ] compliance endpoint exposed
- [ ] tool registration contract stable
- [ ] heartbeat contract stable
- [ ] exposure + route listing contract stable
- [ ] registry persistence is safe across restart
- [ ] local startup path documented and reproducible
- [ ] existing contract/route tests pass

Must output before next app:
- [ ] stable local Systems API base URL
- [ ] tool registration contract
- [ ] tool heartbeat contract
- [ ] dependent service health/status contract

## 2) Nexus-Auth

Current maturity: in-progress

Queue-specific contract checklist:
- [ ] seed admin/user flow works locally
- [ ] service token or API-key validation path works
- [ ] minimal user/session model defined
- [ ] health endpoint exists
- [ ] auth readiness endpoint exists
- [ ] cloud/future app integration notes present

Must output before next app:
- [ ] token validation contract
- [ ] internal service auth contract
- [ ] shared user identity shape

## 3) Nexus-Vault

Current maturity: beyond-mvp

Queue-specific contract checklist:
- [ ] secure secret store abstraction documented for internal apps
- [ ] dev-mode secret read/write API confirmed
- [ ] signing or key-management placeholder flow documented
- [ ] health endpoint exists
- [ ] capability declaration is published
- [ ] secret naming convention documented

Must output before next app:
- [ ] secret retrieval contract
- [ ] signing/key-management stub contract
- [ ] documented secret naming scheme

## 4) Nexus-Guardian

Current maturity: not-started

Queue-specific contract checklist:
- [ ] decision model includes approve/deny/suspend/quarantine
- [ ] evaluation API exists
- [ ] decision recording API exists
- [ ] audit trail event emitted per decision
- [ ] health endpoint exists
- [ ] policy readiness state exists
- [ ] initial policies cover exposure and sensitive actions

Must output before next app:
- [ ] reusable decision contract for Cloud and Tunnel
- [ ] operator action API contract
- [ ] policy event format for audit/monitoring

## 5) Nexus-Tunnel

Current maturity: not-started

Queue-specific contract checklist:
- [ ] local tunnel/session manager stub exists
- [ ] public URL issuance flow defined
- [ ] cloud route registration/renewal handshake works
- [ ] health endpoint exists
- [ ] connection status endpoint exists
- [ ] guardian-aware exposure check integrated

Must output before next app:
- [ ] public URL provisioning handshake
- [ ] tunnel session status contract
- [ ] route publication contract for Cloud/Hosting

## 6) Nexus-Edge

Current maturity: not-started

Queue-specific contract checklist:
- [ ] device registration model defined
- [ ] node heartbeat/status flow defined
- [ ] minimal task dispatch placeholder exists
- [ ] optional tunnel handoff contract exists
- [ ] health endpoint exists
- [ ] node inventory response exists

Must output before next app:
- [ ] device identity contract
- [ ] edge heartbeat contract
- [ ] edge dispatch envelope

## 7) Nexus-Computer

Current maturity: beyond-mvp

Queue-specific contract checklist:
- [ ] local runtime shell entrypoint documented
- [ ] health endpoint confirmed
- [ ] cloud registration as tool/service confirmed
- [ ] remote task execution placeholder defined
- [ ] identity-aware session model defined
- [ ] file/workspace awareness contract documented

Must output before next app:
- [ ] remote task execution API shape
- [ ] edge/device-aware session contract
- [ ] capability declaration for AI/orchestration

## 8) Nexus-AI

Current maturity: beyond-mvp

Queue-specific contract checklist:
- [ ] agent runtime service entrypoint documented
- [ ] health endpoint confirmed
- [ ] Systems API tool discovery integrated
- [ ] model/provider abstraction documented
- [ ] at least one internal workflow calls a registered tool
- [ ] policy-aware execution guardrails active

Must output before next app:
- [ ] tool invocation contract
- [ ] agent execution result schema
- [ ] model/provider abstraction contract

## 9) Nexus-Network

Current maturity: in-progress

Queue-specific contract checklist:
- [ ] node/connectivity inventory view exists
- [ ] reachability and peer status report exists
- [ ] federation/routing metadata surface exists
- [ ] health endpoint exists
- [ ] network summary endpoint exists
- [ ] integrates with cloud route + edge/tunnel state

Must output before next app:
- [ ] peer/connectivity contract
- [ ] routing metadata contract
- [ ] federation status shape

## 10) Nexus-Hosting

Current maturity: beyond-mvp

Queue-specific contract checklist:
- [ ] local dev deployment surface exists
- [ ] workload registration with cloud works
- [ ] tunnel publication handshake works
- [ ] certificate/domain workflow placeholder documented
- [ ] health endpoint exists
- [ ] hosted target inventory endpoint exists
- [ ] guardian exposure policy compatibility exists

Must output before next wave:
- [ ] hosted workload registration contract
- [ ] publication contract aligned with tunnel
- [ ] hosting capability declaration

## Evidence Capture (per checked item)

Use this one-line format next to each completed checkbox in reviews:

- evidence: app/path + endpoint/test/command + date
