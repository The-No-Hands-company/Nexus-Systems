# Nexusclaw Massive Feature Backlog

Last updated: 2026-04-19

## Intent

Nexusclaw is positioned as an everything-claw platform: a Nexus-native, modular, local-first system that can absorb and orchestrate the best capabilities from the AI agent + claw ecosystem.

This document is the working mega backlog generated from:
- Existing Nexusclaw docs and architecture
- Seed repositories:
  - https://github.com/czl9707/build-your-own-openclaw
  - https://github.com/open-jarvis/OpenJarvis
  - https://github.com/aaif-goose/goose
  - https://github.com/Gitlawb/openclaude
- Adjacent ecosystem patterns discovered through those repos (MCP servers, extension registries, orchestration, memory systems, safety models, plugin ecosystems)

## How To Use This Backlog

- Treat this as a feature universe, not a commit checklist.
- Ship by waves: Foundation, Scale, Ecosystem, Platform.
- Every feature should map to one or more modules in src/.
- Do not merge features without acceptance criteria, owner, and safety impact.

---

## 1) Agent Runtime and Cognition

A-001 Multi-agent role templates (researcher, coder, reviewer, planner).
A-002 Agent personality packs with profile switching.
A-003 Dynamic agent spawning by task complexity.
A-004 Delegation graph with parent-child traceability.
A-005 Agent run modes: fast, balanced, deep.
A-006 Tool budget caps per agent run.
A-007 Token budget forecasting before execution.
A-008 Agent self-checkpointing for long tasks.
A-009 Recovery mode after tool/error loops.
A-010 Agent handoff protocol between specialist agents.
A-011 Long-horizon monitor agents (persistent loop).
A-012 Recursive sub-planning for complex goals.
A-013 Pluggable reasoning styles (ReAct, plan-act, code-act).
A-014 Agent capability declarations and negotiation.
A-015 Conflict resolution between concurrent agents.

## 2) Tasking, Planning, and Execution (GSD++)

T-001 Hierarchical task trees with dependencies.
T-002 Blocked-by and unblocks relationships.
T-003 Critical path estimation and scheduling.
T-004 Task retries with strategy mutation.
T-005 Human approval gates at milestone boundaries.
T-006 Cost estimate before execution.
T-007 Effort estimate and ETA scoring.
T-008 Deliverable contracts per task.
T-009 Auto-generated test plans per implementation task.
T-010 Auto-generated rollback plan per risky task.
T-011 Task quality score (completeness, correctness, safety).
T-012 Batch task execution windows (night mode).
T-013 Task labels/tags and saved filters.
T-014 Task templates and reusable playbooks.
T-015 Cross-project task federation.

## 3) Memory and Knowledge Systems

M-001 Memory policies by scope: user, project, org.
M-002 Hybrid retrieval (keyword + vector + recency).
M-003 Memory confidence scoring and decay.
M-004 Contradiction detection across memory entries.
M-005 Memory provenance with source links.
M-006 Semantic memory graph of entities and relations.
M-007 Memory snapshots and rollback.
M-008 Time-aware memory (valid from/to).
M-009 Decision log memory extraction.
M-010 Learned preference profiling from sessions.
M-011 Session summarization tiers (short, medium, deep).
M-012 Memory compaction with quality checks.
M-013 Project memory packs export/import.
M-014 Team-shared memory spaces with ACLs.
M-015 Auto-memory hygiene and dedup jobs.

## 4) Tools and Action Layer

X-001 Tool capability registry with rich metadata.
X-002 Tool risk scoring and required confirmation level.
X-003 Tool execution simulation mode.
X-004 Tool timeouts, retries, and backoff policies.
X-005 Structured tool output contracts.
X-006 Tool health checks and uptime monitoring.
X-007 Tool dependency graph and conflict detection.
X-008 Tool sandbox profiles (read-only, isolated, elevated).
X-009 Tool macros and compound actions.
X-010 Tool runbooks for common workflows.
X-011 Native git intelligence toolset expansion.
X-012 Native package manager toolset (npm/pip/cargo/etc).
X-013 Native deployment toolset (docker/k8s/terraform).
X-014 Browser automation suite with visual assertions.
X-015 Data wrangling tools (csv/json/parquet/sql).

