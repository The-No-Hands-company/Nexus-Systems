# Nexus Product Governance

This directory is the canonical product-control plane for Nexus Systems. It
turns product decisions into visible, reviewable records rather than private
context.

## Scope

The initial core cohort is:

- Nexus Dashboard
- Nexus Mail
- Nexus Calendar
- Nexus Chat
- Nexus Hosting
- Nexus Identity and Kernel

Nexus-Files and Nexus-Docs are deliberately deferred until they are functional
products. New applications must not be started while this cohort remains below
its agreed maturity targets.

## Authority

The Founder owns product prioritisation, release approval, and any documented
exception to this process.

## Canonical records

| Record | Purpose |
|---|---|
| [BACKLOG.md](BACKLOG.md) | Prioritised cross-product work |
| [RELEASE_POLICY.md](RELEASE_POLICY.md) | Release channels, gates, and approvals |
| [MATURITY_MATRIX.md](MATURITY_MATRIX.md) | Readiness model and current targets |
| [CHANGELOG.md](CHANGELOG.md) | Production releases and hotfixes |
| [prds/](prds/) | Product requirements for the core cohort |

`docs/NEXUS-ECOSYSTEM.md` remains the canonical technical ecosystem register.
This directory governs product decisions; it does not duplicate architecture
or runtime configuration.

## Operating cadence

1. Capture proposed work in `BACKLOG.md`.
2. Score it using the documented priority formula.
3. Confirm the affected PRD and acceptance criteria before implementation.
4. Move work through the release policy.
5. Record production releases in `CHANGELOG.md`.
6. Review the top ten ecosystem priorities quarterly.
