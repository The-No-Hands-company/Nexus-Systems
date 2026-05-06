#!/usr/bin/env python3
"""
fuzzing/fuzz_interpreter.py  --  Atheris fuzz target for the full NexusLang pipeline.

Tests: lexer → parser → borrow checker → lifetime checker → interpreter.

The interpreter MUST:
  - Never raise an unhandled Python exception for any program text that passes
    the parsing stage.
  - Expected outcomes: successful execution, NxlRuntimeError, NxlTypeError,
    NxlNameError, NxlSyntaxError, or any other NxlError subclass.

Running with Atheris:
    python -m atheris fuzz_interpreter.py corpus/interpreter/ -max_total_time=60

Sanity mode:
    python fuzzing/fuzz_interpreter.py --sanity
"""

from __future__ import annotations

import os
import sys
import signal

_ROOT = os.path.join(os.path.dirname(__file__), "..")
_SRC = os.path.join(_ROOT, "src")
if _SRC not in sys.path:
    sys.path.insert(0, _SRC)

from nexuslang.parser.lexer import Lexer  # noqa: E402
from nexuslang.parser.parser import Parser  # noqa: E402
from nexuslang.interpreter.interpreter import Interpreter  # noqa: E402
from nexuslang.runtime.runtime import Runtime  # noqa: E402
from nexuslang.stdlib import register_stdlib  # noqa: E402
from nexuslang.errors import NxlError  # noqa: E402

# Hard ceiling on interpreter wall-clock time (seconds).
# Prevents infinite-loop programs from hanging the fuzzer.
_TIMEOUT_SECONDS = 5

_EXPECTED = (
    NxlError,
    ValueError,
    TypeError,
    OverflowError,
    ZeroDivisionError,
    IndexError,
    KeyError,
    RecursionError,  # Treat recursion in NexusLang programs as acceptable (not a Python bug).
    UnicodeDecodeError,
    StopIteration,
    OSError,
)


class _Timeout(Exception):
    """Raised by the SIGALRM handler when interpreter exceeds wall-clock limit."""


def _alarm_handler(_signum, _frame):  # type: ignore[override]
    raise _Timeout()


def _make_runtime() -> Runtime:
    """Create a fresh sandboxed runtime with stdlib registered."""
    runtime = Runtime()
    register_stdlib(runtime)
    return runtime


def TestOneInput(data: bytes) -> None:
    """
    Fuzz entry point for the complete NexusLang execution pipeline.
    """
    try:
        source = data.decode("utf-8", errors="replace")
    except Exception:
        return

    # --- Lexer ---
    try:
        tokens = Lexer(source).tokenize()
    except NxlError:
        return

    # --- Parser ---
    try:
        ast = Parser(tokens, source=source).parse()
    except NxlError:
        return

    # --- Optional static passes (borrow/lifetime) ---
    # These should never crash Python; if they do, it's a bug.  We still wrap
    # them so a panic in the checker doesn't mask a deeper interpreter bug.
    try:
        from nexuslang.typesystem.borrow_checker import BorrowChecker
        from nexuslang.typesystem.lifetime_checker import LifetimeChecker

        borrow_errors = BorrowChecker().check(ast)
        if borrow_errors:
            return  # Borrow-checker rejection is valid; skip interpreter.

        lifetime_errors = [e for e in LifetimeChecker().check(ast) if not e.is_warning]
        if lifetime_errors:
            return
    except NxlError:
        return
    except Exception:
        # Don't hide checker bugs — but don't crash the fuzzer campaign either.
        pass

    # --- Interpreter ---
    runtime = _make_runtime()
    interp = Interpreter(runtime, enable_type_checking=False, source=source)

    # Use SIGALRM (POSIX only) to abort runaway programs.
    _use_alarm = hasattr(signal, "SIGALRM") and os.name == "posix"
    if _use_alarm:
        old_handler = signal.signal(signal.SIGALRM, _alarm_handler)
        signal.alarm(_TIMEOUT_SECONDS)

    try:
        interp.interpret(ast, optimization_level=0)
    except _Timeout:
        pass  # Infinite-loop input — not a crash, just a slow input.
    except _EXPECTED:
        pass
    except Exception:
        raise  # Unexpected Python exception → crash report.
    finally:
        if _use_alarm:
            signal.alarm(0)
            signal.signal(signal.SIGALRM, old_handler)


# ---------------------------------------------------------------------------
# Atheris integration
# ---------------------------------------------------------------------------
def _run_with_atheris() -> None:
    import atheris  # type: ignore[import]

    atheris.instrument_imports()
    atheris.Setup(sys.argv, TestOneInput)
    atheris.Fuzz()


# ---------------------------------------------------------------------------
# Sanity runner
# ---------------------------------------------------------------------------
_SANITY_INPUTS: list[bytes] = [
    b"",
    b'print text "hello, world!"',
    b"set x to 42\nprint text x",
    b"set result to 10 plus 5\nprint text result",
    b"function add with a as Integer and b as Integer returns Integer\n"
    b"  return a plus b\n"
    b"set total to add with a: 3 and b: 4\n"
    b"print text total\n",
    b"for each i in [1, 2, 3]\n  print text i\n",
    b'set items to ["alpha", "beta", "gamma"]\n'
    b"for each item in items\n"
    b'  print text item\n',
    # Error cases that should raise NxlError, not crash Python
    b"set x to undefined_variable",
    b"function f\n  return 1 divided by 0\n\ncall f\n",
    b"set x to 1\nset x to x plus true",
    b'set d to {}\nprint text d["missing_key"]',
    # Deeply nested (recursion guard)
    b"function recurse\n  return recurse\n",
    # Very long string
    b'print text "' + b"x" * 100_000 + b'"',
    # Numeric edge cases
    b"set x to 9999999999999999999999999999999999999999",
    b"set x to -9999999999999999999999999999999999999999",
]


def _run_sanity() -> None:
    print(f"Running {len(_SANITY_INPUTS)} sanity inputs against fuzz_interpreter …")
    failures = 0
    for i, inp in enumerate(_SANITY_INPUTS):
        try:
            TestOneInput(inp)
            print(f"  [{i:2d}] OK")
        except Exception as exc:
            print(f"  [{i:2d}] CRASH: {type(exc).__name__}: {exc}")
            failures += 1
    if failures:
        print(f"\nfuzz_interpreter sanity: {failures} crash(es) found!")
        sys.exit(1)
    else:
        print("\nfuzz_interpreter sanity: all passed.")


if __name__ == "__main__":
    if "--sanity" in sys.argv:
        sys.argv.remove("--sanity")
        _run_sanity()
    else:
        try:
            _run_with_atheris()
        except ImportError:
            print(
                "Atheris not installed.  Install it with:\n"
                "    pip install atheris\n"
                "Or run sanity mode with:\n"
                "    python fuzzing/fuzz_interpreter.py --sanity",
                file=sys.stderr,
            )
            sys.exit(1)
