# Nexus Systems — Architecture Guide

> Ecosystem monotree. 80+ apps. One control plane. Polyglot by design.

---

## 1. Ecosystem Overview

Nexus Systems is a polyglot application ecosystem organized as a single git repository with 89 entries under `apps/`:

| Classification | Count |
|---|---|
| Built (TypeScript, Bun) | 24 |
| Built (Rust, C++, Python) | 6 |
| Empty shells (scaffolded) | 57 |
| Meta (docs, test infra) | 2 |
| **Total** | **89** |

Six apps form the **graphics domain** (Graphic, Design, Draw, Photos, Video, GPU-Test), each using a triple-stack architecture: Bun server → React frontend → Python FastAPI backend → Rust WASM engine.

The **Nexus-Cloud** control plane at port 8787 orchestrates service discovery, topology, routing, and tool registration across all running apps.

---

## 2. Directory Layout

```
Nexus-Systems/
├── .gitmodules              # 9 submodules (some uninitialized)
├── docker-compose.yml       # Infrastructure + Cloud orchestration
├── package.json             # Root workspace (pnpm: packages/*, apps/nexus-*, apps/Nexus-*)
├── run-all.sh               # Launch all ecosystem services
├── e2e-test.sh              # End-to-end ecosystem tests
├── test-integration.sh      # Cross-service integration tests
├── test-apps.sh             # Per-app test runner
├── autopilot.py             # Ecosystem automation/orchestration
│
├── apps/                    # All 89 application directories
│   ├── Nexus-Cloud/         # Control plane (Bun/TS, port 8787)
│   ├── Nexus-Systems-API/   # Cross-service contract definitions
│   ├── Nexus-Graphic/       # Graphics app (Bun+React+Python+Rust)
│   │   ├── src/             # Bun/TS server
│   │   ├── frontend/        # React + Vite (npm package)
│   │   ├── backend/         # Python FastAPI
│   │   └── engine/          # Rust WASM crate
│   ├── Nexus-Design/        # (same structure as Graphic)
│   ├── Nexus-Draw/          # (same structure as Graphic)
│   ├── Nexus-Photos/        # (same structure as Graphic)
│   ├── Nexus-Video/         # (same structure as Graphic)
│   ├── Nexus-GPU-Test/      # (same structure, no Bun server)
│   ├── Nexus/               # Rust monorepo (8 crates, ~66k loc, submodule)
│   ├── Phantom/             # Rust monorepo (8 crates, ~21k loc, submodule)
│   ├── Nexusclaw/           # Hybrid TS/C++ agent framework
│   ├── Nexuslang/           # Python/C++ language compiler
│   ├── Nexus-Modeling/      # C++ geometry kernel (CMake)
│   ├── Nexus-Wiki/          # Python wiki engine
│   ├── docs/                # Ecosystem-level documentation
│   └── tests/               # Root-level test infrastructure
│
├── packages/                # Shared npm packages (workspace)
├── nxl-demos/               # Nexuslang demos
├── dhts/                    # Distributed hash table data
├── data/                    # Mounted data volumes
├── Backups/                 # Backup storage
├── scripts/                 # Build/deploy/ops scripts
└── docs/                    # Top-level ecosystem docs
```

### Standard App Layout (Bun/TS)

```
apps/Nexus-<Name>/
├── AGENTS.md               # AI agent instructions
├── README.md               # App README
├── package.json            # Bun/TS app (name: nexus-<name>, v0.1.0)
├── tsconfig.json           # Strict TypeScript config
├── biome.json              # Biome lint/format config
├── src/
│   ├── index.ts            # Entrypoint
│   ├── server.ts           # HTTP server
│   ├── <name>-engine.ts    # Domain logic
│   ├── contracts.ts        # API contracts
│   └── cloud.ts            # Cloud registration + heartbeat
└── tests/
    └── server.test.ts      # Test harness
```

### Graphics App Layout (triple-stack)

```
apps/Nexus-<App>/
├── src/                    # Bun/TS gateway server
├── frontend/               # React + Vite (package.json with react ^19.0)
├── backend/                # Python FastAPI (pyproject.toml, requires-python >=3.12)
└── engine/                 # Rust WASM crate (Cargo.toml, cdylib + rlib)
```

---

## 3. Port Allocation Scheme

