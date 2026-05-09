# NexusLang Feature Gap Implementation Tracker

Updated: 2026-05-07 (C parallel capture transport + C roundtrip integration tests + telemetry showcase + parser end-alias parity + class/method end-alias parity + C generator-expression foreach materialization + C generator local-identifier source parity + C generator metadata-bounded non-local identifier parity + C generator metadata alias resolution + strict type diagnostics)

Purpose: Convert every matrix row with ⚠️ or ❌ into concrete implementation tasks with file-level targets.

## How To Use This Tracker

- Keep this file in sync with `docs/FEATURE_SUPPORT_MATRIX.md`.
- When a task is complete, switch `[ ]` to `[x]` and add a short completion note.
- Each task should be implemented with tests in `tests/` and at least one NLPL fixture in `test_programs/` when applicable.

## Priority Queue (Execution Order)

1. Generators/yield deep semantic parity
2. Parallel-for true parallel lowering (beyond sequential fallback)
3. Contracts/assertions diagnostics + matcher depth
4. Pattern matching advanced parity depth (Option/Result runtime-backed execution)
5. Interfaces/traits/generics specialization + diagnostics depth

## Recent Completions

- [x] Parser end-alias parity for class/method terminators
  - Completed 2026-05-07: Added parser consume-path parity for lexer aliases `end the class` and `end the method`, and added dedicated parser regression tests for both forms.
  - Files: `src/nexuslang/parser/parser.py`, `tests/unit/compiler/test_parser.py`
  - Validation: `tests/unit/compiler/test_parser.py`, `tests/unit/compiler/test_parser_corpus.py`

## Showcase Applications (Capability Proof)

- [x] Async/Channels/Parallel pipeline demo
  - Completed 2026-05-04: Created `examples/23_async_channels_parallel_pipeline.nlpl` demonstrating integrated async tasks, channel-based inter-task communication, and parallel processing of sensor data with local/global aggregation.
  - Files: `examples/23_async_channels_parallel_pipeline.nlpl`, `examples/README_SHOWCASE.md`
  - Showcases: natural language syntax, struct types, async functions, channels, parallel-for, task spawning, and await semantics in a realistic IoT data pipeline.

- [x] Financial portfolio analyzer (domain: finance)
  - Completed 2026-05-04: Created `examples/financial/portfolio_analyzer.nlpl` demonstrating comprehensive investment analysis with P&L calculations, risk metrics (volatility), and contract-driven development.
  - Files: `examples/financial/portfolio_analyzer.nlpl`, `examples/financial/README_PORTFOLIO.md`
  - Showcases: Struct-based data modeling, mathematical computations, contracts (`require` statements), type-safe float calculations, collection iteration, pattern matching for recommendations.
  - Features: Position valuation, gain/loss calculation, portfolio aggregation, top/bottom performer identification, volatility assessment, recommendation engine.
  - Domain Value: Demonstrates NexusLang for financial computing and analysis tools.

- [x] System log analyzer (domain: systems/DevOps)
  - Completed 2026-05-04: Created `examples/systems/log_analyzer.nlpl` demonstrating comprehensive log processing with parsing, filtering, statistics, and health assessment.
  - Files: `examples/systems/log_analyzer.nlpl`, `examples/systems/README_LOG_ANALYZER.md`
  - Showcases: String parsing and extraction, pattern matching for severity classification, multi-return functions, data aggregation, contract validation, formatted reporting.
  - Features: Log line parsing, level classification, error/warning filtering, statistics aggregation (counts, percentages), health assessment, multi-entry reporting.
  - Domain Value: Demonstrates NexusLang for DevOps utilities and system administration tools.

- [x] Message broker (domain: concurrency/distributed systems)
  - Completed 2026-05-04: Created `examples/concurrency/message_broker.nlpl` demonstrating pub/sub messaging with channels, async functions, and task spawning.
  - Files: `examples/concurrency/message_broker.nlpl`, `examples/concurrency/README_MESSAGE_BROKER.md`
  - Showcases: Channel creation and communication, async function definitions, task spawning patterns, fan-out routing, topic-based filtering, concurrent orchestration.
  - Features: Message routing, publisher/subscriber coordination, topic-specific subscribers, concurrent task management, contracts for reliability.
  - Domain Value: Demonstrates NexusLang for building distributed systems and event-driven architectures.

- [x] Graph analyzer (domain: algorithms/data structures)
  - Completed 2026-05-04: Created `examples/algorithms/graph_analyzer.nlpl` demonstrating graph algorithms with pathfinding, centrality analysis, and connectivity metrics.
  - Files: `examples/algorithms/graph_analyzer.nlpl`, `examples/algorithms/README_GRAPH_ANALYZER.md`
  - Showcases: Complex data structure modeling (nodes, edges), algorithm implementation (BFS, centrality), contract validation, collection iteration, result aggregation.
  - Features: Shortest path finding, node centrality scoring, connectivity analysis, neighbor discovery, graph traversal.
  - Domain Value: Demonstrates NexusLang for algorithm development and data structure handling.

- [x] Spam classifier (domain: machine learning/data science)
  - Completed 2026-05-05: Created `examples/ml/spam_classifier.nlpl` demonstrating complete ML pipeline with feature extraction, model training, and classification metrics.
  - Files: `examples/ml/spam_classifier.nlpl`, `examples/ml/README_SPAM_CLASSIFIER.md`
  - Showcases: Feature extraction, linear classification, model training, metric calculation (TP/FP/TN/FN, accuracy, precision, recall, F1-score), contract-driven development.
  - Features: Email classification, suspicious word detection, text feature engineering, scoring functions, training with data adaptation, comprehensive evaluation metrics.
  - Domain Value: Demonstrates NexusLang for machine learning and data science applications.

