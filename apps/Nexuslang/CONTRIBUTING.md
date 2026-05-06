# Contributing to NexusLang

Thank you for your interest in contributing to NexusLang (NexusLang). This document covers the essentials for getting started. Detailed guides live in [docs/contributing/](docs/contributing/).

---

## Table of Contents

- [Prerequisites](#prerequisites)
- [Development Setup](#development-setup)
- [Running the Tests](#running-the-tests)
- [Project Structure](#project-structure)
- [Making a Change](#making-a-change)
- [Code Style](#code-style)
- [Submitting a Pull Request](#submitting-a-pull-request)
- [Reporting Issues](#reporting-issues)
- [License](#license)

---

## Prerequisites

- Python 3.11 or later
- Git

---

## Development Setup

```bash
git clone https://github.com/The-No-hands-Company/NLPL.git
cd NexusLang

python -m venv .venv
source .venv/bin/activate          # Windows: .venv\Scripts\activate

pip install -r requirements.txt
```

Verify everything works:

```bash
python src/main.py examples/01_basic_concepts.nlpl
```

See [docs/contributing/development-setup.md](docs/contributing/development-setup.md) for a full environment walkthrough including optional tooling (LSP, VS Code extension, benchmarks).

---

## Running the Tests

```bash
# All tests
pytest tests/

# Specific area
pytest tests/unit/compiler/
pytest tests/unit/type_system/
pytest tests/unit/stdlib/
pytest tests/tooling/

# With coverage
pytest tests/ --cov=src/nlpl --cov-report=term-missing

# Verbose
pytest tests/ -v
```

All changes must keep the full test suite passing. New features must include tests.

---

## Project Structure

```
src/nlpl/
  parser/          # Lexer, parser, AST definitions
  interpreter/     # AST execution engine
  runtime/         # Memory management, object model
  stdlib/          # Standard library modules
  typesystem/      # Optional type checking
  lsp/             # Language server (LSP) implementation
  analysis/        # Symbol extraction, workspace indexing
  errors.py        # Enhanced error reporting

tests/
  unit/            # Unit tests by component
  integration/     # Cross-feature integration tests
  tooling/         # LSP, build system, IDE tool tests
  fixtures/        # .nlpl test fixture files

examples/          # Numbered tutorial programs (01_basic_concepts.nlpl ...)
test_programs/     # Test programs by category (unit/, integration/, regression/)
docs/              # Language documentation and internal dev notes
```

See [docs/contributing/architecture.md](docs/contributing/architecture.md) for a detailed walkthrough of the compiler pipeline and key design decisions.

---

## Making a Change

### Adding a language feature

1. Add token type to `src/nlpl/parser/lexer.py` (`TokenType` enum + `keyword_map`)
2. Add AST node to `src/nlpl/parser/ast.py`
3. Add parser method to `src/nlpl/parser/parser.py`
4. Add interpreter handler to `src/nlpl/interpreter/interpreter.py`
5. Add tests in `tests/unit/`
6. Add an example program in `examples/` (numbered, balanced across domains)

See [docs/contributing/compiler-guide.md](docs/contributing/compiler-guide.md) for step-by-step instructions with code examples.

### Adding a standard library function

1. Add implementation to the relevant module in `src/nlpl/stdlib/<module>/`
2. Register it in `src/nlpl/stdlib/__init__.py` via `register_<module>_functions(runtime)`
3. Add tests under `tests/unit/stdlib/`

### Fixing a bug

1. Add a regression test that reproduces the bug before fixing it
2. Fix the root cause (no workarounds)
3. Confirm the new test passes and no existing tests regress

---

## Code Style

- **Formatter:** [Black](https://black.readthedocs.io/) — `black src/ tests/`
- **Import sorter:** [isort](https://pycqa.github.io/isort/) — `isort src/ tests/`
- **Linter:** [Flake8](https://flake8.pycqa.org/) — `flake8 src/ tests/`

No emojis or Unicode decorative symbols in source code, docstrings, comments, test files, or example programs. Plain ASCII only in all permanent project artifacts.

See [docs/contributing/style-guide.md](docs/contributing/style-guide.md) for full conventions.

---

## Submitting a Pull Request

1. Fork the repository and create a branch from `main`:
   ```
   git checkout -b feat/my-feature
   ```
2. Make your changes with clear, atomic commits.
3. Run the full test suite and fix any failures.
4. Run the formatter and linter.
5. Open a pull request against `main` with:
   - A clear title describing what changed
   - A description of why the change is needed
   - A reference to any related issue (`Closes #123`)

Keep pull requests focused — one logical change per PR. Large features should be broken into smaller reviewable chunks.

---

## Reporting Issues

Use [GitHub Issues](https://github.com/The-No-hands-Company/NexusLang/issues). Include:

- NexusLang version / git commit hash
- Python version
- Minimal reproducing program (`.nlpl` source)
- Expected behavior vs actual behavior
- Full error output (run with `--debug` for extra context)

---

## License

By contributing, you agree that your contributions will be licensed under the same license as this project. See [LICENSE](LICENSE) for details.