## 5) MCP and External Integrations

P-001 MCP client manager for stdio/http/sse.
P-002 MCP server capability cache and discovery.
P-003 MCP resource and prompt browsing UI.
P-004 MCP tool trust policy and signatures.
P-005 MCP auth profiles (apikey/oauth/token vault).
P-006 MCP server performance telemetry.
P-007 MCP server marketplace connector.
P-008 Auto-install recipes for popular MCP servers.
P-009 MCP extension testing harness.
P-010 MCP sampling support for tool-side model calls.
P-011 Multi-tenant MCP server routing.
P-012 Remote extension hot-reload.
P-013 Extension bundles by domain (devops, research, finance).
P-014 Integration policy packs (enterprise-safe defaults).
P-015 Protocol bridges (MCP <-> internal adapters).

## 6) Plugin and Skill Ecosystem

S-001 Plugin lifecycle hooks (install, load, update, retire).
S-002 Plugin trust levels (core, verified, community, untrusted).
S-003 Plugin permission manifests with review workflow.
S-004 Plugin dependency solver and conflict reports.
S-005 Skill packs with semantic versioning.
S-006 Skill benchmark harness (quality/cost/latency).
S-007 Skill A/B testing and rollout percentages.
S-008 Skill marketplace sync and metadata cache.
S-009 Private registry support for internal teams.
S-010 Skill linting and validation CLI.
S-011 Skill execution traces and explainability logs.
S-012 Prompt asset management for skills.
S-013 Auto-suggest skills by context.
S-014 Skill deprecation and migration assistant.
S-015 One-click import from curated ecosystems.

## 7) Safety, Policy, and Governance

G-001 Policy engine with layered rules (global/project/agent).
G-002 Fine-grained path, network, and command controls.
G-003 Data exfiltration guards and redaction pipeline.
G-004 Secret scanning before any outbound action.
G-005 Human-in-the-loop policy exceptions.
G-006 Rule simulation and policy dry-run reports.
G-007 Safety incident timeline with full forensics.
G-008 Auto-quarantine for risky plugins/tools.
G-009 Organization policy templates (strict/balanced/dev).
G-010 Risk-aware model routing (safe model fallback).
G-011 Explain-why-blocked responses for usability.
G-012 Compliance profiles (SOC2, ISO-like control mapping).
G-013 Legal/PII-aware memory retention rules.
G-014 Signed audit records and tamper evidence.
G-015 Break-glass mode with mandatory audit trail.

## 8) Security and Secrets

Q-001 Secrets vault abstraction with pluggable backends.
Q-002 Key rotation policies and reminders.
Q-003 Runtime secret lease tokens.
Q-004 Encrypted local memory and config snapshots.
Q-005 Agent isolation with per-agent credentials.
Q-006 Ephemeral credential brokers for tools.
Q-007 Supply-chain scanner for plugins/extensions.
Q-008 Binary/tool checksum verification.
Q-009 Secure remote execution channels.
Q-010 Zero-trust network policy for tool egress.

## 9) Data, Artifacts, and Knowledge Products

D-001 Artifact system with typed outputs.
D-002 Versioned artifact lineage graph.
D-003 Artifact diff and merge operations.
D-004 Artifact export packs (pdf/docx/md/json).
D-005 Artifact quality evaluators by type.
D-006 Citation engine for research outputs.
D-007 Dataset manager for agent experiments.
D-008 Prompt/result dataset labeling workflow.
D-009 Reproducibility bundle per run.
D-010 Artifact retention and archive policies.

## 10) UX, CLI, and Dashboard

U-001 Multi-pane operations cockpit (agents/tasks/memory/tools).
U-002 Live execution timeline with event streaming.
U-003 Trace explorer with tool call drill-down.
U-004 Explainability panel (why this action happened).
U-005 Workspace profiles (coding, research, ops, writing).
U-006 One-command onboarding wizard.
U-007 Interactive slash command discovery.
U-008 Guided recovery flows on failure.
U-009 Accessible terminal UX modes.
U-010 Mobile-friendly dashboard views.
U-011 Agent launch wizard templates.
U-012 Power-user keyboard workflows.
U-013 Session replay and share links.
U-014 Multi-session compare mode.
U-015 Theme/plugin UI extension points.

