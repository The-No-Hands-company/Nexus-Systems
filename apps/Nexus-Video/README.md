# Nexus Video

Status: Scaffold created
Blueprint source: Nexus Systems Ecosystem Blueprint v1.2

## Purpose

TODO: Define product scope and MVP for Nexus Video.

## Dual Mode Target

- Standalone: docker compose based local runtime
- Orchestrated: integrated with Nexus-Cloud via Nexus Systems API

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

## Initial Scaffold Layout

- src/: implementation code
- docs/: design and architecture notes
- tests/: automated tests

## Next Steps

1. Define architecture and service boundaries.
2. Add initial runtime entrypoint and health endpoint.
3. Register with Nexus-Cloud Systems API contract.
