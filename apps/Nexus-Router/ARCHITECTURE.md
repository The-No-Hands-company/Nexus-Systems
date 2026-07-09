# Nexus Router — Architecture

## Overview

Nexus Router is the central orchestration engine for the Nexus Systems ecosystem. It sits between external clients and internal services, providing:

1. **IP/Geo Routing** — Route requests to appropriate cloud region
2. **Centralized Authentication** — Single Phantom DID validation point
3. **API Gateway** — Dynamic service discovery and routing
4. **AI Provider Selection** — Intelligent model routing
5. **Telemetry Collection** — Unified logging, metrics, tracing
6. **Federation Support** — Multi-cloud awareness

---

## Request Pipeline

```
┌─────────────────────────────────────────────────────┐
│ Incoming Request (HTTP)                             │
└────────────────────┬────────────────────────────────┘
                     │
        ┌────────────▼─────────────┐
        │ 1. IP/Geo Router         │
        │ - Extract client IP      │
        │ - Determine region       │
        │ - Route to cloud (if diff)│
        └────────────┬─────────────┘
                     │
        ┌────────────▼──────────────┐
        │ 2. Auth Gate             │
        │ - Extract Phantom DID    │
        │ - Validate JWT           │
        │ - Check capabilities     │
        └────────────┬──────────────┘
                     │
        ┌────────────▼─────────────┐
        │ 3. Request Enricher     │
        │ - Add trace ID           │
        │ - Add context metadata   │
        └────────────┬─────────────┘
                     │
        ┌────────────▼──────────────┐
        │ 4. API Router            │
        │ - Query service registry │
        │ - Select upstream        │
        │ - Load balance           │
        └────────────┬──────────────┘
                     │
        ┌────────────▼─────────────┐
        │ 5. AI Provider Router    │
        │ - Check task type        │
        │ - Select model           │
        │ - Apply fallback         │
        └────────────┬─────────────┘
                     │
        ┌────────────▼──────────────┐
        │ 6. Proxy to Upstream     │
        │ - Forward request        │
        │ - Copy headers           │
        └────────────┬──────────────┘
                     │
        ┌────────────▼─────────────┐
        │ 7. Telemetry Emitter    │
        │ - Log request+response   │
        │ - Emit metrics           │
        │ - Emit trace             │
        └────────────┬─────────────┘
                     │
        ┌────────────▼──────────────┐
        │ Response to Client       │
        └──────────────────────────┘
```

---

## Components

### IP/Geo Router (`middleware/ip-router.ts`)

Routes requests to regional cloud instances based on source IP.

**Logic:**
1. Extract client IP from `X-Forwarded-For` or socket
2. Match against CIDR blocks in config
3. Determine region (eu, us, ap, etc)
4. Add upstream cloud URL to request headers

**Example:**
```
Client IP: 2.1.2.3 (Europe)
  → Region: "eu"
  → Cloud URL: http://nexus-eu.local:8787
```

### Auth Gate (`middleware/auth-gate.ts`)

Centralized authentication and authorization.

**Logic:**
1. Extract `Authorization: Bearer <token>` header
2. Parse token (JWT or Phantom DID)
3. Extract phantom_did, user_id, org_id
4. Store in request context
5. Return 401/403 if invalid

**Token Formats:**
- Bearer `did:phantom:abc123...` (raw DID)
- Bearer `eyJ...` (JWT payload with `sub`, `user_id`, `org_id`)

### Request Enricher (`middleware/request-enricher.ts`)

Adds context for tracing and observability.

**Added Headers:**
- `X-Request-ID` — Unique request identifier
- `X-Trace-ID` — Distributed trace ID
- `X-Phantom-DID` — User identity
- `X-Org-ID` — Organization

### API Router (`middleware/api-router.ts`)

Queries service registry and routes to upstream services.

**Logic:**
1. Extract path from request (e.g., `/api/chat`)
2. Look up in config `routes` section
3. Select upstream based on priority and load balancing
4. Add `X-Upstream-Name` header
5. Continue to next middleware

**Example Route Config:**
```yaml
routes:
  "/api/chat":
    upstreams:
      - name: "nexus-ai"
        priority: 1
      - name: "nexus-inference"
        priority: 2
    load_balance: "round_robin"
```

### AI Provider Router (`middleware/ai-provider-router.ts`)

Intelligent model selection based on task type and preferences.

