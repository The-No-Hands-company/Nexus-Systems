[![CI](https://github.com/The-No-Hands-company/Nexus-Systems/actions/workflows/ci.yml/badge.svg)](https://github.com/The-No-Hands-company/Nexus-Systems/actions)

# Nexus Systems — Quick Start

Sovereign, self-hosted ecosystem of 80+ apps. Free. Open source. Privacy-first.

## Fresh Clone → Running Ecosystem (5 Commands)

```bash
# 1. Clone
git clone https://github.com/The-No-Hands-company/Nexus-Systems.git
cd Nexus-Systems

# 2. Run the preflight check
./deploy/preflight-check.sh

# 3. Start everything (PostgreSQL, Redis, MinIO, Cloud, 5 apps)
docker compose up -d

# 4. Verify it works
./scripts/demo/capstone.sh

# 5. See what's running
./scripts/run-all.sh --status
```

## What You Get

| Service | Port | What it is |
|---------|------|-----------|
| PostgreSQL 16 | 5432 | Metadata store |
| Redis 7 | 6379 | Cache + job queue |
| MinIO | 9000 | S3-compatible object storage |
| Nexus-Cloud | 8787 | Control plane — all apps register here |
| Nexus-Graphic | 3080 | Graphic design suite (Rust WASM + React) |
| Nexus-Design | 3074 | UI/UX design tool |
| Nexus-Draw | 3075 | Collaborative whiteboard |
| Nexus-Photos | 3096 | Photo/album management |
| Nexus-Video | 3113 | Video streaming platform |

### Every App Has

- **Phantom DID** — post-quantum identity (`did:phantom:...`)
- **Cloud heartbeat** — health monitoring
- **API contracts** — validated against Cloud schema
- **Service discovery** — `discovery.resolve("nexus-photos")` works

## Prove It Works

```bash
# Contract validation — 71/71 apps pass
./scripts/test/contract-test.sh

# One-command ecosystem proof
./scripts/demo/demo.sh

# Full capstone: Cloud + Phantom + Discovery + Pipeline
./scripts/demo/capstone.sh

# Smoke test — health checks on all 72 apps
./scripts/test/smoke-test.sh
```

## Architecture

```
                Nexus-Cloud (control plane, :8787)
                       │
          ┌────────────┼────────────┐
          │            │            │
    Discovery Mesh  Phantom SDK   Systems API
    (resolve/call)  (PQ identity)  (contracts)
          │            │            │
    ┌─────┴────────────┴────────────┴─────┐
    │        80+ Apps (ports 3000-4330)    │
    │  Graphic, Photos, Design, Tasks...   │
    └──────────────────────────────────────┘
```

## Federation (Two Clouds)

```bash
cd deploy/dual-cloud
docker compose -f dual-cloud-compose.yml up -d
./federation-test.sh
```

Two Nexus-Cloud instances gossip. Register a tool on cloud-A, it appears on cloud-B.

## Phantom Protocol

Every app generates a PQ identity on startup:
```
did:phantom:6e657875732d6772
Algorithms: Kyber-1024 (KEM), Dilithium-5 (Signatures), Blake3 (Hash)
```

The Phantom SDK (`packages/phantom-sdk/`) provides:
- PQ key generation + encapsulation
- Digital signatures + verification
- Opaque handles (secret keys never leave WASM memory)
- Integrated in 70+ apps via `PhantomApp`

## Ghost Framework — Scaffold New Apps

```bash
bun run ghost/src/index.ts scaffold MyApp --port 4000 --description "My custom app"
```

Generates a complete Nexus app with Phantom binding, Cloud heartbeat, API contracts, and tests.

## Test Suite

| Test | Result | Command |
|------|--------|---------|
| Contract validation | 71/71 pass | `./contract-test.sh` |
| Smoke test | 64/72 pass | `./smoke-test.sh` |
| Capstone demo | 5/5 DIDs | `./capstone.sh` |
| Phantom SDK | 10/10 tests | `cd packages/phantom-sdk/wasm && cargo test` |
| Discovery | 6/6 tests | `cd packages/nexus-discovery && bun test` |

## CI Pipeline

GitHub Actions runs on every push:
- TypeScript typecheck (70+ Bun apps)
- Rust engine tests (6 crates)
- React frontend builds (6 apps)
- Integration smoke test

See `.github/workflows/ci.yml`

## License

AGPL-3.0 — Free. Open source. Forever.

.test