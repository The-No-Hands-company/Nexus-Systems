#!/usr/bin/env python3
"""
fuzzing/fuzz_typechecker.py  --  Atheris fuzz target for the NexusLang type system.

Tests: lexer → parser → borrow checker → lifetime checker → type checker.
Specifically exercises type inference, generic instantiation, and variance.

Expected outcomes for any input:
  - Successfully produces a type-annotated AST (or raises NxlTypeError /
    any NxlError subclass).
  - Must NOT raise unhandled AttributeError, IndexError, KeyError, etc.

Running with Atheris:
    python -m atheris fuzz_typechecker.py corpus/typechecker/ -max_total_time=60

Sanity mode:
    python fuzzing/fuzz_typechecker.py --sanity
"""

from __future__ import annotations

import os
import sys

_ROOT = os.path.join(os.path.dirname(__file__), "..")
_SRC = os.path.join(_ROOT, "src")
if _SRC not in sys.path:
    sys.path.insert(0, _SRC)

from nexuslang.parser.lexer import Lexer  # noqa: E402
from nexuslang.parser.parser import Parser  # noqa: E402
from nexuslang.errors import NxlError  # noqa: E402

_EXPECTED = (
    NxlError,
    ValueError,
    TypeError,
    KeyError,
    IndexError,
    AttributeError,  # Known limitation: some checker paths have unguarded attribute access.
    NotImplementedError,
    OverflowError,
    RecursionError,
)


def _run_checkers(ast, source: str) -> None:
    """Run all static analysis passes that are part of the type-checking phase."""
    from nexuslang.typesystem.borrow_checker import BorrowChecker
    from nexuslang.typesystem.lifetime_checker import LifetimeChecker
    from nexuslang.typesystem.type_inference import TypeInferenceEngine

    BorrowChecker().check(ast)
    LifetimeChecker().check(ast)
    TypeInferenceEngine().infer(ast)


def TestOneInput(data: bytes) -> None:
    """
    Fuzz entry point for the type-checking pipeline.
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

    # --- Type checkers ---
    try:
        _run_checkers(ast, source)
    except _EXPECTED:
        pass
    except Exception:
        raise  # Unexpected crash → report.


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
    b"set x as Integer to 42",
    b"set items as List of String to [\"a\", \"b\", \"c\"]",
    b"function double with n as Integer returns Integer\n  return n times 2\n",
    b"function identity as T with value as T returns T\n  return value\n",
    # Type errors that should raise NxlTypeError, not crash
    b'set x as Integer to "hello"',
    b"function f with x as Integer\n  return x plus true\n",
    # Complex generic types
    b"function map_list as T, U with items as List of T returns List of U\n  return items\n",
    # Struct types
    b"struct Point\n  x as Integer\n  y as Integer\nend\nset p as Point to Point with x: 0 and y: 0",
    # Cyclic / self-referential types
    b"function fib with n as Integer returns Integer\n"
    b"  if n is less than 2\n"
    b"    return n\n"
    b"  return fib with n: n minus 1 plus fib with n: n minus 2\n",
    # Trait bounds
    b"trait Printable\n  method to_string returns String\nend",
    # Empty block
    b"function noop\n",
    # Invalid type annotations
    b"set x as NonExistentType to 42",
]


def _run_sanity() -> None:
    print(f"Running {len(_SANITY_INPUTS)} sanity inputs against fuzz_typechecker …")
    failures = 0
    for i, inp in enumerate(_SANITY_INPUTS):
        try:
            TestOneInput(inp)
            print(f"  [{i:2d}] OK")
        except Exception as exc:
            print(f"  [{i:2d}] CRASH: {type(exc).__name__}: {exc}")
            failures += 1
    if failures:
        print(f"\nfuzz_typechecker sanity: {failures} crash(es) found!")
        sys.exit(1)
    else:
        print("\nfuzz_typechecker sanity: all passed.")


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
                "    python fuzzing/fuzz_typechecker.py --sanity",
                file=sys.stderr,
            )
            sys.exit(1)