| Range | Purpose |
|---|---|
| **8787** | Nexus-Cloud control plane |
| **3000–3099** | Core infrastructure apps (Hosting, Vault, Forge, Engine, Deploy, Monitor) |
| **3100–3199** | Reserved (expansion) |
| **3200–3299** | Network/security apps (Network, Tunnel, Edge, Files, Guardian, Auth) |
| **3300–3399** | API/meta apps (Systems-API, Compliance, Search, AI-Hub, Testing, API, IDE) |
| **3400–3499** | AI apps (Nexus-AI) |
| **3500–3599** | Computing apps (Nexus-Computer) |
| **3600–3699** | Graphics triple-stack apps (Graphic, Design, Draw, Photos, Video, GPU-Test) |
| **3700–3999** | Shell apps (assigned at build time) |
| **5432** | PostgreSQL |
| **6379** | Redis |
| **9000/9001** | MinIO (S3 API / Console) |

Port assignments are registered in `src/server.ts` per app and discoverable via `GET /api/v1/topology` from Nexus-Cloud.

---

## 4. Stack Per Domain

| Domain | Stack | Rationale |
|---|---|---|
| **Standard apps** (Office, Spend, CRM, etc.) | Bun + SQLite + TypeScript (strict) | Fast startup, zero-dependency local DB, unified language |
| **Graphics** (Graphic, Design, Draw, Photos, Video, GPU-Test) | Bun gateway + React 19 + Python/FastAPI + Rust WASM | React for UI responsiveness, Python for async task queues (Celery), Rust for compute-intensive rendering |
| **Security / Crypto** (Vault, Guardian) | Rust / C++ | Memory safety for secrets, zero-cost abstractions for encryption |
| **AI / ML** (Nexus-AI, AI-Hub, Inference) | Python / FastAPI | Ecosystem maturity for ML libraries, FastAPI for high-throughput inference APIs |
| **Networking** (Network, Tunnel, Edge) | Bun + TypeScript | Efficient I/O via Bun's native HTTP, low-latency event loop |
| **Modeling / Simulation** | C++ (CMake) + Vulkan | Geometry kernels, GPU compute, performance-critical simulation |
| **Language Tooling** (Nexuslang) | Python + C++ | Python for compiler frontend, C++ for runtime performance |
| **Wiki** | Python | Python ecosystem for document processing, templates, search |

---

## 5. Service Discovery

### Nexus-Cloud — The Nerve System

Nexus-Cloud (port 8787) is the single control plane for all service discovery:

```
App Startup Flow:
  1. App boots → reads NEXUS_CLOUD_URL from env
  2. App calls POST /api/v1/tools → registers with capability tags
  3. App begins heartbeat loop → POST /api/v1/tools/:toolId/heartbeat (interval: 30s)
  4. Cloud maintains live topology in memory + PostgreSQL
```

### Discovery APIs (all at Nexus-Cloud)

| Endpoint | Method | Returns |
|---|---|---|
| `/api/v1/tools` | GET | All registered tools with health state |
| `/api/v1/tools` | POST | Register new tool |
| `/api/v1/tools/:toolId/heartbeat` | POST | Heartbeat ping |
| `/api/v1/topology` | GET | Full ecosystem graph (apps + connections) |
| `/api/v1/routes` | GET | Active domain-to-upstream routing table |
| `/api/v1/addresses` | POST | Request domain/exposure for an app |
| `/api/v1/status` | GET | Healthy tool count, live state |

### Systems API Contracts

The `apps/Nexus-Systems-API/` package defines cross-service contract types and client utilities. Every app imports and consumes these:

- `Client` class — typed HTTP client for Nexus-Cloud Systems API
- `Contracts` types — Tool, Topology, Route, Address, Heartbeat schemas
- `Examples` — reference usage patterns

---

## 6. Data Flow

