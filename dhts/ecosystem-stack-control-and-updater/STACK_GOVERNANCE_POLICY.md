# Nexus Ecosystem Stack Governance Policy

## Objective

Keep the entire Nexus ecosystem on cutting-edge, security-patched, actively supported stack versions.

## Standards baseline

- JavaScript standard target: ECMAScript 2025 (ES2025).
- Node.js policy: latest current line for active development, latest LTS line for production hardening compatibility checks.
- TypeScript policy: latest stable release.
- Python policy: latest stable release series used by runtime images and local tooling.
- Rust policy: latest stable toolchain.
- Container base images: latest stable tags with regular refresh and digest pinning in production pipelines.

## Operational cadence

- Daily stack scan with this updater tool.
- Weekly update planning and batching.
- Immediate out-of-band patching for high/critical advisories.

## Enforcement expectations

- New services should not introduce end-of-life runtimes.
- New dependency additions should favor actively maintained packages.
- Range-based dependency specs should be reduced where security traceability is required.
- Critical CVEs and actively exploited advisories are release blockers.

## Risk control for "always latest"

Always-latest is the target direction, but updates still pass through:

1. compatibility checks
2. test suite validation
3. staged rollout
4. rollback readiness

This keeps the ecosystem modern without sacrificing reliability.

## Primary tool

Use the stack control helper in this directory to produce actionable reports:

- JSON output for automation
- Markdown output for operator visibility
