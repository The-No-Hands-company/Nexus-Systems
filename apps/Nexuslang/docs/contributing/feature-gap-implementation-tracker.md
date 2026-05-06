# NexusLang Feature Gap Implementation Tracker

Updated: 2026-05-03 (switch/labeled-flow backend depth + LSP hierarchy + error-handling grammar/typechecker parity)

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

## Task Backlog By Matrix Row

### 1) Classes / Methods / Inheritance (Tooling ⚠️, Grammar ⚠️)

- [x] Add class-aware symbol hierarchy in LSP document/workspace symbols.
  - Completed 2026-05-03: Implemented dotted-scope hierarchy assembly for document symbols so nested symbols (class -> method -> parameters/control-flow children) are attached to the correct parent chain.
  - Files: `src/nexuslang/lsp/server.py`, `src/nexuslang/lsp/workspace_index.py`, `src/nexuslang/lsp/symbols.py`
  - Tests: `tests/tooling/test_lsp_document_features.py`
- [x] Improve method override navigation and references (base/derived linking).
  - Completed 2026-05-03: Added override-aware goto-definition for class methods (derived -> nearest base declaration), override-family reference expansion across base/derived declarations, and deterministic deduplication of merged reference sets.
  - Files: `src/nexuslang/lsp/definitions.py`, `src/nexuslang/lsp/references.py`
  - Tests: `tests/tooling/test_lsp_enhancements.py`
- [ ] Align grammar coverage for class members, inheritance clauses, and modifiers.
  - Files: `grammar/NLPL.g4`, `docs/reference/syntax-grammar.md`
  - Tests: `tests/unit/compiler/test_parser.py`

### 2) Interfaces / Traits / Generics (Typechecker ⚠️, Compiler ⚠️, Tooling ⚠️, Grammar ⚠️)

- [ ] Enforce trait/interface conformance diagnostics with precise member mismatch output.
  - Files: `src/nexuslang/typesystem/typechecker.py`, `src/nexuslang/typesystem/types.py`
  - Tests: `tests/unit/type_system/test_generics.py`, `tests/unit/type_system/test_typechecker.py`
- [ ] Implement generic constraint propagation through calls and method dispatch.
  - Files: `src/nexuslang/typesystem/type_inference.py`, `src/nexuslang/interpreter/interpreter.py`
  - Tests: `tests/unit/type_system/test_bidirectional_inference.py`
- [ ] Lower generic specializations consistently in LLVM/C backends.
  - Files: `src/nexuslang/compiler/backends/llvm_ir_generator.py`, `src/nexuslang/compiler/backends/c_generator.py`
  - Tests: `tests/unit/compiler/test_llvm_codegen.py`, `tests/unit/compiler/test_c_codegen.py`
- [x] Add LSP hover/completion support for generic parameters and constraints.
  - Completed 2026-05-03: Added generic-aware hover rendering for function/method signatures (`<T: Bound>` and `where` constraints) plus completion contexts for generic parameter templates and where-clause trait bounds.
  - Files: `src/nexuslang/lsp/hover.py`, `src/nexuslang/lsp/completions.py`
  - Tests: `tests/tooling/test_lsp_enhancements.py`
- [ ] Update grammar for generic bounds and trait composition syntax.
  - Files: `grammar/NLPL.g4`, `docs/reference/syntax-grammar.md`
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
- [ ] Lower try/catch/raise consistently in both backends with runtime-compatible semantics.
  - Files: `src/nexuslang/compiler/backends/llvm_ir_generator.py`, `src/nexuslang/compiler/backends/c_generator.py`
  - Tests: `tests/unit/compiler/test_llvm_codegen.py`, `tests/unit/compiler/test_c_codegen.py`
- [x] Improve LSP diagnostics for exception scope and unreachable catch blocks.
  - Completed 2026-05-03: Added dedicated diagnostics pass for catch-variable scope leakage and likely unreachable catch blocks when try exits via early return before catch.
  - Files: `src/nexuslang/lsp/diagnostics.py`
  - Tests: `tests/tooling/test_lsp_exception_scope_diagnostics.py`, `tests/tooling/test_lsp_diagnostics_merge.py`
- [x] Update grammar for complete error-handling syntax parity.
  - Completed 2026-05-03: Synced formal grammar/reference docs with parser-supported `catch ... with ... as ...` bindings and `raise`/`throw` variants including optional message clauses.
  - Files: `grammar/NLPL.g4`, `docs/reference/syntax-grammar.md`

