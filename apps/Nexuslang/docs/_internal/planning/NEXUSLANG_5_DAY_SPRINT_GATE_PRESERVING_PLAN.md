# Nexuslang 5-Day Sprint Plan (Gate-Preserving)

## Purpose
Compress the 4-week execution plan into a 5-day sprint for faster iteration while preserving the same acceptance gates and evidence standards.

## Gate Preservation Rule
This sprint does not relax quality gates. It only compresses execution scope and sequencing.

Hard gates preserved:
- No contradictory maturity claims across canonical status documents.
- CI-encoded, measurable gates for tests, coverage, LSP smoke, and perf smoke.
- Critical-path correctness and safety validation for parser, interpreter, type system, FFI, asm.
- LSP feature stability on core navigation and diagnostics paths.
- Domain-balanced proof trajectory maintained.

## Canonical Files for This Sprint
- docs/_internal/project-status/ROADMAP.md
- docs/_internal/status-reports/STATUS.md
- README.md
- .github/workflows/ci.yml
- scripts/check_coverage_thresholds.py
- scripts/ci_lsp_smoke.py
- scripts/ci_perf_smoke.py
- src/nlpl/parser/parser.py
- src/nlpl/parser/lexer.py
- src/nlpl/interpreter/interpreter.py
- src/nlpl/typesystem/typechecker.py
- src/nlpl/typesystem/type_inference.py
- src/nlpl/typesystem/borrow_checker.py
- src/nlpl/typesystem/lifetime_checker.py
- src/nlpl/compiler/ffi.py
- src/nlpl/compiler/ffi_advanced.py
- src/nlpl/compiler/ffi_abi_checker.py
- src/nlpl/compiler/backends/llvm_ir_generator.py
- src/nlpl/lsp/server.py
- src/nlpl/lsp/workspace_index.py
- src/nlpl/lsp/definitions.py
- src/nlpl/lsp/references.py
- tests/unit/
- tests/integration/
- tests/tooling/

## Day-by-Day Sprint

### Day 1: Canonical Truth and Gate Declaration
Tasks:
- Execute and verify canonical maturity alignment across roadmap, status, and README.
- Declare gate thresholds and evidence paths in planning docs and CI.
- Update repository links and public references to current project location.

Files:
- docs/_internal/project-status/ROADMAP.md
- docs/_internal/status-reports/STATUS.md
- README.md
- .github/workflows/ci.yml
- docs/_internal/planning/current_priorities.md

Acceptance criteria:
- Shared Release Truth Matrix exists in all three canonical docs.
- No contradictory release-readiness wording remains in those docs.
- CI gate locations are explicit and traceable.

### Day 2: Correctness and Safety Hotspot Hardening
Tasks:
- Fix highest-severity parser and type-system regressions.
- Add missing edge-case tests that reproduce known failures first.
- Harden borrow/lifetime diagnostics where failures are ambiguous.

Files:
- src/nlpl/parser/parser.py
- src/nlpl/parser/lexer.py
- src/nlpl/typesystem/typechecker.py
- src/nlpl/typesystem/type_inference.py
- src/nlpl/typesystem/borrow_checker.py
- src/nlpl/typesystem/lifetime_checker.py
- tests/unit/compiler/test_parser_corpus.py
- tests/unit/type_system/
- tests/unit/memory/test_borrow_checker.py
- tests/unit/memory/test_lifetime_checker.py

Acceptance criteria:
- New regression tests are added before corresponding fixes.
- All newly added parser/type/memory tests pass.
- Diagnostics include actionable location and reason.

### Day 3: FFI, Asm, Runtime Risk Closure
Tasks:
- Validate FFI marshalling and ABI checks on critical paths.
- Harden inline asm invalid-constraint diagnostics.
- Run and stabilize runtime concurrency/promise-sensitive tests.

Files:
- src/nlpl/compiler/ffi.py
- src/nlpl/compiler/ffi_advanced.py
- src/nlpl/compiler/ffi_abi_checker.py
- src/nlpl/compiler/backends/llvm_ir_generator.py
- src/nlpl/interpreter/interpreter.py
- tests/unit/memory/test_ffi_safety.py
- tests/unit/memory/test_ffi_advanced.py
- tests/unit/systems/test_cpu_control.py
- tests/unit/runtime/test_async.py
- tests/unit/runtime/test_promise.py

Acceptance criteria:
- FFI safety tests pass on Linux.
- Invalid asm constraints fail with clear diagnostics.
- No flaky failures in runtime-critical test subset across 3 consecutive runs.

### Day 4: LSP and CI Signal Quality
Tasks:
- Stabilize cross-file navigation and references behavior.
- Ensure hover/signature and diagnostics regressions are covered by tests.
- Tighten CI summaries so failures are directly actionable.

Files:
- src/nlpl/lsp/server.py
- src/nlpl/lsp/workspace_index.py
- src/nlpl/lsp/definitions.py
- src/nlpl/lsp/references.py
- scripts/ci_lsp_smoke.py
- scripts/check_coverage_thresholds.py
- .github/workflows/ci.yml
- tests/tooling/test_cross_file_navigation.py
- tests/tooling/test_lsp_goto_definition.py
- tests/tooling/test_lsp_document_features.py

Acceptance criteria:
- LSP core tooling tests pass.
- LSP smoke latency meets configured threshold.
- CI reports explicitly identify gate failure source and threshold.

### Day 5: Gate Run, Baseline Capture, Next Sprint Cut
Tasks:
- Execute full gate run and capture baseline metrics.
- Record unresolved risks with owners and severity.
- Publish next 5-day sprint backlog ranked by impact and risk.

Files:
- docs/_internal/status-reports/STATUS.md
- docs/_internal/planning/current_priorities.md
- docs/_internal/planning/implementation_roadmap.md
- .github/workflows/ci.yml
- scripts/ci_perf_smoke.py

Acceptance criteria:
- Full gate run passes or failures are formally triaged with owner and mitigation.
- Baseline metrics captured: test pass, coverage by component, lsp smoke latency, perf smoke runtime.
- Next sprint backlog is prioritized and executable.

## Daily Command Pack
- pytest tests/ -q --maxfail=1 --disable-warnings --timeout=30
- python scripts/check_coverage_thresholds.py --json coverage.json --no-fail
- python scripts/ci_lsp_smoke.py --sample examples/01_basic_concepts.nlpl --max-latency-ms 500 --output lsp-smoke.json
- python scripts/ci_perf_smoke.py --sample test_programs/unit/basic/test_hello.nlpl --threshold-ms 1500 --output perf-smoke.json

## Definition of Done for This Sprint
- Canonical truth remains aligned and contradiction-free.
- Same hard gates as 4-week plan remain in force.
- At least one critical risk area is fully closed with tests and diagnostics.
- Next sprint starts from measured baselines, not assumptions.