- [x] Additional domain showcases (planned)
  - Completed 2026-05-05: Closed remaining planned domain showcase coverage by adding dedicated game and embedded examples, and anchoring existing web/ML/graph examples as canonical references.
  - Machine Learning (classification example): `examples/ml/spam_classifier.nlpl`
  - Web Services (HTTP server with routing): `examples/http_rest_api_server.nxl`, `examples/web_backend/rest_api_users.nxl`
  - Graph Processing (network analysis): `examples/algorithms/graph_analyzer.nlpl`
  - Game Development (simple game loop): `examples/game/simple_game_loop.nxl`, `examples/game/README_SIMPLE_GAME_LOOP.md`
  - Embedded Systems (sensor driver): `examples/embedded/sensor_driver.nxl`, `examples/embedded/README_SENSOR_DRIVER.md`

## Showcase Applications (Capability Proof)## Task Backlog By Matrix Row

### 1) Classes / Methods / Inheritance (Tooling ⚠️, Grammar ⚠️)

- [x] Add class-aware symbol hierarchy in LSP document/workspace symbols.
  - Completed 2026-05-03: Implemented dotted-scope hierarchy assembly for document symbols so nested symbols (class -> method -> parameters/control-flow children) are attached to the correct parent chain.
  - Files: `src/nexuslang/lsp/server.py`, `src/nexuslang/lsp/workspace_index.py`, `src/nexuslang/lsp/symbols.py`
  - Tests: `tests/tooling/test_lsp_document_features.py`
- [x] Improve method override navigation and references (base/derived linking).
  - Completed 2026-05-03: Added override-aware goto-definition for class methods (derived -> nearest base declaration), override-family reference expansion across base/derived declarations, and deterministic deduplication of merged reference sets.
  - Files: `src/nexuslang/lsp/definitions.py`, `src/nexuslang/lsp/references.py`
  - Tests: `tests/tooling/test_lsp_enhancements.py`
- [x] Align grammar coverage for class members, inheritance clauses, and modifiers.
  - Files: `grammar/NLPL.g4`, `docs/reference/syntax-grammar.md`
  - Tests: `tests/unit/compiler/test_parser.py`

### 2) Interfaces / Traits / Generics (Typechecker ⚠️, Compiler ⚠️, Tooling ⚠️, Grammar ⚠️)

- [x] Enforce trait/interface conformance diagnostics with precise member mismatch output.
  - Completed 2026-05-05: Implemented `get_trait_conformance_diagnostics()` method in TypeChecker returning comprehensive diagnostic information including missing methods, incompatible signatures, and actionable suggestions.
  - Files: `src/nexuslang/typesystem/typechecker.py`, `docs/reference/trait-conformance-diagnostics.md`
  - Tests: `tests/unit/type_system/test_trait_conformance_diagnostics.py` (10 tests, 100% passing)
  - Coverage: Missing method detection, parameter count mismatches, return type incompatibilities, diagnostic summary generation, trait/class resolution errors.
- [x] Implement generic constraint propagation through calls and method dispatch.
  - Completed 2026-05-04: Added `_infer_generic_substitutions`, `_type_satisfies_constraint`, and `_check_generic_constraints` methods to `TypeChecker`. At every generic function call site, concrete argument types are unified with the function's type parameters; each resolved substitution is then validated against declared trait/interface constraints (Comparable, Numeric, Equatable, Printable, custom trait-method sets, etc.). The existing argument-compatibility check was extended to skip `GenericParameter` positions (letting the new constraint pass handle them). 33 new tests added covering substitution inference, constraint satisfaction for primitives and class types, multi-constraint combinations, and end-to-end wiring through `check_function_call`.
  - Files: `src/nexuslang/typesystem/typechecker.py`
  - Tests: `tests/unit/type_system/test_bidirectional_inference.py` (34 tests total, 33 new)
- [x] Lower generic specializations consistently in LLVM/C backends.
  - Completed 2026-05-05: Added C backend generic specialization parity with LLVM. C generator now collects generic templates, resolves explicit/inferred type arguments at call sites, mangles concrete specialization names (`func_TypeA_TypeB`), materializes substituted specialized function definitions, and stabilizes pending specialization emission across discovery passes. Added LLVM/C parity tests for explicit and inferred specialization lowering.
  - Files: `src/nexuslang/compiler/backends/c_generator.py`, `src/nexuslang/compiler/backends/llvm_ir_generator.py`
  - Tests: `tests/unit/compiler/test_llvm_codegen.py`, `tests/unit/compiler/test_c_codegen.py`, `tests/unit/compiler/`
- [x] Add LSP hover/completion support for generic parameters and constraints.
  - Completed 2026-05-03: Added generic-aware hover rendering for function/method signatures (`<T: Bound>` and `where` constraints) plus completion contexts for generic parameter templates and where-clause trait bounds.
  - Files: `src/nexuslang/lsp/hover.py`, `src/nexuslang/lsp/completions.py`
  - Tests: `tests/tooling/test_lsp_enhancements.py`
- [x] Update grammar for generic bounds and trait composition syntax.
  - Completed 2026-05-04: Synced formal grammar and syntax reference with parser-supported generic bound forms (`T: TraitA + TraitB`) and `where`-clause constraints, and added parser regression coverage for composed and comma-separated constraints.
  - Files: `grammar/NLPL.g4`, `docs/reference/syntax-grammar.md`, `src/nexuslang/parser/parser.py`
  - Tests: `tests/unit/compiler/test_parser.py`

### 3) Extended Control Flow: switch / labels / fallthrough (Parser OK, Interpreter OK, Typechecker OK, Compiler ⚠️, Tooling ⚠️, Grammar ⚠️, Tests OK)

- [x] Complete parser AST for labels and explicit fallthrough semantics.
  - Files: `src/nexuslang/parser/ast.py`, `src/nexuslang/parser/parser.py`
  - Tests: `tests/unit/language/test_switch_statement.py`
- [x] Execute label resolution and fallthrough correctness in interpreter.
  - Files: `src/nexuslang/interpreter/interpreter.py`
  - Tests: `tests/unit/interpreter/test_control_flow.py`
- [x] Add unreachable/case-type mismatch diagnostics in typechecker.
  - Files: `src/nexuslang/typesystem/typechecker.py`
  - Tests: `tests/unit/type_system/test_typechecker.py`
