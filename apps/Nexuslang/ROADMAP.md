# NexusLang Roadmap

## Current Phase

- Foundation hardening and regression expansion across parser, type system, and backends.

## Active Track: Range Expression Prototype Hardening

### Objective

Stabilize range expression parsing and lowering behavior before introducing additional syntax forms.

### Scope (Current Cycle)

- Canonical expression form: `range(start, stop[, step])`
- Tuple shorthand lowering: `(start, stop[, step])` to `range(...)`
- Range loop form: `for i from start to stop [by step]`
- Strict loop-step rule: `by` requires an explicit step expression

### Decision (Accepted 2026-05-11)

- Punctuation range operators `..` and `..=` are deferred for this cycle.
- Function and tuple forms remain the stabilized prototype surface.
- Decision reference: [docs/_internal/planning/range-expression-surface-rfc-2026-05.md](docs/_internal/planning/range-expression-surface-rfc-2026-05.md)

### Verification Matrix

- Parser hardening matrix: [tests/unit/compiler/test_range_expression_hardening_matrix.py](tests/unit/compiler/test_range_expression_hardening_matrix.py)
- Backend lowering matrix (C/LLVM invariants): [tests/unit/compiler/test_range_codegen_hardening_matrix.py](tests/unit/compiler/test_range_codegen_hardening_matrix.py)

### Exit Criteria

1. Both range hardening suites remain green in CI.
2. No regressions in nearby compiler hardening suites.
3. Broad compiler sweep remains green after range-track changes.

## Next Candidate Track

- Revisit punctuation operators only after precedence specification, ambiguity analysis, and full cross-component matrices are ready.

## Completed Track: Day 13 — Security and Error-Surface Hardening

### Objective

Add comprehensive negative-path test coverage for all security subsystems and harden the error formatting surface.

### Completed (2026-05-11)

- Security negative-path hardening matrix: 146 cases across path traversal, command injection, URL scheme validation, SQL injection, XSS payloads, ReDoS patterns, filename sanitization, permission boundary enforcement, taint-sink propagation (all 5 labels x all 8 sinks = 40 parametrized cases), CFI indirect-call checks, and memory safety (use-after-free, type confusion, bounds at boundary).
	- File: [tests/unit/systems/test_security_negative_paths.py](tests/unit/systems/test_security_negative_paths.py)
- Security module bug fixes:
	- `safe_execute` now detects actual newline and carriage-return characters (not only their two-char escape representations).
	- `is_safe_regex` now detects `(X+)+`, `(X*)*`, `(X|X)+`, and `(.*)+ ` ReDoS patterns.
	- File: [src/nexuslang/security/utils.py](src/nexuslang/security/utils.py)
- Error-surface hardening matrix: 48 cases covering `format_source_context` resilience (OOB line/column, empty source, large context), all error class location fields in formatted output, NxlNameError "did you mean" matching edge cases, `get_close_matches` large-list cap, and `suggest_correction` completeness.
	- File: [tests/unit/errors/test_error_surface_hardening.py](tests/unit/errors/test_error_surface_hardening.py)
- CI extended with `security-hardening` job gating `example-smoke` and downstream jobs.

### Exit Criteria Met

1. Existing security suite: 171 passed, 6 skipped.
2. New negative-path suite: 146 passed.
3. New error-surface suite: 48 passed.
4. Broad sweep (compiler + type_system + errors + systems): 2306 passed, 97 skipped, 1 xfailed, zero failures.

## Completed Track: Linux inotify Resilience Hardening

### Objective

Stabilize stdlib watcher and inotify validation under constrained host kernel limits without masking genuine product defects.

### Completed (2026-05-11)

- Watcher lifecycle hardening in [src/nexuslang/stdlib/fs_watch/__init__.py](src/nexuslang/stdlib/fs_watch/__init__.py):
  - Added robust cleanup on startup failures to avoid partially-initialized observer resource leaks.
  - Added stronger shutdown behavior (`unschedule_all`, longer join timeout, explicit observer nulling).
  - Added process-exit cleanup (`atexit`) to stop residual watchers.
- fs-watch test hardening in [tests/unit/stdlib/test_fs_watch.py](tests/unit/stdlib/test_fs_watch.py):
  - Added host-capacity-aware skip path for `ENOSPC`/`EMFILE` inotify exhaustion conditions.
  - Added autouse watcher-registry hygiene fixture (`fs_watch_stop_all` before/after each test).
  - Updated start calls to use shared guarded helper.
  - Updated concurrent thread test to avoid raising pytest skip exceptions from worker threads.
- Raw inotify test hardening in [tests/unit/stdlib/test_platform_linux.py](tests/unit/stdlib/test_platform_linux.py):
  - Added guarded helpers for `inotify_create` and `inotify_add_watch` with skip-on-capacity exhaustion semantics.

### Exit Criteria Met

1. Previously failing focus suites now stable: 19 passed, 48 skipped, 0 failed.
2. Broader stdlib sweep remains green: 2277 passed, 269 skipped, 0 failed.

## Next Candidate Track

- **Day 14 (Week 2 safety checkpoint)**: Compile and publish unresolved-risk register from Days 8-13. Update STATUS.md with baseline metrics. Identify any remaining open items for Week 3 tooling hardening.
- **Optional follow-up**: Add a dedicated CI job for Linux fs_watch/inotify resilience matrices so host-resource edge cases remain explicitly covered.