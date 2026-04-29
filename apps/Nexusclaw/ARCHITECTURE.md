# AnyClaw — Architecture

> The "everything" claw: a comprehensive, local-first AI agent framework that
> combines the best features from 40+ claw-like applications into one unified
> platform for personal development use.

## Inspirations & Feature Sources

| Source | Features Taken |
|--------|---------------|
| **OpenClaw** | Gateway WS control plane, multi-channel inbox, skills platform, browser control, cron/webhooks, session model, agent-to-agent routing |
| **OpenStinger** | 3-tier memory (episodic → vault → gradient), bi-temporal graph, alignment drift detection, MCP tool exposure, memory portability |
| **GSD (Get Shit Done)** | Spec-driven development, atomic task execution, fresh-context anti-rot, meta-prompting, verification layers |
| **NanoClaw** | Lightweight containerized agents, Anthropic Agents SDK integration, minimal codebase philosophy |
| **ZeroClaw** | Rust-level performance, trait-swappable components, deploy-anywhere infrastructure |
| **ClawRouter** | Smart LLM routing, cost optimization, multi-model support |
| **MemOS** | Persistent skill memory, cross-task skill reuse, memory scheduling |
| **OpenViking** | Context database, hierarchical context delivery, self-evolving resources |
| **AionUI** | Multi-agent cowork UI, support for multiple CLI agents |
| **E2B / OpenSandbox** | Secure sandboxed execution, container isolation |

## Core Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                        AnyClaw CLI                          │
│              (commands, REPL, terminal UI)                   │
└──────────────────────────┬──────────────────────────────────┘
                           │
┌──────────────────────────▼──────────────────────────────────┐
│                      Gateway Server                         │
│           (HTTP + WebSocket on localhost:18800)              │
│   ┌──────────┐  ┌─────────────┐  ┌───────────────────┐     │
│   │  Routes   │  │  Dashboard  │  │  WebSocket Hub    │     │
│   └──────────┘  └─────────────┘  └───────────────────┘     │
└──────────────────────────┬──────────────────────────────────┘
                           │
┌──────────────────────────▼──────────────────────────────────┐
│                     Orchestrator                            │
│          (multi-agent coordination + routing)               │
│   ┌──────────────┐  ┌──────────────┐  ┌──────────────┐     │
│   │   Agent A     │  │   Agent B    │  │   Agent N    │     │
│   │ (observe →    │  │ (specialist) │  │ (spawned)    │     │
│   │  plan → act → │  │              │  │              │     │
│   │  reflect)     │  │              │  │              │     │
│   └──────┬───────┘  └──────┬───────┘  └──────┬───────┘     │
└──────────┼─────────────────┼─────────────────┼──────────────┘
           │                 │                 │
┌──────────▼─────────────────▼─────────────────▼──────────────┐
│                    Shared Services                           │
│                                                             │
│ ┌────────────┐ ┌──────────┐ ┌────────┐ ┌────────────────┐  │
│ │  Memory    │ │   GSD    │ │ Tools  │ │    Safety      │  │
│ │  (3-tier)  │ │ (specs)  │ │ (MCP)  │ │  (sandbox)     │  │
│ └─────┬──────┘ └────┬─────┘ └───┬────┘ └───────┬────────┘  │
│       │              │           │               │           │
│ ┌─────▼──────┐ ┌────▼─────┐ ┌──▼────┐ ┌───────▼────────┐  │
│ │ Short-term │ │ Planner  │ │Builtin│ │  Permissions   │  │
│ │ Episodic   │ │ Executor │ │ Shell │ │  Guardrails    │  │
│ │ Semantic   │ │ Verifier │ │ File  │ │  Sandbox Exec  │  │
│ │ Alignment  │ │          │ │ Git   │ │                │  │
│ └────────────┘ └──────────┘ │ HTTP  │ └────────────────┘  │
│                              │Browser│                      │
│                              │  MCP  │                      │
│                              └───────┘                      │
└─────────────────────────┬───────────────────────────────────┘
                          │
