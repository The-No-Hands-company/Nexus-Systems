# NexusLang Feature Completeness Audit

Updated: 2026-05-07

## Purpose

This document is a deep audit of NexusLang feature completeness across the full language stack, not just the interpreter. The goal is to keep one authoritative inventory of what is still missing, partial, inconsistent, or underpowered across:

- lexer
- parser
- AST
- interpreter/runtime
- typechecker and specialized static passes
- grammar reference
- compiler backends
- LSP/editor tooling
- formatter/linter/debugger/build tooling
- top-level documentation claims

This is intentionally evidence-driven. Items below are based on confirmed repository state, not wishlisting.

## Method

The audit was built from direct inspection of:

- `src/nexuslang/parser/lexer.py`
- `src/nexuslang/parser/parser.py`
- `src/nexuslang/parser/ast.py`
- `src/nexuslang/interpreter/interpreter.py`
- `src/nexuslang/typesystem/typechecker.py`
- `src/nexuslang/typesystem/lifetime_checker.py`
- `src/nexuslang/compiler/backends/llvm_ir_generator.py`
- `src/nexuslang/compiler/backends/c_generator.py`
- `src/nexuslang/lsp/*.py`
- `grammar/NLPL.g4`
- `README.md`
- targeted tests in `tests/unit/**`

Additional repository-wide scans were used to compare:

- lexer tokens vs parser usage
- AST node inventory vs interpreter/typechecker/compiler references
- grammar token/rule coverage vs actual lexer keyword coverage

## Executive Summary

NLPL is strongest in the interpreter and parser. That is the most mature path.

The biggest remaining gaps are not in the basic syntax core anymore, but in cross-toolchain consistency:

1. The interpreter supports more than the typechecker, compiler backends, formatter, and LSP explicitly understand.
2. The lexer keyword inventory is significantly broader than the actually parsed/documented language surface.
3. The formal grammar still trails the real parser by a wide margin, even after recent sync work.
4. Tooling claims in `README.md` are stronger than what is currently evidenced for advanced features.
5. NexusLang needs a deliberate feature-matrix discipline so every feature is tracked across lexer, parser, AST, interpreter, typechecker, compiler, LSP, formatter, tests, and grammar.

## May 2026 Revalidation Snapshot

The April 2026 gap inventory contained multiple stale claims. A direct code revalidation in May 2026 confirmed the following closures:

- Channels are no longer interpreter-only: typechecker, LLVM backend, C backend, and LSP diagnostics/completions now contain channel-aware logic.
- Generator/yield support is no longer absent in typechecker/C backend: both have explicit handling (while compiled semantics remain less mature than the interpreter for advanced scenarios).
- Parallel-for is no longer interpreter-only: typechecker, LLVM backend, and C backend all include lowering/checking paths.
- Contract/assertion statements are no longer interpreter-only: typechecker and both compiler backends contain dedicated handling.
- Macro/comptime are no longer parser/interpreter-only: typechecker + LLVM/C lowering exist, including compiled macro expansion and comptime assertion support.
- Pattern matching now has a C backend lowering path for statement-level guarded/bound matching, including Option/Result/Variant/Tuple/List pattern families.

What still remains high-value after revalidation:

- Deep semantic parity work for advanced generator and pattern matching behavior across interpreter, LLVM, and C paths.
- Grammar parity and lexer-surface cleanup to reduce "declared but not uniformly supported" language surface.
- Tooling-depth parity (formatter/LSP semantic richness) for newer language constructs.

## Confirmed High-Priority Gaps

### May 2026 correction of stale P0 claims

The original P0 section below this heading was written before multiple feature-closure batches landed.
Those claims are now stale and should not be used as current status for channels, generators,
parallel-for, contracts, or macro/comptime support.

Closed since the original audit:

- Channels: parser + interpreter + typechecker + LLVM/C backends + LSP diagnostics/completions now contain explicit channel handling.
- Parallel-for: parser/interpreter/compiler/typechecker/LSP support paths now exist.
- Contracts/assertions: interpreter + typechecker + LLVM/C backend paths now include dedicated handling.
- Macro/comptime: parser/interpreter/typechecker/compiler/LSP paths now include dedicated handling.
- Pattern matching on C backend: statement-level guarded/bound lowering now exists for major pattern families.

Current P0-level focus areas after revalidation:

- Semantic parity depth (not surface presence): advanced behavior equivalence across interpreter vs LLVM/C for generators/patterns/contracts.
- Shared-analysis consistency: unify checker traversal and diagnostics semantics to reduce parser-shape drift between tools.
- Grammar/reference parity: align formal grammar and status docs with the implemented parser/language surface.

## Confirmed Core Language Surface Gaps

### 1. The lexer defines more language than the parser actually parses

A current repository scan found 22 token names defined in `TokenType` but not referenced in `src/nexuslang/parser/parser.py`.

Confirmed examples:

- range/reference-like surface:
  - `RANGE`
  - `RANGE_INCLUSIVE`
  - `REFERENCE`
- utility/data verbs:
  - `INTO`
  - `JOIN`
- database/network family:
  - `DATABASE`
  - `CONNECT`
  - `DISCONNECT`
  - `QUERY`
  - `EXECUTE`
  - `DELETE`
  - `SELECT`
  - `NETWORK`
  - `REQUEST`
  - `RESPONSE`
  - `HTTP`
  - `WEBSOCKET`
  - `CONNECT_TO`
  - `DISCONNECT_FROM`
- inline/assembly split tokens:
  - `INLINE`
  - `ASSEMBLY`

Note: prior audit examples listing end-form aliases (`END_LOOP`, `END_CONCURRENT`, `END_TRY`, `END_THE_INTERFACE`, `END_THE_TRAIT`) are stale and have been removed; those aliases are now consumed by parser termination paths.

Why it matters:

The token inventory currently advertises a wider language surface than the parser implements. That makes the lexer a poor proxy for actual feature support and increases maintenance cost.

### 2. File, network, database, and DSL-style vocabularies are not first-class language features yet

The lexer contains extensive vocabulary for system-like and protocol-like constructs, but much of it is not actually parsed as dedicated core syntax.

Confirmed examples in `src/nexuslang/parser/lexer.py` include tokens for:

- database operations
- network operations
- file operations
- protocol nouns and verbs

But parser references are missing or minimal for many of them.

Why it matters:

This creates a false sense of language breadth. These terms are currently closer to planned syntax inventory than finished language features.

## Typechecker and Static Analysis Gaps

### 1. Surface coverage is broader; semantic depth is the remaining issue

As of May 2026 revalidation, the main typechecker and related static passes do include handling for
channels, parallel-for, contracts/assertions, macro/comptime, and ownership-related constructs.

Remaining gap:

- deeper path- and flow-sensitive semantics for advanced constructs (especially when features combine),
  not first-pass node visibility.

Why it matters:

The key risk is now precision/consistency across complex programs rather than outright feature absence.

### 2. Static analysis is split across multiple passes and not obviously unified

Confirmed specialized passes exist:

- `src/nexuslang/typesystem/borrow_checker.py`
- `src/nexuslang/typesystem/lifetime_checker.py`

Confirmed integration exists in the CLI path:

- `src/nexuslang/main.py` runs both `BorrowChecker` and `LifetimeChecker`

But this also means NLPL’s static semantics are fragmented across:

- typechecker
- borrow checker
- lifetime checker
- interpreter-side runtime type enforcement
- LSP diagnostics

Why it matters:

This is workable, but it is not yet a single coherent language-analysis pipeline. The static semantics need a documented ownership model and integration plan.

### 3. LSP diagnostics depend on lower-layer semantic precision

`src/nexuslang/lsp/diagnostics.py` uses:

- parser-based syntax checks
- typechecker-based diagnostics
- supplemental static checks for advanced feature families

Why it matters:

LSP fidelity still depends on parser/typechecker precision. For advanced feature combinations, diagnostics can lag runtime semantics even when feature hooks exist.

## Compiler Backend Gaps

### 1. LLVM backend has broad feature hooks; parity depth remains

The LLVM backend now includes dedicated handling across many previously missing families (channels,
parallel-for, contracts/assertions, macro/comptime).

Remaining gap:

- behavior-level parity and optimization maturity for advanced constructs (for example, complex generator
  behavior and edge-case lowering fidelity), not baseline node support.

### 2. C backend coverage improved; clarify support envelope by feature depth

Recent work closed major statement-level gaps (including richer pattern/match lowering), but C backend
maturity is still uneven across advanced combinations.

Why it matters:

The key documentation need is a support matrix that distinguishes "supported syntax" from "fully parity-tested semantics".

## Grammar Parity Gaps

### 1. The formal grammar still lags the lexer and parser by a large margin

A direct comparison between lexer keyword coverage and `grammar/NLPL.g4` found 116 lexer keyword/token mappings absent from the grammar reference.

Confirmed examples still absent from grammar coverage include:

- structural English surface:
  - `DEFINE`
  - `CALLED`
  - `THAT`
  - `HAS`
  - `TAKES`
  - `METHOD`
  - `PROPERTIES`
  - `METHODS`
- alternate end forms:
  - `END_CLASS`
  - `END_THE_CLASS`
  - `END_METHOD`
  - `END_THE_METHOD`
  - `END_TRAIT`
  - `END_THE_TRAIT`
  - `END_INTERFACE`
  - `END_THE_INTERFACE`
  - `END_IF`
  - `END_WHILE`
  - `END_LOOP`
  - `END_CONCURRENT`
  - `END_TRY`
- metaprogramming and analysis:
  - `MACRO`
  - `EXPAND`
  - `COMPTIME`
  - `SPEC`
  - `ATTRIBUTE`
- ownership and memory surface:
  - `RC`
  - `WEAK`
  - `ARC`
  - `MOVE`
  - `BORROW`
  - `DROP`
  - `LIFETIME`
  - `ALLOCATOR`
- richer control flow and syntax words:
  - `PARALLEL`
  - `FOR_EACH`
  - `ELSE_IF`
  - `SWITCH`
  - `LABEL`
- comparison/operator surface:
  - `DIVIDED_BY`
  - `FLOOR_DIVIDE`
  - `CONCATENATE`
  - `GREATER_THAN`
  - `LESS_THAN`
  - `EQUAL_TO`
  - `NOT_EQUAL_TO`
  - `GREATER_THAN_OR_EQUAL_TO`
  - `LESS_THAN_OR_EQUAL_TO`
  - `LEFT_SHIFT`
  - `RIGHT_SHIFT`
- system/data vocabularies:
  - `DATABASE`
  - `NETWORK`
  - `HTTP`
  - `WEBSOCKET`
  - `FILE`
  - `DIRECTORY`
  - `PATH`

Why it matters:

`grammar/NLPL.g4` is currently not a reliable feature inventory for NexusLang as a whole. It remains a partial reference, not a true grammar-complete spec.

### 2. The token surface is broader than the documented language contract

The grammar is now better than it was, but it still does not accurately communicate the true parser surface. That is a problem for contributors, users, and tooling authors.

## Tooling Gaps

### 1. Formatter is text-based, not AST-aware

Key file:

- `src/nexuslang/lsp/formatter.py`

Confirmed characteristics:

- indentation and spacing are driven by keyword heuristics and regex replacements
- formatting decisions are not based on the parsed AST
- strings are special-cased with simple protection logic, not full syntax-aware rewriting

Why it matters:

A regex formatter will inevitably lag the language as new constructs are added. NexusLang needs an AST-aware formatter if it wants formatting to be trustworthy across the whole language.

### 2. LSP feature breadth is ahead of full semantic depth

`README.md` claims 25 LSP features and describes the tooling ecosystem as comprehensive.

Confirmed current reality:

- a real LSP exists in `src/nexuslang/lsp/`
- diagnostics, formatter, hover, rename, references, symbols, and other modules exist
- advanced feature coverage exists across hover/completions/diagnostics for channels,
  macro/comptime, unsafe/FFI, generators/yield, and parallel capture checks

Why it matters:

The LSP is substantial, but feature-matrix parity and flow-sensitive diagnostics depth still need tightening.

### 3. Diagnostics and formatting are only partly semantic

Confirmed from `src/nexuslang/lsp/diagnostics.py` and `src/nexuslang/lsp/formatter.py`:

- diagnostics combine parser and typechecker output with heuristic checks
- formatting is regex-based

Why it matters:

As the language surface expands, these tools need to migrate from heuristic support to AST-driven and feature-aware support.

## Documentation and Positioning Gaps

### 1. README claims still need stronger support-matrix framing

`README.md` currently states or strongly implies:

- parser handles the full syntax surface
- interpreter executes all language constructs
- type system is working
- tooling ecosystem is comprehensive
- LLVM backend compiles core language constructs to native binaries

The core issue is not that these statements are entirely false. The issue is that they flatten the difference between:

- core constructs
- syntax coverage vs deep semantic parity
- interpreter semantics vs compiled semantics
- feature-specific tooling coverage

Why it matters:

NLPL should present a support matrix instead of a single blended maturity claim.

## Recommended Work Order

### Phase 1: Stop the drift

1. Add and maintain a feature support matrix with columns for:
   - lexer
   - parser
   - AST
   - interpreter
   - typechecker
   - borrow/lifetime checker
   - LLVM backend
   - C backend
   - LSP
   - formatter
   - grammar
   - tests
2. Prune or implement dead lexer tokens.
3. Make `grammar/NLPL.g4` track the real parser much more aggressively.

### Phase 2: Close the highest-value language gaps

1. Bring channels into the typechecker and at least define a compiler story.
2. Complete yield/generator semantics across compilation and typechecking.
3. Add compiler and typechecker support for parallel-for.
4. Add compiler/typechecker handling for contracts.
5. Add compiler/typechecker handling for macro/comptime features.

### Phase 3: Upgrade tooling quality

1. Replace regex formatting with AST-aware formatting.
2. Add feature-specific LSP coverage for newly added and advanced nodes.
3. Expand diagnostics so advanced constructs are not parser-only or runtime-only from the editor’s point of view.

### Phase 4: Reconcile docs with reality

1. Add a public support-matrix document.
2. Soften or qualify README maturity claims until every subsystem is aligned.
3. Keep `grammar/NLPL.g4` and parser evolution tied together in review.

## Concrete Missing/Partial Features to Treat as Top Priority

These are the most important confirmed items to close if the goal is “feature complete NLPL” rather than just “broad interpreter surface”:

1. Channel support in typechecker/compiler/tooling
2. Generator and yield completeness beyond the interpreter
3. Parallel-for compilation support
4. Contract/assertion support outside the interpreter
5. Macro/comptime propagation into typechecker/compiler/tooling
6. Removal or implementation of dead lexer token families
7. Full grammar parity with actual parser surface
8. AST-aware formatter and stronger LSP semantic coverage

## Bottom Line

NLPL is already broad. The remaining work is mostly about consistency, completeness, and trustworthiness across the stack.

The main problem is no longer “does NexusLang have interesting syntax?”

The main problem is:

- does every feature have a complete pipeline,
- is every advertised keyword real,
- can every real feature be checked, formatted, diagnosed, compiled, and documented,
- and is there one truthful source of record for language support.

That is the path to feature completeness.
