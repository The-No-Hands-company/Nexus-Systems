# Nexus Router Deployment Guide

## Quick Start (5 minutes)

### Prerequisites

- Docker & Docker Compose
- Nexus-Cloud running on :8787
- 4 GB RAM available

### Deploy

```bash
cd apps/Nexus-Router

# Start Router
docker-compose up -d

# Start monitoring
docker-compose -f monitoring/docker-compose.monitoring.yml up -d

# Verify
curl http://localhost:9999/health
curl http://localhost:3000  # Grafana
```

### Configure

Edit `config/routing-policy.yaml` with your settings, then:

```bash
docker-compose restart nexus-router
```

---

## Production Deployment

### High Availability (HA)

Deploy multiple Router instances behind a load balancer:

```yaml
version: "3.9"

services:
  load-balancer:
    image: nginx:alpine
    ports:
      - "9999:80"
    volumes:
      - ./nginx.conf:/etc/nginx/nginx.conf:ro
    depends_on:
      - router-1
      - router-2
      - router-3

  router-1:
    build: .
    environment:
      NEXUS_ROUTER_PORT: "9999"
    ports:
      - "9991:9999"

  router-2:
    build: .
    environment:
      NEXUS_ROUTER_PORT: "9999"
    ports:
      - "9992:9999"

  router-3:
    build: .
    environment:
      NEXUS_ROUTER_PORT: "9999"
    ports:
      - "9993:9999"
```

**nginx.conf:**
```nginx
upstream nexus_router {
  least_conn;
  server router-1:9999 weight=1;
  server router-2:9999 weight=1;
  server router-3:9999 weight=1;
}

server {
  listen 80;
  location / {
    proxy_pass http://nexus_router;
  }
}
```

### Kubernetes Deployment

**Helm Chart Values:**

```yaml
replicaCount: 3

image:
  repository: nexus-router
  tag: "0.1.0"
  pullPolicy: IfNotPresent

serviceType: LoadBalancer
servicePort: 9999

resources:
  requests:
    cpu: 200m
    memory: 256Mi
  limits:
    cpu: 1000m
    memory: 1Gi

autoscaling:
  enabled: true
  minReplicas: 2
  maxReplicas: 10
  targetCPUUtilizationPercentage: 70

configMap:
  routing-policy.yaml: |
    router:
      port: 9999
    cloud:
      url: "http://nexus-cloud:8787"
    # ... rest of config

secrets:
  NEXUS_CLOUD_API_KEY: "${NEXUS_CLOUD_API_KEY}"
```

### Systemd Service (Single Machine)

**File:** `/etc/systemd/system/nexus-router.service`

```ini
[Unit]
Description=Nexus Router
After=network.target nexus-cloud.service
Wants=nexus-cloud.service

[Service]
Type=simple
User=nexus
WorkingDirectory=/opt/nexus-systems/apps/Nexus-Router
Environment="NEXUS_ROUTER_PORT=9999"
Environment="NEXUS_CLOUD_URL=http://localhost:8787"
Environment="NEXUS_CLOUD_API_KEY=change-me"
Environment="CONFIG_PATH=/opt/nexus-systems/apps/Nexus-Router/config/routing-policy.yaml"
ExecStart=/usr/bin/bun run src/index.ts
Restart=on-failure
RestartSec=10s

[Install]
WantedBy=multi-user.target
```

Enable and start:

```bash
sudo systemctl enable nexus-router
sudo systemctl start nexus-router
sudo systemctl status nexus-router
```

---

## Configuration

### Environment Variables

Set before starting:

```bash
NEXUS_ROUTER_PORT=9999
NEXUS_ROUTER_HOST=0.0.0.0
NEXUS_CLOUD_URL=http://nexus-cloud:8787
NEXUS_CLOUD_API_KEY=your-secure-key
CONFIG_PATH=/etc/nexus/routing-policy.yaml
LOG_LEVEL=info
NODE_ENV=production
```

### Networking

**Ports:**
- 9999 — HTTP API
- 6831 — Jaeger tracing (UDP, optional)

**Firewall rules:**

```bash
# Allow from load balancer
iptables -A INPUT -s 10.0.0.0/8 -p tcp --dport 9999 -j ACCEPT

# Allow from specific networks
iptables -A INPUT -s 192.168.0.0/16 -p tcp --dport 9999 -j ACCEPT
```

### TLS/HTTPS

Run Router behind reverse proxy (Nginx, Traefik):

**Nginx:**
```nginx
upstream nexus_router {
  server localhost:9999;
}

server {
  listen 443 ssl http2;
  server_name nexus-router.example.com;

  ssl_certificate /etc/letsencrypt/live/nexus-router.example.com/fullchain.pem;
  ssl_certificate_key /etc/letsencrypt/live/nexus-router.example.com/privkey.pem;

  location / {
    proxy_pass http://nexus_router;
    proxy_set_header X-Forwarded-For $remote_addr;
    proxy_set_header X-Forwarded-Proto https;
  }
}
```

