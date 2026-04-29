# Nexus Forge Architecture

## Project Principles

1. **Pluggable VCS backends** — Abstract `VCSBackend` interface allows Git, SVN, Mercurial, Pijul
2. **Federation-first design** — Nexus Cloud contract integration built from day 1
3. **Minimal dependencies** — Use stdlib + battle-tested libraries (simple-git, better-sqlite3)
4. **TypeScript strict mode** — No implicit `any`, full type safety
5. **Bare repositories only** — No working directory on server (pure VCS hosting)

## Backend Architecture

### Request Flow

```
HTTP/SSH Request
    ↓
Protocol Handler (Git smart-http / SSH)
    ↓
VCS Dispatcher (identifies repo, VCS type)
    ↓
VCS Backend (GitBackend, SVNBackend, etc.)
    ↓
Storage Layer (disk operations + SQLite metadata)
    ↓
Response (git-protocol or SSH protocol)
```

### Directory Structure

```
FORGE_STORAGE_PATH=/data/forge-repos/
├── my-project.git/          # Git bare repo
├── legacy-code.svn/          # SVN repo
├── experimental.hg/          # Mercurial repo
└── distributed.pijul/        # Pijul repo
```

### SQLite Schema

**repositories**
- `id`, `name` (unique), `description`, `vcs`, `owner_id`, `created_at`, `updated_at`

**teams**
- `id`, `name` (unique), `description`, `created_at`

**team_members**
- `id`, `team_id`, `user_id`, `role` (owner/member)

**permissions**
- `id`, `repo_id`, `team_id`, `access_level` (read/write/admin)

**activity**
- `id`, `repo_id`, `action`, `actor_id`, `details`, `created_at`

## Frontend Architecture

### React Component Hierarchy

```
<App>
  ├── <Header>
  └── <Pages>
      ├── <RepoList> — Browse all repos
      ├── <RepoDetail> — Repo overview, clone URIs, activity
      ├── <PullRequest> — PR timeline, review, merge
      └── <TeamManage> — Permissions, members
```

### API Contract

**Repository**
```json
{
  "id": 1,
  "name": "my-project",
  "description": "...",
  "vcs": "git",
  "cloneUrl": "http://localhost:8090/my-project.git",
  "sshUrl": "ssh://user@localhost:8091/my-project.git",
  "owner": { "id": 1, "name": "alice" },
  "createdAt": "2024-01-01T00:00:00Z"
}
```

**Commit**
```json
{
  "hash": "abc123...",
  "author": "Alice <alice@example.com>",
  "message": "Fix: handle edge case",
  "timestamp": "2024-01-01T12:00:00Z",
  "files": ["src/main.ts", "tests/main.test.ts"]
}
```

## VCS Backend Interface

All VCS backends must implement:

```typescript
interface VCSBackend {
  init(path: string, bare?: boolean): Promise<void>
  getHead(path: string): Promise<VCSCommit | null>
  getHistory(path: string, limit?, skip?): Promise<VCSCommit[]>
  getCommit(path: string, hash: string): Promise<VCSCommit | null>
  getDiff(path: string, from: string, to: string): Promise<VCSDiff[]>
  getFile(path: string, filePath: string, revision?): Promise<string | null>
  listFiles(path: string, revision?): Promise<string[]>
  isValid(path: string): Promise<boolean>
}
```

### Git Backend

- **Library**: `simple-git` (cross-platform, no native deps)
- **Push/Pull**: Git smart-http protocol handler in backend
- **Clone URIs**: `http://forge.local/my-project.git`, `ssh://user@forge.local/my-project.git`

### SVN Backend

- **Library**: `svn-spawn` or raw `svn` CLI (future)
- **Push/Pull**: SVN DAV protocol (Apache Subversion HTTP module, or SimpleDAV)
- **Clone URIs**: `svn://forge.local/my-project`, `http://forge.local/svn/my-project`

### Mercurial Backend

- **Library**: `hg` CLI
- **Push/Pull**: Mercurial static HTTP or SSH
- **Clone URIs**: `http://forge.local/hg/my-project`, `ssh://user@forge.local/my-project`

### Pijul Backend

- **Library**: `pijul` CLI (once API stabilizes)
- **Push/Pull**: Pijul HTTP protocol
- **Clone URIs**: `pijul clone http://forge.local/my-project`

## Federation Model

### Nexus Cloud Contract

All Nexus Forge instances expose:

```
GET /.well-known/nexus-cloud          → SERVICE METADATA
GET /api/cloud/discovery               → DISCOVER PEER NODES
POST /api/cloud/register               → REGISTER WITH PEER
GET /api/cloud/client                  → ROUTE CLIENT TO PEER
```

### Peer Discovery

1. Admin registers seed peers in config
2. Each peer gossips known peers to others on heartbeat
3. Clients query discovery for repo replicas + peer topology

### Repository Replication

- **Async pull**: Periodic pull of updates from upstream
- **Signed attestation**: Each commit signed with node's Ed25519 key
- **Conflict resolution**: Last-write-wins on force-push; reject conflicting merges

## Authentication & Permissions

### Future: JWT-based Access

```
POST /auth/login { username, password }
  → { token: "jwt..." }

HTTP: Authorization: Bearer <jwt>
SSH: ed25519 key pair (no password login)
```

### Permission Levels

- **Read** — clone, pull history, view code
- **Write** — push to non-main branches, create PRs
- **Admin** — push to main, delete repo, manage team

## Nexus AI Integration

### Code Review on PR

```
User creates PR
  ↓
Webhook: POST /webhooks/pr-created
  ↓
Queue: { type: "code_review", pr_id: 123 }
  ↓
Worker: Call NEXUS_AI_URL
  ↓
AI: Ensemble review (multiple models, security focus)
  ↓
Update PR: Add review comments
```

### Auto-Generate from Issues

```
User creates issue: "Add login form"
  ↓
Webhook: POST /webhooks/issue-created
  ↓
AI: Suggest code (if GENERATE_ON_ISSUE=true)
  ↓
Auto-create PR with suggested implementation
  ↓
Human review + merge
```

## Deployment

### Docker Setup

```bash
docker-compose up
```

Runs:
- **Port 8090**: HTTP API + frontend
- **Port 8091**: SSH server
- **Volume /data**: All repos + SQLite DB

### Production Hardening (Future)

- TLS/HTTPS (behind Traefik, or built-in via Bun)
- SSH key rotation + audit logging
- Rate limiting on clone operations
- Concurrent push conflict detection
- Backup snapshots to S3-compatible storage

## Testing Strategy

### Unit Tests

- VCS backend operations (init, getCommit, getDiff)
- Database operations (CRUD repos, teams)
- API route logic

### Integration Tests

- Clone → Pull → Push workflow
- PR creation → AI review → merge
- Federation peer discovery + sync

### E2E Tests (Future)

- Multi-user collaboration
- Federated sync across 3+ nodes
- Nexus Deploy auto-deploy on merge

## Performance Considerations

- **Lazy loading**: Tree views fetch on-demand (not full history on every clone)
- **Cache**: In-memory LRU for recent commits, diffs
- **Incremental sync**: Only pull changed refs on federation peer heartbeat
- **Compression**: Brotli on HTTP responses
- **SSH batching**: Pipeline multiple commands in single SSH session

---

**Next**: Check [README.md](./README.md) for getting started.