## 11) Collaboration, Teams, and Org Features

C-001 Shared agent directories and team defaults.
C-002 Team inbox with routing rules.
C-003 Multi-user session collaboration.
C-004 Role-based access control across actions.
C-005 Approval workflows with assigned approvers.
C-006 Shared memory namespaces by team.
C-007 Team analytics dashboards.
C-008 Agent ownership and on-call assignments.
C-009 Conflict and lock management for edits.
C-010 Commenting and review threads on tasks.

## 12) Automation, Scheduling, and Operations

O-001 Advanced scheduler (cron + event triggers).
O-002 Trigger graph (webhook, file change, queue, timer).
O-003 Nightly maintenance workflows.
O-004 Auto-healing jobs (restart/recover/retry).
O-005 Background workers with queue priorities.
O-006 Workload throttling and quotas.
O-007 SLA-aware task orchestration.
O-008 Canary runs for automation recipes.
O-009 Batch execution windows with guardrails.
O-010 Multi-stage deployment workflows.

## 13) Observability, Telemetry, and Eval

E-001 Unified event bus for runtime telemetry.
E-002 Structured logs with correlation IDs.
E-003 Cost/latency/token dashboards.
E-004 Tool success/failure heatmaps.
E-005 Agent quality scoring over time.
E-006 Benchmark suites by scenario.
E-007 Regression detector for behavior drift.
E-008 Golden task test bank.
E-009 Replay runner for incident analysis.
E-010 Alerting integrations (Slack, email, webhook).
E-011 Experiment tracking for routing/model policies.
E-012 User feedback loop to ranking/reward signals.

## 14) Model Layer and Routing Intelligence

R-001 Policy-driven model selection matrix.
R-002 Dynamic fallback chains by domain.
R-003 Cost caps with graceful degradation.
R-004 Latency-aware routing during peak load.
R-005 Local/cloud hybrid inference arbitration.
R-006 Specialized model lanes (code, vision, research).
R-007 Prompt caching and response memoization.
R-008 Prompt optimization and auto-tuning loops.
R-009 Response validator model (judge layer).
R-010 Ensemble voting for critical outputs.

## 15) Developer Experience and SDK

V-001 Public SDK for programmatic orchestration.
V-002 Typed client libraries (TS/Python first).
V-003 Local dev sandbox bootstrap command.
V-004 Recipe/package scaffolding CLI.
V-005 Integration test harness for plugins.
V-006 Snapshot test utilities for prompts.
V-007 Mock tool servers for CI.
V-008 Code generation for plugin manifests.
V-009 Lint rules for safe tool usage.
V-010 API contract testing suite.

## 16) Cloud and Distributed Runtime

H-001 Cloud registration and discovery v2.
H-002 Distributed worker pool orchestration.
H-003 Remote execution targets by capability.
H-004 Work stealing for queued tasks.
H-005 Regional failover strategy.
H-006 Resource-aware scheduling (CPU/RAM/GPU).
H-007 Multi-tenant workspace isolation.
H-008 Burst scaling for heavy automation jobs.
H-009 Optional hosted control plane.
H-010 Hybrid local-first with cloud acceleration.

## 17) Marketplace and Economy Layer

K-001 Marketplace search, ranking, and trust badges.
K-002 Publisher verification workflow.
K-003 Package quality scorecards.
K-004 Dependency vulnerability badges.
K-005 Usage analytics for creators.
K-006 Revenue/share-ready metadata rails.
K-007 Private/internal marketplace channels.
K-008 Curated collections by domain.
K-009 Auto-update policy controls.
K-010 Install impact previews before enable.

## 18) Research and Frontier Features

