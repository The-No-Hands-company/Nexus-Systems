# NIT Strategic Test Plan — Ecosystem Testing Framework Blueprint

**Document Status:** Strategic Analysis - Ready for Implementation  
**Date:** April 18, 2026  
**Scope:** 12 Nexus Systems projects + unified testing strategy via NIT

---

## 1. Current NIT Status (nit.manifest.json)

### Repos Currently Orchestrated (8 of 12)

| Repo | Type | Test Command | Status | Next Gap |
|------|------|--------------|--------|----------|
| Nexus | Rust | `cargo test` | ⚠️ Blocked (rust-toolchain) | Formal cloud contract + federation tests |
| Nexus-AI | Python | `python3 -m py_compile main.py` | ✅ Running | Add explicit cloud contract test module |
| Nexus-Cloud | Bun | `bun test src` | ✅ Running | Route manifest/Systems API sync |
| Nexus-Computer | Python | `python3 -m py_compile backend/main.py` | ✅ Running | Add cloud contract test module |
| Nexus-Deploy | Node | `npm run typecheck` | ✅ Running | Promote internal deploy surface to cloud contract |
| Nexus-Hosting | Node | `pnpm --dir artifacts/api-server test` | ✅ Running | Keep docs/OpenAPI cloud routes in sync |
| Nexus-Network | Node | `npx vitest run src/routes/cloud.test.ts` | ✅ Running | Full manifest-backed contract checks |
| Nexus-Vault | Node | `npx vitest run src/routes/cloud.test.ts` | ✅ Running | Import/export + collection/model coverage |

### Repos NOT in NIT Manifest (4 of 12)

| Repo | Type | Test Framework | Status | Priority |
|------|------|----------------|--------|----------|
| Nexusclaw | TypeScript | Vitest (`vitest run`) | ❌ Missing | HIGH — Multi-agent orchestration framework |
| Nexus-Forge | TypeScript/Bun | Bun Test (`bun test`) | ❌ Missing | HIGH — Version control + federation core |
| Nexus-Porter | TypeScript | None (CLI tool) | ❌ No tests | MEDIUM — Port scanning + network intelligence |
| Phantom | Rust (multi-crate) | Cargo (`cargo test`) | ❌ Missing | CRITICAL — Cryptographic protocol + FHE |

---

## 2. Test Infrastructure Summary by Language

### Rust Projects (2)
- **Nexus** — Cargo workspace, likely modular crypto/federation tests in crates (8 crates: api, common, db, gateway, voice, server, federation, desktop)
- **Phantom** — Cargo workspace, 9 crates with explicit tests for FHE, ZK, cryptography, discovery, routing

### TypeScript/Node Projects (8)
- **Vitest:** Nexusclaw, Nexus-Network, Nexus-Vault, (Nexus-Forge uses Bun Test but could add Vitest)
- **Jest:** Nexus-Cloud (strong test structure)
- **Bun Test:** Nexus-Forge, Nexus-Deploy (npm typecheck currently)
- **Manual:** Nexus-Hosting (pnpm test via api-server)

### Python Projects (2)
- **Nexus-AI** — pytest fixture ready (5 test files found: v1 contracts, tools, provider routing, integration, autonomy)
- **Nexus-Computer** — minimal testing (uses py_compile only)

---

## 3. Test Gaps Analysis (vs. ECOSYSTEM_TESTING_CATALOG.md)

### Tier 1 — Critical Gaps (Block Release)

#### 3.1 Federation & Cross-Instance Tests (L2-121 to L2-135)
- **Status:** Minimal
- **Gap:** No formal federation contract tests, split-brain resolution, gossip protocol validation
- **Repos Affected:** Nexus, Nexus-Forge (versioning), Nexus-Network (if federation-enabled)
- **Action:** Add federation adversarial test suite to Nexus (canon tests for protocol invariants)

#### 3.2 Cryptography Deep Assurance (L2-641 to L2-660)
- **Status:** Phantom has some, Nexus has none
- **Gap:** KAT tests, post-quantum validation, key rotation, side-channel resistance, constant-time verification
- **Repos Affected:** Phantom (core), Nexus (if federation uses crypto), Nexus-Vault (if crypto-backed)
- **Action:** Add crypto test suite to Phantom (already has scaffold), integrate into Nexus federation tests

