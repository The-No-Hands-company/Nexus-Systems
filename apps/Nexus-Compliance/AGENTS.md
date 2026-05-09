# AGENTS

Scope: Nexus Compliance scaffold workspace.

Guidelines:
- Keep changes minimal and reversible while scaffold phase is active.
- Prioritize API contract compatibility with Nexus-Cloud.
- Add tests with each behavior change.

Standards Enforcement:
- Follow ../docs/ENGINEERING_STANDARDS.md for runtime, typing, API, testing, and security requirements.
- Keep this service on Bun + strict TypeScript; do not add plain JavaScript sources.
- Required validation before handoff: bun run check && bun test.
