# Nexuslang 4-Week Execution Plan (Apr 29 to May 26, 2026)

## Objective
Drive Nexuslang toward production-grade general-purpose systems language maturity by executing a strict 28-day plan focused on consistency, correctness, tooling hardening, performance, and domain-balanced proof projects.

## Scope and Constraints
- Keep language direction universal across domains. Avoid domain-specialized framing.
- Prioritize correctness and stability over adding syntax.
- Every day ends with measurable acceptance criteria.
- No placeholder implementations.

## Baseline Files Used by This Plan
- Status and roadmap:
  - docs/_internal/project-status/ROADMAP.md
  - docs/_internal/status-reports/STATUS.md
  - README.md
- Core compiler and runtime:
  - src/nlpl/parser/lexer.py
  - src/nlpl/parser/parser.py
  - src/nlpl/parser/ast.py
  - src/nlpl/interpreter/interpreter.py
  - src/nlpl/runtime/runtime.py
  - src/nlpl/errors.py
- Type and memory safety:
  - src/nlpl/typesystem/typechecker.py
  - src/nlpl/typesystem/type_inference.py
  - src/nlpl/typesystem/borrow_checker.py
  - src/nlpl/typesystem/lifetime_checker.py
- LSP and tooling:
  - src/nlpl/lsp/server.py
  - src/nlpl/lsp/workspace_index.py
  - src/nlpl/lsp/definitions.py
  - src/nlpl/lsp/references.py
  - src/nlpl/lsp/hover.py
  - src/nlpl/lsp/signature_help.py
  - src/nlpl/lsp/code_lens.py
  - scripts/ci_lsp_smoke.py
  - scripts/ci_perf_smoke.py
  - scripts/check_coverage_thresholds.py
  - .github/workflows/ci.yml
- Test suites:
  - tests/unit/
  - tests/integration/
  - tests/tooling/
  - tests/fuzz/
  - test_programs/unit/
  - test_programs/integration/
  - test_programs/regression/

## Weekly Outcomes
- Week 1: Single source of truth and hard release gates established.
- Week 2: Correctness and safety hardening complete for parser, type, runtime, FFI, asm.
- Week 3: Tooling and performance gates enforced in CI.
- Week 4: Three showcase projects and release-readiness report.

## Day-by-Day Plan

### Week 1: Truth Alignment and Release Gates

Day 1
- Tasks:
  - Create a canonical status matrix and reconcile contradictions.
  - Define one authoritative release state field set.
- Files:
  - docs/_internal/project-status/ROADMAP.md
  - docs/_internal/status-reports/STATUS.md
  - README.md
- Acceptance criteria:
  - No contradictory maturity statements remain across these files.
  - A shared section named Release Truth Matrix appears in all three.

Day 2
- Tasks:
  - Define v1 hard gates for correctness, safety, tooling, and performance.
  - Add explicit pass/fail thresholds.
- Files:
  - docs/_internal/planning/current_priorities.md
  - docs/_internal/planning/comprehensive_development_plan.md
  - .github/workflows/ci.yml
- Acceptance criteria:
  - A single gate table exists with numeric thresholds for coverage, smoke latency, and perf smoke.
  - CI references gate-enforcing scripts only, no vague checks.

Day 3
- Tasks:
  - Build parser conformance backlog from existing failures and edge cases.
  - Add missing parser regression fixtures.
- Files:
  - src/nlpl/parser/parser.py
  - src/nlpl/parser/lexer.py
  - tests/unit/compiler/test_parser_corpus.py
  - test_programs/regression/error_tests/
- Acceptance criteria:
  - New parser regressions are reproducible by tests before fixes.
  - At least 10 new parser edge-case tests added.

Day 4
- Tasks:
  - Fix highest-severity parser ambiguities and recovery bugs.
  - Improve error diagnostics for those paths.
- Files:
  - src/nlpl/parser/parser.py
  - src/nlpl/errors.py
  - tests/unit/errors/test_error_reporting.py
- Acceptance criteria:
  - All Day 3 parser regressions pass.
  - Error messages include line and column and one actionable suggestion.

Day 5
- Tasks:
  - Create runtime determinism checklist for nondeterministic behaviors.
  - Add deterministic test mode where applicable.
- Files:
  - src/nlpl/runtime/runtime.py
  - tests/unit/runtime/test_async.py
  - tests/unit/runtime/test_promise.py
- Acceptance criteria:
  - Flaky runtime tests are identified and tagged.
  - Deterministic mode exists for affected paths or documented as blocked with issue link.

Day 6
- Tasks:
  - Stabilize existing CI failures and remove test flakiness in critical paths.
- Files:
  - .github/workflows/ci.yml
  - tests/unit/
  - tests/integration/