#### 3.3 API Semantics & HTTP Contract (L2-309 to L2-326)
- **Status:** Partial (Nexus-Hosting has some)
- **Gap:** Cache-control, conditional requests, pagination determinism, content negotiation, idempotency-key behavior
- **Repos Affected:** All REST API repos (Nexus, Nexus-AI, Nexus-Cloud, Nexus-Deploy, Nexus-Hosting, Nexus-Network, Nexus-Vault)
- **Action:** Add OpenAPI-based contract tests (generate from OpenAPI spec, validate responses)

#### 3.4 Data Lifecycle & Archive Tests (L2-399 to L2-416)
- **Status:** None found
- **Gap:** Archive transitions, retrieval correctness, legal holds, retention, historical schema decoding
- **Repos Affected:** Nexus (message store), Nexus-Hosting (static archives), Nexus-Cloud (artifacts)
- **Action:** Add data lifecycle test harness

### Tier 2 — High-Value Gaps (High ROI)

#### 3.5 Browser/Runtime Edge Cases (L2-327 to L2-346)
- **Status:** Minimal visual regression
- **Gap:** Service worker lifecycle, offline, IndexedDB, localStorage, WebSocket through proxy, BroadcastChannel
- **Repos Affected:** Nexus (desktop), Nexus-Computer (if PWA), Nexus-Forge (if web UI), Nexus-Cloud (if browser-based)
- **Action:** Add Playwright E2E suite for browser edge cases

#### 3.6 AI Evaluation (L2-527 to L2-550)
- **Status:** Partial (Nexus-AI has some)
- **Gap:** Bias/fairness, hallucination rate, tool-call precision, memory contamination, drift traceability
- **Repos Affected:** Nexus-AI (core), Nexusclaw (multi-agent), Nexus-Computer (if agentic)
- **Action:** Add AI evaluation harness with benchmark suite

#### 3.7 Hardware/Device Variability (L2-363 to L2-380)
- **Status:** None found
- **Gap:** ARM/x86 parity, low-RAM degradation, thermal throttling, GPU fallback, high DPI scaling, Bluetooth, webcam
- **Repos Affected:** Nexus (desktop on multiple platforms), Nexus-Computer (if mobile), Nexus-Network (if hardware-aware)
- **Action:** Add cross-platform device test matrix

#### 3.8 Storage/Filesystem Durability (L2-381 to L2-398)
- **Status:** None found
- **Gap:** Atomic writes, fsync, permission errors, low-disk-space recovery, archive extraction safety, symlink traversal
- **Repos Affected:** Nexus (message store), Nexus-Hosting (static site storage), Nexus-Cloud (artifact storage)
- **Action:** Add storage durability test suite

---

## 4. Test File Inventory (Current State)

### Test Count by Repo
```
Nexusclaw:      3 .test.ts files    (secrets, orchestrator, artifacts)
Nexus-Cloud:   10 .test.ts files    (state, topology, routes, manifest, handlers, expose, deploy, registry, public-address, audit)
Nexus-Forge:    3 .test.ts files    (vcs, federation, api)
Nexus-Hosting: ~15 .test.ts files   (twoFactor, tokenScopes, retention, + more in api-server)
Phantom:        4 .rs test files    (multi_node_discovery, integration, e2e_routing, anonymous_routing)
Nexus-AI:       5 .py test files    (v1_contracts, tools, stub_batch, provider_routing, integration)
Nexus:          0 explicit test files (cargo test runs inline #[test] modules)
Nexus-Computer: 0 test files
Nexus-Deploy:   0 explicit test files
Nexus-Network:  0-1 test files
Nexus-Vault:    0-1 test files
Nexus-Porter:   0 test files
```

**Total:** ~40-50 test files across ecosystem  
**Target:** 200-300+ test files aligned with ECOSYSTEM_TESTING_CATALOG.md (720 L2 tests)

---

## 5. Strategic Test Implementation Plan (Priority Order)

### Phase 1: Fill NIT Manifest (2-3 weeks)

#### 1.1 Add Missing Repos to Manifest
**Priority:** CRITICAL

