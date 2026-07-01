# AGENTS

Scope: Nexus-Logistics workspace.

Guidelines:
- Keep changes minimal and reversible.
- Prioritize API contract compatibility with Nexus-Cloud Systems API.
- Add tests with each behavior change.

Standards Enforcement:
- Follow ../docs/ENGINEERING_STANDARDS.md for runtime, typing, API, testing, and security requirements.
- Keep this service on Bun + strict TypeScript.
- Required validation before handoff: bun run check && bun test.