F-001 Knowledge graph reasoning assistants.
F-002 Multi-modal memory (image/audio/video notes).
F-003 Synthetic data generation for skill training.
F-004 Self-reflection loops with bounded autonomy.
F-005 Program synthesis agents for tool composition.
F-006 Causal memory extraction from long sessions.
F-007 Continual learning from approved traces.
F-008 Agent swarm strategies for large objectives.
F-009 Long-context compression research tracks.
F-010 Policy-learning for safer autonomy.

## 19) Vertical Packs (Optional Product Lines)

Z-001 Software engineering pack.
Z-002 Deep research and citation pack.
Z-003 Marketing/SEO operations pack.
Z-004 Customer support automation pack.
Z-005 Data analytics and BI pack.
Z-006 DevOps and SRE automation pack.
Z-007 Security operations pack.
Z-008 Content production pipeline pack.
Z-009 E-commerce operations pack.
Z-010 Enterprise governance pack.

## 20) Non-Functional Mega Requirements

N-001 Deterministic replay for critical workflows.
N-002 Graceful degradation under provider outages.
N-003 Large-workspace performance guarantees.
N-004 Backpressure and queue resilience.
N-005 Backward-compatible plugin APIs.
N-006 Versioned contracts for all public surfaces.
N-007 Data portability and migration tooling.
N-008 Install footprint and startup-time budgets.
N-009 Disaster recovery and backup strategy.
N-010 End-to-end security posture reviews.

---

## Suggested Rollout Waves

Wave 1 (Now):
- A, T, M, X, G foundational slices
- Ship safety-first, memory-first, observability-first

Wave 2 (Scale):
- P, S, O, E, R
- Turn Nexusclaw into extension and orchestration powerhouse

Wave 3 (Platform):
- C, H, K, V
- Team and ecosystem gravity

Wave 4 (Frontier):
- F and advanced Z packs
- High-risk/high-reward innovation tracks

---

## Immediate Top 25 Candidate Features (First Implementation Queue)

1. A-001 Multi-agent role templates
2. A-003 Dynamic agent spawning
3. T-001 Hierarchical task trees
4. T-005 Human approval milestones
5. M-002 Hybrid retrieval
6. M-005 Memory provenance
7. M-009 Decision log extraction
8. X-002 Tool risk scoring
9. X-008 Tool sandbox profiles
10. P-001 MCP client manager
11. P-003 MCP resource/prompt browser
12. S-003 Plugin permission manifests
13. S-010 Skill linting CLI
14. G-001 Layered policy engine
15. G-004 Secret scanning before outbound
16. Q-001 Secrets vault abstraction
17. D-001 Typed artifact system
18. U-002 Live execution timeline
19. U-003 Trace explorer
20. O-001 Advanced scheduler
21. E-003 Cost/latency/token dashboards
22. E-008 Golden task test bank
23. R-001 Policy-driven model routing
24. V-001 Public SDK surface
25. H-003 Remote execution targets

---

## Notes

- This backlog intentionally over-shoots normal scope.
- You can trim to a practical roadmap by assigning each feature a score:
  - Impact (1-5)
  - Effort (1-5)
  - Risk (1-5)
  - Strategic fit (1-5)
- Start by implementing the highest Impact + Strategic fit with manageable Effort and acceptable Risk.

---

## Deep Dive v2: Ecosystem Coverage and Backlog Deltas

This section captures deltas from a wider scan across major agent ecosystems, MCP protocol/server repos, browser automation MCPs, coding-agent workflows, and memory-specific stacks.

### Coverage Snapshot

Primary ecosystems reviewed in deep dive v2:
- microsoft/autogen
- crewAIInc/crewAI
- langchain-ai/langgraph
- langchain-ai/langchain
- openai/openai-agents-python
- microsoft/semantic-kernel
- run-llama/llama_index
- huggingface/smolagents
- modelcontextprotocol/modelcontextprotocol
- modelcontextprotocol/servers
- microsoft/playwright-mcp
- aaif-goose/goose
- aider-ai/aider
- browser-use/browser-use
- mem0ai/mem0

### Capability Matrix (Normalized)

