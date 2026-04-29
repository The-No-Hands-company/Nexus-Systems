# Nexusclaw Phase 1 Implementation Plan

## Goal
Build the Phase 1 Nexusclaw MVP inside the existing repo by delivering a stable Nexus-native agent runtime, CLI + gateway, persistent session and memory support, tool registry and safe execution, basic multi-agent coordination, plugin/adapter contract, Nexus Cloud health/registration, and a lightweight management surface.

## Existing repo map
The current repo already contains the right modules for Phase 1. The plan below maps existing folders and files to the precise implementation tasks.

### Core modules
- `src/index.ts`
  - bootstrap pipeline
  - CLI/command entry point
  - component wiring
  - skill and plugin loading
  - should become the Phase 1 command surface and gateway bootstrapper
- `src/core/orchestrator.ts`
  - multi-agent lifecycle
  - agent listing/status
  - delegation/spawn primitives
  - core coordination foundation
- `src/core/agent.ts`
  - agent loop and message processing
  - session lifecycle and tool use
- `src/tools/index.ts`
  - tool registry and tool execution
  - built-in tools are already registered here
- `src/memory/index.ts`
  - 3-tier memory manager
  - episodic persistence and vault support
- `src/gsd/index.ts`
  - task / spec manager
  - GSD tools registration and lifecycle
- `src/safety/index.ts`
  - permission model
  - confirmation gate logic
- `src/gateway/index.ts`
  - HTTP + WebSocket gateway server
  - service health and status routes
- `src/plugins/index.ts`
  - plugin discovery / load path
  - adapter contract surface
- `src/hooks/index.ts`
  - hook points and event dispatch
- `src/llm/index.ts`
  - provider router and model selection
- `src/config/index.ts`
  - configuration loading and validation
  - center for Nexus Cloud registration config

## Phase 1 implementation tasks

### 1. Stabilize bootstrap and CLI command surface
- Verify `src/index.ts` bootstraps all required Phase 1 components reliably.
- Add explicitly supported Phase 1 commands in `src/index.ts` or adjacent CLI module:
  - `chat`
  - `gateway`
  - `status`
  - `health`
  - `agents`
  - `sessions`
  - `tools`
  - `plugins`
  - `doctor`
- Ensure CLI commands are wired through `Commander` and that `dist/index.js` exposes them correctly.
- Add an explicit `gateway` command that starts the Fastify server from `src/gateway/index.ts`.

### 2. Harden tool registry + safe execution
- Confirm `src/tools/index.ts` provides a registry class with `register` and `execute` APIs.
- Implement or verify safe execution semantics:
  - tool allowlist / denylist support
  - per-tool permission checks
  - command path restrictions via `src/safety/index.ts`
  - audit logging on tool invocation
- Add a `tools list` CLI command / route that returns registered tools and metadata.
- Add a `tools.execute` test harness if not present.

### 3. Complete persistent sessions + memory
- Ensure `src/memory/index.ts` supports:
  - in-memory short-term context
  - SQLite episodic session transcripts
  - vault storage for alignment / notes
  - vector/embedding function injection (Phase 1 can keep as optional stub)
- Wire session persistence into `src/core/agent.ts` and `src/core/orchestrator.ts`:
  - create or resume sessions
  - store transcripts to SQLite
  - expose `sessions list` and `sessions show` commands
- Add `memory` health checks in `src/gateway/index.ts`.

### 4. Validate multi-agent primitives
- Make sure `src/core/orchestrator.ts` supports:
  - `createAgent`
  - `getAgent`
  - `listAgents`
  - `removeAgent`
  - `spawnSubAgent`
  - `delegate`
  - simple swarms (parallel tasks)
- Add session/agent status routes in gateway:
  - `GET /api/agents`
  - `GET /api/agents/:id`
  - `GET /api/sessions`
- Add CLI commands for agent lifecycle:
  - `agents list`
  - `agents create`
  - `agents remove`
  - `agents status`

### 5. Define plugin/adapter contract
- Create schema and manifest docs for Phase 1 plugin support.
- Add `src/plugins/index.ts` contract definitions:
  - `plugin.name`
  - `plugin.description`
  - `plugin.commands`
  - `plugin.tools`
  - `plugin.hooks`
  - `plugin.load()`
- Support local plugin directories from config and package discovery.
- Expose a plugin loader in `src/index.ts`.
- Add CLI commands:
  - `plugins list`
  - `plugins load`
- Capture contract details in docs:
  - new `NEXUSCLAW_PLUGIN_SPEC.md`
  - update `README.md` plugin section

### 6. Add Nexus Cloud health/registration support
- Add a Nexus Cloud registration manifest generator in `src/config/index.ts` or new `src/config/nexusCloud.ts`.
- Add a `health` module or route in `src/gateway/index.ts` exposing:
  - `GET /api/health/live`
  - `GET /api/health/ready`
  - `GET /api/status`
