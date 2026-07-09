# Nexus Router Integration Guide

This guide shows how to integrate Nexus Router into your Nexus Systems deployment.

---

## Overview

Nexus Router becomes the central request gateway for your entire ecosystem. Instead of apps calling Nexus-Cloud directly, they call Nexus Router, which:

1. Validates authentication (Phantom DID)
2. Determines geographic region
3. Routes to appropriate service
4. Collects telemetry
5. Proxies request to upstream

---

## Step 1: Deploy Nexus Router

### Option A: Docker Compose (Recommended)

```bash
cd apps/Nexus-Router

# Start Router + monitoring stack
docker-compose up -d
docker-compose -f docker-compose.monitoring.yml up -d

# Verify
curl http://localhost:9999/health
# {"status": "healthy", ...}
```

### Option B: Direct (Development)

```bash
cd apps/Nexus-Router
bun install
cp .env.example .env
bun run dev
# Listens on :9999
```

---

## Step 2: Update Configuration

Edit `config/routing-policy.yaml`:

```yaml
router:
  port: 9999

cloud:
  url: "http://nexus-cloud:8787"  # Your Nexus-Cloud
  apiKey: "change-me"

auth:
  phantom_enabled: true

geography:
  default_region: "us-east-1"
  regions:
    eu:
      cidrs: ["2.0.0.0/8"]         # Your EU IP ranges
      cloud_url: "http://nexus-eu.local:8787"
    us:
      cidrs: ["1.0.0.0/8"]         # Your US IP ranges
      cloud_url: "http://nexus-us.local:8787"

routes:
  "/api/chat":                      # Chat requests
    upstreams:
      - name: "nexus-ai"
        priority: 1
    load_balance: "round_robin"

  "/api/host":                      # Hosting
    upstreams:
      - name: "nexus-hosting"
    federation_aware: true

  "/api/forge":                     # Source control
    upstreams:
      - name: "nexus-forge"

ai_providers:
  default_model: "ollama"
  routing_rules:
    - pattern: "legal:*"
      model: "claude"
      fallback: "gpt-4"
    - pattern: "code:*"
      model: "groq"
      fallback: "ollama"
    - pattern: "*"
      model: "ollama"
      fallback: "groq"

telemetry:
  logs:
    enabled: true
    elasticsearch:
      url: "http://elasticsearch:9200"
  metrics:
    enabled: true
    prometheus:
      url: "http://prometheus:9090"
  tracing:
    enabled: true
    jaeger:
      url: "http://jaeger:6831"
```

---

## Step 3: Integrate Nexus-Cloud

### Update Nexus-Cloud to Register with Router

**File:** `apps/Nexus-Cloud/src/index.ts`

```typescript
import { registerWithRouter } from './lib/router';

async function main() {
  // ... existing code ...

  // Register with Nexus Router (not just standalone)
  try {
    const routerUrl = process.env.NEXUS_ROUTER_URL || 'http://localhost:9999';
    await registerWithRouter({
      name: 'nexus-cloud',
      baseUrl: `http://localhost:${PORT}`,
      capabilities: ['service-registry', 'tool-discovery', 'federation'],
      version: '1.0.0',
    }, routerUrl);
    logger.info('Registered with Nexus Router');
  } catch (err) {
    logger.warn('Failed to register with Router (continuing)', err);
  }

  server.listen(PORT, () => {
    logger.info({ port: PORT }, 'Nexus-Cloud started');
  });
}

main();
```

### Create Router Helper

**File:** `apps/Nexus-Cloud/src/lib/router.ts`

```typescript
import axios from 'axios';
import { logger } from './logger';

export async function registerWithRouter(
  tool: any,
  routerUrl: string
): Promise<void> {
  try {
    await axios.post(
      `${routerUrl}/api/v1/tools`,
      tool,
      {
        headers: {
          'Authorization': `Bearer ${process.env.NEXUS_CLOUD_API_KEY}`,
          'Content-Type': 'application/json',
        },
        timeout: 5000,
      }
    );
  } catch (err) {
    logger.error({ error: String(err) }, 'Router registration failed');
    throw err;
  }
}
```

---

## Step 4: Integrate Nexus-AI

### Route AI Requests Through Router

Instead of apps calling Nexus-AI directly, they call Router:

**Old (direct to AI):**
```typescript
const response = await fetch('http://localhost:3400/api/chat', {
  method: 'POST',
  body: JSON.stringify({ message: 'Hello' }),
});
```

**New (through Router):**
```typescript
const response = await fetch('http://localhost:9999/api/chat', {
  method: 'POST',
  headers: {
    'Authorization': `Bearer ${phantomDid}`,
    'X-Task-Type': 'legal:review',  // Tells Router which model to use
  },
  body: JSON.stringify({ message: 'Hello' }),
});
```

### Update Nexus-AI to Accept Router Headers

**File:** `apps/Nexus-AI/src/server.ts`

```typescript
import express from 'express';

const app = express();

// Middleware: Accept model routing from Router
app.use((req, res, next) => {
  const aiModel = req.headers['x-ai-model'] as string;
  const tracedId = req.headers['x-trace-id'] as string;

  // Store in request context
  (req as any).aiModel = aiModel || 'default';
  (req as any).traceId = tracedId || crypto.randomUUID();

  logger.debug({
    trace_id: (req as any).traceId,
    ai_model: (req as any).aiModel,
  }, 'Request routed through Nexus Router');

  next();
});

