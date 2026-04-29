# Nexusclaw Product Specification

## 1. Product vision

Nexusclaw is the Nexus Systems mega-tool for agentic orchestration, inspired by the broader `claw` ecosystem but designed as a Nexus-native platform. It is not a simple OpenClaw fork; it is a self-hosted, extensible agent runtime that absorbs the best features from the `claw` ecosystem via native implementation and adapter/plugin integration.

### Vision statement

- Provide a best-in-class Nexus-native agent tool for orchestration, tools, safety, memory, and multi-agent workflows.
- Enable rich integrations with the broader `claw` ecosystem through adapters and shared manifests.
- Keep the core runtime focused, with extensibility for desktop UX, marketplace, sandboxing, enterprise controls, and specialized pipelines.
- Position Nexusclaw as a Nexus Systems tool that is large, modular, ecosystem-aware, and fit-for-purpose.

## 2. Scope

### Core capability domains

- Agent runtime / CLI / gateway
- Tool registry and safe execution
- Memory and session persistence
- Multi-agent coordination
- Plugin/adapter architecture
- Service discovery and registration
- Health, status, and dashboard surfaces
- Safety, permissions, and approval hooks

### Adapter/plugin domains

- Desktop UI and managed applications
- Skill/package registry and marketplace
- Specialized vertical workflows (research, economic tasks)
- Agent-native model routing / cost-aware routing
- Secure sandbox runtime integration
- Enterprise RBAC, audit, and distributed worker orchestration

## 3. Repo-inspired component map

| Nexusclaw component | Inspired by | What it brings |
|---|---|---|
| Core agent runtime | `ultraworkers/claw-code` | canonical agent harness, tool system, session/memory persistence, CLI + gateway, providers, safety policy, MCP lifecycle |
| Multi-agent coordination | `HKUDS/ClawTeam`, `aiming-lab/AutoResearchClaw`, `agentscope-ai/HiClaw` | leader/worker flows, task boards, inbox messaging, team templates, swarm orchestration |
| UX/dashboard surfaces | `ValueCell-ai/ClawX`, `openclaw/clawhub` | desktop and web UX, skill browser, settings management, onboarding wizard |
| Agent-native model routing | `BlockRunAI/ClawRouter` | local routing proxy, wallet-based auth, automatic model selection, cost control |
| Sandbox/runtime hardening | `NVIDIA/NemoClaw` | secure onboarding, sandbox lifecycle, credential isolation, hardened runtime policies |
| Skill marketplace | `openclaw/clawhub`, `dataelement/Clawith` | registry, install/uninstall, skill metadata, packaging, discovery |
| Vertical workflows | `aiming-lab/AutoResearchClaw`, `HKUDS/ClawWork` | pipeline orchestration, gated HITL stages, work/budget tracking, domain-specific runner |
| Enterprise coordination | `dataelement/Clawith`, `agentscope-ai/HiClaw` | RBAC, organization controls, long-lived agent identity, shared knowledge, centralized monitoring |

## 4. Core product spec

### 4.1 Nexusclaw core

#### 4.1.1 Runtime & orchestration
- CLI-based command surface for chat, gateway, session, agents, memory, tools, health
- Local gateway with HTTP/WS endpoints for external integrations
- Multi-provider model routing (Anthropic, OpenAI-compatible, Ollama, local proxies)
- Tool registry and execution safe-by-default semantics
- Session lifecycle with persistence to SQLite and optional archive storage
- Memory tiers: short-term session, episodic transcripts, semantic vault
- Safety and permission modes with user-confirmation gates, tool allowlists, audit logs

#### 4.1.2 Multi-agent and task orchestration
- Agent spawn / sub-agent lifecycle support
- Task creation and lifecycle states (pending, in-progress, verifying, done, failed)
- Agent inbox / message coordination primitives
- Workspace isolation per agent (git worktrees or sandboxed directories)
- Basic agent discovery and status listing

#### 4.1.3 Plugin & adapter architecture
- Skill/adapter manifest contract for external integrations
- Plugin loader for local dirs and installed packages
- Hook points for tool calls, message events, session transitions, errors
- Adapter contract for external repos and ecosystem tools

