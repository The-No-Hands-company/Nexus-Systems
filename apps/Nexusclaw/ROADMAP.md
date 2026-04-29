# AnyClaw Feature Roadmap

> Comprehensive gap analysis against OpenClaw and 30+ ecosystem variants (NanoClaw, Nanobot,
> IronClaw, ZeroClaw, PicoClaw, KiloClaw, TinyClaw, NullClaw, DroidClaw, MicroClaw, Autobot,
> HermitClaw, AstrBot, BabyClaw, LettaBot, SupaClaw, Clawlet, safeclaw, shrew, moxxy, memU,
> Squaer, Flowly AI, and others).
> Last updated: 2026-03-11

for even more AI and "claw" ecosystem context, see the [Nexus AI Platform Taxonomy](NEXUS_AI_PLATFORM_TAXONOMY.md) and look into https://www.youtube.com/@JulianGoldieSEO/videos for deep dives on the landscape.

---

## Legend

| Symbol | Meaning |
|--------|---------|
| ✅ | Implemented and working |
| ⚠️ | Partial / stub — needs significant work |
| ❌ | Not started |

---

## 1. Current State — What Works Today

### Core Agent Loop
| Feature | Status | Notes |
|---------|--------|-------|
| Observe → Plan → Act → Reflect cycle | ✅ | Up to 20 tool iterations per turn |
| Multi-turn conversation | ✅ | Session-based context tracking |
| LLM fallback (primary → fallback model) | ✅ | Automatic retry on failure |
| Configurable thinking depth | ✅ | off / minimal / low / medium / high |
| System prompt injection (soul, vault, tools, safety) | ✅ | Dynamic prompt construction |
| Context compaction via LLM summarization | ✅ | `/compact` in REPL |

### LLM Providers
| Feature | Status | Notes |
|---------|--------|-------|
| Anthropic (Claude) | ✅ | Native SDK |
| OpenAI-compatible | ✅ | Works with OpenAI, any compatible API |
| Ollama (local models) | ✅ | Via OpenAI-compatible provider |
| Prefix-based routing (`anthropic/`, `openai/`, `ollama/`) | ✅ | |

### Memory
| Feature | Status | Notes |
|---------|--------|-------|
| Tier 1 — Short-term (in-memory session context) | ✅ | Map-based |
| Tier 2 — Episodic (SQLite, searchable transcripts) | ✅ | SQL LIKE search only |
| Tier 3 — Semantic vault (identity, expertise, preferences) | ✅ | MD files + DB |
| Alignment tracking & drift detection | ✅ | Alerts if pass rate < 85% |
| Session ingestion to episodic memory | ✅ | Auto on session end |

### Tools (25+ built-in + memory/session/GSD tools)
| Feature | Status | Notes |
|---------|--------|-------|
| Shell execution with command blocking | ✅ | |
| File read (with line ranges) | ✅ | |
| File write (with parent dir creation) | ✅ | |
| Directory listing | ✅ | |
| Text search (grep-based) | ✅ | |
| Git (with dangerous op guards) | ✅ | |
| HTTP request (SSRF-protected) | ✅ | |
| Memory search & store | ✅ | |
| Web fetch (HTML → text extraction) | ✅ | Phase 3.3 |
| Apply patch (safe text replacement) | ✅ | Phase 3.4 |
| Process spawn/list/kill | ✅ | Phase 3.5 |
| Web search (5 backends: SearXNG, Brave, Tavily, Jina, DDG) | ✅ | Phase 3.2 |
| Browser automation (7 Playwright tools) | ✅ | Phase 3.1, peer dep |
| Recipe list/run | ✅ | Phase 13.6 |
| Agents list (introspection) | ✅ | Phase 3.12 |
| Session list/delete/tag/prune | ✅ | Phase 8 |

### GSD (Get Stuff Done) — Spec-Driven Task Management
| Feature | Status | Notes |
|---------|--------|-------|
| Spec CRUD | ✅ | |
| Natural language → task plan parsing | ✅ | |
| Task lifecycle (pending → in-progress → verifying → done/failed) | ✅ | |
| Fresh context per task | ✅ | |
| Progress tracking (% complete) | ✅ | |
| 6 GSD tools exposed to agent | ✅ | |

