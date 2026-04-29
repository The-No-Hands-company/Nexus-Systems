# 🦀 AnyClaw

The everything claw — a comprehensive, self-hosted, federated local-first AI agent framework that combines the best features from the entire claw ecosystem into a single, unified application.

## What's Inside

- **Multi-provider LLM routing** — Anthropic, OpenAI, Ollama with automatic fallback
- **3-tier memory system** (OpenStinger-inspired) — short-term context, episodic SQLite store, semantic vault with alignment tracking
- **GSD task management** — spec-driven atomic tasks with fresh context, verification gates, and progress tracking
- **Safety guardrails** — path restrictions, command blocklists, confirmation gates, full audit logging
- **Built-in tools** — shell, filesystem, git, HTTP, memory search/store, grep
- **Multi-agent orchestration** — spawn sub-agents with different models and tool permissions
- **Local gateway** — HTTP + WebSocket server with embedded dashboard
- **Extended thinking** — configurable thinking depth (off/minimal/low/medium/high)
- **Plugin system** — extend the runtime with local or npm plugins
- **Nexus Cloud registration** — register with Nexus Cloud for discovery

## Quick Start

```bash
# Install dependencies
npm install

# Copy and configure environment
cp .env.example .env
# Edit .env with your API keys

# Build
npm run build

# Check system health
node dist/index.js doctor

# Start interactive chat
node dist/index.js chat

# Start the gateway server
node dist/index.js gateway
```

## CLI Commands

### Core

| Command | Description |
|---|---|
| `chat` | Interactive REPL with the default agent |
| `message <text>` | Send a one-shot message |
| `pipe` | Read input from stdin |
| `gateway` | Start the HTTP + WebSocket gateway server |
| `status` | Full runtime status summary |
| `health` | Runtime health (memory, agents, safety) |
| `doctor` | Check system configuration and requirements |
| `validate` | Validate the YAML config file |
| `init` | Zero-config quickstart |

### Agents

```bash
node dist/index.js agents list
node dist/index.js agents create --name "Researcher" [--model <model>]
node dist/index.js agents remove <id>
node dist/index.js agents status <id>
```

### Sessions

```bash
node dist/index.js sessions list [--limit 20]
node dist/index.js sessions show <id> [--limit 20]
```

### Tools

```bash
node dist/index.js tools          # list all registered tools
node dist/index.js tools --json   # output as JSON
```

### Plugins

```bash
node dist/index.js plugins list                           # list loaded plugins
node dist/index.js plugins load ./my-plugin               # load from a directory
node dist/index.js plugins load ./tools/custom.js         # load from a file
```

See [NEXUSCLAW_PLUGIN_SPEC.md](NEXUSCLAW_PLUGIN_SPEC.md) for the plugin manifest format.

### Safety

```bash
node dist/index.js safety          # show permissions, blocked commands, action log
node dist/index.js safety --json   # output as JSON
```

### Skills

```bash
node dist/index.js skill add <source>
node dist/index.js skill list
node dist/index.js skill remove <id>
```

### Config

```bash
node dist/index.js config show     # print resolved config
node dist/index.js config edit     # open config in $EDITOR
```

### Cloud

```bash
node dist/index.js cloud status      # show local Nexus Cloud manifest
node dist/index.js cloud validate    # validate the manifest
node dist/index.js cloud register    # register with Nexus Cloud
node dist/index.js cloud deregister  # remove cloud registration
```

Cloud command flags:
- `--endpoint <url>` — override `cloud.endpoint` from config
- `--api-key <key>` — set the bearer token
- `--force` — act even when `cloud.enabled` is `false`

## Gateway Endpoints

Once `gateway` is running:

| Method | Route | Description |
|---|---|---|
| GET | `/api/health/live` | Liveness probe |
| GET | `/api/health/ready` | Readiness probe |
| GET | `/api/status` | Full runtime status |
| GET | `/api/agents` | List active agents |
| GET | `/api/agents/:id` | Agent detail |
| GET | `/api/sessions` | List sessions |
| GET | `/api/sessions/:id` | Session detail |
| GET | `/api/tools` | List registered tools |
| GET | `/api/safety` | Permissions and action log |
| GET | `/api/plugins` | List loaded plugins |
| GET | `/api/memory/status` | Memory status |
| GET | `/api/memory/vault` | Vault notes |
| POST | `/api/memory/search` | Search episodic memory |
| POST | `/api/chat` | One-shot chat |
| WS  | `/ws` | WebSocket streaming |
| GET | `/` | Embedded dashboard |

## Configuration

AnyClaw looks for config in this order:
1. `./anyclaw.yaml` (project directory)
2. `./anyclaw.yml`
3. `~/.anyclaw/anyclaw.yaml`

See [anyclaw.yaml](anyclaw.yaml) for the full example config.

### Key settings

