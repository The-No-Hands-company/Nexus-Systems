# Nexus Router

**Central orchestration engine for the Nexus Systems ecosystem.**

Nexus Router is the unified gateway for all request routing, authentication, authorization, AI provider selection, telemetry collection, and federation across the entire Nexus Systems platform.

---

## Purpose

Nexus Router solves fragmented routing by providing:

- **IP/Geo Routing** — Route by source IP, region, jurisdiction
- **Centralized Auth** — Single Phantom DID validation point
- **API Gateway** — Dynamic service discovery and upstream routing
- **AI Provider Routing** — Intelligent model selection (replaces Nexus-AI internal logic)
- **Telemetry Pipeline** — Unified logs, metrics, traces from all apps
- **Federation Aware** — Routes across multi-cloud Nexus instances
- **Policy-Driven** — YAML-based routing rules and configuration

---

## Architecture

```
External Request
     │
     ▼
┌─────────────────────────────────────────┐
│    Nexus Router (Port 9999 / reverse proxy) │
├─────────────────────────────────────────┤
│                                         │
│ 1. IP/Geo Router                        │
│    └─ Determine region, jurisdiction    │
│       └─ Route to appropriate Cloud     │
│                                         │
│ 2. Auth Gate                            │
│    └─ Extract Phantom DID               │
│    └─ Validate JWT / signature          │
│    └─ Authorize capability              │
│                                         │
│ 3. Request Enrichment                   │
│    └─ Add context (user, org, region)   │
│    └─ Start distributed trace           │
│                                         │
│ 4. API Route Decision                   │
│    └─ Query service registry            │
│    └─ Select upstream (app, version)    │
│    └─ Load balance                      │
│                                         │
│ 5. AI Provider Selection (if needed)    │
│    └─ Check model preferences           │
│    └─ Route to Ollama / Claude / etc    │
│    └─ Enforce quota / rate limits       │
│                                         │
│ 6. Proxy to Upstream                    │
│                                         │
│ 7. Telemetry Emission                   │
│    └─ Log request+response              │
│    └─ Metrics (latency, status, etc)    │
│    └─ Send trace to Jaeger/etc          │
│                                         │
└─────────────────────────────────────────┘
     │
     ├─→ Nexus-Cloud :8787 (service registry)
     ├─→ Nexus-AI :3400
     ├─→ Nexus-Hosting :3000
     ├─→ 80+ apps
     │
     └─→ External:
         ├─→ Ollama (local models)
         ├─→ Groq / Cerebras / Claude (cloud providers)
         ├─→ Prometheus / Grafana (metrics)
         ├─→ ElasticSearch / Datadog (logging)
         └─→ Jaeger (distributed tracing)
```

---

## Quick Start

### Prerequisites
- Bun ^1.3
- Nexus-Cloud running on :8787
- YAML config file (see `config/` directory)

### Installation

```bash
cd apps/Nexus-Router
bun install
```

### Development

```bash
bun run dev
# Listens on http://localhost:9999
```

### Configuration

Create `config/routing-policy.yaml`:

```yaml
router:
  port: 9999
  hostname: "0.0.0.0"

cloud:
  url: "http://localhost:8787"
  apiKey: "change-me"
  heartbeat_interval_sec: 30

auth:
  phantom_enabled: true
  jwt_secret: "your-jwt-secret"

geography:
  default_region: "us-east-1"
  regions:
    eu:
      cidrs: ["2.0.0.0/8", "3.0.0.0/8"]
      cloud_url: "http://nexus-eu.local:8787"
    us:
      cidrs: ["1.0.0.0/8"]
      cloud_url: "http://nexus-us.local:8787"

routes:
  "/api/chat":
    upstreams:
      - name: "nexus-ai"
        priority: 1
      - name: "nexus-inference"
        priority: 2
    load_balance: "round_robin"

  "/api/host":
    upstreams:
      - name: "nexus-hosting"
    federation_aware: true

ai_providers:
  default_model: "ollama"
  routing_rules:
    - pattern: "legal:*"
      model: "claude"
      fallback: "gpt-4"
    - pattern: "code:*"
      model: "claude"
      fallback: "groq"
    - pattern: "*"
      model: "ollama"
      fallback: "groq"

telemetry:
  logs:
    enabled: true
    format: "json"
    elasticsearch:
      url: "http://localhost:9200"
  metrics:
    enabled: true
    prometheus:
      url: "http://localhost:9090"
  tracing:
    enabled: true
    jaeger:
      url: "http://localhost:6831"
```