### Safety
| Feature | Status | Notes |
|---------|--------|-------|
| Path whitelisting | ✅ | |
| Command blocklist | ✅ | |
| Tool allow/deny lists | ✅ | |
| Confirmation gates for destructive ops | ✅ | Interactive readline prompts wired |
| Audit logging (bounded 10k) | ✅ | |
| Elevated mode (bypass safety) | ✅ | Phase 9.2 |

### Multi-Agent
| Feature | Status | Notes |
|---------|--------|-------|
| Multiple agents sharing backend | ✅ | |
| Sub-agent spawning | ✅ | Role-specific configs |
| Orchestrated message routing | ✅ | |

### Gateway & UI
| Feature | Status | Notes |
|---------|--------|-------|
| REST API (health, chat, agents, memory, GSD, safety) | ✅ | Fastify |
| WebSocket real-time chat | ✅ | JSON protocol |
| Full web dashboard | ✅ | Phase 11.1 — multi-page SPA with chat, sessions, GSD, memory, tools, settings |

### CLI
| Feature | Status | Notes |
|---------|--------|-------|
| `chat` — Interactive REPL | ✅ | 25+ slash commands |
| `gateway` — HTTP+WS server | ✅ | Graceful shutdown |
| `message <text>` — One-shot mode | ✅ | Auto session ingest |
| `status` — System health | ✅ | |
| `doctor` — Config validation | ✅ | |
| `validate` — Config lint & validation | ✅ | Phase 10.8 |
| `skill` — Install/list/remove skills | ✅ | Phase 5.3 |
| `update` — Self-update from npm | ✅ | Phase 13.4 |
| `config` — Import/export configuration | ✅ | Phase 13.5 |
| `pipe` — Stdin/stdout scripting | ✅ | Phase 12.6 |
| `init` — Zero-config quickstart | ✅ | Phase 14.3 |

### Configuration
| Feature | Status | Notes |
|---------|--------|-------|
| YAML config with Zod validation | ✅ | |
| Multi-path config resolution | ✅ | project → home → .anyclaw |
| Environment variable overrides | ✅ | |
| Path expansion (`~/`) | ✅ | |
| Plugin config (dirs, packages) | ✅ | Phase 10.1 |
| Context files (.context.md) | ✅ | Phase 13.7 |

### Plugin System & Extensibility
| Feature | Status | Notes |
|---------|--------|-------|
| Plugin manager (npm, dirs, single files) | ✅ | Phase 10.1 |
| Hook runner (before/after tool, LLM, message, error) | ✅ | Phase 10.3 |
| Recipe system (5 built-in workflows) | ✅ | Phase 13.6 |
| Parallel tool execution (read-only concurrent) | ✅ | Phase 7.7 |

---

## 2. Gap Analysis — What's Missing

Below is a prioritized breakdown of features present in the claw ecosystem (primarily OpenClaw + ecosystem tools) that AnyClaw does not yet have.

---

### Phase 1 — Foundation Fixes (Critical)

These are deficiencies in the _existing_ code that limit real-world use.

| # | Feature | Current State | Target | Effort |
|---|---------|---------------|--------|--------|
| 1.1 | **Persistent sessions** | ~~In-memory only~~ | ✅ SQLite-backed sessions | ~~Small~~ Done |
| 1.2 | **Persistent GSD specs** | ~~In-memory only~~ | ✅ SQLite-backed GSD | ~~Small~~ Done |
| 1.3 | **FTS5 search** | ~~`LIKE` queries~~ | ✅ SQLite FTS5 | ~~Small~~ Done |
| 1.4 | **Interactive confirmation prompts** | ~~Stubbed~~ | ✅ Readline-based approval flow | ~~Small~~ Done |
| 1.5 | **Streaming responses** | ~~Not streaming~~ | ✅ Real streaming from LLM providers | ~~Medium~~ Done |
| 1.6 | **Rate limiting enforcement** | ~~Only warnings logged~~ | ✅ Sliding-window RPM + token bucket TPM, retry with exponential backoff | ~~Small~~ Done |
| 1.7 | **Extended thinking pass-through** | ~~Configured but not exposed~~ | ✅ Thinking blocks forwarded | ~~Small~~ Done |

