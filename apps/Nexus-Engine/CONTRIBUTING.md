# Contributing to Nexus Engine

Thank you for contributing.

## Development Setup

1. Use Node.js and npm compatible with this repository.
2. Install dependencies:
   `npm install`
3. Run type-check:
   `npm run type-check`
4. Run tests:
   `npm test -- --run`

Run commands from `apps/Nexus-Engine`.

## Workflow

- Create focused changes with clear intent.
- Keep architecture and docs in sync with code changes.
- Add tests for behavior changes.
- Avoid mixing unrelated refactors in one change.

## Pull Request Expectations

- Explain what changed and why.
- Describe risks and mitigations.
- Include test evidence.
- Update docs for new systems or behavior changes.

## Coding Guidelines

- Prefer extracted systems over growing manager conditionals.
- Keep orchestration layers thin.
- Keep interfaces explicit and typed.
- Avoid hidden side effects across system boundaries.

## Commit Hygiene

- Do not commit secrets or credentials.
- Keep generated artifacts out of git.
- Keep commits scoped and descriptive.

## Where to Discuss Design

Use docs under `docs/` for architecture proposals and updates:

- `docs/DESIGN.md`
- `docs/ARCHITECTURE-RUNTIME.md`
- `docs/SCRIPTING.md`
- `docs/ANIMATION.md`
- `docs/PHYSICS.md`
- `docs/ROADMAP.md`
