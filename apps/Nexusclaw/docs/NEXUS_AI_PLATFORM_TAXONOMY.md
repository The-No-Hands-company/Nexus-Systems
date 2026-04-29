# Nexus Systems AI Platform Taxonomy

## Purpose

This document defines a lightweight Nexus Systems AI platform taxonomy and integration approach for the evolving Nexus ecosystem.
It is intended to help position Nexusclaw, Phantom, Nexus Cloud, and Nexus Hosting as complementary Nexus-native components rather than standalone OpenClaw clones.

## 0. Nexus Systems technology strategy

### Polyglot by fit, unified by contract
- Use the best technology stack for each tool, not a single-stack mandate.
- Enforce shared integration standards across the ecosystem:
  - common manifest/schema for service registration
  - API/auth standards for cross-service access
  - container/package conventions for deployment
  - observability and health wireframes for runtime consistency
- Avoid wild-west fragmentation by documenting a preferred stack map and expected integration patterns.

### Role of Node/TS in NS
- Node/TS is the default glue layer for UX, orchestration, service adapters, and developer-facing tooling.
- It should power dashboards, gateways, management surfaces, and rapid orchestration composition.
- It does not need to be the only runtime technology in NS.

### Role of other stacks
- Keep Python where it is strongest: AI/ML, rapid experimentation, data workflows.
- Keep Rust where it is strongest: protocol runtimes, security-sensitive infrastructure, performance-critical components.
- Use other languages only when they bring a clear advantage for a specific tool.

### What this means for NS documentation
- Document why each project chose its stack.
- Explain how the chosen stack fits into the shared integration layer.
- Make the polyglot strategy visible so contributors understand the architecture decisions.

---

## 1. Nexus Systems AI platform taxonomy

### Nexusclaw
**Role:** Agent framework / AI orchestration engine

- Core agent loop and planning orchestration
- Multi-provider LLM routing
- 3-tier local memory
- GSD/task-based execution
- Tool registry and tool safety
- CLI + gateway + dashboard
- Local-first, self-hosted agent compute

### Nexus Phantom
**Role:** Privacy / sandbox protocol

- Secure sandboxed execution boundary
- Permissioned tool/runtime access
- Identity and trust layer for agent actions
- Optional containerized isolation for untrusted flows
- Protocol for safe interaction between agent environments and host systems

### Nexus Cloud
**Role:** Registry / orchestration / discovery layer

- System registry for tools and services
- Heartbeat-based availability
- Tool routing and proxy metadata
- Central discovery for Nexus-native components
- Shared API gateway for cross-project integration

### Nexus Hosting
**Role:** Runtime host and deployment platform

- Static/federated hosting runtime
- Site and service serving
- Deployment lifecycle and node federation
- Runtime network and hosting infrastructure
- Integration point for hosted Nexus-native tools

---

## 2. Nexus AI integration surface

### 2.1 Nexus Cloud discovery

Nexus Cloud should discover Nexusclaw and Nexus Phantom as registered tools/services.

- **Service registration:** Nexusclaw and Nexus Phantom register themselves with Nexus Cloud using a consistent tool manifest.
- **Heartbeats:** Services send regular heartbeats to keep their registry entry alive.
- **Metadata:** Each registration includes `id`, `name`, `description`, `upstreamUrl`, `mode`, `health`, and `capabilities`.
- **Routing:** Cloud uses metadata to route dashboard UI, agent control commands, and runtime tool access.

### 2.2 Shared auth and permissions

- **Single auth authority:** Nexus Cloud should provide a shared authentication and permission model where possible.
- **Service tokens / API keys:** Nexusclaw and Phantom can use API keys or OIDC-style tokens issued by Cloud or a shared identity provider.
- **Tool permission model:** Permissions map tools to allowed agents, and agents to allowed actions.
- **Cross-service trust:** Registered services should share a trust policy that can be validated by Cloud.

### 2.3 Common dashboard / portal UI

- **Unified entrypoint:** Nexus Cloud or a Nexus dashboard presents services as discoverable nodes.
- **Service tiles:** Show Nexusclaw, Nexus Phantom, Nexus Hosting, and other registered tools.
- **Status and health:** Each service exposes health/status endpoints usable by the dashboard.
- **Action buttons:** Start, stop, view logs, open dashboard, connect to agent session.