---

### Phase 2 — Memory & Intelligence

OpenClaw's memory system far exceeds AnyClaw's. These upgrades bring parity.

| # | Feature | OpenClaw Reference | Effort |
|---|---------|-------------------|--------|
| 2.1 | **Vector embeddings** | ✅ OpenAI/Ollama embedding APIs via LLMRouter, wired to MemoryManager | ~~Medium~~ Done |
| 2.2 | **Hybrid search (BM25 + vector)** | ✅ FTS5 score + cosine sim, combined ranking (hybrid×0.5 + vec×0.3 + temporal×0.2) | ~~Medium~~ Done |
| 2.3 | **MMR re-ranking** (Maximal Marginal Relevance) | ✅ Lambda=0.7 diversity-aware deduplication | ~~Small~~ Done |
| 2.4 | **Temporal decay scoring** | ✅ Half-life=30 days recency weighting | ~~Small~~ Done |
| 2.5 | **sqlite-vec acceleration** | Native vector operations in SQLite (no external DB) | Medium |
| 2.6 | **Embedding cache** | ✅ FNV-hash keyed embedding_cache table in SQLite | ~~Small~~ Done |
| 2.7 | **Session memory search** | ✅ session_search tool + session_id filter on memory_search | ~~Small~~ Done |
| 2.8 | **Memory compaction with flush** | ✅ `compactSession()` uses LLM to summarize, then `autoFlushFromSession()` extracts decisions/learnings to vault heuristically; called on every turn and on manual compact | ~~Medium~~ Done |
| 2.9 | **QMD experimental backend** | Quantized Memory Database for large-scale vault | Large |

---

### Phase 3 — Tools Expansion

OpenClaw exposes 20+ tools. AnyClaw has 8 core + memory tools. These fill the gaps.

| # | Tool | Description | Effort |
|---|------|-------------|--------|
| 3.1 | **browser** | ✅ Playwright — navigate, click, type, screenshot, snapshot, eval, close | ~~Large~~ Done |
| 3.2 | **web_search** | ✅ 5 backends — SearXNG, Brave, Tavily, Jina, DuckDuckGo | ~~Medium~~ Done |
| 3.3 | **web_fetch** | ✅ Fetch & extract text from URLs | ~~Small~~ Done |
| 3.4 | **apply_patch** | ✅ Safe text replacement with uniqueness check | ~~Small~~ Done |
| 3.5 | **process** | ✅ Spawn, list, kill background processes | ~~Medium~~ Done |
| 3.6 | **image** | ✅ Multi-modal LLM vision analysis (PNG/JPG/GIF/WebP/BMP/SVG) | ~~Medium~~ Done |
| 3.7 | **pdf** | ✅ PDF text extraction (pdftotext → mutool → BT/ET parser fallback) | ~~Small~~ Done |
| 3.8 | **canvas (A2UI)** | ✅ `artifact_create/list/delete` tools + `/api/artifacts/:id` endpoint + dashboard iframe panel + artifact links in chat (parseArtifacts `[ARTIFACT:id:title]` syntax) | ~~Medium~~ Done |
| 3.9 | **cron** | ✅ `cron_schedule`, `cron_list`, `cron_cancel` — 5-field cron syntax, next-run preview | ~~Medium~~ Done |
| 3.10 | **message** | Cross-platform messaging (for multi-channel scenarios) | Medium |
| 3.11 | **sessions** | ✅ Session list/delete/tag/prune tools | ~~Small~~ Done |
| 3.12 | **agents_list** | ✅ Agent introspection tool | ~~Small~~ Done |
| 3.13 | **gateway** (self-management) | ✅ `gateway_info`, `agent_config_get`, `agent_config_set`, `agent_restart` tools registered at bootstrap | ~~Medium~~ Done |

---

### Phase 4 — MCP (Model Context Protocol) Integration

Config supports MCP server definitions but they aren't wired in.