| Setting | Default | Description |
|---------|---------|-------------|
| `agent.model` | `anthropic/claude-sonnet-4-6` | Default LLM model |
| `agent.thinking` | `medium` | Thinking depth |
| `memory.episodic_db` | `~/.anyclaw/memory.db` | SQLite memory store |
| `memory.vault_dir` | `~/.anyclaw/vault/` | Markdown vault directory |
| `gateway.port` | `18800` | Local server port |
| `gateway.bind` | `127.0.0.1` | Bind address (localhost only) |
| `safety.sandbox_profile` | `standard` | Safety profile: `strict`, `standard`, `permissive` |
| `cloud.enabled` | `false` | Enable Nexus Cloud registration |
| `cloud.endpoint` | `""` | Nexus Cloud registration endpoint |
| `cloud.api_key` | `""` | Optional Nexus Cloud bearer token |
| `cloud.service_id` | `anyclaw` | Registered service identifier |
| `plugins.dirs` | `[]` | Plugin directories to auto-discover |

When `cloud.enabled` is `true` and `cloud.endpoint` is set, the gateway will attempt to register the service with Nexus Cloud at startup.

## Architecture

```
┌─────────────────────────────────────────────────┐
│                   CLI / Gateway                  │
├─────────────────────────────────────────────────┤
│                  Orchestrator                    │
│         (agent creation, routing)                │
├──────────┬──────────┬───────────┬───────────────┤
│  Agent   │  Memory  │   GSD     │    Safety     │
│  Loop    │  3-tier  │   Tasks   │   Guardrails  │
├──────────┴──────────┴───────────┴───────────────┤
│              LLM Router (multi-provider)         │
├─────────────────────────────────────────────────┤
│           Tool Registry (MCP-compatible)         │
├─────────────────────────────────────────────────┤
│         Plugin System (tools/skills/hooks)       │
└─────────────────────────────────────────────────┘
```

See [ARCHITECTURE.md](ARCHITECTURE.md) for the full Phase 1 design document.

## Project Structure

```
src/
├── index.ts           # CLI entry point + all commands
├── cloud/             # Nexus Cloud manifest + registration
├── config/            # YAML config loading + Zod validation
├── core/
│   ├── types.ts       # All TypeScript types
│   ├── agent.ts       # Core agent loop (observe→plan→act→reflect)
│   └── orchestrator.ts # Multi-agent coordination
├── gateway/           # Fastify HTTP + WebSocket server + dashboard
├── hooks/             # Lifecycle event dispatch
├── llm/               # LLM provider abstraction
├── memory/            # 3-tier memory (short-term, episodic, vault)
├── plugins/           # Plugin loader + manifest processing
├── recipes/           # Recipe registry and execution
├── safety/            # Path/command safety + audit logging
└── tools/             # Tool registry + built-in tools
examples/
└── plugins/
    └── greeting/      # Sample plugin (hello world)
workspace/
├── SOUL.md            # Agent personality/identity
├── AGENTS.md          # Agent definitions
└── TOOLS.md           # Tool documentation
```

## Memory System

Inspired by OpenStinger's 3-tier architecture, simplified to use SQLite:

| Tier | Purpose | Storage |
|------|---------|---------|
| **Short-term** | Current conversation context | In-memory |
| **Episodic** | Past interactions, searchable | SQLite |
| **Vault** | Permanent knowledge + alignment tracking | SQLite + Markdown files |

## GSD (Get Shit Done)

Break work into specs with atomic tasks:

```
Spec: "Refactor auth module"
├── Task 1: Read current auth implementation ✓
├── Task 2: Extract token validation to separate file ✓
├── Task 3: Add refresh token support → in progress
└── Task 4: Update tests (pending)
```

Each task gets fresh context. Verification gates prevent marking tasks done without checking results.

## Requirements

- Node.js >= 22
- At least one LLM API key (Anthropic recommended, or Ollama for local models)

## What's Inside

- **Multi-provider LLM routing** — Anthropic, OpenAI, Ollama with automatic fallback
- **3-tier memory system** (OpenStinger-inspired) — short-term context, episodic SQLite store, semantic vault with alignment tracking
- **GSD task management** — spec-driven atomic tasks with fresh context, verification gates, and progress tracking
- **Safety guardrails** — path restrictions, command blocklists, confirmation gates, full audit logging
- **Built-in tools** — shell, filesystem, git, HTTP, memory search/store, grep
- **Multi-agent orchestration** — spawn sub-agents with different models and tool permissions
- **Local gateway** — HTTP + WebSocket server with embedded dashboard
- **Extended thinking** — configurable thinking depth (off/minimal/low/medium/high)

## Quick Start

