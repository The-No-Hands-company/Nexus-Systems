# Ecosystem Stack Control and Updater

## Role

This helper is the stack freshness and vulnerability radar for the Nexus ecosystem.

## Primary responsibilities

- Build cross-language inventory (Node, Python, Rust, Docker base images).
- Detect version drift from latest stable releases.
- Surface exploit/advisory risk early.
- Produce reports suitable for operator dashboards and issue automation.

## Safety constraints

- Read-only scanner. Do not auto-edit dependency files in this helper.
- Never include secrets, tokens, or local credentials in reports.
- Exclude backup and vendor directories from scans.

## Operational expectations

- Run on demand locally during development.
- Run daily in CI for continuous stack governance.
- Treat critical advisory findings as immediate patch candidates.
