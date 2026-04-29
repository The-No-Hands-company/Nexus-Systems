# Nexus Systems Integration Architecture

## 1. Nexus Cloud as the central hub

Yes — Nexus Cloud should be the central hub for Nexus Systems.
It is the registry, discovery plane, and metadata router for all Nexus-native services.

### NS technology strategy
- NS is polyglot by design: choose the best stack per tool, but keep integration consistent.
- Node/TS is the preferred ecosystem glue for UI/dashboard, orchestration, and service adapters.
- Python should remain the primary choice for AI/ML and rapid experimentation.
- Rust should remain the primary choice for protocol, privacy, and performance-sensitive runtimes.
- Other languages are allowed only when they deliver a clear, tool-specific advantage.
- Shared contracts are required across projects: manifest/schema, auth/API standards, deployment conventions, and health/observability surfaces.

### Nexus Cloud responsibilities

- Tool/service registration
- Heartbeat and liveness tracking
- Proxy/routing metadata (`upstreamUrl`)
- Dashboard/UI discovery
- Shared service metadata and capabilities
- Exposure and public URL issuance
- Central hub for cross-project integration

This makes Nexus Cloud the central Nexus Systems control plane.

---

## 2. Phantom crate capabilities mapped to Nexus roles

### Phantom workspace -> Nexus role mapping

| Phantom crate | Primary capability | Nexus role | Integration purpose |
|---|---|---|---|
| `phantom-core` | Packet model, protocol primitives, core packet format | Nexus Phantom core engine | Base privacy protocol and transport semantics |
| `phantom-crypto` | PQ key exchange, FHE, ZK primitives | Nexus Phantom crypto backend | Provides post-quantum, FHE, and proof primitives for secure transport and sandboxing |
| `phantom-routing` | Oblivious routing engine | Nexus Phantom route substrate | Implements routing on encrypted metadata and private forwarding |
| `phantom-zkvm` | Zero-knowledge VM integration and proof generation | Nexus Phantom attestation layer | Verifies correct protocol execution and sandbox proofs |
| `phantom-discovery` | Anonymous node discovery and zk membership | Nexus Phantom discovery service | Finds privacy nodes without leaking topology |
| `phantom-node` | Runtime node implementation + CLI | Nexus Phantom runtime service | Deployable node process for the Nexus privacy network |
| `phantom-circuit` | Circuit construction, proof circuits | Nexus Phantom proof infrastructure | Builds and validates zk circuits used by the protocol |
| `phantom-simulation` | Network simulation and stress testing | Nexus Phantom validation/testing | Validates large-scale network behavior before production deployment |

### What Phantom brings to Nexus Systems

- A dedicated privacy transport layer for Nexus service traffic
- Strong protocol-level isolation and proof-based correctness
- A runtime that can be used by Nexus Hosting and Nexusclaw for secure comms
- A separate product/technology axis, not a duplicate of Nexusclaw

---

## 3. Nexus service manifest / Cloud registration schemas

### Cloud query/registration contract

Nexus Cloud exposes these endpoints:

- `GET /api/v1/tools`
- `POST /api/v1/tools`
- `GET /api/v1/tools/:toolId`
- `PATCH /api/v1/tools/:toolId`
- `POST /api/v1/tools/:toolId/heartbeat`

### Registration schema

```ts
export type SystemsApiToolRegistrationRequestDTO = {
  id: string;
  name: string;
  description: string;
  upstreamUrl?: string;
  mode?: "standalone" | "orchestrated";
  exposed?: boolean;
  health?: "healthy" | "degraded" | "offline";
  capabilities?: readonly string[];
};
```

### Heartbeat schema

```ts
export type SystemsApiNodeHeartbeatRequestDTO = {
  upstreamUrl?: string;
  health?: "healthy" | "degraded" | "offline";
};
```

### Core tool schema

```ts
export type SystemsApiTool = {
  id: string;
  name: string;
  description: string;
  mode: "standalone" | "orchestrated";
  exposed: boolean;
  exposure: "private" | "public" | "pending";
  health: "healthy" | "degraded" | "offline";
  capabilities: readonly string[];
  publicUrl?: string;
  upstreamUrl?: string;
  registeredAt: string;
  updatedAt: string;
};
```

---

## 4. Example Nexus Cloud registrations

### Nexusclaw registration

```json
{
  "id": "nexusclaw",
  "name": "Nexusclaw",
  "description": "Nexus AI agent orchestration framework",
  "upstreamUrl": "http://localhost:18800",
  "mode": "standalone",
  "exposed": true,
  "health": "healthy",
  "capabilities": [
    "agent-framework",
    "memory",
    "tools",
    "safety",
    "gsd",
    "gateway"
  ]
}
```

### Phantom registration

```json
{
  "id": "nexus-phantom",
  "name": "Nexus Phantom",
  "description": "Privacy protocol runtime for secure, oblivious network transport",
  "upstreamUrl": "http://localhost:19700",
  "mode": "standalone",
  "exposed": true,
  "health": "healthy",
  "capabilities": [
    "privacy",
    "oblivious-routing",
    "zk-verification",
    "post-quantum",
    "secure-transport"
  ]
}
```

### Nexus Hosting registration

```json
{
  "id": "nexus-hosting",
  "name": "Nexus Hosting",
  "description": "Decentralized runtime host for Nexus services",
  "upstreamUrl": "http://localhost:8788",
  "mode": "standalone",
  "exposed": true,
  "health": "healthy",
  "capabilities": [
    "hosting",
    "static-sites",
    "federation",
    "ssl",
    "custom-domains"
  ]
}
```

### Nexus Cloud registration