| # | Feature | Description | Effort |
|---|---------|-------------|--------|
| 4.1 | **MCP client** | ✅ stdio + SSE transports | ~~Medium~~ Done |
| 4.2 | **MCP tool discovery** | ✅ Auto-register discovered tools | ~~Medium~~ Done |
| 4.3 | **MCP tool execution** | ✅ Transparent tool routing | ~~Small~~ Done |
| 4.4 | **MCP resource access** | ✅ listResources/readResource/listPrompts/getPrompt + auto-discovery | ~~Small~~ Done |

---

### Phase 5 — Skills System

OpenClaw has a full skills framework. AnyClaw has none.

| # | Feature | Description | Effort |
|---|---------|-------------|--------|
| 5.1 | **SKILL.md loader** | ✅ Parses SKILL.md files from configured paths + auto-discovery | ~~Medium~~ Done |
| 5.2 | **Skill gating** | ✅ Enable/disable per skill | ~~Small~~ Done |
| 5.3 | **Skill installer** | ✅ `anyclaw skill install/list/remove` — git clone or local copy | ~~Medium~~ Done |
| 5.4 | **Shared skills** | ✅ `agents` field on skills — empty = shared, list = per-agent only | ~~Small~~ Done |
| 5.5 | **ClawHub registry** | Browse and install community skills from a registry | Large |
| 5.6 | **Skill-provided tools** | ✅ Skills register tools into agent on load | ~~Medium~~ Done |

---

### Phase 6 — Slash Commands

AnyClaw now has 25+ slash commands (up from 2).

| # | Command | Description | Effort |
|---|---------|-------------|--------|
| 6.1 | `/help` | ✅ Show all commands | ~~Small~~ Done |
| 6.2 | `/thinking [level]` | ✅ Get/set thinking depth | ~~Small~~ Done |
| 6.3 | `/model <name>` | ✅ Switch LLM model | ~~Small~~ Done |
| 6.4 | `/config [key]` | ✅ View config values | ~~Small~~ Done |
| 6.5 | `/elevated [on/off]` | ✅ Toggle elevated mode | ~~Small~~ Done |
| 6.6 | `/exec <cmd>` | ✅ Direct shell execution | ~~Small~~ Done |
| 6.7 | `/debug` | ✅ Show debug info | ~~Small~~ Done |
| 6.8 | `/kill <agent>` | ✅ Terminate sub-agent | ~~Small~~ Done |
| 6.9 | `/steer <text>` | ✅ Inject steering instruction | ~~Small~~ Done |
| 6.10 | `/memory [query]` | ✅ Search memory or show stats | ~~Small~~ Done |
| 6.11 | `/sessions` / `/resume` / `/new` | ✅ Full session management | ~~Small~~ Done |
| 6.12 | `/agents` | ✅ List active agents | ~~Small~~ Done |
| 6.13 | `/gsd` | ✅ List GSD specs & progress | ~~Small~~ Done |
| 6.14 | `/export [path]` | ✅ Export session to JSON | ~~Small~~ Done |
| 6.15 | `/import <file>` | ✅ Import session from JSON | ~~Small~~ Done |
| 6.16 | `/undo` | ✅ Undo last file change (tracks writes + patches, restores or deletes) | ~~Medium~~ Done |
| 6.17 | `/diff` | ✅ Show all file changes with type, path, and time ago | ~~Small~~ Done |
| 6.18 | `/clear` | ✅ Clear screen | ~~Small~~ Done |
| 6.19 | `/compact` | ✅ Compress session context | Done |
| 6.20 | `/status` | ✅ Memory & alignment stats | Done |
| 6.21 | `/vault` | ✅ Show vault context | Done |
| 6.22 | `/tools` | ✅ List available tools | Done |
| 6.23 | `/skills` | ✅ List loaded skills | Done |
| 6.24 | `/recipe [id]` | ✅ List or run recipes | Done |
| 6.25 | `/hooks` | ✅ List registered hooks | Done |
| 6.26 | `/version` | ✅ Show version | Done |

---

### Phase 7 — Advanced Agent Features