- [x] Lower switch/labels/fallthrough into structured LLVM/C control-flow blocks.
  - Completed 2026-05-03: Added switch-context-aware break lowering in LLVM and labeled loop break/continue lowering in C backend to preserve nested control-flow semantics.
  - Files: `src/nexuslang/compiler/backends/llvm_ir_generator.py`, `src/nexuslang/compiler/backends/c_generator.py`
  - Tests: `tests/unit/compiler/test_switch_codegen.py`, `tests/integration/test_compiler_roundtrip.py`
- [x] Add LSP semantic tokens/outline entries for labels and switch cases.
  - Completed 2026-05-03: Added keyword semantic token emission for switch/case/default/fallthrough/label and workspace/document symbol extraction for label and switch-case outlines.
  - Files: `src/nexuslang/lsp/semantic_tokens.py`, `src/nexuslang/lsp/workspace_index.py`, `src/nexuslang/lsp/server.py`, `src/nexuslang/lsp/symbols.py`
  - Tests: `tests/tooling/test_lsp_switch_label_symbols.py`, `tests/tooling/test_lsp_document_features.py`
- [x] Align formal grammar with implemented switch variants.
  - Completed 2026-05-03: Added formal grammar coverage for `switch`, `case`, `default`, labeled loop prefixes, and labeled `break`/`continue`; synced reference syntax guide with canonical forms.
  - Files: `grammar/NLPL.g4`, `docs/reference/syntax-grammar.md`

### 4) Error Handling: try/catch, raise, panic (Typechecker ⚠️, Compiler ⚠️, Tooling ⚠️, Grammar ⚠️)

- [x] Typecheck thrown/raised values and catch variable typing.
  - Completed 2026-05-03: Enforced string-compatible raise messages, typed catch variables (default string or declared exception type), and unreachable-after-raise detection in try/catch paths.
  - Files: `src/nexuslang/typesystem/typechecker.py`
  - Tests: `tests/unit/errors/test_error_propagation.py`
- [x] Lower try/catch/raise consistently in both backends with runtime-compatible semantics.
  - Completed 2026-05-05: Verified C setjmp/longjmp lowering and LLVM invoke/landingpad lowering with executable roundtrip coverage for catch, continuation-after-catch, uncaught raise termination, and nested try/catch behavior.
  - Files: `src/nexuslang/compiler/backends/llvm_ir_generator.py`, `src/nexuslang/compiler/backends/c_generator.py`
  - Tests: `tests/unit/compiler/test_llvm_parallel_contracts.py`, `tests/unit/compiler/test_c_parallel_contracts.py`, `tests/integration/test_compiler_roundtrip.py`
- [x] Improve LSP diagnostics for exception scope and unreachable catch blocks.
  - Completed 2026-05-03: Added dedicated diagnostics pass for catch-variable scope leakage and likely unreachable catch blocks when try exits via early return before catch.
  - Files: `src/nexuslang/lsp/diagnostics.py`
  - Tests: `tests/tooling/test_lsp_exception_scope_diagnostics.py`, `tests/tooling/test_lsp_diagnostics_merge.py`
- [x] Update grammar for complete error-handling syntax parity.
  - Completed 2026-05-03: Synced formal grammar/reference docs with parser-supported `catch ... with ... as ...` bindings and `raise`/`throw` variants including optional message clauses.
  - Files: `grammar/NLPL.g4`, `docs/reference/syntax-grammar.md`

### 5) Contracts / Assertions (Typechecker ⚠️, Compiler ⚠️, Tooling ⚠️, Grammar ⚠️)

- [x] Enforce contract expression type rules and side-effect restrictions.
  - Completed 2026-05-04: Implemented `_has_side_effects` walker that rejects `VariableDeclaration`, `IndexAssignment`, `MemberAssignment`, and `DereferenceAssignment` inside contract conditions. `_check_contract_condition` enforces boolean-only type requirement on all four contract kinds (`require`, `ensure`, `guarantee`, `invariant`) and emits distinct diagnostics for side-effect violations vs. non-boolean types. Optional `message` expressions are validated as string-typed via `_check_contract_message`.
  - Files: `src/nexuslang/typesystem/typechecker.py`
  - Tests: `tests/unit/language/test_contracts.py`, `tests/unit/type_system/test_contracts_typechecker.py`
- [x] Compile `expect`, `require`, `ensure`, `invariant` with clear failure paths.
  - Completed 2026-05-04: Both LLVM and C backends emit conditional branch-to-panic/abort paths for all four contract kinds plus `expect`. LLVM uses `_emit_contract_assert` (br i1 -> contract.fail -> nxl_panic/unreachable -> contract.ok) with `_contract_message_ptr` for optional custom messages. C uses `_emit_contract_guard` (if !cond fprintf(stderr) + exit(1)) with per-kind default messages. `expect` supports equal, greater_than, less_than, be_true, be_false, be_null, contain, start_with, end_with, approximate_equal (fabs) matchers in both backends.
  - Files: `src/nexuslang/compiler/backends/llvm_ir_generator.py`, `src/nexuslang/compiler/backends/c_generator.py`
  - Tests: `tests/unit/compiler/test_llvm_codegen.py` (35 tests: LLVM require/ensure/guarantee/invariant/expect lowering + C backend parity)
- [x] Add richer LSP diagnostics and quick-fix suggestions for contract failures.
  - Completed 2026-05-03: Added contract-aware diagnostic fix metadata for boolean/mutation/message contract violations and LSP quick-fix actions to append contract message clauses, convert contract conditions to explicit boolean checks, and convert non-string contract messages to string literals.
  - Files: `src/nexuslang/lsp/diagnostics.py`, `src/nexuslang/lsp/code_actions.py`
  - Tests: `tests/tooling/test_lsp_contract_code_actions.py`, `tests/tooling/test_lsp_refactoring.py`
- [x] Align grammar contract forms and nesting rules.
  - Completed 2026-05-04: Synced contract grammar with parser-supported optional `message` payloads for `require`/`ensure`/`guarantee`/`invariant`, documented parser-level nesting rules (contracts as regular statements inside nested blocks), and added targeted parser regressions for message and nested block placement.
  - Files: `grammar/NLPL.g4`, `docs/reference/syntax-grammar.md`, `src/nexuslang/parser/parser.py`
  - Tests: `tests/unit/compiler/test_parser.py`

