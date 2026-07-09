# Nexus Router Monitoring Guide

## Overview

Nexus Router includes comprehensive observability through:

- **Prometheus** — Metrics collection and alerting
- **Grafana** — Dashboards and visualization
- **Elasticsearch** — Log storage
- **Kibana** — Log analysis
- **Jaeger** — Distributed tracing

---

## Prometheus Metrics

### Request Metrics

```
nexus_router_requests_total{method, route, status}
  Total HTTP requests

nexus_router_request_duration_seconds_bucket{le, method, route}
  Request latency histogram (p50, p95, p99)

nexus_router_request_duration_seconds_sum
nexus_router_request_duration_seconds_count
  Latency aggregates for averaging
```

### Authentication Metrics

```
nexus_router_auth_failures_total{reason}
  Total auth failures (missing_header, invalid_token, etc)

nexus_router_auth_latency_seconds
  Time spent in auth gate
```

### AI Provider Metrics

```
nexus_router_provider_selections_total{provider, pattern}
  Count of times each provider was selected

nexus_router_provider_fallback_total{provider, fallback}
  Count of fallback provider usage
```

### Federation Metrics

```
nexus_router_federation_peers_healthy
  Number of healthy federation peers

nexus_router_federation_requests_total{peer, region}
  Requests routed to each federation peer
```

---

## Grafana Dashboards

Included dashboard (`monitoring/grafana-dashboard.json`) shows:

1. **Overview**
   - Requests per second
   - Error rate
   - p50/p95/p99 latency

2. **Routing**
   - Requests by region (geo routing)
   - Requests by API path
   - Upstream service health

3. **AI Providers**
   - Provider selection frequency
   - Fallback rate
   - Cost breakdown (if available)

4. **Federation**
   - Healthy peers
   - Peer response times
   - Cross-cloud traffic

5. **Alerts**
   - High latency
   - High error rate
   - Auth failures
   - Service down

---

## Key Queries

### Request Rate by Region

```promql
sum by(region) (rate(nexus_router_requests_total[5m]))
```

### Error Rate

```promql
rate(nexus_router_requests_total{status=~"5.."}[5m])
```

### P99 Latency by Route

```promql
histogram_quantile(0.99, nexus_router_request_duration_seconds_bucket) by(route)
```

### Auth Failure Rate

```promql
rate(nexus_router_auth_failures_total[5m])
```

### AI Provider Distribution

```promql
sum by(provider) (rate(nexus_router_provider_selections_total[5m]))
```

---

## Elasticsearch / Kibana Logs

