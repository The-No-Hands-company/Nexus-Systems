# Nexus-Photos

## Purpose

Photo and video backup with AI tagging, face recognition, and album sharing

## Quick Start

```bash
npm install --no-audit --no-fund --workspaces=false
bun run check
bun test
bun run dev
```

Use the isolated npm install above from this directory. The parent repository
contains a pnpm workspace lock for other services, and a plain `bun install`
can try to interpret that lock instead of installing this app's declared
`bun-types`, TypeScript, and Biome dependencies.

## Endpoints

| Method | Path | Purpose |
|--------|------|---------|
| GET | /health | Health check |
| GET | /api/v1/status | Service status and contracts |

## Configuration

| Variable | Default | Purpose |
|----------|---------|---------|
| PORT | 3096 | Listen port |
| NEXUS_CLOUD_URL | http://localhost:8787 | Cloud control plane |
| NEXUS_CLOUD_API_KEY | (none) | Cloud API key |
| `NEXUS_PHOTOS_ENABLE_CLOUD_INTEGRATION` | true | Enable/disable cloud heartbeat |