### 6) Ownership / Borrowing / Lifetimes (Typechecker ⚠️, Compiler ⚠️, Tooling ⚠️, Grammar ⚠️)

- [x] Consolidate borrow/lifetime checking into deterministic pass boundaries.
  - Completed 2026-05-05: Integrated ownership pipeline directly into `TypeChecker` with deterministic phase order (`BorrowChecker` then `LifetimeChecker`), configurable strict boundary behavior (`stop_on_ownership_errors`), normalized ownership diagnostics, and separate warning channels.
  - Files: `src/nexuslang/typesystem/typechecker.py`, `src/nexuslang/typesystem/borrow_checker.py`, `src/nexuslang/typesystem/lifetime_checker.py`
  - Tests: `tests/unit/memory/test_typechecker_ownership_pipeline.py`, `tests/unit/memory/test_borrow_checker.py`, `tests/unit/memory/test_lifetime_checker.py`, `tests/unit/type_system/`
- [x] Emit ownership-aware move/borrow semantics in compiled backends.
  - Completed 2026-05-05: Added explicit compiled lowering for `MoveExpression`, `BorrowExpression`, and `BorrowExpressionWithLifetime` in both LLVM and C backends so ownership syntax no longer falls through unknown-expression paths. `MoveExpression` now returns the source value and invalidates moved-from storage with a neutral value write (`0`, `0.0`, `false`, or `null`/`NULL` depending on type). `DropBorrowStatement` is now explicitly handled in statement dispatch as backend bookkeeping/no-op lowering instead of implicit fallthrough.
  - Files: `src/nexuslang/compiler/backends/llvm_ir_generator.py`, `src/nexuslang/compiler/backends/c_generator.py`
  - Tests: `tests/unit/compiler/test_llvm_codegen.py`, `tests/unit/memory/test_lifetime_checker.py`, `tests/unit/compiler/`
  - [x] Add borrow conflict diagnostics + related ranges in LSP.
  - Completed 2026-05-05: Added ownership-aware LSP diagnostic shaping (`Ownership error: ...`) for borrow/lifetime pass failures, attached `relatedInformation` ranges pointing at borrow/move/set/drop sites, and surfaced memory-focused quick-fix guidance while preserving deterministic typechecker ownership phase behavior.
  - Files: `src/nexuslang/lsp/diagnostics.py`
  - Tests: `tests/tooling/test_lsp_ownership_diagnostics.py`, `tests/tooling/test_lsp_diagnostics_merge.py`, `tests/tooling/test_lsp_diagnostic_payload.py`
- [x] Update grammar and reference docs for ownership syntax forms.
  - Completed 2026-05-04: Added ownership grammar rules to `grammar/NLPL.g4` (moveExpression, borrowExpression, lifetimeAnnotation, dropBorrowStatement, rcCreation, arcCreation, downgradeExpression, upgradeExpression) and lexer token declarations (ARC, BORROW, DOWNGRADE, DROP, LIFETIME, MOVE, MUTABLE, RC, UPGRADE). Added Ownership/Borrow/Lifetime section to `docs/reference/syntax-grammar.md` with grammar forms and typechecker enforcement notes. Extended `docs/guide/type-system.md` with Ownership/Borrow Semantics section.
  - Files: `grammar/NLPL.g4`, `docs/reference/syntax-grammar.md`, `docs/guide/type-system.md`
  - Tests: `tests/unit/compiler/test_parser.py` (TestOwnershipSyntax, 6 tests)

### 7) Macros / Comptime (Typechecker ✅, Compiler ✅, Tooling ⚠️, Grammar ⚠️, Tests ✅)

- [x] Define typechecker rules for macro expansion results and comptime expression typing.
  - Completed 2026-05-02: Added comptime/macro statement checking coverage in the typechecker, including comptime expression/const/assert handling and macro-aware flow through compiler-facing semantics.
  - Files: `src/nexuslang/typesystem/typechecker.py`, `src/nexuslang/parser/ast.py`
  - Tests: `tests/unit/language/test_comptime_typechecker.py`, `tests/unit/type_system/test_typechecker.py`
- [x] Implement compiler lowering for compile-time evaluation and expansion barriers.
  - Completed 2026-05-02: Added LLVM/C lowering for comptime expression/const/assert, top-level initialization ordering, macro definition/expansion support, and hygiene-sensitive expansion behavior.
  - Files: `src/nexuslang/compiler/backends/llvm_ir_generator.py`, `src/nexuslang/compiler/backends/c_generator.py`
  - Tests: `tests/unit/compiler/test_comptime_codegen.py`, `tests/unit/compiler/test_macro_codegen.py`, `tests/integration/test_compiler_roundtrip.py`
- [x] Add macro/comptime awareness to hover, completion, and diagnostics.
  - Completed 2026-05-02: Added macro/comptime keyword docs in hover, context-aware completion support for `expand` + `comptime` forms, and diagnostics for undefined macro expansion + comptime const reassignment.
  - Files: `src/nexuslang/lsp/hover.py`, `src/nexuslang/lsp/completions.py`, `src/nexuslang/lsp/diagnostics.py`
  - Tests: `tests/tooling/test_lsp_enhancements.py`, `tests/tooling/test_lsp_macro_comptime_diagnostics.py`
- [x] Expand grammar for macro definitions, hygienic params, and comptime blocks.
  - Completed 2026-05-05: Added parser-aligned `macroDefinition`, `macroExpansion`, and `comptimeStatement` productions to the reference grammar, including explicit `comptime eval|const|assert` forms and bare-expression fallback. Added macro/comptime syntax and hygiene notes to the syntax reference and parser-corpus regression tests covering macro parameter lists, expansion arguments, and comptime node forms.
  - Files: `grammar/NLPL.g4`, `docs/reference/syntax-grammar.md`, `tests/unit/compiler/test_parser_corpus.py`
  - Tests: `tests/unit/compiler/test_parser_corpus.py`
