# Nexus Forge

A self-hosted, federated code forge supporting Git, SVN, Mercurial, and Pijul. Free forever, zero paywalls.

## Vision

**Nexus Forge** is a privacy-first alternative to GitHub, GitLab, and Gitea — designed to:
- ✅ **Self-host** on a single VPS or local machine
- ✅ **Federate** across teams without a central authority
- ✅ **Integrate** seamlessly with Nexus AI for code generation, review, and CI/CD
- ✅ **Support multiple VCS backends** (Git, SVN, Mercurial, Pijul)
- ✅ **Remain free forever** — no subscriptions, no paywalls

## Core Features (MVP)

- **HTTP & SSH push/pull** over Git, SVN, etc.
- **Web UI** for repository browsing, PR/MR timeline, code diff viewing
- **Nexus AI integration** for auto-review, code suggestions, issue→PR generation
- **Federation** via Nexus Cloud contract (`/.well-known/nexus-cloud`, `/api/cloud/*`)
- **Team management** with permission levels (read, write, admin)
- **Activity feeds** and audit logs

## Architecture

### Backend (TypeScript/Bun)
- **Port 8090** — HTTP API + Git/SVN/etc. protocol handlers
- **Port 8091** — SSH server for push/pull
- **Storage**: Bare repos on disk + SQLite for metadata (teams, perms, activity)
- **Federation**: Gossip discovery + peer heartbeat + signed attestation

### Frontend (React/Vite)
- **Port 8090** (served from backend `/` root)
- Repo browser, PR timeline, code viewer
- Clone URI quick-copy (SSH / HTTPS)
- Team management dashboard

### VCS Backends
```
src/vcs/
├── vcs-interface.ts      # Abstract VCS contract
├── git-backend.ts        # Git implementation (git and libgit2)
├── svn-backend.ts        # SVN stub (future)
└── hg-backend.ts         # Mercurial stub (future)
```

## Development Setup

### Prerequisites
- Node 22 + Bun
- Git (for testing Git backend)
- SQLite3

### Quick Start

```bash
cd Nexus-Forge
bun install
bun run dev
```

Runs on `http://localhost:8090`.

### Environment Variables
```bash
FORGE_STORAGE_PATH=/data/forge-repos         # Where bare repos live
FORGE_DB_PATH=/data/forge-meta.db           # SQLite metadata
FORGE_PUBLIC_DOMAIN=forge.example.com       # For federation
NEXUS_AI_URL=http://localhost:8000           # Optional: AI service
NEXUS_DEPLOY_URL=http://localhost:3000       # Optional: CD trigger
```

## Project Structure

```
Nexus-Forge/
├── README.md
├── package.json
├── tsconfig.json
├── Dockerfile
├── docker-compose.yml
├── src/
│   ├── backend/
│   │   ├── main.ts                 # Server entry point
│   │   ├── api/
│   │   │   ├── routes.ts           # HTTP routes (/api, /repos, /.well-known)
│   │   │   ├── federation.ts       # Nexus Cloud contract endpoints
│   │   │   └── auth.ts             # JWT + team perms
│   │   ├── vcs/
│   │   │   ├── vcs-interface.ts    # Abstract VCS contract
│   │   │   ├── git-backend.ts      # Git push/pull handler
│   │   │   ├── svn-backend.ts      # SVN stub
│   │   │   └── hg-backend.ts       # Mercurial stub
│   │   ├── storage/
│   │   │   ├── repository.ts       # Repo lifecycle + VCS dispatch
│   │   │   ├── db.ts               # SQLite ops (teams, perms, activity)
│   │   │   └── ssh-server.ts       # SSH push/pull handler
│   │   └── ai/
│   │       ├── review.ts           # Code review (Nexus AI integration)
│   │       └── hooks.ts            # Pre-commit, PR auto-actions
│   └── frontend/
│       ├── index.html              # Entry HTML
│       ├── src/
│       │   ├── App.tsx             # Root component
│       │   ├── pages/
│       │   │   ├── RepoList.tsx
│       │   │   ├── RepoDetail.tsx
│       │   │   ├── PullRequest.tsx
│       │   │   └── TeamManage.tsx
│       │   └── components/
│       │       ├── CodeViewer.tsx
│       │       ├── DiffView.tsx
│       │       └── CloneUri.tsx
│       ├── package.json
│       └── vite.config.ts
├── tests/
│   ├── vcs.test.ts                 # VCS backend tests
│   ├── federation.test.ts          # Federation contract tests
│   └── api.test.ts                 # HTTP endpoint tests
└── .gitignore
```

## Roadmap

### Sprint K (MVP Scope)
- [ ] HTTP Git push/pull (bare repos on disk)
- [ ] Web UI for repo browsing + basic PR timeline
- [ ] SQLite metadata store (repos, teams, basic perms)
- [ ] SSH server for Git operations
- [ ] Federation discovery endpoint (`/.well-known/nexus-cloud`)

### Sprint L (AI + Advanced Features)
- [ ] Nexus AI code review on PR creation
- [ ] Auto-suggest fixes, tests, security improvements
- [ ] SVN backend proof-of-concept
- [ ] Federation peer gossip + heartbeat

### Sprint M+ (Feature Parity)
- [ ] Advanced search (semantic + git-log)
- [ ] Mercurial + Pijul backends
- [ ] Issue templates, automation rules
- [ ] Audit logs, activity feeds
- [ ] Backup/restore across federated nodes
- [ ] Nexus Deploy integration (auto-deploy on merge)

## Contributing

- Follow TypeScript strict mode
- All new features must have tests
- PR review via Nexus AI (once ready)
- Keep it simple — this is a forge, not an IDE

## License

GPL-3.0 — Free forever, for everyone.

---

**Next Steps:** See [ARCHITECTURE.md](./ARCHITECTURE.md) for deep-dive on VCS backends and federation.
