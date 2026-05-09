# Nexus Systems — Engineering Standards

Version: 1.0  
Effective: 2026-05-08  
Scope: All built-out Nexus Systems codebases

> **Principle:** Every line of code in this ecosystem must meet or exceed the bar set by
> engineering organisations at Alphabet, Meta, and Microsoft. Code that falls below this
> standard must be treated as a defect and corrected immediately.

---

## 1. Language & Runtime Standards

### TypeScript / Node.js / Bun services
- **Runtime:** Bun (`>=1.1`) — TypeScript runs natively, no transpile step required
- **TypeScript:** strict mode is mandatory. The following flags are required in every `tsconfig.json`:
  - `"strict": true`
  - `"noUncheckedIndexedAccess": true`
  - `"exactOptionalPropertyTypes": true`
  - `"noImplicitReturns": true`
  - `"noFallthroughCasesInSwitch": true`
- **`any` is forbidden.** Use `unknown` + type narrowing. All `@ts-ignore` and `@ts-expect-error` must include a justification comment and a tracking issue ID.
- **Linting & formatting:** Biome (`>=1.9`) — single tool for lint + format. `biome check --write` must pass with zero warnings in CI.
- **Package manager:** Bun (for TypeScript services) or npm (for projects already on npm). No mixing within a project.
- **Imports:** path aliases preferred over deep relative paths. No implicit `.js` extensions needed with Bun.

### Python services
- **Version:** Python 3.12+
- **Type hints:** required on all function signatures and module-level variables
- **Type checking:** mypy in strict mode (`strict = true` in `pyproject.toml`)
- **Linting & formatting:** ruff (`ruff check` + `ruff format`). Must pass with zero warnings.
- **Dependencies:** pinned in `pyproject.toml` (PEP 621 format) with `uv` or `pip-compile` for lock files. `requirements.txt` is acceptable as an interim but `pyproject.toml` must be added.

### Rust services
- **Edition:** 2024
- **Clippy:** `#![deny(clippy::all, clippy::pedantic)]` in every crate root, or equivalent `deny.toml`
- **Formatting:** `rustfmt` with `rustfmt.toml` — hard requirement
- **Unsafe:** forbidden unless explicitly justified with a `// SAFETY:` comment explaining invariants
- **Error handling:** `thiserror` for library errors, `anyhow` for application errors. `unwrap()` is forbidden in non-test code. Use `expect("reason")` with a meaningful message if panicking is intentional.

---

## 2. Ecosystem Architecture Standards

### Brain-inspired system design
- Prefer **event-driven flows** over polling. If a state change can be represented as an event, queue message, webhook, WebSocket frame, or SSE update, that is the default architecture.
- Polling is allowed only when there is no durable event source available or when integrating with a third-party system that cannot push events.
- Services must be designed as **reactive components**: small inputs produce explicit state changes, emitted events, or both.
- Favor append-only event records, audit trails, and immutable state transitions for control-plane actions.

### Nexus-Cloud as the ecosystem nerve system
- **Nexus-Cloud is the control plane and nerve system of the ecosystem.** Service registration, heartbeat, topology, routing intent, and global coordination must converge there.
- Built-out services must integrate with Nexus-Cloud for identity, health signaling, routing metadata, or event propagation unless there is a documented reason not to.
- Repo-local architectures must treat Nexus-Cloud as the canonical source for ecosystem awareness, not as an optional sidecar.
- Cross-service protocols must be explicit, versioned, and typed. Hidden coupling through ad hoc environment variables or undocumented JSON payloads is forbidden.

### Edge, local-first, and autonomous behavior
- Push computation to the edge when practical. Prefer local-first behavior, cached state, and degraded offline-capable operation over unnecessary round-trips to a central service.
- User-facing applications should continue to function in a reduced mode when the control plane or a dependent service is unavailable.
- AI or inference features should prefer on-device or near-edge execution when latency, privacy, or resilience materially improves.

### Homeostatic and self-healing behavior
- Systems must degrade gracefully. When a dependency is unhealthy, services should fail closed for sensitive flows and fail soft for non-critical flows with explicit status reporting.
- Use circuit breakers, bounded retries, queue handoff, cached responses, and alternate execution paths where appropriate.
- A failure path must be observable. Silent drop, hidden fallback, or unbounded retry loops are defects.
- Recovery behavior must be explicit in code: state transitions, route changes, and degraded-mode decisions should be logged and, where relevant, surfaced through metrics.

