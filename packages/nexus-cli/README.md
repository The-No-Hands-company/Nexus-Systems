# Nexus CLI

`nexus` is the command-line client for local development and authorized
ecosystem operations. It keeps local process controls separate from hosted
Cloud reads.

## Read-only discovery commands

```sh
nexus catalog             # approved Cloud catalog
nexus catalog develop     # catalog entries on one shelf
nexus knowledge           # ecosystem-grounded Nexus AI index
nexus knowledge "terminal audit"  # search the knowledge index
```

Set `NEXUS_CLOUD_URL` and, when Cloud requires it, `NEXUS_CLOUD_API_KEY` for
catalog reads. Set `NEXUS_AI_HUB_URL` to point knowledge queries at a specific
AI Hub instance. These commands only read contracts and catalog projections;
mutating operator actions will be added behind explicit scopes and approval
records.

## Validation

```sh
bun install
bun run check
```

The package owns its `tsconfig.json` and declares `bun-types` and TypeScript as
development dependencies, so a clean package install can resolve the Bun
runtime types without depending on another app's `node_modules`.