### Start with Docker Compose

See `docker-compose.yml` for full stack setup.

---

## API Reference

### Health Check

```
GET /health
```

Response:
```json
{
  "status": "healthy",
  "uptime_sec": 3600,
  "services_registered": 42,
  "federation_peers": 3
}
```

### Router Status

```
GET /api/v1/router/status
```

Response:
```json
{
  "version": "0.1.0",
  "region": "us-east-1",
  "routes_loaded": 45,
  "ai_providers_available": ["ollama", "groq", "claude"],
  "telemetry_pipeline": "connected"
}
```

### Service Discovery (passthrough to Nexus-Cloud)

```
GET /api/v1/tools
```

Proxies to Nexus-Cloud service registry.

---

## Integration Guide

### Step 1: Configure Nexus Router in Your Deployment

Create `config/routing-policy.yaml` with your routes and rules.

### Step 2: Point Your Apps to Nexus Router

Instead of calling Nexus-Cloud directly, apps call Nexus Router:

```typescript
// Old: direct to Cloud
const cloudUrl = process.env.NEXUS_CLOUD_URL || "http://localhost:8787";

// New: through Router (Router proxies to Cloud)
const routerUrl = process.env.NEXUS_ROUTER_URL || "http://localhost:9999";
const toolsResp = await fetch(`${routerUrl}/api/v1/tools`, {
  headers: { Authorization: `Bearer ${phantomDid}` },
});
```

### Step 3: Configure Telemetry in Apps

Structure all logs as JSON:

```typescript
import pino from "pino";

const logger = pino({
  level: process.env.LOG_LEVEL || "info",
  transport: {
    target: "pino/file",
    options: {
      destination: process.env.TELEMETRY_ENDPOINT || "http://localhost:9999/logs",
    },
  },
});

logger.info({
  message: "User login",
  userId: user.id,
  phantom_did: user.phantomDid,
  timestamp: new Date().toISOString(),
});
```

### Step 4: Federation Setup

Configure peer clouds in `config/routing-policy.yaml`:

```yaml
federation:
  enabled: true
  peers:
    - name: "cloud-eu"
      url: "http://nexus-eu.local:8787"
      region: "eu-west-1"
      trust_level: "verified"
    - name: "cloud-ap"
      url: "http://nexus-ap.local:8787"
      region: "ap-southeast-1"
      trust_level: "trusted"
```

---

## Configuration Reference

See `docs/CONFIG.md` for complete YAML schema.

---

## Examples

See `examples/` directory for:
- `geo-routing.yaml` — Route by geography
- `model-selection.yaml` — AI provider routing rules
- `federation-multi-cloud.yaml` — Cross-cloud setup
- `rate-limiting.yaml` — Quota enforcement

---

## Performance

Benchmarks (single instance, 4 vCPU / 8 GB RAM):

| Operation | p50 | p95 | p99 |
|-----------|-----|-----|-----|
| Auth validation | 2ms | 5ms | 10ms |
| Service discovery | 5ms | 15ms | 30ms |
| AI provider routing | 3ms | 8ms | 15ms |
| Request proxy | 1ms | 2ms | 5ms |
| Full pipeline | 15ms | 35ms | 60ms |

---

## Monitoring

### Prometheus Metrics

- `nexus_router_requests_total` — Request count by route, status
- `nexus_router_request_duration_seconds` — Latency histogram
- `nexus_router_auth_failures_total` — Auth denial count
- `nexus_router_provider_selections_total` — AI provider routing count
- `nexus_router_federation_peers_healthy` — Healthy peer count

### Grafana Dashboards

Included in `monitoring/` directory.

---

## Security

- All auth validated against Phantom DID
- JWT tokens signed with ecosystem key
- TLS in production (behind reverse proxy)
- Rate limiting per user/org/API key
- Audit logging on all decisions

See `docs/SECURITY.md` for full model.

---

## License

AGPL-3.0 — Part of Nexus Systems.
