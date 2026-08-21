# Nexus-Terminal

## Purpose

Nexus-Terminal provides the Dashboard with real interactive shells on the host.
It is not a sandbox: a shell has the permissions of the account running the
service. Read [`../../docs/TERMINAL-SECURITY.md`](../../docs/TERMINAL-SECURITY.md)
before enabling it.

## Quick Start

```bash
bun install
NEXUS_TERMINAL_ENABLED=true bun run dev
```

The enable switch is deliberately required. Without it the service remains
healthy and registers with Cloud, but every shell attach returns `403`.

## Endpoints

| Method | Path | Purpose |
|--------|------|---------|
| GET | /health | Health check |
| GET | /api/v1/status | Service status and contracts |
| WebSocket | `/api/v1/terminal/attach?cols=80&rows=24` | Attach a new host shell |
| GET | `/api/v1/terminal/audit` | List recent audited shell sessions |

Attach and audit both ask Nexus-Auth to validate the caller's cookie. Only the
exact roles `founder` and `admin` are authorized. Dashboard repeats that check,
requires its exact browser Origin, and relays only to its configured Terminal
URL; Nexus-Terminal still enforces authorization independently.

## Configuration

| Variable | Default | Purpose |
|----------|---------|---------|
| `PORT` | `3110` | Listen port |
| `NEXUS_BIND_HOST` | `127.0.0.1` | Listen address; keep this on loopback |
| `NEXUS_TERMINAL_ENABLED` | `false` | Explicitly allow privileged shell attachment |
| `NEXUS_AUTH_INTERNAL_URL` | `http://127.0.0.1:4310` | Nexus-Auth session validation URL |
| `NEXUS_CLOUD_URL` | `http://localhost:8787` | Cloud control plane |
| `NEXUS_CLOUD_API_KEY` | (none) | Cloud API key |
| `NEXUS_NEXUS_TERMINAL_BASE_URL` | `http://localhost:$PORT` | Loopback URL registered with Cloud |
| `NEXUS_TERMINAL_ENABLE_CLOUD_INTEGRATION` | true | Enable/disable cloud heartbeat |

Production starts Terminal on `127.0.0.1:3110` before Dashboard. Terminal has
no public route: browsers connect to Dashboard, which relays to the loopback
service. Production also passes the enable value explicitly, with `false` as
the safe default. Export `NEXUS_TERMINAL_ENABLED=true` only for a deployment
where host-shell access has been deliberately approved.

## Session behavior and audit warning

Each Dashboard tab opens an independent shell; output and input are never
shared between tabs. Closing a tab kills its shell, and the service also caps
concurrency and reaps abandoned sessions.

The requested `cols` and `rows` are applied when the shell starts. The current
`script`-based PTY cannot resize after attachment, so a later browser resize
does not change the running shell dimensions.

Every input byte is written to the audit database before it is sent to the
shell. That includes passwords, tokens, and other secrets typed into an
interactive prompt. Prefer credential mechanisms that do not put secrets on
terminal input when using this service.