CM-001 Durable orchestration state and checkpointing.
CM-002 Interrupt/resume with human approval and staged continuation.
CM-003 Multi-agent handoff contracts and delegation semantics.
CM-004 Planner ledger and iterative plan revision loops.
CM-005 Tool loop governance (max iterations, force-final mode, retries).
CM-006 Structured output channels (text + typed/structured blocks).
CM-007 Roots-aware workspace boundary enforcement.
CM-008 Transport abstraction (stdio + streamable HTTP + legacy SSE).
CM-009 Capability negotiation cache per session and server profile.
CM-010 Task-augmented operations with progress/cancel semantics.
CM-011 Elicitation and sampling bridges for client-assisted reasoning.
CM-012 Resource subscription and list-changed synchronization.
CM-013 Tool annotation-driven safety hints (read-only, destructive, idempotent).
CM-014 Browser automation split: accessibility snapshot vs vision coordinates.
CM-015 Network/storage/browser-state control as first-class tools.
CM-016 Repo map generation for large-codebase context compression.
CM-017 Architect/editor split mode for reasoning vs edit execution.
CM-018 Session replay and trace-based judge/evaluator loops.
CM-019 Memory extraction, deduplication, decay, and profile summarization.
CM-020 Graph memory relations (entity and relationship persistence).
CM-021 Scoped memory keys (user_id, agent_id, run_id).
CM-022 Policy packs and allowlist-driven extension governance.
CM-023 Sandboxed app/UI extensions with CSP and permission metadata.
CM-024 Unified telemetry for cost, latency, quality, and incidents.

### Backlog Delta Additions (v2)

DX2-001 Add checkpoint backends in `src/core` with resumable run handles. [CM-001]
DX2-002 Add workflow interrupts and resume tokens in `src/workflows`. [CM-002]
DX2-003 Add explicit handoff schema in `src/core/types.ts`. [CM-003]
DX2-004 Add planner-ledger artifact in `src/gsd` for plan revision traceability. [CM-004]
DX2-005 Add tool-loop guardrail policy in `src/safety` and `src/tools`. [CM-005]
DX2-006 Add structured content response contract in `src/artifacts`. [CM-006]
DX2-007 Add roots boundary adapter and sync hooks in `src/mcp`. [CM-007]
DX2-008 Add transport manager abstraction in `src/gateway` and `src/mcp`. [CM-008]
DX2-009 Add capability negotiation cache in `src/mcp` with TTL + invalidation. [CM-009]
DX2-010 Add task progress/cancel model in `src/core` and `src/workflows`. [CM-010]
DX2-011 Add elicitation/sampling bridge adapter in `src/llm` + `src/mcp`. [CM-011]
DX2-012 Add resource subscription manager in `src/mcp` + `src/memory`. [CM-012]
DX2-013 Add tool annotation safety mapper in `src/safety`. [CM-013]
DX2-014 Add dual-mode browser tools (snapshot + coordinate) in `src/tools/browser.ts`. [CM-014]
DX2-015 Add network/storage/browser-state operator tools in `src/tools`. [CM-015]
DX2-016 Add repo-map context compactor in `src/memory` + `src/core/agent.ts`. [CM-016]
DX2-017 Add architect/editor split execution mode in `src/core/orchestrator.ts`. [CM-017]
DX2-018 Add run judge/eval loop with replay in `src/metrics` + `src/artifacts`. [CM-018]
DX2-019 Add memory dedup + contradiction pipeline in `src/memory`. [CM-019]
DX2-020 Add graph-memory adapter layer in `src/memory` (optional provider backends). [CM-020]
DX2-021 Add identity scoping contract in `src/core/types.ts`. [CM-021]
DX2-022 Add extension allowlist and policy profiles in `src/marketplace` + `src/safety`. [CM-022]
DX2-023 Add sandbox UI extension metadata model in `src/plugins`. [CM-023]
DX2-024 Add telemetry schema unification in `src/logger` + `src/metrics`. [CM-024]

### Priority Re-Ranking (After v2)

P0 (build first):
- DX2-001, DX2-002, DX2-005, DX2-007, DX2-009, DX2-010, DX2-013, DX2-019, DX2-021, DX2-024