┌─────────────────────────▼───────────────────────────────────┐
│                   LLM Provider Layer                        │
│ ┌───────────┐ ┌──────────┐ ┌────────┐ ┌─────────────────┐  │
│ │ Anthropic │ │  OpenAI  │ │ Ollama │ │  Router (cost   │  │
│ │  Claude   │ │ / compat │ │ (local)│ │  + capability)  │  │
│ └───────────┘ └──────────┘ └────────┘ └─────────────────┘  │
└─────────────────────────────────────────────────────────────┘
```

## Memory System (OpenStinger-inspired, built-in)

Three additive tiers, all stored locally:

| Tier | Name | Purpose | Storage |
|------|------|---------|---------|
| 1 | **Short-Term** | Current session context, working memory | In-memory |
| 2 | **Episodic** | Session transcripts, searchable episodes | SQLite + vector embeddings |
| 3 | **Semantic Vault** | Distilled self-knowledge: identity, domain expertise, preferences, constraints | SQLite + JSON vault notes |
| 3+ | **Alignment / Gradient** | Drift detection, alignment scoring, behavioral monitoring | SQLite audit log |

Key improvements over raw OpenStinger:
- No FalkorDB dependency — uses SQLite for simplicity (single-file, no Docker required for memory)
- Vector search via embedded SQLite extensions or in-process HNSW
- Vault notes are plain Markdown files (human-readable, version-controllable)

## GSD (Get Shit Done) Integration

Built-in spec-driven task management inspired by GSD:

1. **Discuss phase** — define requirements in natural language
2. **Plan phase** — break into atomic, verifiable tasks with clear acceptance criteria
3. **Execute phase** — each task runs in fresh context (anti-rot), produces atomic Git commits
4. **Verify phase** — automated checks + human review gates

Key behaviors:
- Tasks have explicit `DONE` criteria (not fuzzy "looks good")
- Context is compacted between tasks to prevent degradation
- Each task produces a single, revertable commit
- Verification happens before marking complete

## Safety & Guardrails

```
┌─────────────────────────────────┐
│       Permission Layers         │
│                                 │
│  Layer 1: Tool Allowlists       │
│    → which tools agent can use  │
│                                 │
│  Layer 2: Path Restrictions     │
│    → which dirs are writable    │
│                                 │
│  Layer 3: Command Filters       │
│    → blocklist dangerous cmds   │
│                                 │
│  Layer 4: Confirmation Gates    │
│    → human approval for risky   │
│    → operations (destructive,   │
│    → external, irreversible)    │
│                                 │
│  Layer 5: Sandbox Execution     │
│    → optional Docker/container  │
│    → isolation for untrusted    │
│    → agent actions              │
└─────────────────────────────────┘
```

## Configuration

Single `anyclaw.yaml` at workspace root:

```yaml
agent:
  model: anthropic/claude-opus-4-6
  fallback_model: ollama/llama3
  thinking: high

memory:
  episodic_db: ~/.anyclaw/memory.db
  vault_dir: ~/.anyclaw/vault/
  alignment: true

safety:
  sandbox: false                    # Docker sandbox for tool execution
  allowed_paths:
    - ~/projects/
    - /tmp/anyclaw/
  blocked_commands:
    - rm -rf /
    - dd if=
  confirmation_required:
    - git push
    - docker rm

gsd:
  auto_commit: true
  verify_before_done: true
  max_task_context_tokens: 50000

gateway:
  port: 18800
  bind: 127.0.0.1                  # localhost only

tools:
  builtin:
    shell: true
    filesystem: true
    git: true
    browser: false                  # opt-in
    http: true
  mcp_servers: {}                   # external MCP servers
```

## Workspace Structure

```
~/.anyclaw/
├── anyclaw.yaml              # Main config
├── memory.db                 # SQLite memory database
├── vault/                    # Semantic vault notes (Markdown)
│   ├── identity.md
│   ├── expertise.md
│   ├── preferences.md
│   └── constraints.md
├── sessions/                 # Session transcripts (JSONL)
├── workspace/
│   ├── AGENTS.md             # Agent definitions
│   ├── SOUL.md               # Agent identity/personality
│   ├── TOOLS.md              # Tool configurations
│   └── skills/               # Loaded skills
└── logs/                     # Operation logs
```

## Tech Stack

- **Runtime**: Node.js >= 22 (TypeScript)
- **Database**: SQLite (via better-sqlite3) — zero external deps
- **Web Server**: Fastify (HTTP + WebSocket gateway)
- **LLM SDKs**: @anthropic-ai/sdk, openai (for compatible providers)
- **CLI**: Commander.js + Ink (React-based terminal UI)
- **Build**: tsup (fast TypeScript bundler)

---

## Phase 1 Runtime Architecture

### Module Responsibilities

| Module | File | Phase 1 Role |
|---|---|---|
| **Bootstrap** | `src/index.ts` | Wires all components; CLI entry point |
| **Orchestrator** | `src/core/orchestrator.ts` | Agent lifecycle: create/list/remove/delegate/swarm |
| **Agent** | `src/core/agent.ts` | Message loop, tool execution, session transcripts |
| **Tool Registry** | `src/tools/index.ts` | Register/execute built-in and plugin tools |
| **Memory** | `src/memory/index.ts` | Episodic SQLite DB, vault Markdown files, session store |
| **GSD** | `src/gsd/index.ts` | Spec/task management with SQLite persistence |
| **Safety** | `src/safety/index.ts` | Tool allow/deny lists, command blocking, rate limiting, SSRF policy |
| **Gateway** | `src/gateway/index.ts` | Fastify HTTP + WebSocket server; all API routes |
| **Plugins** | `src/plugins/index.ts` | Hot-loadable extensions (tools, providers, skills, hooks) |
| **Hooks** | `src/hooks/index.ts` | Lifecycle event dispatch across the runtime |
| **LLM Router** | `src/llm/index.ts` | Multi-provider routing, rate limiting, embeddings |
| **Cloud** | `src/cloud/index.ts` | Nexus Cloud manifest generation, registration, discovery |
| **Config** | `src/config/index.ts` | Zod-validated YAML config loading |

### Phase 1 CLI Command Surface

```
anyclaw chat              Interactive REPL with the default agent
anyclaw message <text>    One-shot message
anyclaw gateway           Start the Fastify HTTP/WS gateway
anyclaw health            Show runtime health (memory, agents, safety)
anyclaw status            Full runtime status summary