#### 4.1.4 Health and discovery
- Health endpoints and status metadata
- Nexus Cloud registration manifest `POST /api/v1/tools`
- Heartbeat support and capability reporting
- Web dashboard stub for service discovery and basic UX

### 4.2 Phase 1 MVP capabilities

The Phase 1 minimum viable Nexusclaw must include:
- Core runtime + CLI + gateway
- Tool registry + safe execution
- Persistent sessions + memory
- Basic multi-agent coordination primitives
- Plugin/adapter contract
- Health/status + Nexus Cloud registration
- Safety and permission model
- CLI and lightweight dashboard management surface

### 4.3 Phase 2 extensibility

Phase 2 expands the runtime and ecosystem integration:
- Rich skills system with SKILL.md discovery
- Built-in skill installer and registry adapter
- Advanced memory search and hybrid retrieval
- Model proxy / router adapter support
- Better multi-agent team templates and coordination tools
- External gateway adapters for OpenClaw/ACP/MCP
- Desktop/web UX improvements

### 4.4 Phase 3 ecosystem adapters

Phase 3 focuses on ecosystem connection, not core runtime only:
- ClawHub-style skill marketplace integration
- ClawRouter-style local model routing and cost metrics
- NemoClaw-style sandbox runtime adapter
- HiClaw-style manager/worker orchestration adapter
- ClawX-style desktop management companion
- ClawWork-style vertical workflow plugins (economic, research, benchmarks)

### 4.5 Phase 4 maturity

Phase 4 is maturity and platform convergence:
- Unified Nexusclaw ecosystem layer across Nexus Cloud, Hosting, Phantom
- Enterprise controls, RBAC, audit, distributed agent orchestration
- Marketplace, reusable workflows, and adapter marketplace
- Full end-to-end Nexus-native experience across UI, runtime, and privacy-enabled execution

## 5. Detailed implementation roadmap

### Phase 1 — Nexusclaw Core MVP

#### 1.1 Core runtime implementation
- Ensure CLI/gateway/core service startup is stable
- Implement tool registry and execution engine
- Implement session persistence in SQLite
- Implement memory tiers and search index
- Implement provider routing and model config
- Implement safety / permission modes

#### 1.2 Nexus Cloud integration
- Implement service registration manifest
- Implement heartbeat + health metadata
- Add upstreamUrl + capability reporting
- Add Cloud-aware dashboard discovery

#### 1.3 Basic multi-agent coordination
- Add agent list / status commands
- Add task/inbox primitives
- Add workspace isolation support
- Add simple message routing and task state updates

#### 1.4 Plugin/adapter contract
- Define `nexusclaw-plugin.json` / manifest schema
- Support local plugin dirs + package discovery
- Add hook points for tools, sessions, and messages
- Add adapter interface for external repo connectors

#### 1.5 UX and CLI
- Build CLI commands: `chat`, `gateway`, `status`, `agents`, `sessions`, `tools`, `skills`, `health`, `doctor`
- Build lightweight dashboard skeleton for runtime status
- Add onboarding docs and quickstart

### Phase 2 — Expansion and parity features

#### 2.1 Skills and registry
- Implement SKILL.md parser and loader
- Add install / list / remove / update commands
- Add registry adapter for community skill sources
- Add skill-provided tool registration

#### 2.2 Memory & retrieval
- Add vector/embedding-backed memory search
- Implement hybrid ranking (FTS + vectors)
- Add temporal decay and MMR re-ranking
- Add embedding cache and deduplication support

#### 2.3 Advanced toolset
- Add web_fetch tool
- Add apply_patch tool
- Add process spawn/list/kill tool
- Add pdf/image tools
- Add cron scheduler / scheduled task support

#### 2.4 Multi-agent maturity
- Add team templates and reusable agent graphs
- Add task dependencies / blocked-by semantics
- Add monitor/board command for active agent teams
- Add basic orchestration policies and stop/restart primitives

#### 2.5 Ecosystem adapter foundation
- Build OpenClaw/ACP adapter interface
- Build model router adapter interface
- Build sandbox/runtime adapter interface
- Build skill registry adapter interface

