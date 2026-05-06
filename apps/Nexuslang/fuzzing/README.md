# NexusLang Fuzzing Infrastructure

Continuous fuzz testing for the NexusLang compiler pipeline. Five independent harnesses
cover every major trust boundary: lexer input, parser input, interpreter execution,
type checker analysis, and FFI type marshalling.

## Quick Start

### Sanity mode (no extra dependencies)

Runs each harness against a small built-in corpus to verify they do not crash on
known inputs. Works without Atheris.

```bash
# All targets
./scripts/run_fuzzing.sh --sanity

# Single target
python fuzzing/fuzz_lexer.py --sanity
```

### Fuzz mode (requires Atheris)

```bash
pip install atheris

# All targets, 60 s each
./scripts/run_fuzzing.sh --target all --timeout 60

# Single target, 5 minutes
./scripts/run_fuzzing.sh --target parser --timeout 300
```

### Run via pytest (normal CI)

The harnesses are wrapped in a pytest test suite that runs in every CI pipeline:

```bash
pytest tests/fuzz/ -v
```

---

## Targets

| Target | File | What it tests |
|--------|------|---------------|
| `lexer` | `fuzzing/fuzz_lexer.py` | `Lexer.tokenize()` on arbitrary bytes |
| `parser` | `fuzzing/fuzz_parser.py` | Lexer + `Parser.parse()` pipeline |
| `interpreter` | `fuzzing/fuzz_interpreter.py` | Full pipeline including borrow/lifetime checks + execution |
| `typechecker` | `fuzzing/fuzz_typechecker.py` | BorrowChecker + LifetimeChecker + TypeInferenceEngine |
| `ffi` | `fuzzing/fuzz_ffi_marshal.py` | FFIManager.map_type, register_callback, to_c_string |

---

## Directory Structure

```
fuzzing/
    fuzz_lexer.py           Lexer harness
    fuzz_parser.py          Parser harness
    fuzz_interpreter.py     Interpreter harness (SIGALRM timeout: 5 s per input)
    fuzz_typechecker.py     Type checker harness
    fuzz_ffi_marshal.py     FFI marshal harness (3 sub-fuzzers)
    __init__.py

    corpus/
        lexer/              Seed inputs for the lexer corpus
        parser/             Seed inputs for the parser corpus
        interpreter/        Seed inputs for the interpreter corpus
        typechecker/        Seed inputs for the type-checker corpus
        ffi/                Seed inputs for the FFI marshal corpus

    crashes/                Crash artifacts (gitignored)

tests/fuzz/
    test_fuzz_harnesses.py  Pytest unit tests for all 5 harnesses

.github/workflows/
    fuzzing.yml             Weekly / manual-dispatch fuzzing CI

scripts/
    run_fuzzing.sh          Local convenience script
```

---

## Reproducing a Crash

If Atheris or the CI workflow finds a crash, the artifact file is saved under
`fuzzing/crashes/<target>_<hash>`. Reproduce it with:

```bash
python fuzzing/fuzz_<target>.py <crash-file>
```

For example:

```bash
python fuzzing/fuzz_parser.py fuzzing/crashes/parser_abc123
```

The harness will re-run `TestOneInput` on the file bytes and re-raise the
unexpected exception with its full traceback.

---

## Exception Policy

Each harness distinguishes expected failures (normal NexusLang error conditions) from
unexpected Python exceptions (bugs):

| Category | Treatment |
|----------|-----------|
| `NxlError`, `NxlSyntaxError`, `NxlRuntimeError`, `NxlTypeError`, `NxlNameError` | Silently ignored (correct language error) |
| `ValueError`, `UnicodeDecodeError`, `OverflowError` | Silently ignored |
| `RecursionError` | Silently ignored in interpreter harness (infinite NexusLang recursion is acceptable) |
| Any other Python exception | Re-raised (treated as a bug, causes fuzzer to save crash file) |

---

## Adding Corpus Inputs

Seed inputs for libFuzzer live in `fuzzing/corpus/<target>/`. Each file should
contain a single NexusLang snippet (raw UTF-8 text). Aim for:

- Small: 50-200 bytes is ideal; libFuzzer mutates from here
- Diverse: cover different language features per target
- Corner-case-rich: nested structures, empty input, max-length tokens

```bash
echo 'set x to 42' > fuzzing/corpus/lexer/set_variable
echo 'function f returns Integer\n  return 0' > fuzzing/corpus/parser/simple_function
```

---

## CI Integration

Fuzzing is split into two CI workflows:

### Normal CI (`ci.yml`)

Runs `tests/fuzz/` via pytest on every push and pull request. Uses sanity mode
only (no Atheris required). Takes < 30 seconds.

### Fuzzing CI (`fuzzing.yml`)

Runs actual Atheris fuzzing on a schedule (every Sunday at 02:00 UTC) or on
manual dispatch. Each target runs for `FUZZ_TIMEOUT` seconds (default: 120).

Trigger manually from GitHub Actions with a custom timeout:

```
Actions -> Fuzzing -> Run workflow -> timeout_seconds: 300
```

Crash artifacts are uploaded and a summary is posted to the GitHub step summary.

---

## Development Notes

- **Interpreter timeout**: `fuzz_interpreter.py` installs a `SIGALRM` handler that
  kills runaway programs after 5 seconds. This is POSIX-only; the harness degrades
  gracefully on Windows.
- **FFI safety**: `fuzz_ffi_marshal.py` tests only the marshalling layer
  (`map_type`, `register_callback`, `to_c_string`). It does not load real C
  libraries or call live C functions, so it is safe on all platforms.
- **Atheris availability**: All harnesses fall back to `--sanity` mode when Atheris
  is not installed. Atheris requires Linux and a compatible LLVM version; see the
  [Atheris repo](https://github.com/google/atheris) for installation details.
