# Nexus Modeling Roadmap

This roadmap tracks delivery timing for production-grade kernel milestones. Detailed work items live in GitHub Issues/Projects.

## Active Principle

Document the vision in PRD, the logic in SDD, and the timing in this roadmap.

## Milestones

| Milestone | Status | Priority | Target | Notes |
|---|---|---|---|---|
| M1: Renderer Core Stabilization | In Progress | Critical | v0.2 | Complete GBuffer sampling contract and harden pass sequencing |
| M2: Shadow Pipeline | Planned | Critical | v0.3 | Add depth-only shadow passes and lighting integration |
| M3: Render Path Parity | Planned | High | v0.3 | Match scheduler and non-scheduler behavior or formally de-scope |
| M4: Advanced Pipelines | Planned | High | v0.4 | Replace mesh shader and RT placeholders with production implementations |
| M5: Contributor Scale-Up | In Progress | High | v0.2-v0.4 | Expand tests/docs and improve onboarding workflow |

## Next 4-6 Weeks

1. Descriptor and material resource binding for true deferred composite input sampling.
2. Shadow map target lifecycle and explicit transition choreography.
3. Behavior and regression tests for pass ordering, barriers, and resource lifetime.
4. Documentation updates for every renderer feature landing.

## How Work Is Tracked

- Roadmap timing and sequencing: this file.
- Task execution and ownership: GitHub Issues/Projects.
- Implemented behavior reference: docs and merged pull requests.
