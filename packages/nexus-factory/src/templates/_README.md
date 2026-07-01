# {{APP_TITLE}}

{{APP_DESCRIPTION}}

## Quick Start

```bash
bun install
bun dev        # start with hot reload
bun test       # run tests
bun run check  # typecheck + lint
```

## Endpoints

- `GET /health` — health check

## Environment Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `PORT` | `{{APP_PORT}}` | HTTP listen port |
| `SERVICE_NAME` | `{{APP_NAME}}` | Service name in logs |
