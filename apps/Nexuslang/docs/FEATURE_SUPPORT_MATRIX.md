# NexusLang Feature Support Matrix

Updated: 2026-04-30

Legend:

- ✅ complete / implemented
- ⚠️ partial / limited / inconsistent
- ❌ missing
- ➖ not applicable

Scope columns:

- Lexer: tokenization and keyword surface
- Parser+AST: syntax parse and node representation
- Interpreter: runtime execution path
- Typechecker: static checking in `TypeChecker`
- Compiler (LLVM/C): compilation support in backends
- Tooling (LSP/Fmt): language tooling support depth
- Grammar: formal grammar parity in `grammar/NLPL.g4`
- Tests: dedicated or strong coverage

| Feature Area           | Lexer | Parser+AST | Interpreter | Typechecker | Compiler (LLVM/C) | Tooling (LSP/Fmt) | Grammar | Tests |
| ---                             | ---| ---| ---         | ---         | ---               | ---               | ---     | ---   |
| Variables / assignment          | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ⚠️ | ✅ |
| Functions / calls               | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ⚠️ | ✅ |
| Classes / methods / inheritance | ✅ | ✅ | ✅ | ✅ | ✅ | ⚠️ | ⚠️ | ✅ |
| Interfaces / traits / generics  | ✅ | ✅ | ✅ | ⚠️ | ⚠️ | ⚠️ | ⚠️ | ✅ |
| Control flow basics (`if`, `while`, `for each`) | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ⚠️ | ✅ |
| Extended control flow (`switch`, labels, fallthrough) | ✅ | ⚠️ | ⚠️ | ⚠️ | ⚠️ | ⚠️ | ⚠️ | ⚠️ |
| Error handling (`try/catch`, `raise`, `panic`) | ✅ | ✅ | ✅ | ⚠️ | ⚠️ | ⚠️ | ⚠️ | ✅ |
| Contracts / assertions (`expect`, `require/ensure/...`) | ✅ | ✅ | ✅ | ⚠️ | ⚠️ | ⚠️ | ⚠️ | ✅ |
| Ownership/borrowing/lifetimes | ✅ | ✅ | ✅ | ⚠️ (split passes) | ⚠️ | ⚠️ | ⚠️ | ✅ |
| Macros / comptime | ✅ | ✅ | ✅ | ❌ | ❌ | ⚠️ | ⚠️ | ⚠️ |
| Async / await / spawn | ✅ | ✅ | ✅ | ⚠️ | ⚠️ | ⚠️ | ⚠️ | ⚠️ |
| Generators / `yield` | ✅ | ✅ | ✅ | ⚠️ | ⚠️ | ⚠️ | ⚠️ | ⚠️ |
| Parallel for | ✅ | ✅ | ✅ | ⚠️ | ⚠️ | ⚠️ | ⚠️ | ⚠️ |
| Channels (`create/send/receive`) | ✅ | ✅ | ✅ | ✅ | ✅ | ⚠️ | ⚠️ | ✅ |
| FFI / extern / unsafe | ✅ | ✅ | ✅ | ⚠️ | ⚠️ | ⚠️ | ⚠️ | ✅ |
| Inline assembly | ✅ | ✅ | ⚠️ (interp is NOP) | ⚠️ | ✅ | ⚠️ | ⚠️ | ✅ |
| LSP diagnostics | ➖ | ➖ | ➖ | ⚠️ (depends on checker) | ➖ | ⚠️ | ➖ | ✅ |
| Formatter | ➖ | ➖ | ➖ | ➖ | ➖ | ⚠️ (regex-based) | ➖ | ⚠️ |

## Immediate Priority Queue

1. Generators/yield: move from baseline lowering to full generator/coroutine semantics in compiled backends.
2. Parallel-for: deepen compiler semantics beyond sequential fallback.
3. Contracts/assertions: extend matcher coverage and add richer diagnostics/tooling.
4. Macro/comptime: define static and compilation semantics.
5. Interfaces/traits/generics: tighten conformance, specialization, and tooling depth.

## Notes

- This matrix complements, and should be maintained alongside, [FEATURE_COMPLETENESS_AUDIT.md](FEATURE_COMPLETENESS_AUDIT.md).
- For each row marked ❌ or ⚠️, create/track concrete implementation tasks with file-level targets.

### Recent Gap Fill Progress

- 2026-04-30: Channels tooling moved from ❌ to ⚠️ after adding baseline LSP completions, hover docs, and diagnostics with dedicated tooling tests.
- 2026-04-30: Channels compiler semantics hardened in C/LLVM backend integration: thread-safe blocking receive with condition-variable wakeup, close-aware receive path, and emitted close runtime declaration. Compiler remains ⚠️ until payload ownership and full close operation surface are complete.
- 2026-04-30: Channels gained language-level close operation surface (`close ch`) across parser, typechecker, interpreter, and both compiler backends. Interpreter channel payload transfer now snapshots mutable payloads to avoid post-send aliasing surprises.
- 2026-04-30: Channels compiler payload ownership contracts were completed in LLVM lowering: pointer payload transport now correctly handles pointer types, smart-pointer sends retain ownership before enqueue, and receive-bound smart-pointer payloads are tracked for cleanup in compiled code.
- 2026-04-30: Generator typechecking semantics were tightened: generator functions now use generator-aware signatures, `yield` is validated against generator element type, mixed incompatible yield types are diagnosed, and generator functions reject value-return statements.