### Phase 3 — Ecosystem integration

#### 3.1 Marketplace & ecosystem adapters
- Integrate with a skill marketplace (ClawHub-style)
- Integrate with a model router adapter (ClawRouter-style)
- Integrate with a sandbox runtime adapter (NemoClaw-style)
- Integrate with multi-agent manager/worker adapter (HiClaw-style)

#### 3.2 Vertical workflow plugins
- Add research pipeline plugin adapter
- Add economic/task benchmark plugin adapter
- Add enterprise task/workflow plugin adapter

#### 3.3 Desktop and management UX
- Build a managed desktop companion or integrate with existing Node/TS dashboard
- Add guided setup wizard, provider config, and skill marketplace browser
- Add dashboards for agents, tasks, memory, and integrations

### Phase 4 — Platform maturity

#### 4.1 Nexus platform convergence
- Align Nexusclaw with Nexus Cloud service model
- Add standardized Nexus tool manifest and public URL issuance
- Add Phantom privacy runtime integration hooks
- Add Hosting deployment extension points

#### 4.2 Governance and enterprise
- Add RBAC and audit logging adapters
- Add centralized management for distributed agents
- Add policy enforcement and compliance hooks

#### 4.3 Ecosystem deliverables
- Publish Nexusclaw adapter spec for third-party `claw` repos
- Document “Nexusclaw plugin adapter” patterns
- Build reference integrations for 3 ecosystem components:
  - marketplace / skills registry
  - model routing / cost proxy
  - secure sandbox runtime

## 6. Minimum viable Phase 1 definition

Nexusclaw Phase 1 must deliver:

- A stable Nexus-native agent runtime with CLI + gateway
- Persistent sessions and memory
- Tool registry and safe tool execution
- Basic multi-agent coordination
- Plugin/adapter contract and manifest
- Health/status exporting for Nexus Cloud
- Safety/permission controls
- Lightweight web/dashboard management surface

This is the foundation on which all repo-inspired ecosystem capabilities can be added later.

## 7. Notes on ecosystem breadth

There are 30+ `claw`-related repos in the broader GitHub ecosystem. Nexusclaw does not need to copy them all directly. Instead, it should:

- own the core agent runtime features
- support ecosystem capabilities via adapters and plugin manifests
- preserve a clear contract for repo-inspired integration
- keep the platform extensible so future verticals can plug in without bloating the core

---

## 8. Repo-to-Implementation Mapping (Phase 1)

This section maps existing source modules to the Phase 1 tasks they fulfil.

| Phase 1 Task | Module(s) | Status |
|---|---|---|
| Bootstrap and CLI wiring | `src/index.ts` | ✅ Done |
| Multi-provider LLM routing | `src/llm/index.ts` | ✅ Done |
| Tool registry + safe execution | `src/tools/index.ts`, `src/safety/index.ts` | ✅ Done |
| Audit logging on tool use | `src/core/agent.ts` (`logAction`) | ✅ Done |
| Session persistence (SQLite) | `src/memory/index.ts` | ✅ Done |
| Episodic memory + vault | `src/memory/index.ts` | ✅ Done |
| Multi-agent lifecycle | `src/core/orchestrator.ts` | ✅ Done |
| Sub-agent delegation + swarm | `src/core/orchestrator.ts` (`delegate`, `swarm`) | ✅ Done |
| Plugin/adapter contract | `src/plugins/index.ts`, `NEXUSCLAW_PLUGIN_SPEC.md` | ✅ Done |
| GSD spec/task management | `src/gsd/index.ts` | ✅ Done |
| Hook event dispatch | `src/hooks/index.ts` | ✅ Done |
| Recipe execution | `src/recipes/index.ts` | ✅ Done |
| Safety + SSRF policy | `src/safety/index.ts` | ✅ Done |
| Gateway health routes | `src/gateway/index.ts` (`/api/health/*`) | ✅ Done |
| Gateway status + agents routes | `src/gateway/index.ts` (`/api/status`, `/api/agents`) | ✅ Done |
| Gateway sessions routes | `src/gateway/index.ts` (`/api/sessions`) | ✅ Done |
| Gateway safety route | `src/gateway/index.ts` (`/api/safety`) | ✅ Done |
| Embedded dashboard | `src/gateway/index.ts` (`DASHBOARD_HTML`) | ✅ Done |
| Nexus Cloud config schema | `src/config/index.ts` (`CloudSchema`) | ✅ Done |
| Nexus Cloud manifest | `src/cloud/index.ts` | ✅ Done |
| Cloud registration/deregistration | `src/cloud/index.ts` | ✅ Done |
| `cloud *` CLI subcommands | `src/index.ts` | ✅ Done |
| `agents *` CLI subcommands | `src/index.ts` | ✅ Done |
| `sessions *` CLI subcommands | `src/index.ts` | ✅ Done |
| `tools` CLI command | `src/index.ts` | ✅ Done |
| `plugins *` CLI subcommands | `src/index.ts` | ✅ Done |
| `safety` CLI command | `src/index.ts` | ✅ Done |
| `health` CLI command | `src/index.ts` | ✅ Done |
| Phase 1 architecture docs | `ARCHITECTURE.md` | ✅ Done |
| Plugin spec docs | `NEXUSCLAW_PLUGIN_SPEC.md` | ✅ Done |
| Sample loadable plugin | `examples/plugins/greeting/` | ✅ Done |