- Add a registration endpoint client that can POST `POST /api/v1/tools` to Nexus Cloud if configured.
- Add config options to `src/config/index.ts` for Nexus Cloud:
  - `cloud.enabled`
  - `cloud.registry_url`
  - `cloud.upstream_url`
  - `cloud.capabilities`
- Add a `cloud register` CLI command or auto-registration on startup when enabled.
- Update `README.md` and `NEXUSCLAW_PRODUCT_SPEC.md` to document Nexus Cloud support.

### 7. Implement safety and permission model
- Confirm `src/safety/index.ts` handles:
  - allowed / denied tool lists
  - blocked command patterns
  - confirmation requests
  - SSRF host rules
- Expose configuration from `src/config/index.ts` for safety settings.
- Add `safety status` CLI output and gateway route.
- Add documentation for Phase 1 safety model in `ARCHITECTURE.md` and `README.md`.

### 8. Build a lightweight Phase 1 dashboard surface
- Implement a basic web/status dashboard stub in `src/gateway/index.ts`.
- Add a JSON status endpoint with core runtime info:
  - active agent count
  - tool registry summary
  - memory/db availability
  - uptime
- Add a minimal HTML page or `GET /` redirect to `/status` for local browser use.
- Document dashboard usage in `README.md`.

## Exact doc changes
- `README.md`
  - Add a Phase 1 section describing the new Nexusclaw MVP goals.
  - Document CLI commands and gateway endpoints.
  - Document how to configure Nexus Cloud registration.
  - Document plugin manifest and local plugin directory.
- `ARCHITECTURE.md`
  - Add a Phase 1 runtime architecture section with module responsibilities.
  - Add a Nexus Cloud registration and health section.
  - Add a Phase 1 plugin contract summary.
- `NEXUSCLAW_PRODUCT_SPEC.md`
  - Append a “Repo-to-Implementation Mapping” section linking current modules to Phase 1 tasks.
  - Add acceptance criteria for Phase 1.
- New file: `NEXUSCLAW_PHASE1_IMPLEMENTATION_PLAN.md`
  - This file itself is the actionable plan and should remain updated as work progresses.
- Optional doc: `NEXUSCLAW_PLUGIN_SPEC.md`
  - Define manifest schema and hook points for Phase 1 adapter support.

## Milestones and acceptance criteria

### Milestone 1: Phase 1 bootstrap and CLI
- `npm run build` succeeds.
- `node dist/index.js status` returns runtime status.
- `node dist/index.js gateway` starts Fastify on configured port.
- `node dist/index.js health` returns live/ready status.

### Milestone 2: Memory and session persistence
- `sessions list` shows at least one session.
- Session transcripts are written to SQLite.
- `memory` CLI or status route reports DB path and vault status.

### Milestone 3: Tool registry + safety
- `tools list` enumerates registered built-in tools.
- Attempting to invoke a denied tool is blocked with a safety message.
- Tool usage events are recorded in audit logs.

### Milestone 4: Multi-agent coordination
- `agents list` returns active agents.
- `agents create` and `agents remove` work.
- The orchestrator can spawn a sub-agent and return a valid response.

### Milestone 5: Nexus Cloud registration
- Configured registration payload can be generated.
- `cloud register` or auto-registration logic sends manifest successfully to configured Nexus Cloud endpoint.
- Gateway health route includes cloud-capable metadata.

### Milestone 6: Plugin contract
- Local plugin directory discovery works.
- Plugins expose metadata and hook points.
- At least one sample plugin is loadable.

## Implementation sequence
1. Stabilize bootstrap/CLI and `src/index.ts`
2. Harden `src/tools/index.ts` + `src/safety/index.ts`
3. Complete `src/memory/index.ts` and session persistence wiring
4. Expose agent/multi-agent state in `src/core/orchestrator.ts`
5. Build `src/gateway/index.ts` health/status/dashboard
6. Add Nexus Cloud config and registration support
7. Define plugin manifest in `src/plugins/index.ts` and docs
8. Update docs and README to reflect Phase 1 capabilities

## Recommended immediate first code changes
- Add `src/gateway/status.ts` or extend `src/gateway/index.ts`
- Add `src/config/nexusCloud.ts` and update `src/config/index.ts`
- Add `src/plugins/plugin-spec.ts` for manifest schema
- Add CLI command stubs in `src/index.ts` for `status`, `agents`, `sessions`, `tools`, `plugins`, `cloud`
- Add a `src/core/health.ts` or `src/gateway/health.ts` to centralize health checks

## Notes
- Keep the Phase 1 core runtime small: avoid Phase 2 skill marketplace, advanced embeddings, and sandbox adapters.
- Phase 1 should own the core platform contracts; Phase 2 can add richer skill loading and model routing.
- The current repo is already close to Phase 1 shape; the main work is stabilization, explicit command surface, cloud registration, health routes, and clear documentation.
