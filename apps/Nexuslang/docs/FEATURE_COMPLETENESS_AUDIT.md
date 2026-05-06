# NexusLang Feature Completeness Audit

Updated: 2026-05-02

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

### P0: Cross-toolchain inconsistencies that block “feature complete” status

#### 1. Channels are runtime-capable but not toolchain-complete

Status:

- lexer: yes
- parser: yes
- AST: yes
- interpreter: yes
- tests: yes
- typechecker: no direct handling found
- LLVM backend: no direct handling found
- C backend: no direct handling found
- LSP feature-specific support: no direct handling found

Evidence:

- parser/interpreter support exists in:
  - `src/nexuslang/parser/lexer.py`
  - `src/nexuslang/parser/parser.py`
  - `src/nexuslang/parser/ast.py`
  - `src/nexuslang/interpreter/interpreter.py`
  - `tests/unit/language/test_channels.py`
- no `SendStatement`, `ReceiveExpression`, or `ChannelCreation` references were found in:
  - `src/nexuslang/typesystem/typechecker.py`
  - `src/nexuslang/compiler/backends/llvm_ir_generator.py`
  - `src/nexuslang/lsp/*.py`

Why it matters:

NLPL now has working channels, but they are still effectively interpreter-only. A language is not feature complete when new core concurrency primitives do not propagate into typechecking, compilation, and tooling.

#### 2. Yield and generator support are not complete across the stack

Status:

- lexer: yes
- parser: yes
- AST: yes
- interpreter: yes
- compiler: partial / placeholder implementation
- typechecker: no direct handling found
- grammar: now documented, but implementation parity is still uneven

Evidence:

- parser support:
  - `src/nexuslang/parser/parser.py` contains `parse_yield_expression`
- interpreter support:
  - `src/nexuslang/interpreter/interpreter.py` contains runtime yield handling
- LLVM backend:
  - `src/nexuslang/compiler/backends/llvm_ir_generator.py` contains `_generate_generator_expression`
  - the implementation explicitly states generators are still an MVP approximation and uses placeholder behavior such as a hard-coded size placeholder
- no direct `YieldExpression` or `GeneratorExpression` references were found in:
  - `src/nexuslang/typesystem/typechecker.py`
  - `src/nexuslang/compiler/backends/c_generator.py`

Why it matters:

This is not just a documentation gap. The compiled path for generators is not at the same semantic maturity as the interpreter path.

#### 3. Parallel-for is interpreter-only

Status:

- parser: yes
- AST: yes
- interpreter: yes
- compiler: no direct support found

Evidence:

- parser has `parse_parallel_for`
- interpreter has `execute_parallel_for_loop`
- no `ParallelForLoop` or `parallel_for_loop` references were found in `src/nexuslang/compiler/backends/llvm_ir_generator.py`

Why it matters:

Parallel iteration is a major feature family. If it cannot compile, NexusLang still has a major gap between its execution modes.

#### 4. Contract/assertion features are interpreter-only

Status:

- parser and runtime support exist
- typechecker: no direct handling found
- compiler: no direct handling found

Evidence:

- interpreter contains:
  - `execute_expect_statement`
  - `execute_require_statement`
  - `execute_ensure_statement`
  - `execute_guarantee_statement`
  - `execute_invariant_statement`
- no direct references for `ExpectStatement`, `RequireStatement`, `EnsureStatement`, `GuaranteeStatement`, or `InvariantStatement` were found in:
  - `src/nexuslang/typesystem/typechecker.py`
  - `src/nexuslang/compiler/backends/llvm_ir_generator.py`

Why it matters:

Contract programming is part of the language surface, but today it is not enforced consistently outside the interpreter.

#### 5. Macros and comptime features do not propagate across the toolchain

Status:

- lexer: yes
- parser: yes
- AST: yes
- interpreter: yes
- typechecker: no direct handling found
- compiler backend: no direct handling found
- grammar parity still incomplete overall

Evidence:

- parser dispatch includes:
  - `TokenType.MACRO`
  - `TokenType.EXPAND`
  - `TokenType.COMPTIME`
- interpreter contains:
  - `execute_macro_definition`
  - `execute_macro_expansion`
  - `execute_comptime_expression`
  - `execute_comptime_const`
  - `execute_comptime_assert`