```json
{
  "name": "Nexusclaw",
  "path": "/home/workspace/Nexusclaw",
  "type": "bun",
  "testCommand": "vitest run",
  "requiredConfig": "ANTHROPIC_API_KEY",
  "cloudEndpoints": [
    "/api/claw/discovery",
    "/api/claw/orchestrate",
    "/api/claw/agent-state"
  ],
  "requiredEnv": ["ANTHROPIC_API_KEY"],
  "nextGap": "Add multi-agent orchestration contract tests",
  "blockedBy": "none",
  "env": {}
},
{
  "name": "Nexus-Forge",
  "path": "/home/workspace/Nexus-Forge",
  "type": "bun",
  "testCommand": "bun test tests/**/*.test.ts",
  "requiredConfig": "DATABASE_URL, GIT_REPO_ROOT",
  "cloudEndpoints": [
    "/.well-known/nexus-cloud",
    "/api/vcs/discovery",
    "/api/vcs/federation"
  ],
  "requiredEnv": ["DATABASE_URL"],
  "nextGap": "Add VCS federation and version upgrade contract tests",
  "blockedBy": "none",
  "env": {}
},
{
  "name": "Nexus-Porter",
  "path": "/home/workspace/Nexus-Porter",
  "type": "node",
  "testCommand": "npm test",
  "requiredConfig": "none",
  "cloudEndpoints": [
    "/api/porter/ports",
    "/api/porter/scan"
  ],
  "requiredEnv": [],
  "nextGap": "Add port scanning accuracy and network intelligence contract tests",
  "blockedBy": "none",
  "env": {}
},
{
  "name": "Phantom",
  "path": "/home/workspace/Phantom",
  "type": "rust",
  "testCommand": "cargo test --all",
  "requiredConfig": "none",
  "cloudEndpoints": [
    "/protocol/anonymous-routing",
    "/protocol/fhe-evaluation"
  ],
  "requiredEnv": [],
  "nextGap": "Add formal verification and cryptographic deep assurance tests",
  "blockedBy": "none",
  "env": {}
}
```

#### 1.2 Upgrade Existing Manifest Entries
- **Nexus:** Unblock rust-toolchain or document rustup update requirement
- **Nexus-Deploy:** Upgrade from `npm run typecheck` to real test suite (`bun test` or vitest)
- **Nexus-Network:** Expand from single cloud.test.ts to full suite
- **Nexus-Vault:** Add import/export + collection tests

### Phase 2: Add Contract/API Tests (3-4 weeks)

#### 2.1 OpenAPI-Based Contract Tests (All REST API Repos)
**Coverage:** L2-309 to L2-326 (API Semantics)

Add to each repo's test suite:
- HTTP method correctness (GET, POST, PUT, DELETE, PATCH semantics)
- Cache-Control header validation
- Conditional request handling (If-Match, If-None-Match, 304 Not Modified)
- Range request support
- Content negotiation (Accept headers)
- Pagination stability (sort determinism, offset/limit)
- Error response codes
- Idempotency-Key behavior

**Implementation:**
- Generate contract tests from OpenAPI spec using tool like `ApiSpec` or `Dredd`
- Or manually curate test suite for each endpoint

**Repos:** Nexus, Nexus-AI, Nexus-Cloud, Nexus-Deploy, Nexus-Hosting, Nexus-Network, Nexus-Vault

### Phase 3: Add Cryptographic & Federation Tests (4-6 weeks)

#### 3.1 Cryptography Deep Assurance Suite (L2-641 to L2-660)
**Repos:** Phantom (core), Nexus-Vault (if crypto-backed)

Tests:
- KAT (Known-Answer Tests) for all algorithms (Kyber, Dilithium, SPHINCS+, TFHE, Halo2)
- Post-quantum correctness
- Key generation randomness
- Key rotation without data loss
- Signature malleability resistance
- Replay attack prevention
- Constant-time operations
- Side-channel review

**Implementation:**
- Add to Phantom::phantom-crypto crate
- Create test vectors for each algorithm
- Benchmark constant-time operations

#### 3.2 Federation Adversarial Suite (L2-621 to L2-640)
**Repos:** Nexus (federation core), Nexus-Forge (VCS federation)

Tests:
- Identity boundary enforcement across instances
- Message signature verification (replay prevention)
- Split-brain resolution
- Eventual consistency convergence
- Gossip protocol propagation timing
- Version upgrade compatibility
- Byzantine conditions (slow/malicious peers)

**Implementation:**
- Multi-instance test harness with fault injection
- Network partition simulation
- Clock skew injection

### Phase 4: Add Domain-Specific Tests (4-8 weeks)

#### 4.1 AI Evaluation & Drift (L2-527 to L2-550)
**Repos:** Nexus-AI, Nexusclaw

Tests:
- Task success benchmarks (tool-call precision)
- Hallucination rate on known-answer queries
- Bias/fairness across user groups
- Memory contamination between sessions
- Provider routing correctness
- Fallback behavior under rate-limiting
- Response drift over time

