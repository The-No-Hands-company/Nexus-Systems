# Nexus Forge Roadmap

Last updated: 2026-04-13

This roadmap tracks what is already implemented in Nexus Forge and what is planned next.

## Status Legend

- `[x]` Implemented
- `[~]` In progress
- `[ ]` Planned

## Implemented (Scaffold Foundation)

- `[x]` Monorepo folder scaffold at `Nexus-Forge/`
- `[x]` Base backend entrypoint in `src/backend/main.ts`
- `[x]` Health endpoint: `GET /health`
- `[x]` Nexus Cloud metadata endpoint: `GET /.well-known/nexus-cloud`
- `[x]` API route scaffolds in `src/backend/api/routes.ts`
- `[x]` Federation route scaffolds in `src/backend/api/federation.ts`
- `[x]` Multi-VCS interface contract in `src/backend/vcs/vcs-interface.ts`
- `[x]` Git backend skeleton in `src/backend/vcs/git-backend.ts`
- `[x]` Repository manager skeleton in `src/backend/storage/repository.ts`
- `[x]` SQLite schema bootstrap in `src/backend/storage/db.ts`
- `[x]` Frontend scaffold (React + Vite) in `src/frontend/`
- `[x]` Basic repository list UI in `src/frontend/src/App.tsx`
- `[x]` Docker + Compose scaffold (`Dockerfile`, `docker-compose.yml`)
- `[x]` Bun test scaffold in `tests/vcs.test.ts`
- `[x]` Core docs: `README.md`, `ARCHITECTURE.md`, `AGENTS.md`

## Sprint K (MVP Buildout)

- `[ ]` Implement Git backend operations (init, history, diff, file read)
- `[ ]` Implement repository create/list/get API routes on real storage + DB
- `[ ]` Add auth baseline (JWT login, team membership checks)
- `[ ]` Add permission model enforcement (read/write/admin)
- `[ ]` Implement HTTP Git smart protocol flow
- `[ ]` Implement SSH Git push/pull server on port `8091`
- `[ ]` Persist repository metadata and activity logs end-to-end
- `[ ]` Wire frontend repo list to real backend responses
- `[ ]` Add repository detail page (branches, commits, files)
- `[ ]` Add clone URL generation (HTTPS + SSH)
- `[ ]` Add unit tests for backend VCS + DB modules
- `[ ]` Add integration tests for create -> clone -> push -> log flow

## Sprint L (Federation + AI)

- `[ ]` Implement `/api/cloud/discovery` with peer registry data
- `[ ]` Implement `/api/cloud/register` with signature verification
- `[ ]` Implement `/api/cloud/client` routing for replica selection
- `[ ]` Add peer heartbeat + gossip discovery loop
- `[ ]` Add PR model and endpoints (create/list/detail)
- `[ ]` Add Nexus AI PR review hook (`NEXUS_AI_URL`)
- `[ ]` Add issue -> draft PR generation workflow (feature-flagged)
- `[ ]` Add merge hooks to trigger Nexus Deploy (`NEXUS_DEPLOY_URL`)
- `[ ]` Add audit logs for all AI-driven actions

## Sprint M (Multi-VCS Expansion)

- `[ ]` Implement SVN backend adapter (read operations first)
- `[ ]` Implement Mercurial backend adapter (read operations first)
- `[ ]` Add backend registry for per-repo VCS dispatch
- `[ ]` Support repository creation by VCS type in UI and API
- `[ ]` Add compatibility tests for each supported backend

## Sprint N+ (Platform Maturity)

- `[ ]` Advanced search (repo, commit, code, semantic)
- `[ ]` Branch protection rules and required checks
- `[ ]` Full PR review UX (inline comments, approvals, checks)
- `[ ]` Webhooks framework for external integrations
- `[ ]` Backup and restore tooling (local + object storage)
- `[ ]` High availability peer sync hardening
- `[ ]` Performance optimization for large repositories
- `[ ]` Admin dashboard (nodes, peers, storage, background tasks)

## Ongoing Non-Negotiables

- `[~]` Free forever, no paywalls
- `[~]` Self-hostable on a single VPS
- `[~]` Federation-first design
- `[~]` Privacy-first defaults
- `[~]` Production quality and test coverage before release

## Release Targets

- `v0.1.0-alpha`: Scaffold + basic local Git read/write flow
- `v0.2.0-alpha`: Auth + permissions + PR basics
- `v0.3.0-alpha`: Federation endpoints live + peer discovery
- `v0.4.0-alpha`: Nexus AI review + Nexus Deploy hooks
- `v1.0.0`: Stable single-node production release
- `v1.5.0`: Federated multi-node production release


-------------------

Here's the most comprehensive expanded list of **GitHub-like sites** and **VCS hosting platforms** (software forges / code hosting services) as of 2026. I've included platforms for **Git**, **SVN (Subversion)**, **Mercurial (Hg)**, **Pijul**, **Fossil**, and other/niche VCS where public hosting exists.

Most modern platforms are **Git-only** because Git dominates. True **multi-VCS** platforms (supporting Git + SVN + Mercurial and sometimes more) are rarer but still actively used for legacy or mixed environments.

