# Nexusclaw

Nexusclaw is a local-first AI agent framework and should be developed with explicit edge-first and event-driven behavior.

## Standards Enforcement

- Follow ../docs/ENGINEERING_STANDARDS.md as the baseline for TypeScript quality, testing, observability, and security.
- Preserve local-first execution as the default. Cloud coordination is allowed, but core agent behavior should not depend on constant central availability.
- Prefer event-driven flows for task execution, tool signaling, and state propagation.
- When integrating with the wider ecosystem, treat Nexus-Cloud as the nerve system and register capabilities through explicit typed contracts.

## Validation Target

- `npm run check`
- `npm test`