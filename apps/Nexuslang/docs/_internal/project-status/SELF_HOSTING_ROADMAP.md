# NexusLang Self-Hosting and Tool Rewrite Roadmap

**Current Date:** March 2, 2026  
**Project Status:** 95% toward v1.0 (Q2 2026 target)  
**Goal:** Rewrite key Python-based tools into NexusLang for full self-hosting, enabling a native toolchain independent of Python. Focus on compiler and build tools first, while keeping the interpreter optional.

## Phase 1: Pre-v1.0 Preparation (1-3 Months, Now → April-June 2026)

- Complete remaining v1.0 milestones:
  - LSP server: ✅ COMPLETE — 13+ features working including code lens, inlay hints, dead code detection, refactoring actions (Weeks 4-5 commits `7e237a1`, `ab5015a`)
  - Performance profiling: Complete the remaining 13% of the framework
  - Debugger: ✅ COMPLETE (100%) — 109 tests passing (Mar 3, 2026)
- Why first? Better IDE support and debugging will make the rewrite process smoother and less error-prone.
- Optional early prototypes: Port small utilities (e.g., benchmark_performance.py) to NexusLang as demos to validate tooling capabilities.

## Phase 2: Core Self-Hosting – Compiler Rewrite (High Priority, 2-4 Months Post-v1.0)

Rewrite the heart of the compiler in NexusLang for bootstrapping capability.

| Component                  | Location                  | Purpose                                      | Priority | Notes / Estimated Effort                  |
|----------------------------|---------------------------|----------------------------------------------|----------|-------------------------------------------|
| Lexer                      | src/lexer                 | Tokenization of NexusLang source                  | High     | ~1,060 lines; use pattern matching        |
| Parser                     | src/parser                | AST construction from tokens                 | High     | ~7,469 lines; ANTLR integration           |
| AST Nodes & Visitors       | src/ast                   | Abstract Syntax Tree representation          | High     | ~1,030 lines; generics for nodes          |
| Type Checker               | src/type_checker          | Static type analysis & inference             | High     | ~1,541 lines; leverage constraints        |
| LLVM IR Generator          | src/llvm_codegen          | LLVM IR emission for native compilation      | High     | ~10,171 lines; FFI to LLVM C API          |
| Borrow Checker & Lifetimes | dev_tools/borrow_checker  | Compile-time memory safety                   | High     | Recent addition (Feb 21); integrate tightly |

**Bootstrapping Plan:**

1. Write NexusLang versions of above components.
2. Use existing Python compiler to compile the new NexusLang compiler binary.
3. Switch to the new binary for future compilations.
4. Run equivalence tests (benchmarks, examples, full self-compile).

## Phase 3: Build & Utility Tools Rewrite (Medium Priority, 1-2 Months)

After compiler is self-hosting, port supporting tools.

| Tool/Script                | Location                        | Purpose                                      | Priority | Notes                                      |
|----------------------------|---------------------------------|----------------------------------------------|----------|--------------------------------------------|
| compile.py                 | dev_tools/scripts/              | CLI for compiling .nlpl → native binary      | High     | Core entry point for native builds         |
| benchmark_performance.py   | benchmarks/                     | Performance testing & dashboard              | Medium   | Use NexusLang stdlib math/collections           |
| Auxiliary Scripts          | scripts/ & dev_tools/           | Runtime bootstrap, Rc setup, testing utils   | Medium   | Smaller scope; can parallelize             |

## Phase 4: Advanced Tooling Rewrite (Lower Priority, 1-2 Months)

Once basics are self-hosted, enhance developer experience.

| Tool                       | Location                        | Purpose                                      | Priority | Notes                                      |
|----------------------------|---------------------------------|----------------------------------------------|----------|--------------------------------------------|
| LSP Server                 | vscode-extension/ & editors/    | IDE features (completion, go-to-def, etc.)   | Medium   | Use networking stdlib + FFI if needed      |
| Additional Dev Tools       | dev_tools/                      | Profiler integration, coverage tools         | Low      | Leverage recent additions (Feb 24)         |

## Optional / Low-Value Rewrites

- Core interpreter logic (`src/interpreter`): ~2,658 lines  
  → Not required; defeats purpose of shifting to compiled language. Keep as reference or for testing only.

## Overall Timeline Estimate (Solo Dev Pace)

- v1.0 completion: April–June 2026
- Core compiler self-hosting: Q3–Q4 2026
- Full native toolchain: End of 2026
- Total rewrite effort: ~3–9 months post-v1.0, depending on scope and testing depth

## Benefits

- Native performance for toolchain
- Full independence from Python
- Better optimization potential (e.g., for 3D suite)
- Strong flagship demo: "NLPL compiler written in NLPL"

**Recommendation:** Start small — prototype one module (e.g., a mini-parser or benchmark tool) in NexusLang now to build momentum and identify any early gaps.
