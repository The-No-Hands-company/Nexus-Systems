# Local Dev Setup

Get the full Nexus Systems stack running in your browser locally in 3 steps.

## Recommended (Cloud Hub Starts Ecosystem)

From the workspace root:

```bash
bash scripts/ecosystem-local.sh start
```

Or from `Nexus-Cloud`:

```bash
cd Nexus-Cloud
bun run dev:ecosystem
```

Useful commands:

```bash
bash scripts/ecosystem-local.sh status
bash scripts/ecosystem-local.sh health
bash scripts/ecosystem-local.sh stop
```

From `Nexus-Cloud`:

```bash
bun run health:ecosystem
```

Readiness colors:
- GREEN: endpoint responds with 2xx/3xx
- YELLOW: endpoint responds but with non-2xx/3xx status
- RED: endpoint unreachable on local machine

You can skip selected services when needed:

```bash
NEXUS_ECOSYSTEM_SKIP="Nexus,Nexus-Computer-frontend" bash scripts/ecosystem-local.sh start
```

Logs are written to `.local/ecosystem/logs/`.

---

## Step 1 — Nexus-Cloud

This manual flow remains available as a fallback.

```bash
cd Nexus-Cloud
cp .env.example .env
```

Edit `.env` — change only these lines:

```env
PORT=8787
NEXUS_CLOUD_API_KEY=localdev
NEXUS_CLOUD_DOMAIN=localhost
NEXUS_CLOUD_URL=http://localhost:8787
SERVER_PUBLIC_IP=127.0.0.1
```

Leave `CF_API_TOKEN`, `CF_ZONE_ID`, and `BOOTSTRAP_PEERS` empty.
Skip Caddy and CoreDNS — neither works locally without DNS delegation.

Start:

```bash
bun run src/index.ts
# or: bun dev
```

Verify: `curl http://localhost:8787/health`

---

## Step 2 — Nexus-Hosting

```bash
cd Nexus-Hosting
cp .env.example .env
```

Add these three lines to the `.env`:

```env
NEXUS_CLOUD_URL=http://localhost:8787
NEXUS_CLOUD_API_KEY=localdev
PUBLIC_URL=http://localhost:8080
```

Start:

```bash
docker compose up
# or: pnpm dev   (inside artifacts/api-server)
```

On startup it automatically calls `POST http://localhost:8787/api/v1/tools` to register itself. You'll see it in Cloud's console output.

---

## Step 3 — Nexus chat server (optional)

Add to `Nexus/.env`:

```env
NEXUS_CLOUD_URL=http://localhost:8787
NEXUS_CLOUD_API_KEY=localdev
PUBLIC_URL=http://localhost:8080
```

The registration spawns non-blocking in the background at startup. Default ports: API `8080`, Gateway `8081`, Voice `8082`.

---

## What to open in the browser

| URL | What you see |
|-----|-------------|
| `http://localhost:8787/health` | Cloud liveness check |
| `http://localhost:8787/api/v1/tools` | Registered tools (nexus-hosting appears here after Step 2) |
| `http://localhost:8787/api/v1/users` | User address registry (`@user:shortId`) |
| `http://localhost:8787/v1/federation/announcement` | This Cloud node's gossip payload |
| `http://localhost:8787/.well-known/nexus-cloud` | Discovery document |

> The browser dashboard (`public/status.html`) is not yet wired to the API. The endpoints above are the live window into the system until the dashboard is built.