---

## 9. Phase 1 Acceptance Criteria

### Milestone 1 — Bootstrap and CLI
- [ ] `npm run build` succeeds with zero errors
- [ ] `node dist/index.js doctor` completes without fatal failures
- [ ] `node dist/index.js status` returns a runtime status object
- [ ] `node dist/index.js gateway` starts Fastify on the configured port
- [ ] `node dist/index.js health` prints live/ready status

### Milestone 2 — Memory and Sessions
- [ ] `node dist/index.js sessions list` returns at least one session after a chat
- [ ] Session transcripts are written to SQLite (`memory.episodic_db`)
- [ ] `GET /api/memory/status` reports DB path, episode count, vault note count
- [ ] `node dist/index.js sessions show <id>` renders messages

### Milestone 3 — Tool Registry and Safety
- [ ] `node dist/index.js tools` lists all registered built-in tools
- [ ] Attempting to use a tool in `safety.denied_tools` is blocked with a clear error
- [ ] Tool invocations appear in `safety.getActionLog()` and `GET /api/safety`
- [ ] Shell commands matching `blocked_commands` are rejected before execution

### Milestone 4 — Multi-Agent Coordination
- [ ] `node dist/index.js agents list` returns the default agent
- [ ] `node dist/index.js agents create --name Test` registers a new agent
- [ ] `node dist/index.js agents remove <id>` removes the agent
- [ ] The orchestrator can spawn a sub-agent and return a valid response (`delegate`)
- [ ] `GET /api/agents` returns the running agent list
- [ ] `GET /api/sessions` returns saved sessions

### Milestone 5 — Nexus Cloud Registration
- [ ] `node dist/index.js cloud validate` exits 0 when endpoint is configured
- [ ] `node dist/index.js cloud register` POSTs the manifest to the configured endpoint
- [ ] `node dist/index.js cloud deregister` sends DELETE to the endpoint
- [ ] Gateway health route (`/api/health/ready`) includes `nexus-cloud/1.0` metadata

### Milestone 6 — Plugin Contract
- [ ] `node dist/index.js plugins list` runs without error
- [ ] `node dist/index.js plugins load examples/plugins/greeting` loads the sample plugin
- [ ] Loaded plugin tools appear in `node dist/index.js tools` output
- [ ] `GET /api/plugins` reflects loaded plugins

### Milestone 7 — Safety Model
- [ ] `node dist/index.js safety` shows current profile, denied tools, blocked commands
- [ ] `GET /api/safety` returns permissions and recent action log
- [ ] SSRF: fetching a blocked host returns an error message, not a response
- [ ] Three sandbox profiles (`strict`, `standard`, `permissive`) are selectable via config
