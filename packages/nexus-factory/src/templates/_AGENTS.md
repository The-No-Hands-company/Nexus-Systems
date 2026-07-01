# {{APP_TITLE}}

{{APP_DESCRIPTION}}

## Conventions

- **Runtime**: Bun + TypeScript strict mode
- **Framework**: `@nexus/core` (NexusServer, logging, validation)
- **Linting**: Biome (recommended rules + unused variable enforcement)
- **Testing**: Bun test runner, smoke-contract style
- **Logging**: Structured JSON to stdout
- **Health**: Every Nexus app exposes `GET /health`

## Architecture

Document your app's architecture, data model, and external dependencies here.