P1 (next):
- DX2-003, DX2-004, DX2-008, DX2-011, DX2-012, DX2-014, DX2-016, DX2-018, DX2-022

P2 (scale/advanced):
- DX2-006, DX2-015, DX2-017, DX2-020, DX2-023

### Proposed Implementation Phases

Phase A: Runtime Safety Core
- Deliver P0 with strict tests around resume semantics, tool-loop limits, and policy gating.

Phase B: Protocol and Tooling Expansion
- Deliver P1 with transport abstraction, sampling/elicitation bridge, subscription flow, and browser split modes.

Phase C: Advanced Orchestration and Memory Graph
- Deliver P2 with architect/editor lane, graph memory adapters, and sandbox UI extension metadata.

### Verification Gates (Add to Definition of Done)

VG-001 Every orchestration run is replayable from persisted state.
VG-002 Every destructive tool call is policy-evaluated and auditable.
VG-003 MCP capability negotiation and roots sync are deterministic under reconnect.
VG-004 Memory write path enforces dedup/contradiction checks.
VG-005 Browser tooling can run in both snapshot-first and coordinate-mode flows.
VG-006 Metrics include latency, token/cost, safety events, and completion quality.

---

## Deep Dive v3: Claw-First Sweep (GitHub-Wide Claw Signal)

This section captures a claw-keyword-first ecosystem sweep intended to mirror prior AI breadth while preserving Nexusclaw identity.

### Discovery Snapshot

- Raw claw keyword sweep scale observed: ~45k repository matches.
- High noise expected in raw keyword results (unrelated "claw" strings, mirrors, low-signal forks).
- Signal-first shortlist criteria used:
  - Explicit OpenClaw/claw runtime alignment.
  - Clear architecture docs + actionable implementation surfaces.
  - Evidence of production concerns (safety, policy, lifecycle, telemetry, tests).
  - Reusable protocols/metadata/workflow patterns.

### Claw-First High-Signal Repos

- openclaw/clawhub
- NVIDIA/NemoClaw
- ValueCell-ai/ClawX
- HKUDS/ClawTeam
- HKUDS/ClawWork
- Gen-Verse/OpenClaw-RL
- aiming-lab/AutoResearchClaw
- ultraworkers/claw-code

### Claw Capability Matrix (Normalized)

CCM-001 Skill registry contracts with frontmatter-driven runtime requirements and install specs.
CCM-002 Skill moderation snapshots, malware/suspicion gates, and role-based delete/restore lifecycle.
CCM-003 Canonical slug ownership operations (rename/merge/redirect) and compatibility-safe install flows.
CCM-004 Structured skill packaging metadata (`origin`, lockfiles, deterministic zip/build provenance).
CCM-005 Sandbox-first claw deployment with layered controls (network/filesystem/process/inference).
CCM-006 Inference indirection via controlled gateway route instead of direct model endpoint exposure.
CCM-007 Policy tiering (restricted/balanced/open) with operator approval loops and hot-reloadable controls.
CCM-008 Blueprint/image hardening patterns (digest pinning, stripped build tools, startup integrity checks).
CCM-009 Desktop claw UX pattern: dual-process host API proxy with WS/HTTP/IPC fallback and gateway supervision.
CCM-010 Multi-agent account binding and channel routing models (`@agent` direct context switching).
CCM-011 Swarm CLI model: team spawn, task dependencies, inbox messaging, worktree isolation, lifecycle protocol.
CCM-012 Worker persistence loops (idle/report/receive/replan) for long-running autonomous teams.
CCM-013 Economic execution loop: task valuation, artifact submission, quality scoring, cost-aware survivability.
CCM-014 Artifact-first submission/evaluation protocol with path validation and auto-wrap-up on iteration limits.
CCM-015 Asynchronous RL loop for claw agents (conversation-derived reward, proxy training, continuous improvement).
CCM-016 Multi-mode RL methods (binary RL, OPD, hybrid) and environment-specific rails (terminal/GUI/SWE/tool-call).
CCM-017 Research pipeline orchestration as stage machine with gate checkpoints and optional human co-pilot.
CCM-018 Agent/skill discovery contracts in claw CLIs with structured JSON output and source precedence.
CCM-019 Plugin contract validation against unsupported manifest fields to prevent silent compatibility drift.
CCM-020 Claw-native docs posture: operation guides, troubleshooting, policy docs, and explicit runbook commands.