```bash
# Install dependencies
npm install

# Copy and configure environment
cp .env.example .env
# Edit .env with your API keys

# Build
npm run build

# Check system health
node dist/index.js doctor

# Start interactive chat
node dist/index.js chat

# Start the gateway server
node dist/index.js gateway

## Gateway status endpoints
Once the gateway is running, use the following endpoints to verify service status:

- `GET /api/health/live` — liveness probe
- `GET /api/health/ready` — readiness probe with memory and tool status
- `GET /api/status` — full runtime status payload with agents, memory, safety, tools, and plugins
- `GET /api/agents` — list active agents
- `GET /api/agents/:id` — inspect a specific agent

# Send a one-shot message
node dist/index.js message "explain this codebase"

# View system status
node dist/index.js status

# Nexus Cloud registration
Use the cloud commands to inspect, validate, register, and deregister the local gateway.

Show the local manifest before registration:

```bash
node dist/index.js cloud status
```

Validate the local Nexus Cloud manifest:

```bash
node dist/index.js cloud validate
```

Register with Nexus Cloud:

```bash
node dist/index.js cloud register
```

Deregister from Nexus Cloud:

```bash
node dist/index.js cloud deregister
```

Overrides:
- `--endpoint <url>` to set the Nexus Cloud API endpoint
- `--api-key <key>` to provide a registration token
- `--force` to register or deregister even when `cloud.enabled` is false
- `--force` to register/deregister even if `cloud.enabled` is false in config

## Configuration

AnyClaw looks for config in this order:
1. `./anyclaw.yaml` (project directory)
2. `./anyclaw.yml`
3. `~/.anyclaw/anyclaw.yaml`

See [anyclaw.yaml](anyclaw.yaml) for the full example config.

### Key settings

| Setting | Default | Description |
|---------|---------|-------------|
| `agent.model` | `anthropic/claude-sonnet-4-6` | Default LLM model |
| `agent.thinking` | `medium` | Thinking depth |
| `memory.episodic_db` | `~/.anyclaw/memory.db` | SQLite memory store |
| `memory.vault_dir` | `~/.anyclaw/vault/` | Markdown vault directory |
| `gateway.port` | `18800` | Local server port |
| `gateway.bind` | `127.0.0.1` | Bind address (localhost only) |
| `cloud.enabled` | `false` | Enable Nexus Cloud registration |
| `cloud.endpoint` | `""` | Nexus Cloud registration endpoint |
| `cloud.api_key` | `""` | Optional Nexus Cloud bearer token |
| `cloud.service_id` | `anyclaw` | Registered service identifier |

When `cloud.enabled` is `true` and `cloud.endpoint` is set, the gateway will attempt to register the service with Nexus Cloud at startup.

## Architecture

```
┌─────────────────────────────────────────────────┐
│                   CLI / Gateway                  │
├─────────────────────────────────────────────────┤
│                  Orchestrator                    │
│         (agent creation, routing)                │
├──────────┬──────────┬───────────┬───────────────┤
│  Agent   │  Memory  │   GSD     │    Safety     │
│  Loop    │  3-tier  │   Tasks   │   Guardrails  │
├──────────┴──────────┴───────────┴───────────────┤
│              LLM Router (multi-provider)         │
├─────────────────────────────────────────────────┤
│           Tool Registry (MCP-compatible)         │
└─────────────────────────────────────────────────┘
```

See [ARCHITECTURE.md](ARCHITECTURE.md) for the full design document.

## Project Structure

```
src/
├── index.ts           # CLI entry point
├── config/            # YAML config loading + Zod validation
├── core/
│   ├── types.ts       # All TypeScript types
│   ├── agent.ts       # Core agent loop (observe→plan→act→reflect)
│   └── orchestrator.ts # Multi-agent coordination
├── llm/               # LLM provider abstraction
├── memory/            # 3-tier memory (short-term, episodic, vault)
├── tools/             # Tool registry + built-in tools
├── gsd/               # GSD spec/task management
├── safety/            # Path/command safety + audit logging
└── gateway/           # Fastify HTTP + WebSocket server
workspace/
├── SOUL.md            # Agent personality/identity
├── AGENTS.md          # Agent definitions
└── TOOLS.md           # Tool documentation
```

## Memory System

Inspired by OpenStinger's 3-tier architecture, simplified to use SQLite:

| Tier | Purpose | Storage |
|------|---------|---------|
| **Short-term** | Current conversation context | In-memory |
| **Episodic** | Past interactions, searchable | SQLite |
| **Vault** | Permanent knowledge + alignment tracking | SQLite + Markdown files |

## GSD (Get Shit Done)

Break work into specs with atomic tasks:

```
Spec: "Refactor auth module"
├── Task 1: Read current auth implementation ✓
├── Task 2: Extract token validation to separate file ✓
├── Task 3: Add refresh token support → in progress
└── Task 4: Update tests (pending)
```

Each task gets fresh context. Verification gates prevent marking tasks done without checking results.

## Requirements

- Node.js >= 22
- At least one LLM API key (Anthropic recommended, or Ollama for local models)