### 5) Contracts / Assertions (Typechecker ⚠️, Compiler ⚠️, Tooling ⚠️, Grammar ⚠️)

- [ ] Enforce contract expression type rules and side-effect restrictions.
  - Files: `src/nexuslang/typesystem/typechecker.py`
  - Tests: `tests/unit/language/test_contracts.py`
- [ ] Compile `expect`, `require`, `ensure`, `invariant` with clear failure paths.
  - Files: `src/nexuslang/compiler/backends/llvm_ir_generator.py`, `src/nexuslang/compiler/backends/c_generator.py`
  - Tests: `tests/unit/compiler/test_llvm_codegen.py`
- [x] Add richer LSP diagnostics and quick-fix suggestions for contract failures.
  - Completed 2026-05-03: Added contract-aware diagnostic fix metadata for boolean/mutation/message contract violations and LSP quick-fix actions to append contract message clauses, convert contract conditions to explicit boolean checks, and convert non-string contract messages to string literals.
  - Files: `src/nexuslang/lsp/diagnostics.py`, `src/nexuslang/lsp/code_actions.py`
  - Tests: `tests/tooling/test_lsp_contract_code_actions.py`, `tests/tooling/test_lsp_refactoring.py`
- [ ] Align grammar contract forms and nesting rules.
  - Files: `grammar/NLPL.g4`, `docs/reference/syntax-grammar.md`

### 6) Ownership / Borrowing / Lifetimes (Typechecker ⚠️, Compiler ⚠️, Tooling ⚠️, Grammar ⚠️)

- [ ] Consolidate borrow/lifetime checking into deterministic pass boundaries.
  - Files: `src/nexuslang/typesystem/typechecker.py`, `src/nexuslang/typesystem/lifetime.py`
  - Tests: `tests/unit/memory/test_borrow_checker.py`
- [ ] Emit ownership-aware move/borrow semantics in compiled backends.
  - Files: `src/nexuslang/compiler/backends/llvm_ir_generator.py`, `src/nexuslang/compiler/backends/c_generator.py`
  - Tests: `tests/unit/memory/test_lifetimes.py`, `tests/unit/compiler/test_llvm_codegen.py`
- [ ] Add borrow conflict diagnostics + related ranges in LSP.
  - Files: `src/nexuslang/lsp/diagnostics.py`
  - Tests: `tests/tooling/test_lsp_diagnostics.py`
- [ ] Update grammar and reference docs for ownership syntax forms.
  - Files: `grammar/NLPL.g4`, `docs/reference/syntax-grammar.md`, `docs/guide/type-system.md`

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
- [ ] Expand grammar for macro definitions, hygienic params, and comptime blocks.
  - Files: `grammar/NLPL.g4`, `docs/reference/syntax-grammar.md`
- [ ] Add NLPL fixtures for macro/comptime regression coverage.
  - Files: `test_programs/integration/features/`, `tests/unit/language/test_macros.py`

### 7.1) Pattern Matching Cross-Backend Parity (Interpreter ✅, LLVM ✅, C ⚠️ advanced depth pending)

- [x] Add statement-level C backend lowering for guarded and bound match cases.
  - Completed 2026-05-02: C backend now lowers guarded/bound match execution paths with literal/identifier/wildcard plus Option/Result/Variant/Tuple/List pattern-family handling.
  - Files: `src/nexuslang/compiler/backends/c_generator.py`
  - Tests: `tests/unit/compiler/test_c_pattern_matching.py`
- [x] Add executable compiled roundtrip coverage for guarded/bound match behavior.
  - Completed 2026-05-02: Added LLVM native roundtrip tests for identifier binding and guard selection behavior.
  - Files: `tests/integration/test_compiler_roundtrip.py`
  - Tests: `tests/integration/test_compiler_roundtrip.py`
- [ ] Deepen runtime-backed semantics for Option/Result/Tuple/List bindings across compiled backends.
  - Progress 2026-05-02: Added scalar-safe Option/Result fallback matching + bindings in C backend to reduce helper-linkage coupling (`Some/None` via nonzero/null semantics, `Ok/Err` via sign semantics) with new compiler unit coverage.
  - Files: `src/nexuslang/compiler/backends/llvm_ir_generator.py`, `src/nexuslang/compiler/backends/c_generator.py`
  - Tests: `tests/unit/compiler/test_llvm_pattern_matching.py`, `tests/unit/compiler/test_c_pattern_matching.py`, `tests/integration/test_compiler_roundtrip.py`

### 8) Async / Await / Spawn (Typechecker ⚠️, Compiler ⚠️, Tooling ⚠️, Grammar ⚠️, Tests ⚠️)