- [x] Add NLPL fixtures for macro/comptime regression coverage.
  - Completed 2026-05-05: Added integration-feature fixtures for macro/comptime hygiene behavior and intentional invalid comptime/assert misuse, and expanded fixture-based unit coverage to validate parse/typecheck success for valid fixtures plus diagnostic surfacing for invalid fixtures.
  - Files: `test_programs/integration/features/test_macro_comptime_regression.nxl`, `test_programs/integration/features/test_macro_comptime_hygiene_regression.nxl`, `test_programs/integration/features/test_macro_comptime_invalid_regression.nxl`, `tests/unit/language/test_macros.py`
  - Tests: `tests/unit/language/test_macros.py`, `tests/unit/language/test_comptime_typechecker.py`, `tests/unit/language/test_metaprogramming.py`

### 7.1) Pattern Matching Cross-Backend Parity (Interpreter ✅, LLVM ✅, C ⚠️ advanced depth pending)

- [x] Add statement-level C backend lowering for guarded and bound match cases.
  - Completed 2026-05-02: C backend now lowers guarded/bound match execution paths with literal/identifier/wildcard plus Option/Result/Variant/Tuple/List pattern-family handling.
  - Files: `src/nexuslang/compiler/backends/c_generator.py`
  - Tests: `tests/unit/compiler/test_c_pattern_matching.py`
- [x] Add executable compiled roundtrip coverage for guarded/bound match behavior.
  - Completed 2026-05-02: Added LLVM native roundtrip tests for identifier binding and guard selection behavior.
  - Files: `tests/integration/test_compiler_roundtrip.py`
  - Tests: `tests/integration/test_compiler_roundtrip.py`
- [x] Deepen runtime-backed semantics for Option/Result/Tuple/List bindings across compiled backends.
  - Completed 2026-05-05: Hardened compiled pattern-binding semantics for runtime-backed and scalar-fallback paths. C backend now preserves scalar `Err` payload bindings instead of substituting static placeholder strings, and propagates temporary list-literal lengths in match expressions so tuple/list pattern length checks are enforced beyond identifier-only sources. LLVM backend scalar `Result Err` bindings now preserve payload values (with pointer-to-integer normalization for runtime helper paths) instead of synthetic fallback strings.
  - Files: `src/nexuslang/compiler/backends/llvm_ir_generator.py`, `src/nexuslang/compiler/backends/c_generator.py`
  - Tests: `tests/unit/compiler/test_llvm_pattern_matching.py`, `tests/unit/compiler/test_c_pattern_matching.py`, `tests/integration/test_compiler_roundtrip.py`

### 8) Async / Await / Spawn (Typechecker ⚠️, Compiler ⚠️, Tooling ⚠️, Grammar ⚠️, Tests ⚠️)

- [x] Typecheck awaitable contracts and task result typing across nested awaits.
  - Completed 2026-05-04: Added explicit async function typing to expose `Awaitable<T>` return handles, await-expression payload unwrapping, and diagnostics for non-awaitable nested/identifier await operands while preserving synchronous direct-call await compatibility.
  - Files: `src/nexuslang/typesystem/typechecker.py`, `src/nexuslang/typesystem/types.py`
  - Tests: `tests/unit/type_system/test_async_typechecker.py`
- [x] Compile async state machines/coroutine frames consistently.
  - Completed 2026-05-05: Added C-backend coroutine-frame lowering parity for async/await. Async functions now lower to `NXLAsyncFrame*` producers with frame allocation, typed payload storage, completion state updates, and explicit frame returns. Await expressions now lower to frame polling (`nxl_async_resume`), typed result extraction, and frame destruction. Added C runtime async helpers (`nxl_async_frame_create`, `nxl_async_resume`, `nxl_async_destroy`) and backend parity tests alongside LLVM coroutine-path assertions.
  - Expanded 2026-05-05: Hardened await lowering with null-safe task-handle guards and default result initialization, added compiler-depth coverage for multiple await suspension points and await-with-arguments forms, and added executable mixed sync/async call-graph + async contract-failure path integration scenarios.
  - Files: `src/nexuslang/compiler/backends/c_generator.py`, `src/nexuslang/compiler/backends/llvm_ir_generator.py`
  - Tests: `tests/unit/compiler/test_c_async_codegen.py`, `tests/unit/compiler/test_llvm_async_codegen.py`, `tests/unit/compiler/test_llvm_codegen.py`, `tests/integration/test_compiler_roundtrip.py`
- [x] Add LSP diagnostics/completions for await/spawn contexts.
  - Completed 2026-05-03: Added context-aware completion support after `spawn` (function targets) and `await` (task/promise handles), plus diagnostics for await outside async functions and malformed bare await/spawn expressions.
  - Files: `src/nexuslang/lsp/diagnostics.py`, `src/nexuslang/lsp/completions.py`
  - Tests: `tests/tooling/test_lsp_async_diagnostics.py`, `tests/tooling/test_lsp_enhancements.py`
- [x] Align grammar async forms with parser behavior.
  - Completed 2026-05-04: Removed outdated statement-level `async/await/spawn` grammar variants, documented parser-aligned async function declaration and await expression forms, and clarified current `spawn` status in syntax reference notes.
  - Files: `grammar/NLPL.g4`, `docs/reference/syntax-grammar.md`

### 9) Generators / yield (Typechecker ⚠️, Compiler ⚠️, Tooling ⚠️, Grammar ⚠️, Tests ⚠️)

- [x] Typecheck yield/send/return interplay and generator function signatures.
  - Completed 2026-04-30: Generator-aware function typing added to `TypeChecker` with per-function generator context. `yield` is now checked against generator element type (`List<T>` return annotations), incompatible mixed yield types are diagnosed, non-list explicit return types on generator functions are rejected, inferred generator signatures default to `List<yield_type>`, and generator functions reject `return value`.
  - Files: `src/nexuslang/typesystem/typechecker.py`, `src/nexuslang/typesystem/types.py`
  - Tests: `tests/unit/type_system/test_typechecker.py`