anyclaw agents list       List active agents
anyclaw agents create     Create a new agent
anyclaw agents remove     Remove an agent by ID
anyclaw agents status     Show state of a specific agent

anyclaw sessions list     List saved sessions
anyclaw sessions show     Show messages of a session

anyclaw tools             List registered tools

anyclaw plugins list      List loaded plugins
anyclaw plugins load      Load a plugin from a directory or file

anyclaw safety            Show current safety and permission settings

anyclaw cloud status      Show the local Nexus Cloud manifest
anyclaw cloud validate    Validate the manifest before registration
anyclaw cloud register    Register with Nexus Cloud
anyclaw cloud deregister  Remove registration from Nexus Cloud

anyclaw doctor            Check system configuration
anyclaw validate          Validate the YAML config file
anyclaw skill             Manage skills
anyclaw config            View/edit configuration
anyclaw init              Zero-config quickstart
```

### Phase 1 Gateway API Routes

| Method | Route | Description |
|---|---|---|
| GET | `/api/health/live` | Liveness probe |
| GET | `/api/health/ready` | Readiness probe (includes agent/memory/tool counts) |
| GET | `/api/status` | Full runtime status snapshot |
| GET | `/api/agents` | List active agents |
| GET | `/api/agents/:id` | Individual agent detail |
| GET | `/api/sessions` | List saved sessions |
| GET | `/api/sessions/:id` | Session detail with messages |
| GET | `/api/tools` | List registered tools |
| GET | `/api/safety` | Current permissions and recent action log |
| GET | `/api/plugins` | List loaded plugins |
| GET | `/api/hooks` | List registered hooks |
| GET | `/api/recipes` | List registered recipes |
| GET | `/api/skills` | List registered skills |
| GET | `/api/memory` | Memory status |
| GET | `/api/gsd/tasks` | GSD task list |
| POST | `/api/chat` | One-shot chat request |
| WS | `/ws` | WebSocket streaming gateway |
| GET | `/health` | Backward-compatible health check |
| GET | `/` | Minimal status dashboard (HTML) |

### Nexus Cloud Registration

Nexusclaw implements the `nexus-cloud/1.0` discovery protocol. When `cloud.enabled = true`:

1. On gateway startup, `registerWithNexusCloud()` POSTs the service manifest to `{cloud.endpoint}/api/v1/tools`.
2. The manifest includes service identity, health URLs, and declared capabilities.
3. The `anyclaw cloud` commands allow manual management of registration state.

**Key config options:**

```yaml
cloud:
  enabled: true
  endpoint: "https://cloud.nexus.example.com"
  api_key: "..."
  service_id: "anyclaw"
  capabilities: ["agent-runtime", "tool-registry", "memory", "gateway", "health"]
  auto_register: true
```

### Phase 1 Safety Model

Safety is enforced at two layers:

1. **Tool Registry** (`src/tools/index.ts`): Tools check `context.permissions.allowedTools` and `deniedTools` before execution.
2. **Safety Manager** (`src/safety/index.ts`): Validates every agent action against:
   - Tool allow/deny lists
   - Blocked shell command patterns
   - Paths outside `allowedPaths`
   - Per-minute rate limits
   - Confirmation gates for destructive operations
   - SSRF host rules for outbound HTTP

Three built-in sandbox profiles (`strict`, `standard`, `permissive`) provide default settings that can be overridden in config.

### Phase 1 Plugin Contract

See [NEXUSCLAW_PLUGIN_SPEC.md](NEXUSCLAW_PLUGIN_SPEC.md) for the full plugin contract. Summary:

- Plugins are loaded from local directories (with `plugin.json`) or npm packages.
- They export `tools`, `skills`, `providers`, `hooks`, and/or a `setup(context)` function.
- Plugin directories are configured via `config.plugins.dirs`.
- The `anyclaw plugins` commands and `GET /api/plugins` route expose loaded plugin state.
