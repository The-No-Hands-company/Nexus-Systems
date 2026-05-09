# Nexuslang

Nexuslang is a mature language/runtime/tooling codebase, not a scaffold.

## Standards Enforcement

- Follow ../docs/ENGINEERING_STANDARDS.md as the ecosystem baseline for Python quality, testing rigor, API/tooling behavior, and security.
- Keep this project typed and test-driven. Parser, compiler, analyzer, runtime, and LSP changes all require regression coverage.
- Preserve explicit state transitions, event propagation, and observable failure handling across tooling and runtime surfaces.
- Nexus-Cloud remains the ecosystem nerve system for service-level coordination; Nexuslang should expose clear typed contracts when integrating with the wider ecosystem.

## Repo Conventions

- Prefer durable fixes over compatibility hacks.
- Avoid broad parser or typechecker relaxations unless backed by tests and a clear language-design reason.
- Keep analyzer and diagnostics changes precise, typed, and deterministic.

## Validation Target

- `pytest`
- targeted compiler/runtime/analyzer suites for the area you touched