- Acceptance criteria:
  - Three consecutive local full test runs complete with zero flaky failures.

Day 7
- Tasks:
  - Week 1 consolidation and gate report.
- Files:
  - docs/_internal/status-reports/STATUS.md
  - docs/_internal/planning/current_priorities.md
- Acceptance criteria:
  - Week 1 report records baseline metrics: pass rate, coverage, lsp smoke latency, perf smoke runtime.

### Week 2: Correctness and Safety Hardening

Day 8
- Tasks:
  - Type checker gap audit and bug backlog creation.
- Files:
  - src/nlpl/typesystem/typechecker.py
  - src/nlpl/typesystem/type_inference.py
  - tests/unit/type_system/
- Acceptance criteria:
  - Prioritized bug list with severity and owner exists.
  - At least 8 missing type edge-case tests added.

Day 9
- Tasks:
  - Fix soundness issues in inference/checker interaction.
- Files:
  - src/nlpl/typesystem/typechecker.py
  - src/nlpl/typesystem/type_inference.py
  - tests/unit/type_system/test_type_inference.py
  - tests/unit/type_system/test_bidirectional_inference.py
- Acceptance criteria:
  - All newly added type tests pass.
  - No regression in existing type suite.

Day 10
- Tasks:
  - Harden borrow and lifetime diagnostics.
- Files:
  - src/nlpl/typesystem/borrow_checker.py
  - src/nlpl/typesystem/lifetime_checker.py
  - tests/unit/memory/test_borrow_checker.py
  - tests/unit/memory/test_lifetime_checker.py
- Acceptance criteria:
  - Borrow/lifetime errors include source location and conflict reason.
  - Memory safety tests pass in full.

Day 11
- Tasks:
  - FFI safety validation: pointer, struct marshalling, variadic boundaries.
- Files:
  - src/nlpl/compiler/ffi.py
  - src/nlpl/compiler/ffi_advanced.py
  - src/nlpl/compiler/ffi_abi_checker.py
  - tests/unit/memory/test_ffi_safety.py
  - tests/unit/memory/test_ffi_advanced.py
- Acceptance criteria:
  - FFI tests pass on Linux.
  - Undefined behavior paths emit explicit runtime/type errors.

Day 12
- Tasks:
  - Inline assembly constraints and diagnostics hardening.
- Files:
  - src/nlpl/compiler/backends/llvm_ir_generator.py
  - src/nlpl/compiler/optimizer.py
  - tests/unit/systems/test_cpu_control.py
  - tests/unit/systems/test_kernel_primitives.py
- Acceptance criteria:
  - Invalid asm constraints fail with clear diagnostics.
  - Existing asm tests continue passing.

Day 13
- Tasks:
  - Security and error-surface hardening pass.
- Files:
  - src/nlpl/security/
  - src/nlpl/safety/
  - src/nlpl/errors.py
  - tests/unit/systems/test_security.py
  - tests/unit/systems/test_security_hardening.py
- Acceptance criteria:
  - Security test suite passes.
  - New negative-path tests added for unsafe operations.

Day 14
- Tasks:
  - Week 2 safety checkpoint and unresolved-risk register.
- Files:
  - docs/_internal/status-reports/STATUS.md
  - docs/_internal/assessments/
- Acceptance criteria:
  - All unresolved risks have owner, severity, and mitigation.

### Week 3: Tooling and Performance Enforcement

Day 15
- Tasks:
  - LSP feature parity audit against current test suite.
- Files:
  - src/nlpl/lsp/server.py
  - tests/tooling/test_lsp_features_check.py
  - tests/tooling/test_lsp_integration_real.py
- Acceptance criteria:
  - Feature matrix exists with tested and untested capabilities.

Day 16
- Tasks:
  - Cross-file navigation hardening.
- Files:
  - src/nlpl/lsp/workspace_index.py
  - src/nlpl/lsp/definitions.py
  - src/nlpl/lsp/references.py
  - tests/tooling/test_cross_file_navigation.py
  - tests/tooling/test_lsp_goto_definition.py
- Acceptance criteria:
  - Cross-file definition and references pass all tests.
  - No unresolved symbol regressions in mixed-workspace sample.

Day 17
- Tasks:
  - Improve hover, signature help, and code lens stability.
- Files:
  - src/nlpl/lsp/hover.py
  - src/nlpl/lsp/signature_help.py
  - src/nlpl/lsp/code_lens.py
  - tests/tooling/test_lsp_document_features.py
  - tests/tooling/test_lsp_code_lens.py
- Acceptance criteria:
  - LSP document feature tests pass.
  - Latency remains under smoke thresholds.

Day 18
- Tasks:
  - Compiler and interpreter profiling pass; establish baseline snapshots.