- [x] Implement full generator frame lowering in LLVM backend (not baseline-only).
  - Completed 2026-05-05: Validated state-machine lowering depth for function-body yields with persistent generator state globals, parameter/local spill globals, and per-state resume labels. Added executable roundtrip coverage for interleaved yielded functions to verify isolated state machines across multiple generator functions.
  - Files: `src/nexuslang/compiler/backends/llvm_ir_generator.py`
  - Tests: `tests/unit/compiler/test_llvm_codegen.py`, `tests/integration/test_compiler_roundtrip.py`
- [x] Add C backend lowering for `for each` over generator expressions (materialized path).
  - Completed 2026-05-07: C backend now lowers generator-expression iterables in `for each` loops by materializing mapped/filtered values into temporary arrays, then iterating the materialized slice. This closes the previous unhandled-expression path for `GeneratorExpression` in C loop emission.
  - Expanded 2026-05-07: Extended the same lowering path to accept identifier sources backed by local list-initialized arrays by propagating local array bounds into `array_sizes`, removing the prior unsupported-source failure for this real-world form.
  - Expanded 2026-05-07: Added safe bounded lowering for non-sized/non-local identifier sources by resolving explicit `<source>_length` metadata in scope; this path uses bounded materialization with allocation-failure checks and pointer-index casting for metadata-sized sources.
  - Expanded 2026-05-07: Added flexible metadata alias resolution (`_len`, `_size`, `_count` suffixes, plus standalone fallbacks) with strict type checking to ensure metadata variables are integer-typed; errors now list valid aliases for user guidance.
  - Files: `src/nexuslang/compiler/backends/c_generator.py`
  - Tests: `tests/unit/compiler/test_c_backend_hardening.py`, `tests/integration/test_c_backend_roundtrip.py`
  - Tests added: `test_c_foreach_over_generator_identifier_source_local_array_materializes`, `test_foreach_over_generator_identifier_source_local_array_executes`, `test_c_foreach_over_generator_identifier_source_uses_length_metadata_bound`, `test_foreach_over_generator_identifier_source_with_length_metadata_executes`, `test_c_foreach_over_generator_identifier_source_uses_len_alias`, `test_foreach_over_generator_identifier_source_with_len_alias_executes`, `test_c_foreach_over_generator_identifier_source_uses_size_alias`, `test_foreach_over_generator_identifier_source_with_size_alias_executes`, `test_c_foreach_over_generator_identifier_source_uses_count_alias`, `test_foreach_over_generator_identifier_source_with_count_alias_executes`, `test_c_foreach_over_generator_identifier_source_missing_metadata_fails`
- [x] Improve tooling awareness for generator symbols and yield flow hints.
  - Completed 2026-05-03: Added hover docs for `yield`/`generator`, function-level generator flow hints (yield previews) in hover inference, and generator-aware symbol tagging (`containerName: generator`) for workspace-index, AST, and regex fallback symbol paths.
  - Files: `src/nexuslang/lsp/hover.py`, `src/nexuslang/lsp/symbols.py`
  - Tests: `tests/tooling/test_lsp_enhancements.py`
- [x] Align grammar for generator declarations and yield expressions.
  - Completed 2026-05-03: Updated ANTLR reference grammar with parser-aligned generator declaration notes (yield-based function semantics), added explicit `generatorExpression` form `(<expr> for <target> in <iterable> [if <cond>])`, and refreshed reference syntax docs with canonical generator/yield forms.
  - Files: `grammar/NLPL.g4`, `docs/reference/syntax-grammar.md`

### 10) Parallel for (Typechecker ⚠️, Compiler ⚠️, Tooling ⚠️, Grammar ⚠️, Tests ⚠️)

- [x] Typecheck loop-carried dependencies and reduction variable constraints.
  - Completed 2026-05-03: Extended parallel-for typechecking to classify outer-scope write hazards, reject reduction-style accumulator updates with guidance toward `parallel_reduce`, and catch outer member/index mutations as loop-carried dependencies.
  - Files: `src/nexuslang/typesystem/typechecker.py`
  - Tests: `tests/unit/type_system/test_parallel_for_typechecker.py`