---

## 3. API Design Standards

### HTTP APIs
- All endpoints must return `Content-Type: application/json; charset=utf-8`
- All responses must include a request ID header: `X-Request-Id: <uuid>`
- Security headers required on every response:
  ```
  X-Content-Type-Options: nosniff
  X-Frame-Options: DENY
  Cache-Control: no-store
  ```
- HTTPS-only in production. Services running locally on HTTP must set `Strict-Transport-Security` at the reverse-proxy layer.
- HTTP 4xx responses must return a structured error body:
  ```json
  { "error": "human-readable message", "code": "MACHINE_READABLE_CODE", "requestId": "uuid" }
  ```
- HTTP 5xx responses must NOT leak stack traces or internal details to the caller. Log the details server-side.
- Every service must expose `GET /health` returning:
  ```json
  { "service": "<name>", "status": "ok", "version": "0.1.0", "uptimeSeconds": 0 }
  ```

### Input validation
- All incoming request bodies must be validated before processing. Validation must be strict:
  - Required fields explicitly checked
  - Types explicitly checked
  - Enum fields checked against an allowlist
  - Reject unknown fields (extra keys should not cause silent data loss)
- Validation errors must return HTTP 400 with a descriptive error message indicating which field failed and why.

### Versioning
- All APIs are versioned at the path level: `/api/v1/...`
- Breaking changes require a new major version. Non-breaking additions are allowed in the current version.

---

## 4. Observability Standards

### Logging
- **No `console.log` in production code.** Use a structured logger (pino for Node/Bun, structlog for Python, tracing for Rust).
- Log entries must be JSON-formatted in production with fields: `level`, `time`, `service`, `requestId`, `message`, and relevant context.
- Log levels: `trace` (debug noise), `debug` (development detail), `info` (normal operation events), `warn` (recoverable anomalies), `error` (failures requiring attention). Never log sensitive data (tokens, passwords, PII).
- Every inbound HTTP request must produce an `info`-level log entry with: method, path, status code, duration_ms, requestId.
- Event-driven systems must also log meaningful control-plane events: registration, heartbeat failure, routing change, degraded-mode entry, circuit-open, and recovery.

### Metrics
- Expose a Prometheus-compatible `/metrics` endpoint (or integrate with the platform metric system) for services beyond MVP.
- At minimum, track: request count, error count, request latency histogram, uptime.

### Tracing
- Instrument with OpenTelemetry. Propagate `traceparent` / `tracestate` headers. At MVP, headers are accepted and forwarded even if not actively exported.

---

## 5. Testing Standards

### Coverage requirements
- **Unit tests:** all validation logic, all business logic, all error paths
- **Integration/contract tests:** all HTTP endpoints (happy path + error paths)
- **Target:** 80%+ line coverage for non-trivial services; 100% for critical paths (auth, billing, compliance)
- Tests must be deterministic. No sleep, no flakiness. Use dependency injection or test doubles for time, randomness, and external I/O.

### Test quality
- Every test must have a descriptive name that reads as a specification: `"returns 400 when source field is missing"`
- Tests must be independent and runnable in any order
- No `beforeAll` that does real network calls against production systems
- Error-path tests are mandatory: if a handler returns 4xx, test that branch
- Event-driven code must include tests for event emission, ordering-sensitive behavior where applicable, and degraded-mode or failover paths.

### Tools
- **Bun services:** `bun test` with `bun:test` (describe/it/expect)
- **Python:** `pytest` with `pytest-asyncio`, `pytest-cov` for coverage
- **Rust:** `cargo test` + `cargo nextest` for better output

---

## 6. Security Standards (OWASP Top 10 baseline)

- **No hardcoded secrets** in source code. All secrets via environment variables.
- **Authentication:** all non-health endpoints must require authentication in production. MVP shells may skip this with a documented TODO and a tracking issue.
- **Input validation** (see API Design Standards above) — prevents injection attacks.
- **Dependency scanning:** `bun audit` / `npm audit` / `cargo audit` / `pip-audit` must pass with no high/critical vulnerabilities.
- **Content security:** `Content-Type` headers on all responses prevent MIME sniffing attacks.
- **CORS:** explicit allowlist, never wildcard `*` for credentialed requests.

---

## 7. Code Quality Standards

