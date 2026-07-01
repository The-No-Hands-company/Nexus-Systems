# Nexus Porter

**Port Intelligence Tool** — everything there is to know about ports on the machine that runs it.

Reads `/proc/net/tcp`, `/proc/net/tcp6`, `/proc/net/udp`, `/proc/net/udp6` + `/proc/[pid]/fd/` to build a live picture of every listening socket, the process that owns it, its service name, and (optionally) whether it responds over HTTP and whether it can be embedded in an `<iframe>`.

---

## Quick start

```bash
npm install
npx tsx src/index.ts           # instant port table
npx tsx src/index.ts --probe   # + HTTP probing and iframe embeddability
npx tsx src/index.ts serve     # start JSON API on :2978
npx tsx src/index.ts free 3000 4000  # find free ports in range
npx tsx src/index.ts capabilities      # implemented + planned features
npx tsx src/index.ts conflicts 3000 5432
npx tsx src/index.ts plan 3000 3100    # dry-run reconfiguration plan
```

---

## Commands

| Command | Description |
|---|---|
| *(default)* | Scan all listening ports and print a table |
| `serve` | Start the JSON API server (port 2978 by default) |
| `free [from] [to]` | List port numbers in range that are NOT in use |
| `capabilities` | Show implemented features and stubbed roadmap |
| `conflicts [ports...]` | Check if selected ports are currently taken |
| `plan <from> [to]` | Build a dry-run migration plan to move a service port |
| `help` | Show usage info |

### Flags

| Flag | Description |
|---|---|
| `--probe` / `-p` | HTTP-probe every TCP port — adds status code, server header, page title, X-Frame-Options, CSP `frame-ancestors`, and an `embeddable` boolean |
| `--json` / `-j` | Output raw JSON instead of the table |
| `--no-color` | Disable ANSI colours |
| `--api-port=N` | Change API server port (default: 2978) |
| `--ports=A,B,C` | Comma-separated targets for the `conflicts` command |
| `--from=N` / `--to=N` | Explicit source/target for the `plan` command |
| `--range-from=N` / `--range-to=N` | Candidate range when `plan` auto-selects target port |

---

## JSON API (serve mode)

Start once, query forever:

```bash
npx tsx src/index.ts serve
```

| Endpoint | Description |
|---|---|
| `GET /` | Summary + metadata |
| `GET /ports` | All listening ports (full detail) |
| `GET /ports/:n` | Single port by number |
| `GET /scan` | Force fresh scan (bypasses 15 s cache) |
| `GET /available?from=N&to=M` | Free ports in range |
| `GET /capabilities` | Implemented + planned feature matrix |
| `GET /conflicts?ports=3000,5432` | Conflict report for selected ports |
| `GET /reconfigure/plan?from=3000&to=3100` | Dry-run reconfiguration plan |
| `GET /outbound` | Outbound intelligence roadmap (stub) |
| `GET /health` | `{ ok: true }` |

Query params available on every endpoint:
- `?probe=1` — enable HTTP probing for that request
- `?force=1` — bypass the 15 s cache

### Example response for `GET /ports/5432`

```json
{
  "port": {
    "port": 5432,
    "proto": "tcp6",
    "state": "LISTEN",
    "bind": "0.0.0.0",
    "process": { "pid": 104579, "name": "rootlessport", "user": "zajferx" },
    "probe": null,
    "known": { "name": "PostgreSQL", "category": "database", "description": "..." },
    "container": null,
    "protocols": ["tcp6"]
  }
}
```

---

## What it collects per port

| Field | Source |
|---|---|
| Port, protocol, state, bind address | `/proc/net/tcp`, `tcp6`, `udp`, `udp6` |
| PID, process name, full cmdline, user | `/proc/[pid]/fd/` inode→PID mapping |
| Service name & category | Built-in database (100+ well-known ports) |
| HTTP status, title, server header | Native `http`/`https` probe (opt-in via `--probe`) |
| `X-Frame-Options`, `frame-ancestors` | Same HTTP probe |
| `embeddable` boolean | Derived from the two headers above |
| Container name, image | `podman ps` / `docker ps` JSON output |

---

## Feature matrix (Implemented vs Stubbed)

Implemented now:
- Live listening inventory (TCP/TCP6/UDP/UDP6)
- Free port discovery in ranges
- Port ownership resolution (PID/process/user with fallbacks)
- Service recognition via built-in port DB
- Optional HTTP/iframe security probe
- Container host-port mapping intelligence

Stubbed now (safe scaffolding, no auto-write side effects):
- Port reconfiguration planner (`plan` command + `/reconfigure/plan`)
- Conflict checks for explicit target sets (`conflicts` + `/conflicts`)
- Capability introspection (`capabilities` + `/capabilities`)
- Outbound intelligence API scaffold (`/outbound`)
- Apply/rewrite engine for automatic config mutation (planned, intentionally disabled)
- Reservation/policy registry for ecosystem-wide port allocation (planned)

Why stubs first:
- Lets you build confidence in planning/intelligence output before any config writes.
- Keeps Nexus Porter safe as an internal helper while you iterate on the ecosystem.
- Makes adapter-based integration straightforward for each repo (Nexus, Nexus-AI, Nexus-Cloud, etc.).

---

## Architecture

```
src/
  types.ts        — shared TypeScript interfaces
  port-db.ts      — well-known port → service name/category database
  scanner.ts      — /proc/net/* parser + inode→PID resolver + ss fallback
  prober.ts       — HTTP/HTTPS probe with redirect following
  containers.ts   — podman/docker port resolver
  scan.ts         — orchestrator: combines all sources into ScanResult
  report.ts       — ANSI terminal table renderer
  server.ts       — Node HTTP JSON API (15 s cache, CORS enabled)
  index.ts        — CLI entry point
```

---

## Permissions

- **As a regular user** — resolves processes owned by your own user. Unknown processes show `pid` via the `ss` fallback.
- **As root** — resolves every process on the machine.

No external runtime dependencies. Uses Node 22 built-ins only.