---

## Monitoring & Logging

### Prometheus

Scraped from `:9999/metrics`

```yaml
global:
  scrape_interval: 15s

scrape_configs:
  - job_name: 'nexus-router'
    static_configs:
      - targets: ['nexus-router.example.com:9999']
```

### Elasticsearch

Logs shipped to Elasticsearch at startup.

**Index:** `nexus-router-logs-YYYY.MM.DD`

**Retention:** Configure in `elasticsearch.yml`:

```yaml
index:
  lifecycle:
    enabled: true
    rollover:
      max_age: 1d
    delete:
      delete_after: 30d
```

### Log Aggregation

For multi-instance deployments, centralize logs:

**Filebeat:**
```yaml
filebeat.inputs:
  - type: container
    enabled: true
    containers.ids:
      - '*'

processors:
  - add_docker_metadata: ~

output.elasticsearch:
  hosts: ["elasticsearch:9200"]
```

---

## Backup & Recovery

### Configuration Backup

```bash
# Daily backup
0 2 * * * cp /etc/nexus/routing-policy.yaml /backups/routing-policy-$(date +%Y%m%d).yaml
```

### Data Backup (Prometheus/Elasticsearch)

```bash
# Stop services
docker-compose down

# Backup volumes
sudo tar -czf prometheus-backup-$(date +%Y%m%d).tar.gz \
  /var/lib/docker/volumes/*/prometheus_data

sudo tar -czf elasticsearch-backup-$(date +%Y%m%d).tar.gz \
  /var/lib/docker/volumes/*/elasticsearch_data

# Restart
docker-compose up -d
```

### Recovery

```bash
# Restore volumes
sudo tar -xzf prometheus-backup-YYYYMMDD.tar.gz -C /
sudo tar -xzf elasticsearch-backup-YYYYMMDD.tar.gz -C /

# Restart services
docker-compose restart prometheus elasticsearch
```

---

## Performance Tuning

### Resource Allocation

**Recommended (production):**
- CPU: 2 cores minimum, 4+ recommended
- RAM: 2 GB minimum, 4+ GB recommended
- Disk: 50 GB for logs (Elasticsearch)

### JVM Tuning (Elasticsearch)

```bash
# In docker-compose.yml
environment:
  ES_JAVA_OPTS: "-Xms1g -Xmx1g"
```

### Connection Pooling

**Nexus Router** reuses HTTP connections:

```typescript
const agent = new http.Agent({
  keepAlive: true,
  maxSockets: 100,
  maxFreeSockets: 10,
});
```

### Caching

Implement Redis cache for service registry:

```typescript
const tools = await redis.get('nexus:tools:cache');
if (!tools) {
  const fresh = await queryCloud();
  await redis.setex('nexus:tools:cache', 300, fresh);
}
```

---

## Scaling

### Horizontal Scaling

Add more Router instances:

```bash
# Scale to 5 instances
docker-compose up -d --scale router=5
```

### Load Distribution

- **Round Robin** (default) — Each upstream gets equal traffic
- **Least Connections** — Route to least busy upstream
- **IP Hash** — Client IP determines upstream (for stickiness)

Configure in `routing-policy.yaml`:

```yaml
routes:
  "/api/chat":
    load_balance: "round_robin"  # or "least_conn" or "ip_hash"
```

---

## Troubleshooting

### Router won't start

```bash
# Check logs
docker-compose logs nexus-router

# Validate config
bun run src/lib/config.ts

# Check port availability
lsof -i :9999
```

### High memory usage

```bash
# Monitor
watch -n 1 'docker stats nexus-router'

# Increase limit
docker-compose down
# Edit docker-compose.yml: add memory limit
docker-compose up -d
```

### Metrics not being scraped

```bash
# Verify endpoint exists
curl http://localhost:9999/metrics

# Check Prometheus targets
curl http://prometheus:9090/api/v1/targets
```

---

## Security Hardening

1. **API Key Management**
   - Generate strong key: `openssl rand -hex 32`
   - Store in secure vault (HashiCorp Vault, AWS Secrets Manager)
   - Rotate quarterly

2. **Network Isolation**
   - Run in private network
   - Restrict incoming ports
   - Use VPN for remote access

3. **Audit Logging**
   - Enable all request logging
   - Send to centralized SIEM
   - Monitor for suspicious patterns

4. **Rate Limiting**
   - Implement per-user, per-org limits
   - Use Redis for distributed rate limiting
   - Alert on abuse

---

For more information, see `INTEGRATION.md` and `MONITORING.md`.