| # | Feature | Description | Effort |
|---|---------|-------------|--------|
| 7.1 | **Agent-level tool policies** | ✅ allowedTools/deniedTools config, 3-layer enforcement (LLM filter + agent block + safety) | ~~Small~~ Done |
| 7.2 | **Agent-level sandbox** | ✅ `sandboxRoot` config — file/shell tools jailed to root dir; escape throws `Sandbox violation` | ~~Medium~~ Done |
| 7.3 | **Agent bindings / routing rules** | ✅ Pattern-based routing (substring / glob / regex) with priority — `agent_route_add/list/remove` tools; `sendMessage` auto-routes | ~~Medium~~ Done |
| 7.4 | **Agent delegation** | ✅ `delegate_to_agent` tool — delegate to existing agent or spawn ephemeral sub-agent; orchestrator.delegate() ingest + cleanup | ~~Medium~~ Done |
| 7.5 | **Agent personas** | ✅ Structured PersonaConfig (name, role, personality, voice, constraints, expertise) | ~~Small~~ Done |
| 7.6 | **Agent-to-agent messaging** | ✅ `send_to_agent` tool + `orchestrator.sendToAgent()` — direct inter-agent message passing with from/to context, webhook event | ~~Medium~~ Done |
| 7.7 | **Parallel tool execution** | ✅ Read-only tools run concurrently via Promise.allSettled | ~~Medium~~ Done |
| 7.8 | **Conversation branching** | Fork sessions for exploration, merge back | Large |

---

### Phase 8 — Session Management

OpenClaw has sophisticated session management. AnyClaw's is basic.

| # | Feature | Description | Effort |
|---|---------|-------------|--------|
| 8.1 | **Session persistence to disk** | ✅ SQLite-backed + `--last` auto-resume + recent sessions display | ~~Medium~~ Done |
| 8.2 | **Session pruning** | ✅ `/sessions` prune + session_prune tool | ~~Small~~ Done |
| 8.3 | **Session compaction with memory flush** | ✅ Auto-flush to vault on compact | ~~Medium~~ Done |
| 8.4 | **Session history browser** | ✅ session_list tool + /sessions command | ~~Medium~~ Done |
| 8.5 | **Session export/import** | ✅ /export + /import commands | ~~Small~~ Done |
| 8.6 | **Session tagging** | ✅ session_tag tool | ~~Small~~ Done |

---

### Phase 9 — Security Hardening

OpenClaw has Docker-based sandboxing. AnyClaw has path/command restrictions only.

| # | Feature | Description | Effort |
|---|---------|-------------|--------|
| 9.1 | **Docker sandbox** | Run tool execution inside Docker containers | Large |
| 9.2 | **Elevated mode** | ✅ /elevated on|off — bypasses all safety checks | ~~Small~~ Done |
| 9.3 | **Network policies** | Restrict outbound network access per agent/tool | Medium |
| 9.4 | **Secret management** | ✅ AES-256-GCM encrypted store (`~/.anyclaw/secrets.enc`), `anyclaw secrets set/get/delete/list` | ~~Medium~~ Done |
| 9.5 | **SSRF policy expansion** | ✅ SSRFPolicy with blockedHosts/allowedHosts/allowPrivate/blockedUrlPatterns | ~~Small~~ Done |
| 9.6 | **Sandbox profiles** | ✅ 3 presets (strict/standard/permissive) with rate limits, blocked commands, confirmations | ~~Small~~ Done |

---

### Phase 10 — Developer Experience

| # | Feature | Description | Effort |
|---|---------|-------------|--------|
| 10.1 | **Plugin system** | ✅ PluginManager — load from npm, dirs, single files | ~~Large~~ Done |
| 10.2 | **Custom tool authoring** | ✅ Via plugin system (export `tools` array) | ~~Medium~~ Done |
| 10.3 | **Hooks / middleware** | ✅ HookRunner — 6 events, priority-sorted, cancellable | ~~Medium~~ Done |
| 10.4 | **Structured logging** | ✅ Logger class — JSONL file output, daily rotation, levels, child loggers, `anyclaw logs tail` CLI | ~~Medium~~ Done |
| 10.5 | **Telemetry / metrics** | ✅ Token usage, LLM/tool latency, RPM/TPM gauges, per-model/agent/tool breakdowns — dashboard page + REST API | ~~Medium~~ Done |
| 10.6 | **Test harness** | ✅ vitest 3.x configured, 18 passing tests: ArtifactStore (7), SecretsManager (5), Orchestrator routing (6) | ~~Medium~~ Done |
| 10.7 | **Dev mode** | ✅ `MockLLMProvider` streams locally, `--dev` flag for `chat` + `gateway` commands (model=mock/dev, no API calls) | ~~Medium~~ Done |
| 10.8 | **Config validation CLI** | ✅ `anyclaw validate` — Zod validation, warnings, config summary | ~~Small~~ Done |