// Chat endpoint
app.post('/api/chat', async (req, res) => {
  const aiModel = (req as any).aiModel;

  // Use Router-selected model instead of app's internal logic
  const response = await selectAIProvider(aiModel, req.body);
  res.json(response);
});

app.listen(3400, () => {
  logger.info('Nexus-AI listening on :3400');
});
```

---

## Step 5: Update App Telemetry

All apps should ship logs to Router for centralized collection.

### Configure Pino Logger

**File:** Any app's `src/lib/logger.ts`

```typescript
import pino from 'pino';

const logger = pino(
  {
    level: process.env.LOG_LEVEL || 'info',
  },
  pino.transport({
    target: 'pino-http',
    options: {
      url: process.env.NEXUS_ROUTER_TELEMETRY_URL 
        || 'http://localhost:9999/telemetry',
      method: 'POST',
    },
  })
);

export { logger };
```

### Structured Logging

```typescript
import { logger } from './lib/logger';

// Log with context
logger.info({
  event: 'user_login',
  user_id: user.id,
  phantom_did: user.phantomDid,
  region: req.headers['x-region'],
  trace_id: req.headers['x-trace-id'],
  timestamp: new Date().toISOString(),
}, 'User authenticated');
```

---

## Step 6: Setup Monitoring

### Start Monitoring Stack

```bash
cd apps/Nexus-Router

# Start Prometheus, Grafana, Elasticsearch, Kibana, Jaeger
docker-compose -f monitoring/docker-compose.monitoring.yml up -d
```

### Access Dashboards

- **Grafana:** http://localhost:3000 (admin/admin)
- **Kibana:** http://localhost:5601
- **Jaeger:** http://localhost:16686
- **Prometheus:** http://localhost:9090

### Import Grafana Dashboard

1. Open http://localhost:3000
2. Import: `monitoring/grafana-dashboard.json`
3. Select Prometheus data source
4. View real-time metrics

---

## Step 7: Federation Setup (Optional)

For multi-cloud deployments:

**File:** `config/routing-policy.yaml`

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

routes:
  "/api/host":
    upstreams:
      - name: "nexus-hosting"
    federation_aware: true    # Route across federation
```

Router will automatically:
1. Discover peer clouds
2. Query their service registries
3. Route based on geo/jurisdiction
4. Handle failover

---

## Step 8: Testing

### Test Health

```bash
curl http://localhost:9999/health
# {"status": "healthy", "uptime_sec": 3600, ...}
```

### Test Auth

```bash
# Without auth (should fail)
curl http://localhost:9999/api/v1/tools
# {"error": "Unauthorized"}

# With Phantom DID
curl -H "Authorization: Bearer did:phantom:abc123" \
  http://localhost:9999/api/v1/tools
# {"tools": [...]}
```

### Test Routing

```bash
# Request to /api/chat (routed to nexus-ai)
curl -X POST \
  -H "Authorization: Bearer did:phantom:abc123" \
  -H "X-Task-Type: legal:review" \
  -H "Content-Type: application/json" \
  -d '{"message": "Review this contract"}' \
  http://localhost:9999/api/chat
```

### Test Geo Routing

```bash
# EU client
curl -H "X-Forwarded-For: 2.1.2.3" http://localhost:9999/health

# Check Router logs
# Should see: region="eu", upstream_cloud_url="http://nexus-eu.local:8787"
```

---

## Environment Variables

Set in your deployment:

```bash
# Router
NEXUS_ROUTER_PORT=9999
NEXUS_ROUTER_HOST=0.0.0.0

# Cloud
NEXUS_CLOUD_URL=http://nexus-cloud:8787
NEXUS_CLOUD_API_KEY=change-me

# Config
CONFIG_PATH=/app/config/routing-policy.yaml

# Logging
LOG_LEVEL=debug
NODE_ENV=production

# Telemetry
ELASTICSEARCH_URL=http://elasticsearch:9200
PROMETHEUS_URL=http://prometheus:9090
JAEGER_URL=http://jaeger:6831
```

---

## Troubleshooting

### Router won't start

```bash
# Check logs
docker-compose logs -f nexus-router

# Validate config
bun run src/lib/config.ts
```

### Can't connect to Nexus-Cloud

```bash
# Verify Cloud is running
curl http://localhost:8787/health

# Check Router can reach Cloud
docker-compose exec nexus-router curl http://nexus-cloud:8787/health
```

### Auth failures

```bash
# Enable debug logging
LOG_LEVEL=debug bun run dev

# Check token format
# Should be: Bearer did:phantom:xxxx or Bearer eyJ...
```

### Metrics not appearing

```bash
# Verify Prometheus scrape
http://localhost:9090/targets

# Check Router metrics endpoint
curl http://localhost:9999/metrics
```

---

## Production Checklist

- [ ] Deploy Nexus Router in HA mode (multiple instances)
- [ ] Use TLS for all communication (behind reverse proxy)
- [ ] Set strong `NEXUS_CLOUD_API_KEY`
- [ ] Configure rate limiting per user/org
- [ ] Enable request signing (Ed25519)
- [ ] Setup alerts in Prometheus
- [ ] Enable audit logging
- [ ] Test failover (kill one Router instance)
- [ ] Monitor disk usage (logs/metrics)
- [ ] Backup configuration regularly

---

## Support

For issues or questions:
1. Check `docs/CONFIG.md` for configuration reference
2. Review `ARCHITECTURE.md` for design details
3. Check `README.md` for quick start
4. Open issue on GitHub