### 1. Major Cloud / Hosted Platforms (Primarily Git)
- **GitHub** — The dominant platform with issues, PRs, Actions, Copilot, etc.
- **GitLab.com** — Full DevOps platform with strong CI/CD, security, and AI features.
- **Bitbucket** (Atlassian) — Good Jira integration; historically supported Mercurial (now mainly Git).
- **Azure DevOps Repos** (Microsoft) — Enterprise-focused with tight Azure and Teams integration.
- **AWS CodeCommit** — Serverless Git hosting integrated with AWS services.
- **SourceForge** — Long-running open-source host supporting **Git, SVN, and Mercurial** + downloads, wikis, and forums.
- **Codeberg** — Non-profit, privacy-focused; runs on Forgejo (ethical Git hosting).

### 2. Multi-VCS Capable Platforms (Git + SVN + Mercurial + others)
These unify different version control systems under one interface:
- **RhodeCode** (Community & Enterprise) — Strong multi-VCS support for **Git, Mercurial, SVN**. Excellent code review, permissions, and enterprise features.
- **SCM-Manager** — Lightweight self-hosted server with native support for **Git, Mercurial, and Subversion**. Plugin ecosystem for CI and issue tracking.
- **Apache Allura** (powers SourceForge) — Supports **Git, SVN, Mercurial** + wikis, tickets, etc.
- **Heptapod** — Friendly fork of GitLab that adds native **Mercurial** support alongside Git.
- **Perforce Helix TeamHub** (now part of Perforce offerings) — Cloud hosting for **Git, Mercurial, or SVN** (free tier with 1GB).
- **Review Board** — Code review tool with multi-SCM support (**Git, SVN, Mercurial, Perforce, ClearCase**).
- **Repositery** — Cloud hosting focused on **SVN, Mercurial, and Git** with Trac integration.

**Note**: Other legacy multi-VCS mentions include support for Bazaar, CVS, or Perforce in older platforms, but these are declining.

### 3. Lightweight & Self-Hosted Git Forges
- **Forgejo** — Community-governed fork of Gitea; powers Codeberg. Strong focus on independence.
- **Gitea** — Fast, lightweight, easy self-hosting with issues, PRs, and CI-like features.
- **Gogs** — Older, simpler predecessor to Gitea.
- **OneDev** — Modern self-hosted with strong CI/CD and project management.
- **Gitness** — Emerging lightweight Git platform.
- **GitBucket** — Java-based self-hosted with GitHub-like UI.
- **GForge** — All-in-one with project management, wiki, and CI/CD.

### 4. Minimalist / Purist Platforms
- **SourceHut (sr.ht)** — Extremely lightweight, email-based workflow. No heavy JavaScript. Great for purists.

### 5. Niche & Specialized VCS Hosting
- **Nest (nest.pijul.com)** — Official hosting for **Pijul** (patch-based distributed VCS). Web UI for collaboration; open-source version in development for self-hosting.
- **Chisel (chiselapp.com)** — Free public hosting for **Fossil** repositories (unlimited repos). Fossil is a distributed VCS with built-in wiki, bug tracking, and forum.
- **Fossil self-hosting** — Fossil has excellent built-in web server; many projects self-host directly (no central large forge needed). Official Fossil project uses its own server.
- **Launchpad (Canonical)** — Primarily for Ubuntu/open-source; strong **Bazaar** support but also handles Git.
- **OSDN** (Japan-focused) — Supports **SVN, Git, Mercurial, Bazaar, CVS** for open-source projects.
- **Savannah** (GNU Project) — For free software projects (mainly Git/CVS/SVN).
- **Radicle** — Decentralized/P2P code collaboration built on Git (no central server).

### 6. Other / Enterprise / Legacy Platforms
- **Assembla** — Enterprise cloud with strong security/compliance (Git-focused).
- **TaraVault** (Inflectra) — Git + Subversion hosting with ALM/issue tracking.
- **Phorge** (fork of Phabricator) — Code review and project management; less common now.
- **repo.or.cz** — Very barebones Git hosting.
- **NotABug.org**, **0x0.st**, and small Gitea/Forgejo instances — Niche or personal forges.

### Quick Categorization by Primary VCS
- **Git-only (vast majority)**: GitHub, GitLab, Bitbucket, Azure DevOps, Codeberg, Gitea/Forgejo, SourceHut, OneDev, etc.
- **Git + Mercurial**: Bitbucket (legacy), Heptapod, SourceForge.
- **Git + SVN + Mercurial**: RhodeCode, SCM-Manager, SourceForge, Perforce Helix TeamHub, Repositery.
- **Pijul**: Nest (main/only major public host).
- **Fossil**: Chisel (public) + easy self-hosting.
- **Bazaar / Others**: Launchpad, OSDN (declining usage).

Many platforms from the "awesome-selfhosted" list or Docker images allow you to run your own forge (e.g., GitLab CE/EE, Gitea, Forgejo, SCM-Manager, RhodeCode CE).

This covers virtually all active public and self-hostable options in 2026. Smaller regional or private instances (e.g., thousands of Gitea instances) exist but aren't listed individually.

Would you like me to:
- Focus on **self-hosting** recommendations?
- Compare **multi-VCS** platforms in detail (RhodeCode vs SCM-Manager)?
- Provide migration tips from GitHub?
- Highlight free/open-source only options?

Just tell me your use case (open source, private team, legacy SVN/Mercurial migration, minimalist, etc.) for more tailored advice!