---

### Phase 11 — UI & Dashboard Enhancements

| # | Feature | Description | Effort |
|---|---------|-------------|--------|
| 11.1 | **Rich dashboard** | ✅ Full SPA — chat, sessions, GSD, memory, tools, settings | ~~Large~~ Done |
| 11.2 | **Tool execution visualization** | ✅ Expandable tool-call blocks in chat (pending pairing, status icons ✓/✗/⏳, collapsible output) | ~~Medium~~ Done |
| 11.3 | **Memory graph** | Visual knowledge graph of vault notes and relationships | Large |
| 11.4 | **GSD board** | ✅ Kanban 5-column layout (Backlog/In Progress/Verifying/Done/Failed) + list toggle, progress bars | ~~Medium~~ Done |
| 11.5 | **Multi-session chat** | ✅ Tabbed chat UI — multiple concurrent conversations, resume-from-session, close/rename tabs | ~~Medium~~ Done |
| 11.6 | **Theme support** | ✅ Light/dark toggle with CSS vars + localStorage persistence | ~~Small~~ Done |
| 11.7 | **Mobile-responsive** | ✅ Hamburger sidebar overlay, CSS media queries ≤768px / ≤480px, slide-in nav, click-outside close | ~~Medium~~ Done |

---

### Phase 12 — Channel Integrations (Optional for Local-First)

OpenClaw supports 20+ channels. For a local-first tool these are lower priority, but some are useful.

| # | Channel | Use Case | Effort |
|---|---------|----------|--------|
| 12.1 | **Telegram bot** | Remote access to your agent from mobile | Medium |
| 12.2 | **Discord bot** | Team/community agent access | Medium |
| 12.3 | **Slack bot** | Workplace agent integration | Medium |
| 12.4 | **Matrix** | Self-hosted encrypted chat integration | Medium |
| 12.5 | **WhatsApp** | Mobile messaging integration | Large |
| 12.6 | **CLI pipe** | ✅ `anyclaw pipe` — stdin→response→stdout, --json for structured output | ~~Small~~ Done |
| 12.7 | **VS Code extension** | Agent as a VS Code copilot-style assistant | Large |
| 12.8 | **Obsidian plugin** | Agent integrated into Obsidian for knowledge work | Large |

---

### Phase 13 — Ecosystem Parity (From Broader Claw Tools)

Features from NanoClaw, ZeroClaw, other ecosystem tools.

| # | Feature | Source | Description | Effort |
|---|---------|--------|-------------|--------|
| 13.1 | **Lightweight mode** | NanoClaw | ✅ --lightweight flag disables memory/GSD | ~~Small~~ Done |
| 13.2 | **Zero-config quickstart** | ZeroClaw | ✅ `anyclaw init` + sensible defaults | ~~Small~~ Done |
| 13.3 | **Code generation focus** | Claw-style tools | Specialized code gen tools (scaffold, refactor, test gen) | Medium |
| 13.4 | **Self-update** | Various | ✅ `anyclaw update` — npm version check + install, `--check` flag | ~~Small~~ Done |
| 13.5 | **Import/export config** | Various | ✅ `anyclaw config export/import` — YAML with Zod validation | ~~Small~~ Done |
| 13.6 | **Recipe system** | Various | ✅ 5 built-in recipes + recipe_list/recipe_run tools | ~~Medium~~ Done |
| 13.7 | **Context file (.context.md)** | Claw standard | ✅ Auto-loads .context.md, CONTEXT.md | ~~Small~~ Done |