### General
- **Functions:** single responsibility. Maximum ~40 lines. If a function does more than one thing, extract.
- **No magic numbers or strings** — use named constants.
- **No dead code.** Unused variables, imports, and functions must be removed. Linters enforce this.
- **Comments explain WHY, not WHAT.** Code should be self-documenting; comments are for non-obvious decisions.

### Naming
- **TypeScript/JS:** `camelCase` for variables and functions, `PascalCase` for types/interfaces/classes, `UPPER_SNAKE_CASE` for constants.
- **Python:** `snake_case` for everything except classes (`PascalCase`) and constants (`UPPER_SNAKE_CASE`).
- **Rust:** follow Rust API guidelines — `snake_case` for functions and variables, `PascalCase` for types, `SCREAMING_SNAKE_CASE` for constants.

### Error handling
- **Never swallow errors silently.** Every caught error must be either logged, re-thrown, or explicitly mapped to a response.
- **Never use empty catch blocks.**
- Errors must carry context: wrap errors with the operation that failed and relevant input identifiers.
- Prefer state machines, event handlers, and explicit transition boundaries over giant imperative orchestration functions.

---

## 8. Repository Standards

### Every project must have
- `README.md` — purpose, quickstart (≤5 commands), endpoint table, architecture note
- `AGENTS.md` — engineering standards for this project, conventions, and AI development rules
- `package.json` / `pyproject.toml` / `Cargo.toml` — with `lint`, `format`, `test`, and `typecheck` scripts
- `tsconfig.json` / `mypy.ini` / `clippy.toml` — linter/type-checker config
- `biome.json` (TypeScript) / `.ruff.toml` (Python) — formatter/linter config
- For ecosystem-aware services, `AGENTS.md` must describe how the repo participates in the Nexus-Cloud control plane and what event-driven or local-first patterns it is expected to preserve.

### Git hygiene
- Branch names: `feat/`, `fix/`, `chore/`, `refactor/` prefixes
- Commit messages: conventional commits (`feat:`, `fix:`, `chore:`, `test:`, `docs:`)
- No `console.log`, debugging artifacts, or `.env` files committed
- Secrets must never appear in git history

---

## 9. AI-Assisted Development Rules

When an AI agent (GitHub Copilot, Claude, or any automated tool) writes code in this ecosystem:

1. **Default to the stricter interpretation** of any ambiguous requirement.
2. **Always generate TypeScript** for Node/Bun services — never plain JavaScript.
3. **Always include types** for every function parameter, return value, and variable that cannot be inferred.
4. **Always include error-path tests** alongside happy-path tests.
5. **Always add security headers** to HTTP handlers.
6. **Always use structured logging** — never `console.log`.
7. **Never generate `any` types** or implicit `any` through untyped generics.
8. **Always validate inputs** at API boundaries using an explicit validation function or schema.
9. **Code review simulation:** before finalising output, mentally apply the checklist above. Output that would fail a code review at a top-tier engineering organisation must not be committed.
10. **Minimal surface:** do not add features, abstractions, or generalisations beyond what is directly required. Premature abstraction is a defect.
11. **Default to event-driven designs** for cross-service coordination and state propagation.
12. **Preserve local-first resilience**: generated code should continue operating in a reduced mode when upstream dependencies are slow or unavailable.

---

## Appendix A: Tool versions (pinned 2026-05-08)

| Tool | Version | Purpose |
|------|---------|---------|
| Bun | >=1.1 | TypeScript runtime + package manager |
| Biome | >=1.9 | TypeScript lint + format |
| TypeScript | >=5.4 | Type checking |
| pytest | >=8.0 | Python test runner |
| ruff | >=0.4 | Python lint + format |
| mypy | >=1.10 | Python type checking |
| cargo-audit | latest | Rust dependency scanning |
| rustfmt | stable | Rust formatting |

## Appendix B: Reference codebases within this ecosystem

| Project | Language | Maturity | Notable patterns |
|---------|----------|----------|-----------------|
| Nexus-AI | Python/FastAPI | high | structlog, OpenTelemetry, graceful shutdown, middleware stack |
| Nexus (core) | Rust/Axum | high | tower-http middleware, workspace Cargo deps, tokio |
| Nexus-Forge | TypeScript/Bun/Elysia | high | federation, VCS abstraction, SQLite, SSH |
| Nexus-Systems-API | TypeScript/Bun | medium | pinned contracts, SDK client, Bun test |
