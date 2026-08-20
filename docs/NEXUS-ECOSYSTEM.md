# The Nexus Systems Ecosystem

**One document. Everything else was a chapter torn out of it.**

*Consolidated 2026-08-19. Supersedes five prior documents — see [Provenance](#provenance).*

---

## What this document is

This is the single canonical register of the Nexus Systems ecosystem: what
exists, what state it is actually in, and what is deliberately planned but not
yet built. It replaces a scatter of overlapping blueprints, audits and
taxonomies that disagreed with each other and, more importantly, disagreed with
the filesystem.

Two rules govern it:

1. **Status is measured, not claimed.** Every status in the register below was
   derived by scanning the repository on 2026-08-19, not by copying a previous
   document's assertion. The method is stated so it can be re-run.
2. **One bible, not a hundred chapters.** When a fact belongs here, it lives
   here. New ecosystem documents should extend this file rather than fork it.

---

## The status ladder

The scan counted source files (`.rs`, `.ts`, `.tsx`, `.py`, `.go`, `.jsx`)
per app, excluding `node_modules`, `target` and `dist`.

The distribution is unusually clean, and that is the whole story: **75 of 112
apps contain exactly six source files.** Those six are always the same six —
`cloud.ts`, `contracts.ts`, `index.ts`, `<name>-engine.ts`, `server.ts` and
`tests/server.test.ts`. They are output of the `ghost` scaffolder. The engine
file is generic CRUD over SQLite with the app's name substituted in, which is
visible in artefacts like `listRecipess()` and a `recipeses` table. There is no
domain logic in any of them.

That gives four honest states:

| Status | Meaning | Count |
|---|---|---|
| **Live** | Deployed and reachable on `tnhc.dev` today | 6 |
| **Beta** | Deployed, feature-incomplete, in active use | 1 |
| **In development** | Substantive code beyond the scaffold; not yet deployed | 24 |
| **Scaffold** | The six-file `ghost` shape: registers with Systems-API, serves health, no domain logic | 75 |
| **Stub** | Less than the scaffold — a directory with intent and little else | 7 |

**113 apps total**: 112 `Nexus-*` directories plus `apps/Nexus`, the Rust chat
runtime, which does not carry the prefix.

The honest summary: **31 apps have real code in them; 82 are placeholders.**
This is not a failure — the scaffolds are a deliberate land-claim so that
naming, routing and registration are settled before the work starts. But a
scaffold is not a product, and this document will not call one a product.

---

## What is actually live

| Host | App | Upstream | Notes |
|---|---|---|---|
| `app.tnhc.dev` | Nexus-Dashboard | — | The ecosystem shell: launcher, account, admin, Cloud console, webmail |
| `auth.tnhc.dev` | Nexus-Auth | `127.0.0.1:4310` | OIDC/SSO for every app |
| `chat.tnhc.dev` | Nexus | `127.0.0.1:3109` | Rust chat runtime, served via Caddy on `:8095` |
| `cloud.tnhc.dev` | Nexus-Cloud | `127.0.0.1:8787` | Control plane and app registry |
| `draw.tnhc.dev` | Nexus-Draw | `127.0.0.1:8090` | Whiteboard, served as a Nexus-Hosting static site |
| `hosting.tnhc.dev` | Nexus-Hosting | — | Static-site hosting with presigned upload and deploy |
| `storage.tnhc.dev` | MinIO | `:9010` | Object storage — infrastructure, not a Nexus app |

Nexus-Email is in beta: the SMTP daemon listens on `2525` (MX), `2587`
(submission) and `2143` (IMAP), and webmail is a shell-native view at
`app.tnhc.dev/mail`. Outbound delivery to the wider internet is blocked by an
upstream port-25 filter — an infrastructure constraint, not a software gap.

All public traffic reaches these through Cloudflare Tunnel into the gate proxy
on `:8080`. **Every public URL is provisioned through Nexus-Tunnel.** This is
the integration rule that keeps public exposure sovereign, and it has no
exceptions.

---

## Stack decision matrix

The stack is chosen by app category, not per app.

| App category | Backend | Frontend | Database | AI/ML | Real-time |
|---|---|---|---|---|---|
| Communication | Rust (axum) | React/Tauri | PostgreSQL + ScyllaDB | — | WebSocket + WebRTC |
| AI/ML | Python (FastAPI) | React | PostgreSQL | Hugging Face, Anthropic | WebSocket |
| Creative/Graphics | Rust (WASM engine) + Python (FastAPI) | React + WebGL2 | PostgreSQL + S3 | Celery + ComfyUI | WebSocket (CRDT) |
| Developer tools | Rust (axum) or Bun (Elysia) | React | PostgreSQL + SQLite | — | WebSocket |
| Productivity (web) | Bun (`Bun.serve`) | React + Tailwind | SQLite (local) + PostgreSQL (cloud) | Optional | — |
| Platform/Infra | Bun (`Bun.serve`) | — | SQLite + PostgreSQL | — | WebSocket |
| Security/Privacy | Rust | — | SQLite (encrypted) | — | P2P (libp2p) |

---

## The register

Every app in the ecosystem, by category, with measured status.

### Core

| App | Role | Status |
|---|---|---|
| **Nexus** | Real-time chat runtime (Rust); the app serving chat.tnhc.dev | **Live** |
| **Nexus-Dashboard** | The ecosystem shell — launcher, account, admin, and shell-native views | **Live** |
| **Nexus-Auth** | Identity provider; OIDC/SSO for every app in the ecosystem | **Live** |
| **Nexus-Cloud** | Control plane — app registry, federation, operator console | **Live** |
| **Nexus-Systems-API** | Service registration and discovery contract every app implements | Stub |
| **Nexus-API** | Public API surface for the ecosystem | Stub |
| **Nexus-Portal** | Public entry point and marketing surface | Scaffold |
| **Nexus-Account** | End-user account and profile management | Scaffold |

### Infrastructure

| App | Role | Status |
|---|---|---|
| **Nexus-Hosting** | Host a website here, or run the node yourself — domains, TLS, builds, forms | **Live** |
| **Nexus-Deploy** | Deployment orchestration and release pipelines | In development |
| **Nexus-Tunnel** | Sovereign public exposure; every public URL is provisioned here | In development |
| **Nexus-Router** | Request routing and upstream selection | In development |
| **Nexus-Edge** | Edge termination and regional request handling | In development |
| **Nexus-Network** | Network topology and connectivity management | In development |
| **Nexus-Monitor** | Health checks, uptime, and alerting | In development |
| **Nexus-Remote** | Remote access to machines and sessions | Scaffold |

### Compute

| App | Role | Status |
|---|---|---|
| **Nexus-Engine** | General execution engine for ecosystem workloads | In development |
| **Nexus-Computer** | Virtual machine and workstation provisioning | In development |
| **Nexus-Inference** | Model serving and inference endpoints | Scaffold |
| **Nexus-GPU-Test** | GPU capability probing and benchmarking | In development |
| **Nexus-Forge** | Build and artifact forge | In development |

### Data

| App | Role | Status |
|---|---|---|
| **Nexus-Data** | Data pipelines and transformation | Scaffold |
| **Nexus-Database** | NexusDB — consistency-tiered database engine (C++ core, Rust federation) | In development |
| **Nexus-Files** | File storage and sharing | Scaffold |
| **Nexus-Vault** | Secrets and credential storage | In development |
| **Nexus-Warehouse** | Analytical warehouse for ecosystem data | Stub |
| **Nexus-Search** | Full-text and semantic search across the ecosystem | Scaffold |
| **Nexus-Knowledge** | Knowledge base and structured reference | Scaffold |

### Security

| App | Role | Status |
|---|---|---|
| **Nexus-Security** | Security posture, policy, and hardening | Stub |
| **Nexus-Guardian** | Runtime threat monitoring and abuse response | In development |
| **Nexus-Confidential** | Confidential workload handling | Scaffold |
| **Nexus-Compliance** | Compliance frameworks and evidence collection | Stub |
| **Nexus-Provenance** | Artifact provenance and supply-chain attestation | Scaffold |
| **Nexus-Contracts** | Contract authoring, signing, and lifecycle | Scaffold |

### AI

| App | Role | Status |
|---|---|---|
| **Nexus-AI** | Core AI services and model access | In development |
| **Nexus-AI-Hub** | Model catalogue and routing across providers and local models | In development |
| **Nexus-Mind** | Reasoning and memory services | Scaffold |
| **Nexus-Agents** | Autonomous agent runtime and tool use | In development |
| **Nexus-Automate** | Workflow automation and triggers | Scaffold |
| **Nexus-Tutor** | AI-assisted tutoring | Scaffold |

### Developer

| App | Role | Status |
|---|---|---|
| **Nexus-Code** | Code hosting and repository management | In development |
| **Nexus-Code-Review** | Review workflow and change gating | Scaffold |
| **Nexus-IDE** | Integrated development environment | In development |
| **Nexus-Editor** | Lightweight text and code editing | Scaffold |
| **Nexus-Terminal** | Browser-accessible shell | Scaffold |
| **Nexus-Testing** | Test orchestration and reporting | Stub |
| **Nexus-Converter** | Format and file conversion | Scaffold |

### Communication

| App | Role | Status |
|---|---|---|
| **Nexus-Email** | Sovereign mail — SMTP, IMAP, DKIM/SPF/DMARC, webmail; no third party in the path | **Beta** |
| **Nexus-Team-Chat** | Team messaging and channels | Scaffold |
| **Nexus-Meet** | Video meetings | Scaffold |
| **Nexus-Broadcast** | One-to-many streaming and announcements | Scaffold |
| **Nexus-Presence** | Presence and availability signalling | Scaffold |
| **Nexus-Social** | Social feed and following | Scaffold |
| **Nexus-Community** | Forums and community spaces | Scaffold |
| **Nexus-Support** | Support ticketing and helpdesk | Scaffold |

### Productivity

| App | Role | Status |
|---|---|---|
| **Nexus-Docs** | Collaborative documents | Scaffold |
| **Nexus-Office** | Office suite shell | Scaffold |
| **Nexus-Notes** | Personal notes | Scaffold |
| **Nexus-Wiki** | Structured internal wiki | In development |
| **Nexus-Tasks** | Task tracking | Scaffold |
| **Nexus-Planner** | Project planning | Scaffold |
| **Nexus-Calendar** | Calendaring | Scaffold |
| **Nexus-Agenda** | Meeting agendas and minutes | Scaffold |
| **Nexus-Schedule** | Scheduling and booking windows | Scaffold |
| **Nexus-Journal** | Journalling and daily logs | Scaffold |
| **Nexus-Forms** | Form building and response collection | Scaffold |
| **Nexus-Survey** | Surveys and questionnaires | Scaffold |
| **Nexus-Book** | Long-form writing and book authoring | Scaffold |
| **Nexus-PDF** | PDF generation and manipulation | Stub |

### Creative

| App | Role | Status |
|---|---|---|
| **Nexus-Draw** | Whiteboard and diagramming (Canvas 2D); live at draw.tnhc.dev | **Live** |
| **Nexus-Design** | Interface and graphic design tooling | In development |
| **Nexus-Graphic** | Raster and vector graphics | In development |
| **Nexus-Modeling** | 3D modelling kernel targeting full-parity DCC | In development |
| **Nexus-Photos** | Photo library and editing | In development |
| **Nexus-Video** | Video editing and processing | In development |
| **Nexus-Media** | Media library and playback | Scaffold |
| **Nexus-Music** | Music library and playback | Scaffold |
| **Nexus-Radio-Live** | Live radio streaming | Scaffold |
| **Nexus-Content** | Content management | Scaffold |
| **Nexus-Publishing** | Publishing and distribution | Scaffold |
| **Nexus-Game** | Game development tooling | Scaffold |
| **Nexus-Arcade** | Game hosting and play | Scaffold |
| **Nexus-Play** | Casual play surface | Scaffold |

### Business

| App | Role | Status |
|---|---|---|
| **Nexus-CRM** | Customer relationship management | Scaffold |
| **Nexus-Sales** | Sales pipeline | Scaffold |
| **Nexus-Commerce** | Commerce engine | Scaffold |
| **Nexus-Store** | Storefront | Scaffold |
| **Nexus-Market** | Marketplace for ecosystem offerings | Scaffold |
| **Nexus-Billing** | Billing and subscriptions | Scaffold |
| **Nexus-Invoice** | Invoicing | Scaffold |
| **Nexus-Accounting** | Bookkeeping and ledgers | Scaffold |
| **Nexus-Finance** | Financial planning and reporting | Scaffold |
| **Nexus-Spend** | Expense tracking and approvals | Scaffold |
| **Nexus-Inventory** | Stock and inventory | Scaffold |
| **Nexus-Logistics** | Shipping and fulfilment | Scaffold |
| **Nexus-HR** | People operations | Scaffold |
| **Nexus-Jobs** | Hiring and job listings | Scaffold |
| **Nexus-Reservations** | Bookings and reservations | Scaffold |
| **Nexus-Vertical** | Industry-specific vertical packs | Scaffold |

### Analytics

| App | Role | Status |
|---|---|---|
| **Nexus-Analytics** | Product and usage analytics | Scaffold |
| **Nexus-Insights** | Derived insight and summarisation | Scaffold |
| **Nexus-Reporter** | Report generation and scheduling | Scaffold |
| **Nexus-SEO** | Search visibility tooling | Scaffold |

### Life

| App | Role | Status |
|---|---|---|
| **Nexus-Health** | Health records and tracking | Scaffold |
| **Nexus-Fitness** | Training and activity | Scaffold |
| **Nexus-Nutrition** | Nutrition tracking | Scaffold |
| **Nexus-Recipes** | Recipe management | Scaffold |
| **Nexus-Home** | Home and device control | Scaffold |
| **Nexus-Maps** | Mapping and geospatial | Scaffold |
| **Nexus-News** | News aggregation | Scaffold |
| **Nexus-Learn** | Courses and learning paths | Scaffold |
| **Nexus-Academy** | Structurededucation programmes | Scaffold |
| **Nexus-Browsing** | Browser and bookmarking | Scaffold |

---

## The expansion manifest

These seventeen modules do **not** exist on disk. They come from the v4 cloud
taxonomy's gap analysis, which mapped the ecosystem against the global cloud
services landscape and found capability domains with no Nexus answer.

Each is listed with the layer it serves, its category, and a proposed port.
**Ports here are proposals, not assignments** — they occupy the free `8800`
block, clear of every port currently in use (`2143`, `2525`, `2587`, `3109`,
`4310`, `8080`, `8090`, `8095`, `8787`, `9010`).

| Module | Role | Layer | Category | Port |
|---|---|---|---|---|
| **Nexus-Enclave** | Hardware-enclave compute with CPU-level attestation (TEE) | Deep | Security | 8800 |
| **Nexus-Sandbox** | Isolated microVM execution for agent and untrusted code | Surface | Compute | 8801 |
| **Nexus-Vector** | Vector indexing and embedding search for RAG and semantic lookup | Surface | Data | 8802 |
| **Nexus-Queue** | Pub/sub event bus and durable message queues | Surface | Infrastructure | 8803 |
| **Nexus-CDN** | Anycast edge caching and web application firewall | Surface | Infrastructure | 8804 |
| **Nexus-Functions** | Ephemeral event-triggered function runtime | Surface | Compute | 8805 |
| **Nexus-Mesh** | P2P and hidden-service fallback routing when centralised paths fail | Deep/Dark | Infrastructure | 8806 |
| **Nexus-Trace** | Distributed tracing and log aggregation | Surface | Analytics | 8807 |
| **Nexus-SIEM** | Security event correlation and threat detection | Surface | Security | 8808 |
| **Nexus-FinOps** | Infrastructure spend tracking and right-sizing | Surface | Analytics | 8809 |
| **Nexus-Spatial** | WebXR rendering, spatial mapping and pixel streaming | Surface | Creative | 8810 |
| **Nexus-Quantum** | Quantum simulation endpoints and QCaaS adapters | Frontier | Compute | 8811 |
| **Nexus-Eco** | Carbon accounting and green-region workload steering | Surface | Analytics | 8812 |
| **Nexus-Space** | Orbital and satellite edge compute endpoints | Frontier | Compute | 8813 |
| **Nexus-Bio** | Molecular and DNA cold-storage archival interface | Frontier | Data | 8814 |
| **Nexus-Edge-Mesh** | Ad-hoc vehicular and drone swarm compute sharing | Frontier | Infrastructure | 8815 |
| **Nexus-Sovereign-Ledger** | Tamper-evident cryptographic audit ledger | Deep | Security | 8816 |

### How to read this list

Eleven of the seventeen are ordinary infrastructure the ecosystem will
genuinely need at scale — `Queue`, `CDN`, `Vector`, `Trace`, `SIEM`,
`Functions`, `Sandbox`, `FinOps` are the sort of thing that becomes urgent the
week you need it and takes months to retrofit. **Nexus-Vector** and
**Nexus-Queue** are the two with the clearest near-term pull: the AI apps want
embedding search, and several apps already need an event bus they do not have.

The frontier entries — `Quantum`, `Space`, `Bio`, `Edge-Mesh` — are honestly
speculative. They are recorded because the taxonomy identified them, not
because they are scheduled. Naming a thing is cheap; building it is not.

Nothing in this table has been scaffolded. When one is, it moves into the
register above and leaves this table.

### Prior-document names not carried forward

Consolidation surfaced 33 names in old documents that are absent from disk. Most
were prose artefacts, not planned apps — `Nexus-AppName` from a fill-in
template, and `Nexus-Ecosystem`, `Nexus-Platform`, `Nexus-Module(s)`,
`Nexus-Tools`, `Nexus-UI`, `Nexus-Systems` used as generic phrases. Others were
near-misses for apps that do exist: `Nexus-Radio` (`Nexus-Radio-Live`),
`Nexus-Team` (`Nexus-Team-Chat`), `Nexus-GPU` (`Nexus-GPU-Test`),
`Nexus-Git`/`Nexus-LLM` (single-document typos in one blueprint copy). One
genuine orphan, `Nexus-Porter`, appeared once with no definition and is dropped;
if it meant something, it needs a fresh case made for it.

---

## Cloud taxonomy reference

The ecosystem is designed against four layers of the global cloud landscape.
This is the compressed form; it exists to justify the expansion manifest above,
not to catalogue the industry.

**Surface Web** — indexable, regulated, enterprise public cloud. Compute and
VMs, bare metal, microVM and serverless runtimes, GPU/TPU accelerators,
container orchestration, quantum-as-a-service. Object, relational, NoSQL,
vector, geospatial, time-series and graph storage. Managed inference, agentic
sandboxes, MLOps, fine-tuning. CDN, WAF, IAM, service mesh, spatial streaming.
CI/CD, observability, SIEM, FinOps, carbon analytics.

**Deep Web** — non-indexed, authenticated, privacy-preserved. Hardware enclaves
(TEE), multi-party computation, fully homomorphic encryption. Government and
air-gapped sovereign clouds, zero-trust private mesh, HSM-rooted vaults,
institutional audit ledgers. Decentralised storage and compute markets,
zero-knowledge verifiable compute, decentralised identity.

**Dark Web** — encrypted overlay networks (Tor, I2P, Hyphanet, Lokinet).
Takedown-resistant hosting, fast-flux proxying, offshore DNS. Hidden-service
runtimes, zero-knowledge vaults, anonymous reverse proxies, non-custodial
payment gateways, traffic camouflage, mesh-over-mesh routing.

Two items from this layer are **explicitly out of scope** for Nexus Systems:
bulletproof hosting that refuses lawful process, and anonymous command-and-control
relay infrastructure. Both are recorded here as landscape, not as roadmap. The
sovereignty principle is about not depending on third parties — it is not about
evading accountability, and the distinction is deliberate.

**Frontier** — non-standard substrates. Orbital LEO compute and space laser mesh
routing; DNA molecular storage and wetware biocomputing; vehicular swarm mesh
and subsea data pods.

---

## Conventions

**Scaffold shape.** A new app is created with the `ghost` scaffolder and lands
as six files: `src/cloud.ts` (Systems-API registration), `src/contracts.ts`
(the registration payload type), `src/index.ts` (signal handling and
lifecycle), `src/<name>-engine.ts` (SQLite CRUD placeholder), `src/server.ts`,
and `tests/server.test.ts`. The engine file is meant to be replaced, not
extended — its generic CRUD is a placeholder, not a foundation.

**Registration.** Every app registers with Nexus-Systems-API, declaring `id`,
`name`, `description`, `mode` (`orchestrated` | `standalone`), `exposed`,
`health`, `upstreamUrl`, `capabilities` and `metadata`.

**Public exposure.** Through Nexus-Tunnel, always. Monitoring via
Nexus-Guardian, edge behaviour via Nexus-Edge.

**Growth.** The core platform stays lean. Further growth belongs in optional
industry branches — Creative, Engineering, VFX & Film, Business & Enterprise,
Education, Health & Medical — rather than in the core register.

---

## Verification method

Re-run this to check the register has not drifted:

```bash
cd apps
for d in Nexus-*/; do
  n="${d%/}"
  c=$(find "$d" -type f \( -name '*.rs' -o -name '*.ts' -o -name '*.tsx' \
      -o -name '*.py' -o -name '*.go' -o -name '*.jsx' \) \
      -not -path '*/node_modules/*' -not -path '*/target/*' \
      -not -path '*/dist/*' | wc -l)
  echo "$c $n"
done | sort -rn
```

Apps at exactly 6 are untouched scaffolds; below 6 are stubs; above 6 have real
code. Cross-check the count of directories against the count of register rows —
they must match exactly. A register that lists an app which does not exist, or
omits one that does, is the failure this document was written to end.

**Scan of 2026-08-19:** 112 `Nexus-*` directories, 75 scaffolds, 7 stubs, 30
with code beyond the scaffold. 113 register rows including `apps/Nexus`.

---

## Provenance

This document consolidates and supersedes:

| Superseded document | What it contributed |
|---|---|
| `docs/Nexus_Systems_Ecosystem_Blueprint.md` | Category structure; the Nexus-Tunnel integration rule; branching policy |
| `docs/Nexus-Systems-ecosystem-blueprint.md` | Near-identical duplicate of the above — differed only by two stray names |
| `docs/DEVELOPMENT_BLUEPRINT.md` | The stack decision matrix |
| `docs/ecosystem-audit-june-2026.md` | The measure-don't-claim audit method, re-run and updated here |
| `apps/docs/Nexus_Global_Cloud_Ecosystem_Architecture_v4.md` | The four-layer cloud taxonomy and the seventeen-module gap analysis |

**Not superseded:** `docs/nexus-ui-intelligence-doctrine.md` (previously the
misleadingly-named `docs/nexus-ui-intelligence-doctrine.md`). It is a design-token and CI-gate
specification, not an ecosystem catalogue — a different book, kept whole.

The superseded files are deleted from the working tree. Four of the five were
tracked, so git retains their full history. The fifth,
`docs/Nexus_Systems_Ecosystem_Blueprint.md`, was **never committed** — it existed
only as an untracked working-tree file and has no history to return to. It was
verified byte-for-byte against its tracked twin before deletion: the two
differed only by the stray names `Nexus-Git` and `Nexus-LLM`, both recorded in
the section above. Nothing unique was lost, and there is now one place to look.
