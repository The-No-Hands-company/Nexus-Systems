# Nexus-Cloud Registration Contract

This document defines the current Nexus Wiki to Nexus-Cloud registration payload contract used by the endpoint:

- `POST /api/v1/integrations/nexus-cloud/register`

## Request

```json
{
  "service_name": "nexus-wiki",
  "service_url": "http://localhost:9000",
  "health_endpoint": "/health",
  "capabilities": ["wiki", "search", "revision-history"]
}
```

Field notes:

- `service_name`: stable service identifier.
- `service_url`: public or internal base URL reachable by Nexus-Cloud.
- `health_endpoint`: relative health route exposed by service.
- `capabilities`: declarative capability list for routing and orchestration decisions.

## Response (Nexus Wiki endpoint)

```json
{
  "status": "registered",
  "upstream_status_code": 201,
  "upstream_body": {
    "registration_id": "abc123"
  }
}
```

`upstream_body` is passthrough from Nexus-Cloud Systems API and may be object, list, string, or null.

## Error Semantics

- If Nexus-Cloud is unreachable, Nexus Wiki returns `502`.
- If Nexus-Cloud rejects registration (`4xx` or `5xx`), Nexus Wiki returns `502` and includes upstream status/body details in `detail`.

## Upstream URL Configuration

The registration target URL is configured via:

- `NEXUS_CLOUD_SYSTEMS_API_URL`

Default value when unset:

- `http://localhost:8080/api/v1/systems/register`