**Logic:**
1. Check if request is AI-related (`/api/ai`, `/api/chat`)
2. Extract task pattern from URL/headers
3. Match against routing rules
4. Select model (ollama, claude, groq, etc)
5. Add `X-AI-Model` header for upstream

**Example Routing Rules:**
```yaml
ai_providers:
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
```

### Telemetry Emitter (`middleware/telemetry-emitter.ts`)

Collects and forwards logs, metrics, and traces.

**Emits:**
- **Logs** → ElasticSearch / Datadog (JSON format)
- **Metrics** → Prometheus (`nexus_router_request_duration_seconds`, etc)
- **Traces** → Jaeger / OpenTelemetry

**Log Entry:**
```json
{
  "timestamp": "2024-01-01T12:00:00Z",
  "request_id": "uuid",
  "trace_id": "uuid",
  "method": "POST",
  "url": "/api/chat",
  "status_code": 200,
  "duration_ms": 45,
  "phantom_did": "did:phantom:...",
  "user_id": "user123",
  "org_id": "org456",
  "ai_provider": "claude",
  "region": "eu-west-1"
}
```

---

## Configuration

All routing logic is defined in `config/routing-policy.yaml`:

```yaml
router:              # Router settings (port, hostname)
cloud:               # Nexus-Cloud connection
auth:                # Authentication config
geography:           # Region/geo config
routes:              # API routing rules
ai_providers:        # Model selection rules
telemetry:           # Observability config
federation:          # Multi-cloud peers
```

See `CONFIG.md` for full schema.

---

## Service Registry Integration

Nexus Router queries Nexus-Cloud's service registry via:

```
GET http://nexus-cloud:8787/api/v1/tools
Authorization: Bearer <api-key>
```

Response:
```json
[
  {
    "id": "nexus-ai",
    "name": "Nexus AI Agent",
    "baseUrl": "http://localhost:3400",
    "capabilities": ["chat", "code", "reasoning"],
    "health": "healthy",
    "last_heartbeat": "2024-01-01T12:00:00Z"
  },
  {
    "id": "nexus-hosting",
    "name": "Nexus Hosting",
    "baseUrl": "http://localhost:3000",
    "capabilities": ["deploy", "federation"],
    "health": "healthy"
  }
]
```

Router uses this to:
1. Discover available services
2. Check health status
3. Route requests to live upstreams
4. Fail over when service is down

---

## Federation Routing

When `federation.enabled: true`, Nexus Router:

1. Discovers peer clouds from config
2. Queries their service registries
3. Routes based on geography/jurisdiction
4. Handles replica selection (e.g., for Nexus-Hosting)

**Example:**
```
Request: GET /api/host/site123
  → Router checks if site exists on local cloud
  → If not, queries federation peers (eu, ap)
  → Selects replica based on geo/latency
  → Routes to peer cloud
```

---

## Telemetry Flow

```
┌─────────────────────┐
│ Nexus Router        │ (collects)
│ - Logs (JSON)       │
│ - Metrics           │
│ - Traces            │
└──────────┬──────────┘
           │
    ┌──────┴──────┬──────────┬──────────┐
    │             │          │          │
    ▼             ▼          ▼          ▼
┌────────┐  ┌────────┐  ┌────────┐  ┌────��─────┐
│Elastic │  │Prometheus│ │Jaeger  │  │Datadog   │
│Search  │  │          │ │        │  │(optional)│
└────────┘  └────────┘  └────────┘  └──────────┘
     │            │          │            │
     └────────────┴──────────┴────────────┘
               │
      Grafana Dashboards
      (unified observability)
```

---

## Security Model

1. **Authentication** — Phantom DID validation in auth gate
2. **Authorization** — Check capabilities per upstream
3. **Audit Logging** — All decisions logged and traced
4. **Rate Limiting** — Per-user, per-org, per-API-key (future)
5. **TLS** — In production (behind reverse proxy)

---

## Performance Considerations

- **Middleware Pipeline** — All synchronous for latency < 15ms
- **Service Registry Caching** — Cache services for 30s to reduce lookups
- **Connection Pooling** — Reuse upstream connections
- **Telemetry Batching** — Batch log/metric emission (future)

---

## Future Enhancements

1. **Rate Limiting** — Per-user quota enforcement
2. **Circuit Breaker** — Fail fast if upstream is overloaded
3. **Retry Logic** — Exponential backoff on failures
4. **Request Signing** — Sign cross-cloud requests with Ed25519
5. **caching** — Cache responses for read-only APIs
6. **A/B Testing** — Route % of traffic to new versions