- [ ] Typecheck awaitable contracts and task result typing across nested awaits.
  - Files: `src/nexuslang/typesystem/typechecker.py`
  - Tests: `tests/unit/runtime/test_async_runtime.py`
- [ ] Compile async state machines/coroutine frames consistently.
  - Files: `src/nexuslang/compiler/backends/c_generator.py`, `src/nexuslang/compiler/backends/llvm_ir_generator.py`, `src/nexuslang/runtime/runtime.py`
  - Tests: `tests/unit/runtime/test_async_runtime.py`, `tests/unit/compiler/test_llvm_codegen.py`
- [x] Add LSP diagnostics/completions for await/spawn contexts.
  - Completed 2026-05-03: Added context-aware completion support after `spawn` (function targets) and `await` (task/promise handles), plus diagnostics for await outside async functions and malformed bare await/spawn expressions.
  - Files: `src/nexuslang/lsp/diagnostics.py`, `src/nexuslang/lsp/completions.py`
  - Tests: `tests/tooling/test_lsp_async_diagnostics.py`, `tests/tooling/test_lsp_enhancements.py`
- [ ] Align grammar async forms with parser behavior.
  - Files: `grammar/NLPL.g4`, `docs/reference/syntax-grammar.md`

### 9) Generators / yield (Typechecker ⚠️, Compiler ⚠️, Tooling ⚠️, Grammar ⚠️, Tests ⚠️)

- [x] Typecheck yield/send/return interplay and generator function signatures.
  - Completed 2026-04-30: Generator-aware function typing added to `TypeChecker` with per-function generator context. `yield` is now checked against generator element type (`List<T>` return annotations), incompatible mixed yield types are diagnosed, non-list explicit return types on generator functions are rejected, inferred generator signatures default to `List<yield_type>`, and generator functions reject `return value`.
  - Files: `src/nexuslang/typesystem/typechecker.py`, `src/nexuslang/typesystem/types.py`
  - Tests: `tests/unit/type_system/test_typechecker.py`
- [ ] Implement full generator frame lowering in LLVM backend (not baseline-only).
  - In progress 2026-04-30: Removed fake generator-size placeholder in LLVM lowering by threading the real produced element count out of comprehension generation into generator struct construction.
  - Files: `src/nexuslang/compiler/backends/llvm_ir_generator.py`
  - Tests: `tests/unit/compiler/test_llvm_codegen.py`, `tests/integration/test_compiler_pipeline.py`
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
- [ ] Replace sequential fallback with true parallel lowering (work partitioning + join).
  - Files: `src/nexuslang/compiler/backends/llvm_ir_generator.py`, `src/nexuslang/runtime/runtime.py`
  - Tests: `tests/unit/runtime/test_parallel_runtime.py`, `tests/unit/compiler/test_llvm_codegen.py`
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
- [ ] Align grammar channel operation variants and pipeline syntax.
  - Files: `grammar/NLPL.g4`, `docs/reference/syntax-grammar.md`

### 12) FFI / extern / unsafe (Typechecker ⚠️, Compiler ⚠️, Tooling ⚠️, Grammar ⚠️)

- [ ] Strengthen typechecker ABI checks (size/layout/calling convention mismatches).
  - Files: `src/nexuslang/typesystem/typechecker.py`, `src/nexuslang/stdlib/ffi/__init__.py`
  - Tests: `tests/unit/memory/test_ffi_safety.py`
- [ ] Harden backend lowering for pointers, structs, callbacks, and variadics.
  - Files: `src/nexuslang/compiler/backends/llvm_ir_generator.py`, `src/nexuslang/compiler/backends/c_generator.py`
  - Tests: `tests/unit/compiler/test_ffi_codegen.py`
- [x] Improve LSP diagnostics for unsafe blocks and FFI signatures.
  - Completed 2026-05-03: Added unsafe-block structure diagnostics (`unsafe do` enforcement and missing `end` detection), extern/foreign signature checks (library clause, typed parameters, explicit returns, calling convention validation), and hover docs for `unsafe`, `extern`, `foreign`, and calling-convention keywords.
  - Files: `src/nexuslang/lsp/diagnostics.py`, `src/nexuslang/lsp/hover.py`
  - Tests: `tests/tooling/test_lsp_unsafe_ffi_diagnostics.py`, `tests/tooling/test_lsp_enhancements.py`
- [ ] Align grammar for extern declarations and unsafe regions.
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
- [ ] Align grammar for labels, templates, operands, and clobbers.
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
