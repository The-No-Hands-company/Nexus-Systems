# Nexus Forge

A self-hosted git forge built on one premise: **the server is not the root of
trust.** Who may change a repository is decided by signatures that live in the
repository, and what the server did with your refs is written to a log any
client can check.

## What works today

| | |
|---|---|
| **Git over smart HTTP** | `git clone` / `fetch` / `push` through `git http-backend`. The forge never parses packfiles; it decides whether a request may reach git at all. Only the upload-pack and receive-pack endpoints exist; the dumb protocol is 404. |
| **One access check, default-deny** | `authorize()` in `src/backend/auth/access.ts` is the only place access is granted. Anonymous may read public repositories; everything else needs a grant (read < write < admin). Private repositories answer the same whether or not they exist. |
| **Expiring tokens** | Opaque `nxf_…` tokens, stored as SHA-256, always expiring (default 8h, max 30 days). Sent as `Bearer` or as the HTTP Basic password, which is what git uses. |
| **Signed-push policy** | Every commit a push introduces must be SSH-signed by a key in the policy in force: `.nexus/allowed_signers` at the default branch's tip *before* the push, or the repository's trust root. See [docs/SECURITY-MODEL.md](./docs/SECURITY-MODEL.md). |
| **Client-side enforcement** | `forge verify <url>` mirrors the repository and replays every push in the ref log through the same policy check the server runs, so a server that skipped its own check is caught. A first run needs the trust root's fingerprint from the owner. |
| **Verified plain git** | `forge install-helper` adds `git-remote-nexus`: `git clone nexus::https://…` and every later fetch/pull verify first and refuse anything that fails. |
| **Hash-chained ref log** | Every accepted ref update is appended to a chained log that `forge verify` checks, replays against the served refs, and pins. |
| **Hardened git invocation** | Repository names from an allowlist; git runs in an environment built from nothing; `fsckObjects`, `denyNonFastForwards`, `denyDeletes` and the hooks path are set on the command line where a repository's own config cannot undo them. |

Not built yet: SSH transport, pull requests, issues, web code browsing,
federation, other VCSes (SVN, Mercurial, Pijul).

## Quick start

```bash
bun run dev                       # http://127.0.0.1:8094 (HOST/PORT to change)

# Admin operations work on the database and storage directly:
bun src/cli/forge.ts admin user add alice
bun src/cli/forge.ts admin repo create demo --owner alice --trust-root ~/.ssh/allowed_signers
bun src/cli/forge.ts admin token alice --ttl-hours 8      # prints the token once
export NEXUS_FORGE_TOKEN=...                              # paste it here

git -c http.extraHeader="Authorization: Bearer $NEXUS_FORGE_TOKEN" \
    clone http://127.0.0.1:8094/demo.git
git -C demo commit -S --allow-empty -m first              # gpg.format=ssh
git -C demo -c http.extraHeader="Authorization: Bearer $NEXUS_FORGE_TOKEN" push origin main

# Collaborators: verify with the fingerprint `admin repo create` printed,
# or use plain git through the helper.
bun src/cli/forge.ts verify http://127.0.0.1:8094/demo.git --trust-root sha256:...
bun src/cli/forge.ts install-helper
git clone -c nexus.trustRoot=sha256:... nexus::http://127.0.0.1:8094/demo.git
```

Forge used 8090 until 2026-10-03; that is Nexus-Hosting's site-proxy port.

### Environment

| Variable | Default | |
|---|---|---|
| `FORGE_DB_PATH` | `./data/forge.db` | SQLite metadata (users, token hashes, repositories, grants) |
| `FORGE_STORAGE_PATH` | `./data/repos` | Bare repositories; forge files live in `<repo>.git/nexus/` |
| `HOST` / `PORT` | `127.0.0.1` / `8094` | Loopback by default; public traffic comes through the ecosystem proxy |
| `NEXUS_FORGE_PUBLIC_URL` | request origin | Base for clone URLs in the API |
| `NEXUS_CLOUD_URL` | unset | Optional Nexus Cloud registration + heartbeat |

## HTTP surface

| Endpoint | Needs |
|---|---|
| `GET /<repo>.git/info/refs?service=git-upload-pack`, `POST /<repo>.git/git-upload-pack` | read |
| `GET /<repo>.git/info/refs?service=git-receive-pack`, `POST /<repo>.git/git-receive-pack` | write |
| `GET /<repo>.git/nexus/ref-log`, `GET /<repo>.git/nexus/trust-root` | read |
| `GET /api/repos`, `GET /api/repos/:name`, `GET /api/repos/:name/activity` | read (lists only what you can read) |
| `POST /api/repos` `{name, visibility, trustRoot, description?}` | signed in |
| `GET /api/auth/status`, `GET /health`, `GET /.well-known/nexus-cloud` | — |

## Layout

```
src/backend/
  server.ts            request routing
  auth/                tokens + the one authorize()
  git/                 smart-HTTP bridge, name allowlist, clean git env
  policy/              signed-push verification, allowed_signers parsing
  hooks/               pre-receive (policy) and post-receive (ref log)
  reflog/              hash chain
  storage/             SQLite + repository lifecycle
  api/routes.ts        JSON API
src/cli/               verify, remote helper, admin
tests/                 real git clients against a real server
```

## Quality gate

`./check.sh` (runs `bun test`). The tests start a real server and drive it
with real `git` and `ssh-keygen`; each security property is also checked by
mutating the code and confirming a test fails.

## License

AGPL-3.0-or-later (see the root LICENSE and LICENSING.md)