#### 4.2 Browser/Runtime Edge Cases (L2-327 to L2-346)
**Repos:** Nexus (if desktop has browser), Nexus-Cloud (if web UI), Nexus-Forge (if web UI)

Tests:
- Service worker lifecycle (install, activate, fetch)
- Offline behavior (IndexedDB, localStorage)
- WebSocket through proxy
- BroadcastChannel cross-tab coordination
- Third-party cookie behavior
- SameSite variance
- Tab suspension recovery

**Implementation:**
- Playwright E2E suite
- Headless browser automation

#### 4.3 Storage/Filesystem Durability (L2-381 to L2-398)
**Repos:** Nexus, Nexus-Hosting, Nexus-Cloud

Tests:
- Atomic writes verification
- fsync/durability guarantees
- Permission error handling
- Low-disk-space recovery
- Archive extraction safety (no path traversal)
- Symlink security
- Cross-filesystem behavior

#### 4.4 Data Lifecycle & Archive (L2-399 to L2-416)
**Repos:** Nexus, Nexus-Hosting, Nexus-Cloud

Tests:
- Archive transition correctness
- Retrieval from cold storage
- Legal hold enforcement
- Retention policy compliance
- Historical schema decoding (reading old data formats)
- Tombstone propagation

#### 4.5 Hardware/Device Variability (L2-363 to L2-380)
**Repos:** Nexus (desktop), Nexus-Computer (if multi-platform)

Tests:
- ARM/x86 parity (cross-compile and test)
- Low-RAM degradation modes
- Thermal throttling behavior
- GPU fallback (if WASM/Metal used)
- High DPI scaling
- Bluetooth connectivity
- Webcam enumeration

---

## 6. Proposed NIT Architecture Enhancements

### 6.1 Add Test Categories to Manifest

```json
{
  "name": "Nexus",
  "testCategories": {
    "unit": { "command": "cargo test --lib", "weight": 0.3 },
    "integration": { "command": "cargo test --test '*'", "weight": 0.3 },
    "federation": { "command": "cargo test federation --", "weight": 0.2 },
    "e2e": { "command": "cargo test --ignored", "weight": 0.2 }
  },
  "minimalBaseline": ["unit", "integration"],
  "releaseGate": ["unit", "integration", "federation"]
}
```

### 6.2 Add Coverage Reporting

```json
{
  "name": "Nexus-AI",
  "coverage": {
    "target": 0.8,
    "exclude": ["docs", "examples"],
    "report": "coverage/lcov.info"
  }
}
```

### 6.3 Add Contract Validation

```json
{
  "name": "Nexus-Cloud",
  "contracts": {
    "openapi": "lib/api-spec/openapi.yaml",
    "validateResponses": true,
    "validateRequests": true
  }
}
```

### 6.4 Add Environment Validation

```json
{
  "name": "Nexus-Hosting",
  "environmentValidation": {
    "DATABASE_URL": { "regex": "^postgresql://", "required": true },
    "JWT_SECRET": { "minLength": 32, "required": true },
    "PUBLIC_DOMAIN": { "regex": "^https?://", "required": true }
  }
}
```

---

## 7. Implementation Roadmap (12 Weeks)

### Week 1-2: NIT Manifest Completion & Setup
- [ ] Add Nexusclaw, Nexus-Forge, Nexus-Porter, Phantom to manifest
- [ ] Fix Nexus rust-toolchain block
- [ ] Create NIT test runner enhancements (categories, coverage, contracts)
- [ ] Document test execution in README

### Week 3-4: Contract/API Tests
- [ ] Generate OpenAPI contract tests for all 7 REST APIs
- [ ] Add to respective test suites (Jest/Vitest/Bun)
- [ ] Integrate into NIT manifest validation

### Week 5-8: Cryptography & Federation Tests
- [ ] Phantom: Expand crypto test suite (KAT, post-quantum, constant-time)
- [ ] Nexus: Add federation adversarial suite (split-brain, byzantine)
- [ ] Nexus-Forge: Add VCS federation contract tests
- [ ] Integrate into NIT

### Week 9-10: Domain-Specific Tests
- [ ] Nexus-AI: Add AI evaluation + bias/fairness harness
- [ ] Nexusclaw: Add multi-agent orchestration tests
- [ ] Nexus-Cloud: Add browser edge case suite (if applicable)

### Week 11-12: Documentation & Hardening
- [ ] Document all test suites in ECOSYSTEM_TESTING_CATALOG.md
- [ ] Create per-repo test runbooks
- [ ] Add CI gate configuration (map to release gates from section 24 of catalog)
- [ ] Perform dry-run of full NIT suite across all 12 repos

