# Nexus Versioning & Update Guide

Companion to `RELEASE_POLICY.md` (the *when* and *why*) and `MATURITY_MATRIX.md`
(the *readiness level*). This document covers the *how*: version numbers, commit
conventions, changelog format, and the submodule update workflow.

---

## Semantic versioning

Every production app uses `MAJOR.MINOR.PATCH` in its `package.json`
(or `Cargo.toml` / `CMakeLists.txt` for Rust/C++ apps).

| Bump | Trigger | Example |
|------|---------|---------|
| **PATCH** | Bug fix, security patch, copy/style change with no new behaviour | `1.2.3 → 1.2.4` |
| **MINOR** | New user-facing feature, new API endpoint (backwards-compatible) | `1.2.3 → 1.3.0` |
| **MAJOR** | Breaking API change, database migration requiring downtime, auth flow change | `1.2.3 → 2.0.0` |

Rules:
- Reset PATCH to 0 on a MINOR bump; reset both PATCH and MINOR to 0 on MAJOR.
- Pre-release apps (L0–L2) stay on `0.x.y` — a MAJOR bump signals L3 graduation.
- Never bump the version without a CHANGELOG entry.

---

## Commit conventions

All commits across the monorepo and every submodule app follow
[Conventional Commits](https://www.conventionalcommits.org/):

```
<type>(<optional scope>): <short description>

<optional body>

<optional footer>
```

| Type | Maps to |
|------|---------|
| `fix` | PATCH |
| `feat` | MINOR |
| `feat!` or `BREAKING CHANGE:` footer | MAJOR |
| `chore`, `docs`, `refactor`, `test`, `ci` | No version bump |

Examples:
```
fix: handle empty inbox on first login
feat(composer): add CC field to email compose
feat!: replace session tokens with OIDC — requires re-login
chore: update Nexus-Hosting submodule ref
```

---

## Changelog format

Every production app keeps a `CHANGELOG.md` at its repo root.
The monorepo root `governance/CHANGELOG.md` records cross-app and
ecosystem-level events (submodule bumps, breaking changes that affect
multiple apps, infrastructure changes).

### App CHANGELOG template

```markdown
# Changelog

## [Unreleased]

## [1.3.0] — 2026-11-05
### Added
- CC field in email composer (#42)

### Fixed
- Empty inbox crash on first login (#41)

## [1.2.4] — 2026-10-28
### Security
- Rate-limit login endpoint (hotfix)
```

Rules:
- Keep an `[Unreleased]` section at the top. Move it to a versioned heading
  on release.
- Group entries under `Added`, `Changed`, `Fixed`, `Removed`, `Security`,
  `Deprecated`.
- One line per change. Link to the PR or issue where it helps.
- Security fixes must be tagged `### Security` and identified as a hotfix in
  the ecosystem changelog.

---

## Submodule update workflow

Nexus-Hosting, Nexus-Email, and future graduated apps live in their own
repositories and are referenced from this monorepo as git submodules.

```
App repo (e.g. Nexus-Hosting)
  │  feature branch → PR → merge to main
  │  bump version in package.json
  │  update CHANGELOG.md
  │  tag: git tag v1.3.0 && git push origin v1.3.0
  ▼
Monorepo (Nexus-Systems)
  │  cd apps/Nexus-Hosting (or Nexus-Email, etc.)
  │  git pull origin main   ← picks up new commits
  │  cd ../..
  │  git add apps/Nexus-Hosting
  │  git commit -m "chore: update Nexus-Hosting submodule ref to v1.3.0"
  │  git push origin main
  ▼
Production deploy
  │  ./deploy/production/deploy.sh   ← or the per-app deploy step
```

### Gate before bumping the submodule ref

Run the app's check gate first. Do not bump the ref if the gate fails.

| App | Gate command |
|-----|-------------|
| Nexus-Hosting | `cd apps/Nexus-Hosting && check.sh` |
| Nexus-Email | `cd apps/Nexus-Email && check.sh` |
| *(future apps)* | `cd apps/<App> && check.sh` |

### Version tag format

```
v<MAJOR>.<MINOR>.<PATCH>
```

Tag on the app repo, not the monorepo. The monorepo commit message records
which version was pulled in (`chore: update Nexus-Hosting submodule ref to v1.3.0`).

---

## Adding a new graduated app

When an app reaches L3 and moves to its own submodule repo:

1. Create the app repo under `github.com/The-No-Hands-company/<App-Name>`.
2. Set initial version to `0.1.0` in `package.json`.
3. Add a `CHANGELOG.md` with an `[Unreleased]` section.
4. Add a `check.sh` gate.
5. Register it in `AGENTS.md` under "Active apps with quality gates".
6. Add a row to `MATURITY_MATRIX.md`.
7. Add a row to this document's gate table above.

---

## Hotfix path

Hotfixes bypass the normal MINOR/MAJOR schedule but not verification:

1. Branch from the production tag: `git checkout -b hotfix/v1.2.4 v1.2.3`
2. Apply the fix, run the gate.
3. Bump PATCH, add a `### Security` or `### Fixed` entry to the CHANGELOG.
4. Tag and merge back to `main`.
5. Update the monorepo submodule ref.
6. Record in `governance/CHANGELOG.md` as a hotfix with reason and risk.

---

## Docs and knowledge base

User-facing documentation for each app lives at **docs.tnhc.dev**
(planned — see `governance/prds/` for the portal PRD when available).
Release notes from each app's `CHANGELOG.md` are mirrored there on every
production release.
