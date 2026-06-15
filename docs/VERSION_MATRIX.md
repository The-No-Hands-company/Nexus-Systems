# Nexus Systems — Version Compatibility Matrix

> Generated: June 14, 2026
> Ecosystem monotree at `apps/` — 91 entries, 70+ Phantom-bound, 3 SDK packages
> Session commits: `f6ae2140 → 6f53a048`

---

## 1. Service Versions

### Core Control Plane

| Service | Port | Runtime | Language | Version |
|---|---|---|---|---|
| **Nexus-Cloud** | 8787 | Bun ^1.3 | TypeScript (strict) | 0.1.0 |

## 0. Packages & SDKs (shared across all apps)

| Package | Language | Version | Tests | Bound To |
|---|---|---|---|---|
| **Phantom SDK** | Rust (WASM) + TypeScript | 0.1.0 | 10 (3 Rust + 7 TS) | 70+ apps |
| **Nexus Discovery** | TypeScript | 0.1.0 | 6 | 70+ apps |
| **Ghost Framework** | TypeScript | 0.1.0 | — | CLI scaffold tool |
| **Nexus SDK** | TypeScript | — | — | Nexus-Cloud only |

### Phantom SDK — API Contract

```typescript
import { createPhantomSDK, PhantomApp } from "@nexus/phantom-sdk";

// Direct SDK
const sdk = await createPhantomSDK();
const id = await sdk.generateIdentity("app-name");
// id → { handle: number, did: "did:phantom:...", publicKey, signingPublicKey }
const sig = await sdk.sign(id.handle, payload);
const valid = await sdk.verify(id.handle, payload, sig);

// Integration module (preferred for Bun apps)
const phantom = new PhantomApp("nexus-myapp");
const id = await phantom.start();
phantom.status() // → { bound: true, did: "...", protocol: "phantom-v1", algorithms: "Kyber-1024, Dilithium-5, Blake3" }
phantom.stop()    // release resources
```

**Crypto primitives:** Kyber-1024 (KEM), Dilithium-5 (signatures), Blake3 (hashing)
**Key storage:** Secret keys in WASM memory, opaque handles exposed to JS

### Nexus Discovery — API Contract

```typescript
import { NexusDiscovery } from "@nexus/discovery";

const discovery = new NexusDiscovery({
  cloudUrl: "http://localhost:8787",
  apiKey: "...",
  ttlMs: 30_000,
});

const svc = await discovery.resolve("nexus-photos");
// → { id: "nexus-photos", url: "http://localhost:3096", health: "healthy" }

const list = await discovery.list();
// → [{ id, url, health, capabilities, lastSeen }, ...]

const data = await discovery.call("nexus-photos", "/api/v1/photos/albums");
// → Response (direct fetch)
```

**Cache:** 30s TTL, graceful degradation (uses cached data when Cloud unreachable)
**Fallback:** `/api/v1/topology` → `/api/v1/tools`

## 1. Inter-Service API Contracts (Systems API v1)

### Built TypeScript Services (Bun + strict TS)

| Service | Port | Runtime | Source Lines | Version |
|---|---|---|---|---|
| **Nexus-Hosting** | 3000 | pnpm | 34,259 | — |
| **Nexus-Vault** | 3100 | Bun | 8,074 | — |
| **Nexus-Forge** | 3101 | Bun | 6,477 | — |
| **Nexus-Engine** | 3102 | Bun | 4,584 | — |
| **Nexus-Deploy** | 3103 | Bun | 1,273 | — |
| **Nexus-Monitor** | 3104 | Bun | 860 | — |
| **Nexus-Network** | 3200 | Bun | 924 | — |
| **Nexus-Tunnel** | 3201 | Bun | 821 | — |
| **Nexus-Edge** | 3202 | Bun | 772 | — |
| **Nexus-Files** | 3203 | Bun | 576 | — |
| **Nexus-Guardian** | 3204 | Bun | 563 | — |
| **Nexus-Auth** | 3205 | Bun | 405 | — |
| **Nexus-Systems-API** | 3300 | Bun | 280 | 0.1.0 |
| **Nexus-Compliance** | 3301 | Bun | 209 | — |
| **Nexus-Search** | 3302 | Bun | 199 | — |
| **Nexus-AI-Hub** | 3303 | Bun | 193 | — |
| **Nexus-Testing** | 3304 | Bun | 193 | — |
| **Nexus-API** | 3305 | Bun | 188 | — |
| **Nexus-IDE** | 3306 | Bun | 187 | — |
| **Nexus-AI** | 3400 | Bun | 49 | — |
| **Nexus-Computer** | 3500 | Bun | 49 | — |

### Graphics Apps — Triple-Stack (Bun + React + Python + Rust)