```
                         ┌──────────────────────────┐
                         │     Nexus-Cloud :8787     │
                         │  ┌────────────────────┐   │
                         │  │  Service Registry   │   │
                         │  │  Topology Graph     │   │
                         │  │  Route Table        │   │
                         │  │  Audit Trail        │   │
                         │  └────────┬───────────┘   │
                         └───────────┼───────────────┘
                                     │
                    ┌────────────────┼────────────────┐
                    │ register       │ heartbeat      │ query
                    ▼                ▼                ▼
        ┌──────────────────┐  ┌──────────┐  ┌──────────────────┐
        │   App (any)      │  │  App N   │  │   Dashboard /     │
        │  ┌────────────┐  │  │  liveness│  │   CLI tool        │
        │  │ cloud.ts   │──┼──│  pulse   │  │                   │
        │  │ contracts  │  │  └──────────┘  │ GET /api/v1/      │
        │  │ engine     │  │                │ topology          │
        │  └────────────┘  │                │ routes            │
        └──────┬───────────┘                │ status            │
               │                            └──────────────────┘
               │ fetch contracts (Systems API)
               ▼
        ┌──────────────────┐
        │   Other App      │
        │  ┌────────────┐  │
        │  │ contracts  │  │
        │  └────────────┘  │
        └──────────────────┘

   App → Cloud:  POST /api/v1/tools         (register)
                 POST /api/v1/tools/:id/beat (heartbeat)
   App → Cloud:  POST /api/v1/addresses     (exposure)

   Any → Cloud:  GET  /api/v1/topology      (map)
                 GET  /api/v1/routes        (routing)
                 GET  /api/v1/status        (health)

   App → App:    fetch contracts via Nexus-Systems-API package
                 (typed, versioned, no ad-hoc JSON)
```

### Graphics App Internal Flow

```
  Browser (React 19)
       │ WebSocket (Yjs for collab) + REST
       ▼
  Bun Gateway (port 36xx)
       │ HTTP
       ├──▶ Python FastAPI backend (uvicorn)
       │       ├──▶ PostgreSQL (SQLAlchemy async)
       │       ├──▶ Redis (Celery task queue)
       │       └──▶ MinIO/S3 (asset storage)
       │
       └──▶ Rust WASM Engine (loaded in browser)
               (compute-heavy ops: filters, transforms, rendering)
```

---

## 7. Git Submodule Status

| Submodule | Path | Remote | Status |
|---|---|---|---|
| **Nexus** | apps/Nexus | github.com:The-No-Hands-company/Nexus.git | Initialized |
| **Nexus-AI** | apps/Nexus-AI | github.com:The-No-Hands-company/Nexus-AI | Initialized |
| **Nexus-Cloud** | apps/Nexus-Cloud | github.com:The-No-Hands-company/Nexus-Cloud.git | Initialized |
| **Nexus-Computer** | apps/Nexus-Computer | github.com:The-No-Hands-company/Nexus-Computer.git | Initialized |
| **Nexus-Deploy** | apps/Nexus-Deploy | github.com:The-No-Hands-company/Nexus-Deploy.git | Initialized |
| **Nexus-Hosting** | apps/Nexus-Hosting | github.com:The-No-Hands-company/Nexus-Hosting.git | Initialized |
| **Nexus-Network** | apps/Nexus-Network | github.com:The-No-Hands-company/Nexus-Network.git | **Not initialized** — must clone separately |
| **Nexus-Vault** | apps/Nexus-Vault | github.com:The-No-Hands-company/Nexus-Vault.git | **Not initialized** — must clone separately |
| **Phantom** | apps/Phantom | github.com:Zajfan/Phantom.git | **Not initialized** — must clone separately |

### To initialize uninitialized submodules:

```bash
git submodule update --init apps/Nexus-Network
git submodule update --init apps/Nexus-Vault
git submodule update --init apps/Phantom
```

Or all at once:

```bash
git submodule update --init --recursive
```

---

## 8. Production Deployment Options

### Option A: Docker Compose (recommended)

```bash
# Start infrastructure + Cloud
docker compose up -d postgres redis minio nexus-cloud

# Start all app services (generated from app definitions)
docker compose up -d

# Check health
docker compose ps
curl http://localhost:8787/api/v1/status
```

### Option B: systemd (single-machine)

Each app runs as a systemd service unit:

```ini
# /etc/systemd/system/nexus-cloud.service
[Unit]
Description=Nexus Cloud Control Plane
After=network.target postgresql.service redis.service

[Service]
Type=simple
User=nexus
WorkingDirectory=/opt/nexus-systems/apps/Nexus-Cloud
Environment=NODE_ENV=production
Environment=PORT=8787
Environment=POSTGRES_URL=postgresql://nexus:nexus@localhost:5432/nexus
Environment=REDIS_URL=redis://localhost:6379
Environment=S3_ENDPOINT=http://localhost:9000
ExecStart=/usr/bin/bun run src/index.ts
Restart=always

[Install]
WantedBy=multi-user.target
```