- [x] Replace sequential fallback with true parallel lowering (work partitioning + join).
  - Completed 2026-05-04: 
    1. Filtered-generator parallel lowering: emit compiled prefilter pass that scans source array via generator predicate, collects matching elements compactly, then dispatches parallel on filtered array.
    2. Arithmetic-mapped generator fast path: detect single-step transforms (`x+C`, `x-C`, `x*C`) in non-identity generators, store transform op/arg in frame fields 5-6, apply via `nxl_generator_apply_transform` helper in next().
    3. Frame layout extended from 5-field (40 bytes) to 7-field (56 bytes) to accommodate transform metadata.
    4. Added `_emit_parallel_prefilter()` method that emits loop-based filter partition, `_extract_generator_arithmetic_transform()` for transform detection, `nxl_generator_apply_transform()` runtime helper.
  - Expanded 2026-05-07:
    1. C backend now emits runtime-backed parallel lowering for safe cases by outlining loop bodies into static callbacks and dispatching through `nxl_parallel_for_i64`.
    2. Added deterministic fallback to sequential lowering when iterable size/capture safety cannot be proven (e.g. unknown identifier sizes or unsafe outer-local captures).
    3. Added C backend coverage for both runtime-backed path and fallback path.
  - Expanded 2026-05-07 (Wave 4 capture transport):
    4. Added `nxl_parallel_for_ctx_i64` to runtime (`nxl_runtime.h`, `nxl_runtime.c`) accepting a caller-supplied `void* ctx` pointer alongside the body callback, backed by the same pthreads worker pool as `nxl_parallel_for_i64`.
    5. C generator now detects outer-local captures via `_collect_parallel_for_captures` (AST walker with enum/primitive guard to avoid infinite recursion on `TokenType` operator fields).
    6. When captures exist, generator builds a `__nxl_ctx_t_N` struct typedef, emits an instance initialised from the outer locals, and dispatches through `nxl_parallel_for_ctx_i64`; the callback receives `void* __nxl_ctx_raw` and unpacks fields into local copies.
    7. Direct (no-ctx) path retained when no captures are detected; sequential fallback retained for unknown iterable size.
    8. Added integration roundtrip tests (`tests/integration/test_c_backend_roundtrip.py`) that compile generated C source together with `nxl_runtime.c`, execute the binary, and assert correct output for: no-capture list parallel-for, single-capture ctx transport, two-capture ctx transport with arithmetic, function-parameter capture transport.
    9. Added telemetry processor showcase app (`examples/telemetry_processor.nxl`) demonstrating concurrent sensor batch processing with parallel-for capture semantics.
  - Files: `src/nexuslang/compiler/backends/llvm_ir_generator.py`, `src/nexuslang/compiler/backends/c_generator.py`, `src/nexuslang/stdlib_native/include/nxl_runtime.h`, `src/nexuslang/stdlib_native/src/nxl_runtime.c`
  - Tests: `tests/unit/compiler/test_llvm_parallel_contracts.py`, `tests/unit/compiler/test_llvm_comprehensions.py`, `tests/unit/compiler/test_c_parallel_contracts.py`, `tests/integration/test_c_backend_roundtrip.py`
  - Tests added: `test_llvm_parallel_for_over_filtered_generator_emits_prefilter_then_parallel`, `test_llvm_parallel_for_over_arithmetic_generator_uses_source_directly`, `test_arithmetic_transform_generator_avoids_materialization`, `test_c_parallel_for_lowers_to_parallel_runtime_call`, `test_c_parallel_for_identifier_without_known_size_falls_back_to_sequential_loop`, `test_c_parallel_for_with_outer_local_uses_ctx_transport`, `test_c_parallel_for_with_multiple_captures_emits_all_fields`, `test_c_parallel_for_no_capture_does_not_emit_ctx`, `test_c_parallel_for_ctx_extern_declaration_emitted`, `TestCBackendRoundtrip` (7 tests)
- [x] Add diagnostics for unsafe captures in parallel regions.
  - Completed 2026-05-03: Added LSP diagnostics pass that detects likely unsafe outer-scope captures inside `parallel for` bodies (outer variable writes, outer object member mutation, outer collection index mutation) with concurrency-focused fix hints.
  - Files: `src/nexuslang/lsp/diagnostics.py`
  - Tests: `tests/tooling/test_lsp_parallel_diagnostics.py`
- [x] Align grammar for parallel loop clauses/options.
  - Completed 2026-05-03: Added parser-aligned `parallelForLoop` reference grammar for the currently supported forms (`parallel for x in items` and `parallel for each x in items`), updated the syntax reference with explicit notes on unsupported clauses, and removed the misleading chunk-size loop example from the concurrency guide.
  - Files: `grammar/NLPL.g4`, `docs/reference/syntax-grammar.md`, `docs/guide/concurrency-levels.md`

### 11) Channels create/send/receive (Compiler ⚠️, Tooling ⚠️, Grammar ⚠️)

- [x] Harden compiler-level channel semantics: blocking/wakeup, close behavior, payload ownership.
  - Completed 2026-04-30: Added thread-safe blocking/wakeup and close-aware runtime semantics in C backend runtime helpers; added LLVM close runtime declaration and compiler tests.
  - Completed 2026-04-30: Added full language-level `close` operation surface (`close ch`) in parser/typechecker/interpreter/C/LLVM with targeted tests.
  - Completed 2026-04-30: Interpreter payload transfer semantics now snapshot mutable payload values on send to avoid aliasing-after-send behavior.
  - Completed 2026-04-30: LLVM channel lowering now retains `Rc`/`Arc` payload ownership on send, tracks receive-bound smart-pointer payloads for cleanup, and correctly routes pointer payload transport through pointer encode/decode paths.
  - Files: `src/nexuslang/parser/ast.py`, `src/nexuslang/parser/parser.py`, `src/nexuslang/interpreter/interpreter.py`, `src/nexuslang/typesystem/typechecker.py`, `src/nexuslang/compiler/backends/c_generator.py`, `src/nexuslang/compiler/backends/llvm_ir_generator.py`, `src/nexuslang/runtime/runtime.py`
  - Tests: `tests/unit/compiler/test_c_channels.py`, `tests/unit/compiler/test_llvm_channels.py`, `tests/unit/runtime/test_channels.py`, `tests/unit/compiler/test_llvm_codegen.py`
- [x] Add first-class LSP tooling for channels: completion, hover, diagnostics.
  - Completed 2026-04-30: Added channel-aware completions, hover docs, and non-channel send/receive diagnostics.
  - Files: `src/nexuslang/lsp/completions.py`, `src/nexuslang/lsp/hover.py`, `src/nexuslang/lsp/diagnostics.py`
  - Tests: `tests/tooling/test_lsp_channel_diagnostics.py`, `tests/tooling/test_lsp_enhancements.py`
- [x] Align grammar channel operation variants and pipeline syntax.
  - Completed 2026-05-04: Documented parser-aligned channel operation variants (`receive [from]`, `close [with]`) in grammar/reference docs and explicitly noted current pipeline-operator syntax as out of scope for the parser reference grammar.
  - Files: `grammar/NLPL.g4`, `docs/reference/syntax-grammar.md`

### 12) FFI / extern / unsafe (Typechecker ⚠️, Compiler ⚠️, Tooling ⚠️, Grammar ⚠️)

- [x] Strengthen typechecker ABI checks (size/layout/calling convention mismatches).
  - Completed 2026-05-05: Added extern ABI validation in typechecker for supported calling conventions, platform ABI mismatch detection, variadic calling-convention constraints (`cdecl`/`sysv`), and by-value layout guards for opaque/managed/struct/function-signature FFI types. Added focused coverage in `tests/unit/memory/test_ffi_safety.py` for invalid conventions, variadic misuse, opaque-type layout misuse, managed container ABI instability, and valid cdecl signatures.
  - Files: `src/nexuslang/typesystem/typechecker.py`, `src/nexuslang/stdlib/ffi/__init__.py`
  - Tests: `tests/unit/memory/test_ffi_safety.py`