### Backlog Delta Additions (v3)

DX3-001 Add `clawdis`-style skill metadata schema support (requires/env/install/os/config) in `src/skills` + `src/plugins`. [CCM-001]
DX3-002 Add skill moderation state model (active/hidden/removed + evidence snapshot) in `src/safety` + `src/marketplace`. [CCM-002]
DX3-003 Add canonical package identity operations (rename/merge/alias redirects) in `src/marketplace`. [CCM-003]
DX3-004 Add install provenance files and lock metadata for reproducible skill installs in `src/artifacts` + `src/config`. [CCM-004]
DX3-005 Add sandbox policy profile abstraction (network/filesystem/process/inference) in `src/safety` + `src/config`. [CCM-005]
DX3-006 Add routed-inference policy contract to block direct provider host leakage in `src/gateway` + `src/llm`. [CCM-006]
DX3-007 Add policy tiers and operator approval workflow in `src/safety` + `src/hooks`. [CCM-007]
DX3-008 Add hardened runtime validation hooks (image/source checksum + config integrity checks) in `src/core` + `src/secrets`. [CCM-008]
DX3-009 Add host-proxy transport fallback strategy for desktop/embedded clients in `src/gateway` + `src/adapters`. [CCM-009]
DX3-010 Add per-agent channel/account binding model in `src/teams` + `src/core/types.ts`. [CCM-010]
DX3-011 Add swarm orchestration primitives (team spawn, dependency tasks, inbox commands) in `src/teams` + `src/workflows`. [CCM-011]
DX3-012 Add worker lifecycle protocol states (active/idle/shutdown pending) in `src/core/orchestrator.ts` + `src/teams`. [CCM-012]
DX3-013 Add economic-mode optional module (task valuation, token spend, payout simulation) in `src/metrics` + `src/workflows`. [CCM-013]
DX3-014 Add artifact submission contract with pre-submit validation and auto-wrap-up hooks in `src/artifacts` + `src/hooks`. [CCM-014]
DX3-015 Add online adaptation interface for reward-feedback loops in `src/llm` + `src/metrics` (feature-flagged). [CCM-015]
DX3-016 Add multi-mode training/eval adapter contracts (binary/opd/hybrid) in `src/recipes` + `src/platform`. [CCM-016]
DX3-017 Add stage-gate pipeline template support for long workflows in `src/workflows` + `src/recipes`. [CCM-017]
DX3-018 Add structured discovery outputs for agents/skills/providers in `src/core` + `src/logger`. [CCM-018]
DX3-019 Add plugin manifest compatibility validator with explicit unsupported-field diagnostics in `src/plugins` + `src/safety`. [CCM-019]
DX3-020 Add operations runbook generator for common claw maintenance tasks in `src/recipes` + `src/workspace`. [CCM-020]

### Priority Re-Ranking (After v3)

P0 (claw-core now):
- DX3-001, DX3-002, DX3-005, DX3-006, DX3-007, DX3-011, DX3-012, DX3-018, DX3-019

P1 (platformizing claw patterns):
- DX3-003, DX3-004, DX3-008, DX3-009, DX3-010, DX3-014, DX3-017, DX3-020

P2 (advanced/optional):
- DX3-013, DX3-015, DX3-016

### Verification Gates (v3 Additions)

VG3-001 Every installed skill has parseable metadata + provenance lock record.
VG3-002 Moderation/quarantine states are enforced on install/execute paths.
VG3-003 Inference calls respect routed gateway policy and reject direct-host bypass attempts.
VG3-004 Swarm worker lifecycle transitions are observable and replayable from logs.
VG3-005 Structured `agents/skills/providers` inventory output is machine-parseable and stable.
VG3-006 Plugin manifest compatibility failures return explicit actionable diagnostics.