- Files:
  - dev_tools/profile_interpreter.py
  - dev_tools/profile_compiler.py
  - dev_tools/benchmark_interpreter.py
  - scripts/ci_perf_smoke.py
- Acceptance criteria:
  - Baseline profile artifacts checked into docs/_internal/status-reports/.
  - Perf smoke thresholds updated based on measured stable baseline.

Day 19
- Tasks:
  - CI hardening for failure clarity and artifact retention.
- Files:
  - .github/workflows/ci.yml
  - scripts/check_coverage_thresholds.py
  - scripts/ci_lsp_smoke.py
  - scripts/ci_perf_smoke.py
- Acceptance criteria:
  - CI publishes clear summaries for coverage, lsp smoke, perf smoke.
  - Gate failures return actionable messages.

Day 20
- Tasks:
  - Coverage strategy update to prioritize critical components.
- Files:
  - scripts/check_coverage_thresholds.py
  - tests/unit/compiler/
  - tests/unit/interpreter/
  - tests/unit/type_system/
- Acceptance criteria:
  - Component thresholds defined for parser, interpreter, type system, lsp.
  - Coverage gates pass locally with documented command.

Day 21
- Tasks:
  - Week 3 release-train dry run.
- Files:
  - .github/workflows/ci.yml
  - docs/_internal/status-reports/STATUS.md
- Acceptance criteria:
  - Full local test and smoke run completes without manual patching.
  - Dry-run report records timings and bottlenecks.

### Week 4: General-Purpose Proof and Release Readiness

Day 22
- Tasks:
  - Implement Showcase A: business and data workflow CLI.
- Files:
  - examples/
  - test_programs/integration/
  - docs/tutorials/
- Acceptance criteria:
  - One 500+ line showcase with tests and tutorial.
  - Uses io, collections, parsing, error handling.

Day 23
- Tasks:
  - Implement Showcase B: scientific or numeric processing workflow.
- Files:
  - examples/
  - test_programs/integration/
  - docs/tutorials/
- Acceptance criteria:
  - One 500+ line showcase with benchmarks and tests.
  - Uses math, iterators, validation, profiling.

Day 24
- Tasks:
  - Implement Showcase C: systems utility with low-level interop.
- Files:
  - examples/
  - test_programs/integration/
  - docs/tutorials/
- Acceptance criteria:
  - One 500+ line showcase with FFI or asm usage and safety tests.
  - Includes documented fallback behavior on unsupported platforms.

Day 25
- Tasks:
  - Domain balance and language-agnostic messaging review.
- Files:
  - README.md
  - docs/getting-started/
  - docs/guide/
- Acceptance criteria:
  - Examples and messaging remain balanced across business, data, scientific, web, systems.
  - No single-domain positioning language remains.

Day 26
- Tasks:
  - Final bug bash focused on release blockers.
- Files:
  - src/nlpl/
  - tests/
  - test_programs/
- Acceptance criteria:
  - All blocker-severity issues closed or explicitly deferred with rationale.

Day 27
- Tasks:
  - Release candidate validation run.
- Files:
  - .github/workflows/ci.yml
  - scripts/
  - docs/_internal/status-reports/STATUS.md
- Acceptance criteria:
  - Full local run passes: tests, lsp smoke, perf smoke, coverage gates.
  - RC checklist is fully marked with evidence links.

Day 28
- Tasks:
  - Publish 4-week outcome report and next 8-week continuation plan.
- Files:
  - docs/_internal/status-reports/STATUS.md
  - docs/_internal/planning/current_priorities.md
  - docs/_internal/planning/implementation_roadmap.md
- Acceptance criteria:
  - Report contains metrics delta from Day 1 baseline.
  - Next phase backlog is prioritized by risk and impact.

## Daily Execution Command Set
Run this at end of each day and paste outcomes into the daily log section of STATUS.md.

- pytest tests/ -q --maxfail=1 --disable-warnings --timeout=30
- python scripts/check_coverage_thresholds.py --json coverage.json --no-fail
- python scripts/ci_lsp_smoke.py --sample examples/01_basic_concepts.nlpl --max-latency-ms 500 --output lsp-smoke.json
- python scripts/ci_perf_smoke.py --sample test_programs/unit/basic/test_hello.nlpl --threshold-ms 1500 --output perf-smoke.json

## Definition of Done for This 4-Week Plan
- One consistent maturity narrative across roadmap, status, and README.
- All hard gates encoded in CI and passing on Linux in local and CI runs.
- Parser, type, runtime, FFI, asm critical-path tests green and non-flaky.
- LSP features stable with measured latency under threshold.
- Three domain-balanced showcase applications shipped with tests and docs.
- A continuation roadmap is published with quantified baseline improvements.