| Service | Port | Bun Server | Frontend (npm) | Python Backend | Rust Engine |
|---|---|---|---|---|---|
| **Nexus-Graphic** | 3600 | 0.1.0 | nexus-graphic-frontend 0.1.0 | nexus-graphic-backend 0.1.0 | nexus-graphic-engine 0.1.0 |
| **Nexus-Design** | 3601 | 0.1.0 | nexus-design-frontend 0.1.0 | nexus-design-backend 0.1.0 | nexus-design-engine 0.1.0 |
| **Nexus-Draw** | 3602 | 0.1.0 | nexus-draw-frontend 0.1.0 | nexus-draw-backend 0.1.0 | nexus-draw-engine 0.1.0 |
| **Nexus-Photos** | 3603 | 0.1.0 | nexus-photos-frontend 0.1.0 | nexus-photos-backend 0.1.0 | nexus-photos-engine 0.1.0 |
| **Nexus-Video** | 3604 | 0.1.0 | nexus-video-frontend 0.1.0 | nexus-video-backend 0.1.0 | nexus-video-engine 0.1.0 |
| **Nexus-GPU-Test** | 3605 | — | nexus-gpu-test-frontend 0.1.0 | nexus-gpu-test-backend 0.1.0 | nexus-gpu-test-engine 0.1.0 |

### Non-TypeScript Built Services

| Service | Runtime | Language | Build System | Version |
|---|---|---|---|---|
| **Nexus** | Rust (8 crates) | Rust (edition 2021) | Cargo.toml | — |
| **Nexus-Modeling** | C++ | C++ (280 files) | CMakeLists.txt | — |
| **Nexuslang** | Python/C++ | Python/C++ | requirements.txt + pytest | — |
| **Phantom** | Rust (8 crates) | Rust (edition 2021) | Cargo.toml | — |
| **Nexusclaw** (anyclaw) | Hybrid TS/C++ | TypeScript/C++ | package.json + .cpp | — |
| **Nexus-Wiki** | Python | Python (661 loc) | pyproject.toml | — |

### Shell Apps (57 — scaffolded, not yet built)

Academy, Accounting, Agents, Analytics, Arcade, Automate, Billing, Book, Calendar, Code-Review, Confidential, Content, Contracts, CRM, Database, Email, Finance, Fitness, Forms, Game, Health, Home, HR, Inference, Insights, Learn, Market, Media, Meet, Mind, Music, Notes, Nutrition, Office, PDF, Planner, Play, Presence, Provenance, Radio-Live, Sales, Security, SEO, Social, Spend, Store, Survey, Tasks, Team-Chat, Terminal, Tutor, Vertical

---

## 2. Rust Engines

| Engine Crate | Version | Edition | Crate Type | Key Dependencies |
|---|---|---|---|---|
| **nexus-graphic-engine** | 0.1.0 | 2021 | cdylib, rlib | wasm-bindgen 0.2, serde 1, serde-wasm-bindgen 0.6 |
| **nexus-design-engine** | 0.1.0 | 2021 | cdylib, rlib | wasm-bindgen 0.2, serde 1, serde-wasm-bindgen 0.6 |
| **nexus-draw-engine** | 0.1.0 | 2021 | cdylib, rlib | wasm-bindgen 0.2, serde 1, serde-wasm-bindgen 0.6 |
| **nexus-photos-engine** | 0.1.0 | 2021 | cdylib, rlib | wasm-bindgen 0.2, serde 1, serde-wasm-bindgen 0.6 |
| **nexus-video-engine** | 0.1.0 | 2021 | cdylib, rlib | wasm-bindgen 0.2, serde 1, serde-wasm-bindgen 0.6 |
| **nexus-gpu-test-engine** | 0.1.0 | 2021 | lib, cdylib | serde 1, image 0.25 |

> Six graphics engines share identical scaffolding for Graphic/Design/Draw/Photos/Video.
> GPU-Test engine uses native `lib` + `cdylib` without wasm-bindgen (no wasm target).

---

## 3. React Frontends

| Frontend (npm package) | React | Bundler | Build Size | Key Dependencies |
|---|---|---|---|---|
| **nexus-graphic-frontend** | ^19.0 | Vite 6 | 208 KB | zustand 5, yjs 13.6, y-websocket 2, lucide-react 0.460 |
| **nexus-design-frontend** | ^19.0 | Vite 6 | 211 KB | zustand 5, yjs 13.6, y-websocket 2, lucide-react 0.460 |
| **nexus-draw-frontend** | ^19.0 | Vite 6 | 212 KB | zustand 5, yjs 13.6, y-websocket 2, lucide-react 0.460 |
| **nexus-photos-frontend** | ^19.0 | Vite 6 | 212 KB | zustand 5, lucide-react 0.460 |
| **nexus-video-frontend** | ^19.0 | Vite 6 | 217 KB | zustand 5, lucide-react 0.460 |
| **nexus-gpu-test-frontend** | ^19.0 | Vite 6 | 206 KB | zustand 5, lucide-react 0.460 |

> All frontends: TypeScript ^5.8, Tailwind CSS 4, PostCSS 8.4, autoprefixer 10.4.
> Graphic/Design/Draw include collaborative editing via Yjs + y-websocket.

---

## 4. Python Backends