- no direct `MacroDefinition`, `MacroExpansion`, `ComptimeExpression`, `ComptimeConst`, or `ComptimeAssert` references were found in:
  - `src/nexuslang/typesystem/typechecker.py`
  - `src/nexuslang/compiler/backends/llvm_ir_generator.py`

Why it matters:

These are foundational meta-language features. Without cross-toolchain support they remain interpreter-centric rather than language-complete.

## Confirmed Core Language Surface Gaps

### 1. The lexer defines more language than the parser actually parses

A repository-wide scan found 27 token names defined in `TokenType` but not referenced in `src/nexuslang/parser/parser.py`.

Confirmed examples:

- end-form aliases:
  - `END_THE_TRAIT`
  - `END_THE_INTERFACE`
  - `END_LOOP`
  - `END_CONCURRENT`
  - `END_TRY`
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

### 1. The main typechecker is narrower than the interpreter

`src/nexuslang/typesystem/typechecker.py` uses a limited explicit dispatch in `check_statement` plus several grouped helper buckets.

Confirmed absent direct handling for these newer or advanced features:

- channels (`SendStatement`, `ReceiveExpression`, `ChannelCreation`)
- unsafe blocks (`UnsafeBlock`)
- yield/generator support (`YieldExpression`, `GeneratorExpression`)
- macro/comptime nodes
- contract/assertion nodes

Why it matters:

The typechecker cannot be described as feature-complete if major language constructs are invisible to it.

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

### 3. LSP diagnostics depend on incomplete lower layers

`src/nexuslang/lsp/diagnostics.py` uses:

- parser-based syntax checks
- typechecker-based diagnostics
- regex-style additional checks such as unused variables

Why it matters:

Diagnostics can only be as complete as the parser/typechecker coverage beneath them. For advanced features, the LSP currently inherits those gaps.

## Compiler Backend Gaps

### 1. LLVM backend is substantially behind the interpreter on advanced features

Confirmed examples:

- no direct channel node handling found
- no direct parallel-for handling found
- no direct contract node handling found
- no direct macro/comptime node handling found
- generator support exists, but is explicitly approximate rather than complete

Key file:

- `src/nexuslang/compiler/backends/llvm_ir_generator.py`

Why it matters:

The LLVM path is functional for core constructs, but it is not yet feature-complete relative to the interpreter.

### 2. C backend appears significantly narrower than the language surface

A broad AST-to-backend scan showed the C generator covers a much smaller subset of node types than the interpreter.

Key file:

- `src/nexuslang/compiler/backends/c_generator.py`

Why it matters:

If the C backend remains intentionally limited, that scope should be documented explicitly. If not, it needs a dedicated expansion plan.

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

### 2. LSP feature breadth is ahead of feature-specific semantic depth

`README.md` claims 25 LSP features and describes the tooling ecosystem as comprehensive.

Confirmed current reality:

- a real LSP exists in `src/nexuslang/lsp/`
- diagnostics, formatter, hover, rename, references, symbols, and other modules exist
- no direct feature-specific references were found in `src/nexuslang/lsp/*.py` for advanced nodes such as:
  - `SendStatement`
  - `ReceiveExpression`
  - `ChannelCreation`
  - `MacroExpansion`
  - `ComptimeExpression`
  - `YieldExpression`
  - `UnsafeBlock`

Why it matters:

The LSP is real and valuable, but “comprehensive” should be interpreted cautiously until feature-matrix parity is established for newer and more advanced constructs.

### 3. Diagnostics and formatting are only partly semantic

Confirmed from `src/nexuslang/lsp/diagnostics.py` and `src/nexuslang/lsp/formatter.py`:

- diagnostics combine parser and typechecker output with heuristic checks
- formatting is regex-based

Why it matters:

As the language surface expands, these tools need to migrate from heuristic support to AST-driven and feature-aware support.

## Documentation and Positioning Gaps

### 1. README claims are stronger than the current cross-toolchain evidence

`README.md` currently states or strongly implies:

- parser handles the full syntax surface
- interpreter executes all language constructs
- type system is working
- tooling ecosystem is comprehensive
- LLVM backend compiles core language constructs to native binaries

The core issue is not that these statements are entirely false. The issue is that they flatten the difference between:

- core constructs
- interpreter-only constructs
- partially compiled constructs
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
