# Ecosystem Documenter

Ecosystem Documenter is an internal development utility for The No Hands Company to bootstrap and maintain documentation across the Nexus Systems ecosystem.

## What This Stub Does

- Centralizes ecosystem repository definitions in a single config file.
- Generates a full docs skeleton for each tool.
- Creates top-level company and platform documentation shells.
- Produces a repeatable structure you can expand as the ecosystem grows.

## Quick Start

1. Install dependencies:

```bash
npm install
```

2. Generate all documentation stubs:

```bash
npm run generate
```

3. Output is created in:

- `generated-docs/`

## Commands

- `npm run generate`: Build and generate docs.
- `npm run dev -- generate`: Run generator with ts-node.
- `npm run start -- generate`: Run built generator.

## Configuration

Edit repository definitions in `src/config/ecosystem.config.json`.

Each entry supports:

- `id`
- `name`
- `repoPath`
- `category`
- `deploymentModel`
- `federation`
- `embeddedInNexusCloud`
- `publicFacing`

## Next Expansion Ideas

- Add source scanners to extract API routes, classes, and modules.
- Add docs quality checks and missing-section reports.
- Add changelog and release-note generation.
- Add static site export for internal or public docs portals.