App services follow the same pattern, each on its assigned port.

### Option C: Single-Machine (direct)

```bash
# Start infrastructure daemons
systemctl start postgresql redis minio

# Launch Cloud
cd apps/Nexus-Cloud && bun run dev &

# Launch apps (each in its own terminal or tmux pane)
cd apps/Nexus-Office && bun run dev &
cd apps/Nexus-Spend && bun run dev &
# ...

# Monitor
curl http://localhost:8787/api/v1/status
```

### Infrastructure Requirements

| Component | Min Version | Notes |
|---|---|---|
| PostgreSQL | 16 | Required by Cloud + Python backends |
| Redis | 7 | Required by Cloud + Celery workers |
| MinIO | latest | S3-compatible object storage |
| Bun | ^1.3 | Runtime for all TypeScript services |
| Python | >=3.12 | Required by graphics backends + AI |
| Rust | stable (edition 2021) | Required for engine crates |

### Environment Variables (global)

```bash
NEXUS_CLOUD_URL=http://localhost:8787
NEXUS_CLOUD_API_KEY=change-me
CORS_ORIGIN=*
POSTGRES_URL=postgresql://nexus:nexus@localhost:5432/nexus
REDIS_URL=redis://localhost:6379
S3_ENDPOINT=http://localhost:9000
S3_ACCESS_KEY=minioadmin
S3_SECRET_KEY=minioadmin
S3_BUCKET=nexus
```

---

## 9. Federation

Nexus Systems is designed for sovereign, multi-node operation. Two or more Nexus-Cloud instances can federate to share tools, topology, and discovery across independent clusters — each with its own identity, domain, and trust policy.

### 9.1 Identity Model

Every Nexus-Cloud node derives a permanent, self-sovereign identity at startup:

| Component | Format | Example | Source |
|---|---|---|---|
| **DID** | `did:nexus:<32-char-hex>` | `did:nexus:a1b2c3d4...` | Derived from `NEXUS_CLOUD_DOMAIN` + `HOSTNAME` + `PORT` via SHA-256, or explicit via `NEXUS_CLOUD_NODE_DID` |
| **Short ID** | 8-char hex | `a1b2c3d4` | First 8 chars of SHA-256(DID) |
| **Address** | `@user:shortId` | `@alice:a1b2c3d4` | Scoped to node — holder of the node's private key controls the namespace |

The DID is permanent for a given node. Short IDs are used for human-readable addressing. No registrar, no cost, no squatting is possible — addresses are cryptographic identities.

### 9.2 Gossip Discovery Protocol

Nodes discover each other through an HTTP gossip protocol (no DHT, no NAT traversal, zero runtime dependencies):

```
                     POST /v1/federation/peers/announce
  Cloud-A ──────────────────────────────────────────────────▶ Cloud-B
           { did, shortId, upstreamUrl }                      
                                                               
           ◀──────────────────────────────────────────────────  
           { peers: [{ did, shortId, upstreamUrl }, ...] }     
```

**Announcement flow:**

1. Cloud-A reads `BOOTSTRAP_PEERS` env var (comma-separated URLs of known peers)
2. Cloud-A POSTs its `selfAnnouncement` to each bootstrap peer at `POST /v1/federation/peers/announce`
3. Cloud-B receives the announcement, registers Cloud-A as a peer, and returns its own known peer list
4. Cloud-A adds any newly discovered peers from Cloud-B's response
5. Both nodes persist their peer lists to `data/platform-state.json`

**Key endpoints:**

| Endpoint | Method | Purpose |
|---|---|---|
| `/v1/federation/peers/announce` | POST | Exchange peer lists (gossip) |
| `/v1/federation/identity` | GET | Return this node's DID + shortId + public key |
| `/v1/federation/peers` | GET | List all known federation peers |
| `/.well-known/nexus-cloud` | GET | Discovery document for external tooling |

### 9.3 Bootstrap Process

The `BOOTSTRAP_PEERS` environment variable seeds the initial peer set:

```bash
BOOTSTRAP_PEERS=http://cloud-b.nexus.local:8788,http://cloud-c.nexus.local:8789
```

On startup, `bootstrapPeers()` (called after the HTTP server is ready) contacts each peer and exchanges announcements. Bootstrap peers are auto-trusted — they bypass the normal trust check because the operator explicitly listed them. Newly discovered peers (via the returned peer list) are registered as `pending` and require explicit trust promotion.

