# Next Batch MVP Contract Checklists

Generated: 2026-05-08
Scope: Recommended next batch (10 items)

## Cross-App Baseline

- [ ] README reflects real scope (no scaffold placeholder language)
- [ ] one runnable local dev command exists
- [ ] one health or status endpoint exists
- [ ] one documented API or integration contract exists
- [ ] one smoke or contract test exists
- [ ] short architecture note exists in app docs

## 1) Nexus-Monitor

Current maturity: mvp-complete

- [x] monitoring data model defined (service, node, heartbeat, alert)
- [x] initial ingest endpoint contract documented
- [x] /health endpoint contract documented
- [x] one smoke contract test planned and scoped
- [x] cloud registration compatibility documented

Gate output:
- monitor MVP contract draft v1 (+ runtime shell and smoke validation)

## 2) Nexus-Compliance

Current maturity: mvp-complete

Gating tracker:
- docs/NEXT_BATCH_ITEM2_NEXUS_COMPLIANCE_GATING.md

- [x] compliance event model defined
- [x] compliance status/report endpoint contract documented
- [x] /health endpoint contract documented
- [x] one smoke contract test planned and scoped
- [x] cloud registration compatibility documented

Gate output:
- compliance MVP contract draft v1 (+ runtime shell and smoke validation)

## 3) Nexus-AI-Hub

Current maturity: mvp-complete

Gating tracker:
- docs/NEXT_BATCH_ITEM3_NEXUS_AI_HUB_GATING.md

- [x] hub integration model defined (agents, tools, providers)
- [x] hub API or routing contract documented
- [x] /health endpoint contract documented
- [x] one smoke contract test planned and scoped
- [x] cloud registration compatibility documented

Gate output:
- AI-Hub MVP contract draft v1 (+ runtime shell and smoke validation)

## 4) Nexus-Files

Current maturity: mvp-complete

Gating tracker:
- docs/NEXT_BATCH_ITEM4_NEXUS_FILES_GATING.md

- [x] file metadata and storage contract defined
- [x] minimal file read/write/list API contract documented
- [x] /health endpoint contract documented
- [x] one smoke contract test planned and scoped
- [x] cloud registration compatibility documented

Gate output:
- files service MVP contract draft v1 (+ runtime shell and smoke validation)

## 5) Nexus-Search

Current maturity: mvp-complete

Gating tracker:
- docs/NEXT_BATCH_ITEM5_NEXUS_SEARCH_GATING.md

- [x] query and result schema defined
- [x] minimal search endpoint contract documented
- [x] /health endpoint contract documented
- [x] one smoke contract test planned and scoped
- [x] cloud registration compatibility documented

Gate output:
- search service MVP contract draft v1 (+ runtime shell and smoke validation)

## 6) Nexus-IDE

Current maturity: mvp-complete

Gating tracker:
- docs/NEXT_BATCH_ITEM6_NEXUS_IDE_GATING.md

- [x] IDE session model defined
- [x] minimal IDE API/integration contract documented
- [x] /health endpoint contract documented
- [x] one smoke contract test planned and scoped
- [x] cloud registration compatibility documented

Gate output:
- IDE MVP contract draft v1 (+ runtime shell and smoke validation)

## 7) Nexus-API

Current maturity: mvp-complete

Gating tracker:
- docs/NEXT_BATCH_ITEM7_NEXUS_API_GATING.md

- [x] API gateway/aggregation role defined
- [x] minimal routing/auth passthrough contract documented
- [x] /health endpoint contract documented
- [x] one smoke contract test planned and scoped
- [x] cloud registration compatibility documented

Gate output:
- Nexus-API MVP contract draft v1 (+ runtime shell and smoke validation)

## 8) Nexus-Testing

Current maturity: mvp-complete

Gating tracker:
- docs/NEXT_BATCH_ITEM8_NEXUS_TESTING_GATING.md

- [x] test job model defined
- [x] minimal test-run trigger/status contract documented
- [x] /health endpoint contract documented
- [x] one smoke contract test planned and scoped
- [x] cloud registration compatibility documented

Gate output:
- testing service MVP contract draft v1 (+ runtime shell and smoke validation)

## 9) Nexus-Forge

Current maturity: beyond-mvp

Alignment note:
- Nexus-Forge/docs/BATCH_ALIGNMENT_NOTE.md

- [x] confirm current health/status contracts
- [x] confirm cloud registration compatibility and payload shape
- [x] identify required contract deltas for this batch only
- [x] pin one interoperability smoke test with batch services

Gate output:
- forge batch-alignment contract note (Nexus-Forge/docs/BATCH_ALIGNMENT_NOTE.md)
- health delta applied: added `service: "nexus-forge"` to /health response

## 10) Nexus-Systems-API SDK and spec hardening

Current maturity: mvp-complete

- [x] pin v1 endpoint contract table
- [x] pin v1 DTO contract definitions
- [x] publish example payload set (registration, heartbeat, status, route, compliance)
- [x] update compatibility notes for batch services
- [x] validate contract tests remain stable

Gate output:
- pinned SDK/spec hardening report (Nexus-Systems-API/docs/README.md)
- batch service registry: 9 services pinned in src/contracts.ts
- 4/4 contract tests pass (bun test)

## Evidence Capture Format

- evidence: app/path + endpoint/test/command + date
