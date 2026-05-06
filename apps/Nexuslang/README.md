# NexusLang — NexusLang

> A general-purpose programming language that reads like English

[![CI](https://github.com/Zajfan/NexusLang/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/Zajfan/NexusLang/actions/workflows/ci.yml)
[![Python](https://img.shields.io/badge/python-3.11+-blue.svg)](https://www.python.org/downloads/)
[![License](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)
[![Tests](https://img.shields.io/badge/tests-4288%20passing-brightgreen)](tests/)

---

## What is NLPL?

NLPL is a programming language designed to read like English prose while compiling to native code via LLVM. It is currently under active development — the interpreter is mature, the LLVM compiler backend is functional, and the tooling ecosystem (LSP, debugger, build system) is comprehensive.

```nlpl
# Hello World
print text "Hello, world!"

# Functions read like English
function greet with name as String returns String
    if name is not empty
        return "Hello, " plus name plus "!"
    else
        return "Hello, stranger!"
    end
end

print text greet("Alice")

# Natural control flow
set scores to [95, 87, 73, 91, 88]
set total to 0
for each score in scores
    set total to total plus score
end
print text "Average: " plus (total divided by length of scores)
```

---

## Current State

NLPL is pre-v1.0 and under active development. This is an honest assessment of where things stand.

### What works reliably

| Component | Status | Notes |
|-----------|--------|-------|
| Lexer | ✅ Mature | Full Unicode, all token types |
| Parser | ✅ Mature | Handles the full syntax surface |
| Interpreter | ✅ Working | Executes all language constructs |
| Type system | ✅ Working | Inference, generics, HKT, borrow checker |
| Standard library | ✅ 85 modules | 2,219 registered functions |
| LSP server | ✅ Working | 25 features: hover, completions, diagnostics, go-to-definition, rename, and more |
| Debugger | ✅ Working | DAP-compliant, full breakpoint/step/variable inspection |
| Build system | ✅ Working | Incremental builds, dependency tracking, caching |
| LLVM backend | ✅ Functional | Compiles core language constructs to native binaries |
| Test suite | ✅ 4,288 passing | 0 failures, 145 test files |

### Known limitations

- **Performance:** The interpreter is significantly slower than C for computation-heavy code. The LLVM compiled path produces fast native code, and compiler coverage has improved, but not every language construct is fully covered with production-grade semantics yet.
- **Concurrency:** The async runtime exists and is tested, but has not been validated under heavy real-world workloads.
- **FFI safety:** The FFI layer allows raw pointer operations; safety is the caller's responsibility inside `unsafe` blocks — there is no automatic checking.
- **Self-hosting:** NexusLang is not self-hosted. The entire toolchain is written in Python.
- **Parser size:** The recursive descent parser is approximately 9,900 lines (~9,913 currently). This works but is a maintenance concern as the language evolves.

---

## Quick Start

```bash
git clone https://github.com/Zajfan/NLPL
cd NexusLang
pip install -r requirements.txt

# Run a program
PYTHONPATH=src python -m nexuslang.main examples/01_basics/01_basic_concepts.nlpl

# Start the LSP server (stdio mode for editor integration)
PYTHONPATH=src python -m nexuslang.lsp --stdio

# Interactive REPL
PYTHONPATH=src python -m nexuslang.repl

# Run the full test suite
PYTHONPATH=src python -m pytest tests/
```

See [QUICKSTART.md](QUICKSTART.md) for full setup instructions.

---

## Language Overview

### Variables

```nlpl
set name to "Alice"
set age as Integer to 30
set score as Float to 98.6
set active as Boolean to true
```

### Control flow

```nlpl
if age is greater than 18
    print text "Adult"
else if age is greater than 12
    print text "Teen"
else
    print text "Child"
end

for each item in my_list
    print text item
end

repeat while score is greater than 0
    set score to score minus 1
end
```

### Functions

```nlpl
function add with a as Integer, b as Integer returns Integer
    return a plus b
end
```

### Classes

```nlpl
class Animal
    set name as String to ""

    function speak
        print text name plus " speaks"
    end
end

class Dog extends Animal
    function speak
        print text name plus " says woof"
    end
end
```

### Pattern matching

```nlpl
match value
    case Integer
        print text "number: " plus value
    case String if length of value is greater than 10
        print text "long string"
    case _
        print text "other"
end
```

### Error handling

```nlpl
try
    set result to risky_operation()
catch error as e
    print text "Error: " plus e
end
```

### Generics

```nlpl
class Stack with T :: *
    set items as List[T] to []

    function push with item as T
        append item to items
    end
end
```

---

## Standard Library

85 modules covering: algorithms, async I/O, audio, build tools, cache, collections, compression, crypto, CSV, databases, datetime, environment, FFI, file I/O, filesystem, graphics, HTTP, image processing, JSON, linear algebra, logging, math, math3d, networking, numerical integration, option/result types, parallel computing, PDF, platform-specific (Linux/macOS/Windows), plotting, property testing, random, reflection, regex, scientific computing, serialization, SIMD, smart pointers, SQLite, statistics, strings, subprocess, sync primitives, system, testing, threading, type traits, UUID, validation, WebSocket, XML.

---

## Tooling

### Editor support

- **VS Code** — Extension in `vscode-extension/` (syntax highlighting, LSP)
- **Neovim** — Config in `editors/neovim/`
- **Emacs** — Mode in `editors/emacs/`
- **Sublime Text** — Syntax in `editors/sublime-text/`

### LSP (Language Server Protocol)

25 features implemented: hover, completions, go-to-definition, find references, rename, signature help, code actions, semantic tokens, inlay hints, code lens, document symbols, workspace symbols, diagnostics, formatting, dead code detection, call hierarchy.

---

## Architecture

```
Source → Lexer → Parser → AST → Optimizer → Interpreter
                                           → LLVM IR Generator → llc/clang → native binary
```

| Path | Description |
|------|-------------|
| `src/nlpl/parser/lexer.py` | Tokenizer |
| `src/nlpl/parser/parser.py` | Recursive descent parser |
| `src/nlpl/parser/ast.py` | 139 AST node types |
| `src/nlpl/interpreter/interpreter.py` | Tree-walking interpreter |
| `src/nlpl/typesystem/` | Type checker, inference engine, generics, HKT |
| `src/nlpl/compiler/backends/llvm_ir_generator.py` | LLVM IR code generation |
| `src/nlpl/lsp/server.py` | LSP server |
| `src/nlpl/stdlib/` | 85 standard library modules |
| `tests/` | 145 test files, 4,288 passing tests |

---

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md). The test suite requires `pytest` and `pytest-timeout`:

```bash
pip install pytest pytest-timeout
PYTHONPATH=src python -m pytest tests/
```

---

## License

MIT — see [LICENSE](LICENSE).