| Backend (pip package) | Python min | Framework | Key Dependencies |
|---|---|---|---|
| **nexus-graphic-backend** | >=3.12 | FastAPI >=0.115 | SQLAlchemy 2, asyncpg, Celery 5.4, Redis 5, boto3 1.35, Pillow 11 |
| **nexus-design-backend** | >=3.12 | FastAPI >=0.115 | SQLAlchemy 2, asyncpg, Celery 5.4, Redis 5, boto3 1.35, Pillow 11 |
| **nexus-draw-backend** | >=3.12 | FastAPI >=0.115 | SQLAlchemy 2, asyncpg, Celery 5.4, Redis 5, boto3 1.35, Pillow 11 |
| **nexus-photos-backend** | >=3.12 | FastAPI >=0.115 | SQLAlchemy 2, asyncpg, Celery 5.4, Redis 5, boto3 1.35, Pillow 11 |
| **nexus-video-backend** | >=3.12 | FastAPI >=0.115 | SQLAlchemy 2, asyncpg, Celery 5.4, Redis 5, boto3 1.35, Pillow 11 |
| **nexus-gpu-test-backend** | >=3.12 | FastAPI >=0.115 | SQLAlchemy 2, asyncpg, Celery 5.4, Redis 5, boto3 1.35, Pillow 11 |

> All six backends share the same FastAPI + Celery + PostgreSQL + Redis + S3 pattern.
> GPU-Test backend additionally includes `pydantic-settings>=2.0`.

---

## 5. Infrastructure Dependencies

| Component | Image / Version | Port | Purpose |
|---|---|---|---|
| **PostgreSQL** | postgres:16-alpine | 5432 | Primary relational store (nexus db) |
| **Redis** | redis:7-alpine | 6379 | Caching, Celery broker, pub/sub |
| **MinIO** (S3-compatible) | minio/minio:latest | 9000 (API), 9001 (console) | Object storage, file assets |
| **Nexus-Cloud** | Built from `apps/Nexus-Cloud/Dockerfile` | 8787 | Control plane, service registry, topology |

### Infrastructure Connection Strings

```
POSTGRES_URL=postgresql://nexus:nexus@postgres:5432/nexus
REDIS_URL=redis://redis:6379
S3_ENDPOINT=http://minio:9000
S3_ACCESS_KEY=minioadmin
S3_SECRET_KEY=minioadmin
S3_BUCKET=nexus
```

---

## 6. Cross-Service Contract Versions

| Contract | Version | Transport | Description |
|---|---|---|---|
| **Tool Registration** | v1 | `POST /api/v1/tools` | App self-registration with Cloud |
| **Heartbeat** | v1 | `POST /api/v1/tools/:toolId/heartbeat` | Liveness pulse from every app |
| **Topology Query** | v1 | `GET /api/v1/topology` | Full ecosystem graph |
| **Route Table** | v1 | `GET /api/v1/routes` | Active domain-to-upstream map |
| **Address Lifecycle** | v1 | `POST /api/v1/addresses` | App domain/exposure requests |
| **Service Auth** | v1 | Bearer token (header) | Internal service-to-service auth |
| **AI Invocation Envelope** | v1 (draft) | JSON over HTTP | Standardized AI mesh call shape |

> Phase roadmap: v1 (now) → signed service identities (v2) replacing bearer tokens.
> See `docs/ECOSYSTEM_CONNECTION_PLAN.md` for full phase rollout.

---

## 7. CI Matrix Summary

| Axis | Variants |
|---|---|
| **Runtimes** | Bun (latest), Python 3.12, Rust stable |
| **Databases** | PostgreSQL 16, SQLite (app-local) |
| **OS Targets** | Linux (x86_64), macOS (arm64) |
| **Check Commands** | `bun run check && bun test` (TS), `cargo check && cargo test` (Rust), `pytest` (Python) |
| **Lint Tools** | Biome 1.9 (TS/JS), Clippy (Rust), Ruff (Python) |
| **E2E** | `e2e-test.sh` (ecosystem-level integration) |
| **Integration Tests** | `test-integration.sh`, `test-apps.sh` |

### Per-App CI Requirements (from ENGINEERING_STANDARDS.md)

- TypeScript: `bun run check` (tsc --noEmit) + `bun test`
- Rust: `cargo check` + `cargo test` + `cargo clippy`
- Python: `pytest` (backend tests)
- All: Biome/Ruff formatting checks before merge

---

## 8. Shell App Build Queue

57 apps are scaffolded with `AGENTS.md` + `README.md` + `src/.gitkeep` + `tests/.gitkeep` and await implementation. Each will receive:

- Bun runtime + strict TypeScript (standard stack)
- Port assignment in the 3000–3999 range
- Contract definitions in `src/contracts.ts`
- Cloud registration + heartbeat client in `src/cloud.ts`
- Test harness in `tests/server.test.ts`

Priority order defined in `apps/docs/ECOSYSTEM_PHASE_ROLLOUT_BOARD.md`.
