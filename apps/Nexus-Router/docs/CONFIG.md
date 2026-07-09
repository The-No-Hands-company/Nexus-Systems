# Nexus Router — Configuration Reference

## Full YAML Schema

```yaml
# Router server settings
router:
  port: 9999                    # Port to listen on
  hostname: "0.0.0.0"          # Bind address

# Nexus-Cloud integration
cloud:
  url: "http://localhost:8787"  # Cloud endpoint
  apiKey: "change-me"           # API key for registration
  heartbeat_interval_sec: 30    # Heartbeat frequency

# Authentication settings
auth:
  phantom_enabled: true         # Enable Phantom DID validation
  jwt_secret: "your-secret"     # JWT signing key (optional)

# Geographic routing
geography:
  default_region: "us-east-1"   # Default if no match
  regions:
    eu:                          # Region name
      cidrs:                     # CIDR blocks for this region
        - "2.0.0.0/8"
        - "3.0.0.0/8"
      cloud_url: "http://..."    # Cloud endpoint for this region
    us:
      cidrs:
        - "1.0.0.0/8"
      cloud_url: "http://..."

# API routing rules
routes:
  "/api/path":                   # Request path pattern
    upstreams:                   # List of target services
      - name: "service-name"     # Service identifier
        priority: 1              # Lower = higher priority
    load_balance: "round_robin" # round_robin or random
    federation_aware: false      # Route across federation?

# AI provider routing
ai_providers:
  default_model: "ollama"        # Default provider
  routing_rules:
    - pattern: "legal:*"         # Task pattern (glob)
      model: "claude"            # Primary model
      fallback: "gpt-4"          # Fallback if unavailable
    - pattern: "*"               # Catch-all
      model: "ollama"
      fallback: "groq"

# Telemetry configuration
telemetry:
  logs:
    enabled: true                # Enable log collection
    format: "json"               # json or text
    elasticsearch:
      url: "http://localhost:9200" # ES endpoint
  metrics:
    enabled: true                # Enable metrics
    prometheus:
      url: "http://localhost:9090"  # Prometheus endpoint
  tracing:
    enabled: true                # Enable distributed tracing
    jaeger:
      url: "http://localhost:6831"  # Jaeger endpoint

# Federation (multi-cloud)
federation:
  enabled: false                 # Enable federation routing
  peers:                         # List of peer clouds
    - name: "cloud-eu"           # Peer name
      url: "http://cloud-eu.local:8787" # Cloud endpoint
      region: "eu-west-1"        # Region name
      trust_level: "verified"    # verified or trusted
```

## Environment Variable Overrides

```bash
# Router settings
NEXUS_ROUTER_PORT=9999
NEXUS_ROUTER_HOST=0.0.0.0

# Cloud connection
NEXUS_CLOUD_URL=http://localhost:8787
NEXUS_CLOUD_API_KEY=change-me

# Configuration file
CONFIG_PATH=./config/routing-policy.yaml

# Logging
LOG_LEVEL=debug
NODE_ENV=production
```

## Example Configurations

### Geo-Routing (US + EU)

```yaml
geography:
  default_region: "us-east-1"
  regions:
    eu:
      cidrs:
        - "2.0.0.0/8"      # Europe
        - "3.0.0.0/8"
      cloud_url: "http://nexus-eu.local:8787"
    us:
      cidrs:
        - "1.0.0.0/8"      # USA
      cloud_url: "http://nexus-us.local:8787"
```

### Model Selection

```yaml
ai_providers:
  default_model: "ollama"
  routing_rules:
    # Legal → Claude (most capable)
    - pattern: "legal:*"
      model: "claude"
      fallback: "gpt-4"
    # Code → Groq (fast, cheap)
    - pattern: "code:quick"
      model: "groq"
      fallback: "ollama"
    # Everything else → local Ollama
    - pattern: "*"
      model: "ollama"
      fallback: "groq"
```

### API Routing

```yaml
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

  "/api/forge":
    upstreams:
      - name: "nexus-forge"
```

## Validation

The configuration is validated against a Zod schema on startup. Invalid config will cause the router to exit with an error.

```bash
Error: Invalid config at routes."/api/chat".upstreams.0
  Expected object with required fields: name
```
