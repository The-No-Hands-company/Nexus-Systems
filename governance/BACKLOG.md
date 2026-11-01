# Nexus Master Backlog

## Priority scoring

Score each item before it enters a planned release:

```text
score = user impact (1-5) + business value (1-5) + complexity (-1 to -5)
```

Higher scores take precedence within the same development layer. A score does
not override Critical work, security obligations, or release dependencies.

## P1 — next 30 days

| Product | Item | Layer | Impact | Value | Complexity | Score | Owner | Status |
|---|---|---:|---:|---:|---:|---:|---|---|
| Mail | Trash, delete, and restore lifecycle | Product feature | 5 | 5 | -1 | 9 | Founder | Proposed |
| Mail | Search and spam handling | Product feature | 5 | 5 | -3 | 7 | Founder | Proposed |
| Calendar | Edit and delete events | Product feature | 5 | 4 | -2 | 7 | Founder | Proposed |
| Calendar | Event notifications | Product feature | 5 | 4 | -3 | 6 | Founder | Proposed |
| Dashboard | Unified notifications | Product feature | 5 | 4 | -3 | 6 | Founder | Proposed |
| Identity and Kernel | Cross-app audit logs and session controls | Product feature | 5 | 5 | -4 | 6 | Founder | Proposed |
| Chat | Restore current heartbeat and health evidence | Critical | 4 | 4 | -1 | 7 | Founder | Proposed |
| Release process | Enforce Nit, staging validation, and changelog evidence | Quality improvement | 5 | 5 | -3 | 7 | Founder | In progress |

## P2 — 60 to 90 days

| Product | Item | Layer | Owner | Status |
|---|---|---|---|---|
| Mail | Labels, rules, and signatures | Product feature | Founder | Proposed |
| Calendar | Sharing, invitations, and recurring events | Product feature | Founder | Proposed |
| Hosting | Product acceptance coverage for site deployment and rollback | Quality improvement | Founder | Proposed |
| Dashboard | Cross-product release and health summary | Quality improvement | Founder | Proposed |

## P3 — future vision

| Product area | Item | Layer | Owner | Status |
|---|---|---|---|---|
| Ecosystem | Federation improvements and cross-app workflows | Future vision | Founder | Deferred |
| Ecosystem | Automation engine and AI agents | Future vision | Founder | Deferred |
| Mail | AI assistance and templates | Future vision | Founder | Deferred |

Files and Docs are intentionally absent. They return only after they become
functional products and the Founder updates the core-cohort decision.