---

## 8. Success Criteria

### Completion Metrics
- [ ] 12/12 repos in NIT manifest with passing tests
- [ ] 200+ test files aligned with ECOSYSTEM_TESTING_CATALOG.md
- [ ] 70%+ coverage of L2-001 through L2-720 test categories
- [ ] Full NIT run completes in < 10 minutes
- [ ] All critical gaps (Tier 1) addressed

### Quality Targets
- [ ] Federation tests verify protocol invariants
- [ ] Crypto tests validate post-quantum correctness
- [ ] API tests validate OpenAPI contract parity
- [ ] No flaky tests (100% pass rate on 10 consecutive runs)

---

## 9. Quick Wins (This Week)

### 9.1 Add 4 Missing Repos to Manifest (30 min)
**Files:** Update `nit.manifest.json`  
**Tests:** Minimal - just add entry with existing test command  
**Impact:** Full ecosystem visibility in one NIT run

### 9.2 Create Nexus-Porter Test Suite (2 hours)
**Files:** Create `Nexus-Porter/src/__tests__/scanner.test.ts`  
**Tests:** Port scanning accuracy, network enumeration edge cases  
**Impact:** Complete coverage of network intelligence tool

### 9.3 Create Federation Contract Test Template (3 hours)
**Files:** Create `Nexus/tests/federation-contract.rs`  
**Tests:** Message signature verification, split-brain resolution, gossip propagation  
**Impact:** Foundation for all federation tests across ecosystem

### 9.4 Create API Contract Test Generator (4 hours)
**Files:** Create `Nit/src/generators/openapi-contracts.ts`  
**Tests:** Validate all REST endpoints against OpenAPI spec  
**Impact:** Scalable contract testing for 7+ REST APIs

---

## 10. Open Questions & Dependencies

1. **Nexus Rust Toolchain:** Can we update the CI environment to support edition 2024 or should we downgrade?
2. **Nexus-Porter Tests:** Should we use Vitest or native Node.js test runner?
3. **Phantom FHE Benchmarks:** Should benchmark performance be part of test suite or separate?
4. **Cross-Repo Test Isolation:** Should NIT tests share a Docker Compose stack or run in isolation?
5. **Coverage Aggregation:** Should we aggregate coverage reports from all 12 repos into a single dashboard?

---

## 11. Appendix: Test Categories to ECOSYSTEM_TESTING_CATALOG.md Mapping

### L2 Tests Already Partially Covered

| Catalog Section | L2 Range | Repos with Partial | Gap |
|---|---|---|---|
| Correctness | L2-001–L2-025 | All | Missing: Edge cases, race conditions, N+1 queries |
| Security | L2-026–L2-055 | Nexus-Hosting, Nexus-AI | Missing: SAST, SCA, secrets scanning |
| Cryptography | L2-056–L2-070 | Phantom | Missing: KAT, side-channel review |
| AI Safety | L2-071–L2-090 | Nexus-AI, Nexusclaw | Missing: Bias, drift, hallucination measurement |
| Federation | L2-121–L2-135 | Nexus-Forge (partial) | Missing: Signature verification, split-brain, gossip |
| API Semantics | L2-309–L2-326 | Nexus-Hosting | Missing: All other REST repos, contract generation |

### L2 Tests NOT YET COVERED (Priority Add)

| Catalog Section | L2 Range | Why Critical | Repo |
|---|---|---|---|
| Formal Verification | L2-293–L2-308 | Protocol correctness | Phantom, Nexus |
| Storage/Durability | L2-381–L2-398 | Data integrity | Nexus, Nexus-Hosting |
| Data Lifecycle | L2-399–L2-416 | Retention/Archive | Nexus, Nexus-Hosting |
| Browser Edge Cases | L2-327–L2-346 | Client reliability | Nexus (desktop) |
| Hardware Variability | L2-363–L2-380 | Cross-platform | Nexus (desktop) |
| Notifications | L2-433–L2-450 | User engagement | Nexus, Nexus-Computer |
| Billing/Metering | L2-487–L2-506 | Financial accuracy | Nexus-Hosting (if SaaS) |
| CLI/Automation | L2-569–L2-584 | Operator UX | Nexus-Porter, Nexus-Deploy |

---

**End of Document**

---

**Next Action:** User approval to proceed with Phase 1 (Update NIT manifest + add 4 missing repos).