**Persistent state:** Peers are saved to `data/platform-state.json`. On restart, previously discovered peers are loaded first, then bootstrap peers are re-contacted to freshen the list.

### 9.4 Tool Federation

When a Nexus-Cloud node receives a tool registration from a federated peer, it propagates the tool to its own local registry:

```
  App → Cloud-A: POST /api/v1/tools  (register tool)
       Cloud-A: stores tool locally
       Cloud-A → Cloud-B: forwards tool registration to trusted peers
       Cloud-B: stores federated tool in local registry
```

Tools registered from a federated peer carry the peer's origin metadata (`federatedBy`, `originPeer`). They appear in `GET /api/v1/tools` listings alongside locally registered tools. Heartbeats from the origin app flow through the proxy cloud to the origin.

**Tool resolution:**

```
  Client → Cloud-B: GET /api/v1/tools
       Cloud-B: returns local tools + federated tools from trusted peers
       
  Client → Cloud-B: GET /api/v1/tools/:toolId
       Cloud-B: checks local registry → if federated, proxies to origin peer
```

### 9.5 `.well-known/nexus-cloud` Discovery Document

Every Nexus-Cloud instance serves a discovery document at `GET /.well-known/nexus-cloud`:

```json
{
  "version": "v1",
  "nodeId": "did:nexus:a1b2c3d4...",
  "shortId": "a1b2c3d4",
  "namingScheme": "@user:shortId",
  "domain": "cloud-a.nexus.local",
  "apiBase": "http://cloud-a.nexus.local:8787",
  "capabilities": [
    "address-issuance",
    "domain-binding",
    "exposure-registry",
    "routing-manifest",
    "tool-registry",
    "node-identity"
  ],
  "endpoints": {
    "register": "/api/v1/tools",
    "heartbeat": "/api/v1/tools/:toolId/heartbeat",
    "addresses": "/api/v1/addresses",
    "exposures": "/api/v1/exposures",
    "domains": "/api/v1/domains",
    "routes": "/api/v1/routes",
    "status": "/api/v1/status",
    "topology": "/api/v1/topology",
    "identity": "/v1/federation/identity"
  }
}
```

External tooling discovers Nexus-Cloud instances by fetching this document. It provides the API base URL, node identity, and a capability map so consumers know what this node supports.

### 9.6 Security Model

Federation security is layered:

| Layer | Mechanism | Description |
|---|---|---|
| **Transport** | TLS (production) | All gossip traffic encrypted in transit |
| **Peer identity** | DID + URL | Peers are identified by their self-sovereign DID and reachable URL |
| **Trust states** | `pending` → `verified` → `trusted` → `quarantined` → `revoked` → `expired` | Lifetime-managed trust that decays and requires renewal |
| **Bootstrap trust** | Operator-configured | Peers listed in `BOOTSTRAP_PEERS` are auto-trusted as `verified` |
| **Signed requests** | Ed25519 + HTTP Signature | Inter-node requests carry cryptographic signatures tied to the node's keypair |
| **Trust expiry** | Configurable TTL (default 168h) | Peer trust expires after `NEXUS_CLOUD_PEER_TRUST_TTL_HOURS` unless renewed |
| **Minimum trust** | Configurable | `NEXUS_CLOUD_FEDERATION_MIN_TRUST` sets the bar: `verified` (permissive) or `trusted` (strict) |
| **Authorization** | `authorizeFederatedPeerAction()` | Every federated action checks the requesting peer's trust state before proceeding |

**Trust lifecycle:**

```
  [BOOTSTRAP] ──▶ verified ──▶ trusted
                     │              │
                     ▼              ▼
                 expired        expired
                     │              │
                 quarantined ──▶ revoked
```

Trust can be promoted, quarantined, or revoked via the API:
```bash
POST /v1/federation/peers/cloud-b.nexus.local/trust
```

### 9.7 Deployment Example: Dual-Cloud Federation

A ready-to-run dual-cloud federation setup is available at `deploy/dual-cloud/`:

```bash
cd deploy/dual-cloud
docker compose -f dual-cloud-compose.yml up -d
bash federation-test.sh
```

This starts two Nexus-Cloud instances (cloud-A:8787, cloud-B:8788) configured with mutual `BOOTSTRAP_PEERS`, demonstrating peer discovery, gossip, and cross-cloud tool registration.