---

### Phase 14 — Lightweight & Edge Modes (From NanoClaw, ZeroClaw, PicoClaw, NullClaw)

Make AnyClaw runnable on minimal hardware — old phones, $10 SBCs, CI containers.

| # | Feature | Description | Effort |
|---|---------|-------------|--------|
| 14.1 | **Ultra-lightweight mode** | ✅ --lightweight flag, in-memory DBs | ~~Small~~ Done |
| 14.2 | **Edge/IoT deployment** | Single-binary build (via pkg/bun compile) for low-resource devices; <10 ms startup | Medium |
| 14.3 | **Zero-config quickstart** | ✅ `anyclaw init` generates default YAML | ~~Small~~ Done |
| 14.4 | **Android/iOS native** | Run on mobile devices as a dedicated always-on agent (Termux / iOS Shortcuts bridge) | Large |

---

### Phase 15 — Advanced Memory & Reasoning (From Stinger, memU, MicroClaw, LettaBot)

Build on the 3-tier memory with proactive, graph-based, and cross-channel persistence.

| # | Feature | Description | Effort |
|---|---------|-------------|--------|
| 15.1 | **Proactive memory graphs** | Knowledge graphs (nodes + edges in SQLite) for long-term recall; agents act preemptively on stored knowledge | Medium |
| 15.2 | **Cross-channel memory persistence** | ✅ Channel filter on searchEpisodes + crossChannelRecall() method, vault always global | ~~Small~~ Done |
| 15.3 | **Layered semantic search** | Combine SQLite FTS5 + optional vector index with identity-aware filtering per agent | Medium |
| 15.4 | **Auto-flush to vault** | ✅ Heuristic fact extraction every 10 turns | ~~Small~~ Done |

---

### Phase 16 — Multimodal & Sensory Tools (From Autobot, OpenClaw Variants)

Voice, vision, and beyond — richer "no hands" interactions.

| # | Feature | Description | Effort |
|---|---------|-------------|--------|
| 16.1 | **Voice input/output** | Speech-to-text + TTS via Whisper/Piper; voice channel support in gateway | Medium |
| 16.2 | **Vision analysis** | Multi-modal LLM image/video processing (camera input, screenshot analysis) | Medium |
| 16.3 | **XR perception** | AR/VR integration for real-time environment monitoring (experimental) | Large |
| 16.4 | **Phone calls** | Outbound/inbound call handling via Twilio/SIP integrations | Medium |

---

### Phase 17 — Agent Collaboration & Swarms (From NanoClaw, TinyClaw, Squaer)

Teams of agents for complex autonomous workflows.

| # | Feature | Description | Effort |
|---|---------|-------------|--------|
| 17.1 | **Agent swarms** | Multiple agents collaborating on shared goals with task splitting | Medium |
| 17.2 | **Multi-team orchestration** | Hierarchical agent teams; chain execution with supervisor agents | Medium |
| 17.3 | **Onchain agent coordination** | Crypto wallet/tx integrations for decentralized agent task settlement | Large |

---

### Phase 18 — UI & Monitoring Enhancements (From TinyClaw, KiloClaw)

Improve observability for development and runtime.

| # | Feature | Description | Effort |
|---|---------|-------------|--------|
| 18.1 | **Live TUI dashboard** | Rich terminal UI (Ink/Blessed) for real-time agent/tool monitoring | Medium |
| 18.2 | **One-click hosted mode** | Cloud deploy bundle (Docker Compose + Fly.io/Railway template) — 60-second setup | Medium |
| 18.3 | **3D/virtual worlds** | Agent interactions in simulated environments (Three.js/A-Frame experimental) | Large |

---

### Phase 19 — Additional Channel Integrations (From AstrBot, LettaBot)

Expand reach for global and mobile access.

| # | Channel | Use Case | Effort |
|---|---------|----------|--------|
| 19.1 | **WeChat / QQ / Feishu** | Asia-focused messaging platforms | Medium |
| 19.2 | **Signal** | End-to-end encrypted secure channel | Medium |
| 19.3 | **Obsidian / VS Code plugins** | Direct IDE and knowledge-base integration (subsumes 12.7/12.8) | Large |