- [x] Harden backend lowering for pointers, structs, callbacks, and variadics. (Completed: C and LLVM backends now normalize callback/function-pointer transport, apply variadic ABI promotions, and preserve pointer-safe lowering paths; validated by `tests/unit/compiler/test_ffi_codegen.py` and `tests/unit/memory/test_ffi_safety.py`.)
  - Files: `src/nexuslang/compiler/backends/llvm_ir_generator.py`, `src/nexuslang/compiler/backends/c_generator.py`
  - Tests: `tests/unit/compiler/test_ffi_codegen.py`
- [x] Improve LSP diagnostics for unsafe blocks and FFI signatures.
  - Completed 2026-05-03: Added unsafe-block structure diagnostics (`unsafe do` enforcement and missing `end` detection), extern/foreign signature checks (library clause, typed parameters, explicit returns, calling convention validation), and hover docs for `unsafe`, `extern`, `foreign`, and calling-convention keywords.
  - Files: `src/nexuslang/lsp/diagnostics.py`, `src/nexuslang/lsp/hover.py`
  - Tests: `tests/tooling/test_lsp_unsafe_ffi_diagnostics.py`, `tests/tooling/test_lsp_enhancements.py`
- [x] Align grammar for extern declarations and unsafe regions.
  - Completed 2026-05-04: Added parser-aligned extern/foreign/unsafe notes to reference grammar and syntax docs, including `unsafe do` optionality and the function-only `foreign` surface.
  - Files: `grammar/NLPL.g4`, `docs/reference/syntax-grammar.md`

### 13) Inline Assembly (Interpreter OK, Typechecker ⚠️, Tooling ⚠️, Grammar ⚠️)

- [x] Replace interpreter NOP behavior with explicit runtime policy (error or simulated execution mode).
  - Files: `src/nexuslang/interpreter/interpreter.py`
  - Tests: `tests/unit/systems/test_inline_assembly.py`
- [x] Add operand constraint and clobber validation in typechecker.
  - Completed 2026-05-03: Typechecker validates inline-asm input/output constraints, enforces output write markers, checks output targets are identifiers with defined variables, validates clobber tokens, and reports duplicate clobbers.
  - Files: `src/nexuslang/typesystem/typechecker.py`
  - Tests: `tests/unit/systems/test_inline_assembly_validation.py`
- [x] Add asm-aware diagnostics and hover docs for constraints.
  - Completed 2026-05-03: Added inline-assembly keyword hover docs (`asm`, `inputs`, `outputs`, `clobbers`, `constraint`) and asm-specific diagnostic fix suggestions for invalid input/output constraints and duplicate clobbers.
  - Files: `src/nexuslang/lsp/diagnostics.py`, `src/nexuslang/lsp/hover.py`
  - Tests: `tests/tooling/test_lsp_asm_diagnostics.py`, `tests/tooling/test_lsp_enhancements.py`
- [x] Align grammar for labels, templates, operands, and clobbers.
  - Completed 2026-05-04: Refined inline-assembly reference docs with parser-aligned code-block, operand-order, and clobber-list behavior notes, and clarified optional `end` handling in the grammar reference.
  - Files: `grammar/NLPL.g4`, `docs/reference/syntax-grammar.md`

### 14) LSP Diagnostics (Typechecker ⚠️ dependency, Tooling ⚠️)

- [x] Split parser/typechecker diagnostic sources with deterministic merge and deduplication.
  - Completed 2026-05-03: Added per-stream origin tagging (`parser`, `typechecker`, etc.), deterministic merge/dedup that preserves normalized `nlpl` source while aggregating origins for duplicate diagnostics, and stable ordering guarantees.
  - Files: `src/nexuslang/lsp/diagnostics.py`, `src/nexuslang/lsp/server.py`
  - Tests: `tests/tooling/test_lsp_diagnostics_merge.py`
- [x] Add stable diagnostic codes and documentation mapping.
  - Completed 2026-05-03: Added strict `EXXX` code validation fallback to `E309`, enriched diagnostic payload with deterministic `reference` links to `docs/reference/error-codes.md` anchors, and expanded normalization/merge tests.
  - Files: `src/nexuslang/lsp/diagnostics.py`, `docs/reference/error-codes.md`
  - Tests: `tests/tooling/test_lsp_diagnostics_merge.py`

### 15) Formatter (Tooling ⚠️, Tests ⚠️)

- [x] Replace regex-only formatter passes with AST-aware formatting pipeline.
  - Completed 2026-05-03: Implemented token-based formatting pipeline in `NLPLFormatter._format_with_tokens`. Uses `Lexer.tokenize()` to reconstruct each source line from canonical token forms with precise operator spacing. Token types drive indentation. String literals and inline comments preserved verbatim. Graceful regex fallback on tokenisation failure.
  - Files: `src/nexuslang/lsp/formatter.py`
  - Tests: `tests/tooling/test_lsp_formatter_ast.py` (33 tests: indentation, operator spacing, string preservation, comment handling, blank-line separators, idempotency, LSP edits, regex fallback)
- [x] Add idempotency and style-profile regression suite.
  - Completed 2026-05-03: Idempotency tests included in `test_lsp_formatter_ast.py` (TestIdempotency class, 4 scenarios).
  - Files: `tests/tooling/test_lsp_formatter_ast.py`
- [x] Add formatter quick-fix integration in LSP code actions.
  - Completed 2026-05-03: Integrated formatter-backed document quick-fix action (`Format document (NexusLang style)`) in LSP code actions, emitted only when formatting introduces a change.
  - Files: `src/nexuslang/lsp/code_actions.py`
  - Tests: `tests/tooling/test_lsp_formatter_code_actions.py`, `tests/tooling/test_lsp_refactoring.py`

## Suggested Milestones

- Milestone A: Channels + LSP diagnostics foundation
- Milestone B: Generators + Parallel-for backend semantics
- Milestone C: Contracts + Macro/comptime semantic completeness
- Milestone D: FFI/Inline-asm hardening + Formatter modernization