### 2.4 Cross-project Nexus agent manifest / MCP bridge

- **Manifest definition:** A shared `nexus-agent.json` or `MCP` manifest to describe agent capabilities and endpoints.
- **Tool discovery:** Nexus Cloud can read manifests and expose registered tools to other services.
- **MCP compatibility:** Where possible, use an MCP-compatible schema for tool definitions so Nexus services can interoperate with broader tool ecosystems.
- **Metadata extension:** Include Nexus-specific metadata fields such as `ecosystem`, `role`, `supportedProtocols`, and `trustLevel`.

---

## 3. 2x2 feature gap chart

This chart compares feature positions for OpenClaw-style systems, Nexusclaw, and Phantom.

| Feature area | OpenClaw-style systems | Nexusclaw | Nexus Phantom |
|---|---|---|---|
| Agent orchestration | Strong | Strong | Limited (focus on safety/runtime) |
| Memory architecture | Strong | Strong | Minimal / not primary |
| Tool ecosystem | Strong | Strong | Tool usage controlled by protocol |
| Privacy & sandboxing | Optional | Moderate | Strong |
| UI/dashboard | Often included | Yes | Protocol-level / boundary-only |
| Federation/discovery | Often ad-hoc | Planned via Cloud | Inbound as trusted runtime |
| Local-first / self-hosting | Yes | Yes | Yes |
| Safety guardrails | Varies | Built-in | Primary focus |
| Nexus ecosystem integration | None | Pending | Pending |

> This chart is intentionally qualitative. It highlights where Nexusclaw aligns with OpenClaw ideas and where Phantom is purposefully differentiated.

---

## 4. Unique strengths of Nexusclaw

1. **Nexus-native orchestration potential**
   - Designed to integrate with Nexus Cloud and Nexus Hosting.
2. **Homegrown memory/tool/safety structure**
   - Not a direct OpenClaw port; it already has its own 3-tier memory and safety layers.
3. **CLI + gateway + dashboard model**
   - A complete local-first agent experience.
4. **Built-in GSD task workflow**
   - Useful for agent planning and verification.
5. **Plugin/hook architecture**
   - Good foundation for Nexus-specific extensions.

---

## 5. Naming and brand alignment

The project should be treated as a Nexus ecosystem component rather than an OpenClaw fork.

### Possible naming options

- `Nexusclaw` / `nexus-claw`
- `Nexus Agent Framework`
- `Nexus Agent Core`
- `Nexus Agent Engine`
- `Nexus Phantom` / `Nexus Phantom Protocol` (for the other project)

### Recommended package name approach

Until the final name is chosen, keep the current code stable and use an internal alias in documentation.

When ready to align naming:
- set `name` in `package.json` to a Nexus-prefixed package name
- update README/ARCHITECTURE docs with Nexus branding
- keep `Nexusclaw` repo name consistent with package identity

---

## 6. Practical next steps

1. **Create a Nexus AI platform taxonomy doc**
   - This is the file you are reading now.
2. **Decide roles for each component**
   - Nexusclaw = agent framework
   - Nexus Phantom = sandbox/privacy protocol
   - Nexus Cloud = registry/discovery/orchestration
   - Nexus Hosting = runtime host
3. **Scope integration surface**
   - What metadata Cloud requires from Nexusclaw/Phantom
   - What shared auth model will look like
   - How the dashboard will expose services
4. **Map the feature gap**
   - Use the 2x2 chart and feature tables to compare with OpenClaw-like systems
   - Prioritize Nexus-native capabilities first
5. **Preserve your architecture**
   - Use claw ideas as inspiration
   - Keep Nexusclaw’s existing core design
   - Make Phantom a complementary safety/runtime protocol

---

## 7. Suggested immediate work items

- Draft a service manifest schema for Nexus Cloud registration
- Add a small Nexus metadata layer to Nexusclaw registrations
- Sketch the Nexus dashboard wireframe for agent and protocol discovery
- Define a permission token flow for cross-service access
- Rename package only once the brand and role are confirmed