```json
{
  "id": "nexus-cloud",
  "name": "Nexus Cloud",
  "description": "Central registry and orchestration hub for Nexus Systems",
  "upstreamUrl": "http://localhost:8787",
  "mode": "standalone",
  "exposed": true,
  "health": "healthy",
  "capabilities": [
    "tool-registry",
    "heartbeat",
    "route-management",
    "discovery",
    "dashboard"
  ]
}
```

---

## 5. Combined Nexus integration architecture

### High-level flow

1. **Nexus Cloud** is the central hub.
2. **Nexusclaw** registers as an AI agent service.
3. **Nexus Phantom** registers as the privacy/sandbox service.
4. **Nexus Hosting** registers as the deployment/runtime host.
5. Each service sends periodic heartbeats to Cloud.
6. Cloud exposes a unified dashboard and routing table.

### Integration surface

#### Nexus Cloud → Nexusclaw

- Cloud discovers Nexusclaw via `POST /api/v1/tools`
- Cloud routes agent UI traffic to `Nexusclaw.upstreamUrl`
- Cloud can display Nexusclaw capabilities and health
- Cloud can expose Nexusclaw public URLs or custom domains if desired

#### Nexus Cloud → Nexus Phantom

- Phantom registers as a separate tool/service
- Cloud tracks its health and `upstreamUrl`
- Cloud can add Phantom to dashboard as a privacy runtime
- Cloud can expose Phantom endpoints or dashboards to authorized users

#### Nexus Cloud → Nexus Hosting

- Hosting registers and exposes deployment UI or hosting API
- Cloud can bind custom domains via exposed `upstreamUrl`
- Cloud can route hosting status/data into a unified Nexus dashboard

#### Nexusclaw ↔ Phantom

- Nexusclaw uses Phantom as a secure transport/sandbox layer
- Nexusclaw agents can call a Phantom tool or service to route sensitive actions
- Phantom can provide a sandbox boundary for Nexusclaw tool execution
- Nexusclaw can indicate when a tool/action must use privacy mode

#### Nexus Hosting ↔ Phantom

- Hosting can deploy Phantom nodes as part of a secure runtime topology
- Hosting can use Phantom to isolate network traffic for hosted apps
- Phantom can secure Nexus Hosting control plane communications

#### Nexusclaw ↔ Nexus Hosting

- Nexusclaw agents can deploy or manage content via Nexus Hosting
- Nexus Hosting can host Nexusclaw dashboards or agent UIs
- Hosting can provide a runtime platform for agent-assisted deployments

### Central hub responsibilities

- **Discovery**: Map services to URLs and capabilities
- **Health**: mark services offline after missed heartbeats
- **Proxy/Routes**: include registered `upstreamUrl` in route table
- **Dashboard**: show service tiles and status
- **Trust**: provide shared service metadata and optional auth tokens

---

## 6. Nexus Cloud manifest recommendations

### Recommended tool manifest fields

| Field | Type | Required | Notes |
|---|---|---|---|
| `id` | string | yes | unique service identifier |
| `name` | string | yes | human-readable service name |
| `description` | string | yes | concise service summary |
| `upstreamUrl` | string | no | backend URL or UI endpoint |
| `mode` | `standalone`  `orchestrated` | no | runtime mode |
| `exposed` | boolean | no | whether Cloud should expose it publicly |
| `health` | `healthy` / `degraded` / `offline` | no | liveness state |
| `capabilities` | string[] | no | service feature set |
| `publicUrl` | string | no | optional issued public URL |

### Recommended capability taxonomy

- `agent-framework`
- `memory`
- `tools`
- `safety`
- `gsd`
- `gateway`
- `privacy`
- `oblivious-routing`
- `zk-verification`
- `post-quantum`
- `secure-transport`
- `hosting`
- `static-sites`
- `federation`
- `ssl`
- `custom-domains`
- `tool-registry`
- `route-management`
- `discovery`
- `dashboard`

---

## 7. Concrete architecture diagram

```
                   +---------------------------+
                   |      Nexus Cloud Hub      |
                   |  (registry / discovery /  |
                   |   health / dashboard)     |
                   +-----------+---------------+
                               |
           +-------------------+-------------------+
           |                   |                   |
+----------v---------+ +-------v--------+ +--------v-------+
|   Nexusclaw         | | Nexus Hosting  | | Nexus Phantom   |
|  (agent framework)  | | (runtime host) | | (privacy layer) |
+----------+---------+ +-------+--------+ +--------+-------+
           |                   |                   |
           |    +--------------+--------------+    |
           |    | Shared Nexus integration     |    |
           |    | - auth/token exchange        |    |
           |    | - service discovery          |    |
           |    | - dashboard/status           |    |
           |    +-----------------------------+    |
           |                   |                   |
           v                   v                   v
      Agent tool         Hosted app            Secure transport
      execution         deployment             and sandboxing
```

---

## 8. Practical recommendations

1. Keep Nexus Cloud as the central hub.
2. Register Phantom as a first-class Nexus service, not just a library.
3. Use the exact Cloud schema shown above for tool registration and heartbeats.
4. Use Nexusclaw as the agent orchestration and service integration layer.
5. Use Phantom as the privacy/sandbox protocol runtime and secure transport layer.
6. Use Nexus Hosting as the service runtime host and deployment environment.

---

## 9. Suggested next action

- Add `nexus-phantom` as a service registration example in Nexus Cloud docs.
- Add a `Nexus Phantom` tile to the Nexus dashboard.
- Add a Nexusclaw `tool` or `plugin` that can call Phantom for privacy-sensitive tasks.
- Add a shared auth/token flow for Cloud-discovered service access.
