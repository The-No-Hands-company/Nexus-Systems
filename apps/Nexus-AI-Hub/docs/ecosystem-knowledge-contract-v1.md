# Nexus AI ecosystem knowledge contract v1

This contract is the initial source for ecosystem-grounded retrieval in Nexus AI
Hub. It describes platform facts and approved Cloud catalog entries. It does not
train a model, select a model provider, or authorize an action.

## Endpoints

| Method | Path | Purpose |
| --- | --- | --- |
| `GET` | `/api/v1/knowledge` | Return the bounded knowledge index |
| `GET` | `/api/v1/knowledge/search?q=...` | Return documents matching every query term |

The response has this shape:

```json
{
  "protocol": "nexus-knowledge/1",
  "status": "ready",
  "generatedAt": "2026-10-18T00:00:00.000Z",
  "source": {
    "status": "ready",
    "cloudUrl": "http://localhost:8787",
    "catalogEndpoint": "http://localhost:8787/api/v1/catalog"
  },
  "documents": [
    {
      "id": "platform-auth",
      "title": "Nexus Auth",
      "kind": "platform",
      "summary": "Identity, authentication, SSO, sessions, and role checks for the hosted platform.",
      "content": "...",
      "tags": ["auth", "login", "sso"],
      "trust": "contract",
      "source": {
        "kind": "architecture",
        "ref": "docs/architecture/nexus-platform-surfaces.md",
        "revision": "v1"
      }
    }
  ]
}
```

## Trust and fallback rules

- `trust: "contract"` means the document comes from a committed Nexus
  architecture contract.
- `trust: "approved-catalog"` means the document was derived from Cloud's
  public `/api/v1/catalog` projection. The catalog record's review status and
  maturity remain in the document content.
- `status: "ready"` means the current request fetched a usable Cloud catalog.
- `status: "degraded"` means Cloud could not be reached. The local platform
  contract is still returned so the assistant can explain known boundaries while
  clearly reporting that live app data may be stale.
- The endpoint is read-only. Retrieval must never imply approval to mutate a
  service, launch a terminal command, or expose a private upstream URL.

## Initial platform facts

The bounded index establishes these facts for all consumers:

- Nexus Auth is a platform service used for login and identity, not a launcher
  app in the hosted cloud.
- Nexus Cloud is the hosted control plane and admin console at `/cloud`.
- Nexus Chat is the user messaging app at `/chat`; `/nexus` is a compatibility
  alias and does not name another product.
- Nexus API is the versioned gateway and integration surface under `/api/v1`.
- Nexus Terminal is the privileged, audited developer and operations tool at
  `/terminal`.
- Nexus AI Hub starts with ecosystem retrieval; model providers can be added
  without replacing the contract source.
