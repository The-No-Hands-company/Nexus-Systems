# AGENTS

Scope: Nexus Analytics scaffold workspace.

Guidelines:
- Keep changes minimal and reversible while scaffold phase is active.
- Prioritize API contract compatibility with Nexus-Cloud.
- Add tests with each behavior change.

Standards Enforcement:
- Follow ../docs/ENGINEERING_STANDARDS.md for runtime, typing, API, testing, and security requirements.
- Prefer strict typing, explicit validation, and production-safe defaults over scaffold shortcuts.
- Required validation before handoff: run the strongest available lint, typecheck, and test commands for this project.