---

### Phase 20 — Niche & Experimental (From HermitClaw, Flowly AI, etc.)

Cutting-edge capabilities for "vibe coding" and autonomous operation.

| # | Feature | Description | Effort |
|---|---------|-------------|--------|
| 20.1 | **Autonomous creatures** | Agents that "live" in folders — self-research, self-report, self-maintain | Medium |
| 20.2 | **Flow-oriented workflows** | Compose tasks as visual/declarative DAG flows (YAML or UI-based) | Medium |
| 20.3 | **Self-sovereign identity** | SSI / DID for agents — decentralized identity without third-party reliance | Large |
| 20.4 | **Experimental backends** | Pluggable storage backends (DuckDB, Turso, LMDB) beyond SQLite | Large |

---

## 3. Recommended Implementation Order

```
NOW (Foundation)          NEXT (Intelligence)       THEN (Power User)         FUTURE (Ecosystem)
─────────────────         ─────────────────         ─────────────────         ─────────────────
Phase 1: Foundation       Phase 2: Memory           Phase 5: Skills           Phase 12: Channels
  1.1 Persist sessions      2.1-2.2 Vector+Hybrid     5.1-5.6 Full skills     Phase 16: Multimodal
  1.2 Persist GSD            2.4 Temporal decay      Phase 7: Adv. Agents       16.1 Voice
  1.3 FTS5 search          Phase 15: Adv. Memory      7.1-7.2 Policies          16.2 Vision
  1.4 Confirmations          15.1 Memory graphs        7.4 Delegation          Phase 17: Swarms
  1.5 Streaming              15.4 Auto-flush           7.7 Parallel tools        17.1-17.2 Teams
  1.7 Extended thinking    Phase 3: Tools            Phase 8: Sessions         Phase 18: UI/Monitor
Phase 14: Lightweight       3.2-3.3 Web search/      Phase 9: Security           18.1 Live TUI
  14.1 Ultra-light                   fetch            Phase 10: DevEx            18.2 Hosted mode
  14.3 Zero-config           3.1 Browser             Phase 11: UI              Phase 19: Channels+
Phase 4: MCP                3.4-3.7 Patch/process   Phase 13: Ecosystem       Phase 20: Experimental
  4.1-4.3 Client+tools              /image/pdf        13.3-13.7                 20.1-20.4
Phase 6: Slash Commands    Phase 15: Adv. Memory
  6.1-6.18 (all small)      15.2-15.3 Cross-ch+
                                     layered search
```

---

## 4. Effort Estimates

| Effort | Count | Description |
|--------|-------|-------------|
| **Small** | ~52 | Hours of work. Isolated change, clear scope. |
| **Medium** | ~52 | Day(s) of work. New subsystem or integration. |
| **Large** | ~18 | Multi-day. Complex feature with dependencies. |

**Total features tracked: ~122** (Phases 1–13: ~90 original + Phases 14–20: ~32 new)

---

## 5. Quick Wins (High Impact, Low Effort)

These deliver the most value for the least work:

1. **1.1 + 1.2** — Persist sessions and GSD specs (stop losing state on restart)
2. **1.3** — FTS5 search (massive search quality improvement)
3. **6.1-6.13** — Slash commands (quick UX wins, all small)
4. **3.3** — web_fetch tool (let agent read web pages)
5. **3.4** — apply_patch tool (better file editing)
6. **13.7** — .context.md auto-loading (instant project awareness)
7. **2.4** — Temporal decay (better memory recall with zero new infrastructure)
8. **1.7** — Extended thinking pass-through
9. **9.2** — Elevated mode (power user safety bypass)
10. **14.1 + 13.1** — Ultra-lightweight mode (combine for instant experimentation without overhead)
11. **14.3** — Zero-config quickstart (`npx anyclaw` just works)
12. **15.2** — Cross-channel memory persistence (builds on existing memory tiers)
13. **15.4** — Auto-flush to vault (proactive fact extraction on compaction)
14. **17.1** — Agent swarms (scales existing multi-agent into collaborative teams)
15. **19.2** — Signal integration (secure encrypted channel)