All request logs are sent to Elasticsearch as JSON:

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
  "region": "eu-west-1",
  "upstream": "nexus-ai",
  "headers": {
    "user_agent": "...",
    "accept_language": "..."
  }
}
```

### Example Kibana Queries

**Find all failed requests:**
```json
{
  "query": {
    "range": {
      "status_code": { "gte": 400 }
    }
  }
}
```

**Find slow requests (>500ms):**
```json
{
  "query": {
    "range": {
      "duration_ms": { "gte": 500 }
    }
  }
}
```

**Find requests from specific user:**
```json
{
  "query": {
    "term": {
      "user_id": "user123"
    }
  }
}
```

---

## Jaeger Distributed Tracing

Every request generates a distributed trace:

1. Trace enters Router (root span)
2. Sub-spans created for each middleware:
   - IP Router
   - Auth Gate
   - Request Enricher
   - API Router
   - AI Provider Router
   - Telemetry Emitter
3. Trace follows request to upstream service
4. Response trace collected

### Example Trace Flow

```
Trace ID: f8c3a1b2-d4e5-4f6a-8c9b-0a1b2c3d4e5f
├─ Root Span: nexus-router (1000ms total)
│  ├─ IP Router (2ms) → region=eu
│  ├─ Auth Gate (5ms) → phantom_did=verified
│  ├─ Request Enricher (1ms) → trace_id=added
│  ├─ API Router (3ms) → upstream=nexus-ai
│  ├─ AI Provider Router (2ms) → model=claude
│  ├─ Proxy to Upstream (980ms)
│  │  └─ nexus-ai service (980ms)
│  │     ├─ Auth (10ms)
│  │     ├─ Model selection (50ms)
│  │     ├─ Inference (900ms)
│  │     └─ Response format (20ms)
│  └─ Telemetry Emitter (2ms) → metrics sent
```

### Access Jaeger UI

http://localhost:16686

1. Select service: "nexus-router"
2. Filter by operation or tags
3. View trace waterfall
4. Identify bottlenecks

---

## Alerting Rules

Pre-configured alerts in `monitoring/prometheus-rules.yaml`:

### High Latency

```yaml
alert: NexusRouterHighLatency
expr: nexus_router:request_duration_seconds:rate5m > 0.5
for: 5m
```

Triggers if p50 latency > 500ms for 5 minutes.

### High Error Rate

```yaml
alert: NexusRouterHighErrorRate
expr: nexus_router:error_rate:rate5m > 0.05
for: 5m
```

Triggers if error rate > 5% for 5 minutes.

### Auth Failures

```yaml
alert: NexusRouterHighAuthFailures
expr: nexus_router:auth_failure_rate:rate5m > 0.1
for: 5m
```

Triggers if auth failure rate > 10% for 5 minutes.

### Service Down

```yaml
alert: NexusRouterServiceDown
expr: up{job="nexus-router"} == 0
for: 2m
```

Triggers if Router is unreachable for 2+ minutes.

---

## Performance Tuning

### Metrics Cardinality

High cardinality labels (e.g., user_id) should be avoided in Prometheus metrics as they create too many time series.

**Good labels:**
- `method`, `route`, `status` (limited set)
- `region`, `provider` (limited set)

**Bad labels:**
- `user_id` (unbounded)
- `request_id` (unique per request)

Use logs for high-cardinality data (Elasticsearch).

### Log Retention

Configure Elasticsearch index lifecycle:

```yaml
index:
  lifecycle:
    name: nexus-router-policy
    rollover_alias: nexus-router-logs
    policy:
      phases:
        hot:
          min_age: 0d
          actions:
            rollover:
              max_primary_store_size: 50gb
        warm:
          min_age: 3d
          actions:
            set_replicas:
              number_of_replicas: 0
        delete:
          min_age: 30d
          actions:
            delete: {}
```

---

## Health Checks

### Prometheus Scrape Targets

http://localhost:9090/targets

Should show:
- ✓ nexus-router
- ✓ nexus-cloud
- ✓ nexus-ai
- ✓ monitoring stack

### Elasticsearch Indices

```bash
curl http://localhost:9200/_cat/indices

# Should show:
# nexus-router-logs-2024-01-01
# nexus-router-logs-2024-01-02
# ...
```

### Jaeger Traces

http://localhost:16686

Select service "nexus-router" and verify recent traces appear.

---

## Dashboard Examples

### Request Timeline

```
Time    Requests    Error Rate    P99 Latency
12:00   500 req/s   0.5%          45ms
12:05   480 req/s   0.4%          42ms
12:10   520 req/s   0.6%          48ms ⚠️  (slight spike)
12:15   490 req/s   0.5%          44ms
```

### Regional Distribution

```
Region    Requests    Avg Latency    Error Rate
eu        40%         38ms           0.3%
us        45%         42ms           0.6%
ap        15%         52ms           0.4%
```

### AI Provider Selection

```
Provider    Count    Success Rate    Avg Latency
ollama      60%      99.8%           35ms
claude      25%      99.9%           55ms
groq        15%      99.5%           40ms
```

---

## Cost Optimization

If using paid AI providers, track spending:

```promql
sum by(provider) (
  nexus_router_provider_selections_total * cost_per_call
)
```

Example dashboard widget showing cost per region/provider.

---

## Troubleshooting

### Metrics Missing

1. Check Router is exposing `/metrics` endpoint
2. Verify Prometheus scrape_interval
3. Check Docker network connectivity
4. Restart Prometheus: `docker-compose restart prometheus`

### High Cardinality Warning

If Prometheus complains about high cardinality:

1. Identify label causing issue: `cardinality(metric)`
2. Remove or constrain label
3. Use logs for detailed breakdown
4. Restart Prometheus

### Elasticsearch Out of Disk

```bash
# Check disk usage
curl http://localhost:9200/_cat/allocation

# Delete old indices
curl -X DELETE http://localhost:9200/nexus-router-logs-2023-*

# Increase volume size
docker-compose down
# Edit docker-compose.yml to increase elasticsearch_data volume
docker-compose up -d
```

---

For more details, see `INTEGRATION.md` and `CONFIG.md`.
