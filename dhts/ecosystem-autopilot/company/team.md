# The No Hands Company - Nexus Autonomous Team

Mission: Run Nexus Systems as a major-tech autonomous engineering organization with strict visibility, quality gates, and deterministic delivery.

## Delivery Principle

- Every change is traceable to a task contract.
- Every task has a role owner and acceptance criteria.
- Every merge passes pre-merge gates.
- Every release passes release gates.
- Every decision is written as durable context.

## Members

| Role Layer | Role | Accountability |
|---|---|---|
| Executive | CTO | Objectives, investment, risk profile |
| Architecture | Chief Architect | Boundaries, contracts, cross-app consistency |
| Program | Program Manager | Sequencing, dependency graph, roadmap order |
| Delivery | Engineering Lead | Technical quality and review control |
| Specialist | Planner | Task decomposition and dependency validation |
| Specialist | Backend | Service and API implementation |
| Specialist | DevOps | Runtime, CI, infra consistency |
| Specialist | QA | Acceptance and regression blocking |
| Specialist | Docs | Decision and architecture context integrity |

## Modes

- Planning mode: architecture and sequence first.
- Execution mode: role dispatch with WIP limits.
- Verification mode: quality gates and cross-repo tests.
- Release mode: release-readiness pipeline and artifact updates.

## Source of Truth

- State: `dhts/ecosystem-autopilot/autopilot.state.json`
- Configuration: `dhts/ecosystem-autopilot/autopilot.config.json`
- Seed backlog: `dhts/ecosystem-autopilot/backlog.seed.json`
- Orchestrator: `dhts/ecosystem-autopilot/autopilot.py`
