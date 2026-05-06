# NexusLang Quick Start

## Requirements

- Python 3.11 or newer
- `pip install -r requirements.txt`
- For compiled mode: `llvm` and `clang` (optional, for native code generation)

## Installation

```bash
git clone https://github.com/Zajfan/NLPL
cd NexusLang
pip install -r requirements.txt
```

## Running programs

```bash
# Interpreter mode (recommended for development)
PYTHONPATH=src python -m nexuslang.main my_program.nlpl

# With type checking disabled (faster)
PYTHONPATH=src python -m nexuslang.main --no-type-check my_program.nlpl
```

## Interactive REPL

```bash
PYTHONPATH=src python -m nexuslang.repl
```

## Running examples

```bash
# Hello world
PYTHONPATH=src python -m nexuslang.main examples/01_basics/01_basic_concepts.nlpl

# Data analysis showcase
PYTHONPATH=src python -m nexuslang.main showcase/data_processor/analyze.nlpl

# Source code analyzer
PYTHONPATH=src python -m nexuslang.main showcase/source_analyzer/analyze.nlpl
```

## Running the test suite

```bash
pip install pytest pytest-timeout
PYTHONPATH=src python -m pytest tests/
```

## Editor setup

### VS Code

1. Install the extension from `vscode-extension/`
2. The LSP server starts automatically when you open a `.nlpl` file

### Neovim / Emacs

See `editors/neovim/` and `editors/emacs/` for configuration files.

### Manual LSP (any editor supporting LSP)

```bash
PYTHONPATH=src python -m nexuslang.lsp --stdio
```

## Compiled mode (optional)

Install LLVM and clang, then:

```bash
PYTHONPATH=src python -m nexuslang.main --compile my_program.nlpl
```

This emits LLVM IR, compiles to native code, and runs the binary.

## Getting help

- See `docs/guide/` for language documentation
- See `docs/tutorials/` for step-by-step tutorials
- See `docs/reference/` for the language specification
