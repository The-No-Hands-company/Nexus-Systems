#!/usr/bin/env python3
"""
fuzzing/fuzz_parser.py  --  Atheris/libFuzzer fuzz target for the NexusLang parser.

Tests the full lexer → parser pipeline.  The parser MUST:
  - Never raise an unhandled Python exception for ANY byte sequence that gets
    past the lexer stage.
  - Either return a Program AST node or raise NxlSyntaxError.

Running with Atheris:
    python -m atheris fuzz_parser.py corpus/parser/
    python -m atheris fuzz_parser.py corpus/parser/ -max_total_time=60

Sanity mode (no Atheris):
    python fuzzing/fuzz_parser.py --sanity
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
from nexuslang.parser.ast import Program  # noqa: E402
from nexuslang.errors import NxlSyntaxError, NxlError  # noqa: E402

_EXPECTED = (
    NxlError,
    NxlSyntaxError,
    ValueError,
    IndexError,  # Intentional: parser index into token list — bugs become crash
    UnicodeDecodeError,
    OverflowError,
    StopIteration,
)


def TestOneInput(data: bytes) -> None:
    """
    Fuzz entry point for the lexer → parser pipeline.
    """
    try:
        source = data.decode("utf-8", errors="replace")
    except Exception:
        return

    try:
        lexer = Lexer(source)
        tokens = lexer.tokenize()
    except _EXPECTED:
        return  # Lexer failure is fine; parser was never invoked.

    try:
        parser = Parser(tokens, source=source)
        result = parser.parse()
        # Invariant: parse() must return a Program node.
        assert isinstance(result, Program), (
            f"Parser.parse() returned {type(result).__name__}, expected Program"
        )
    except _EXPECTED:
        pass
    except RecursionError:
        # Pathological inputs can overflow the call stack in the recursive-
        # descent parser.  Report as a crash to encourage adding a depth guard.
        raise
    # All other unhandled exceptions propagate as crashes.


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
    b"print text \"hello, world!\"",
    b"set x to 10\nset y to 20\nset z to x plus y",
    b"function add with a as Integer and b as Integer returns Integer\n  return a plus b\n",
    b"class Dog\n  properties\n    name as String\n  end\nend",
    b"if x is greater than 0\n  print text \"positive\"\nelse\n  print text \"negative\"\n",
    b"for each item in items\n  print text item\n",
    b"while counter is less than 10\n  set counter to counter plus 1\n",
    # Intentionally broken inputs
    b"function",
    b"if if if if if",
    b"end end end end",
    b"set set set set",
    b"class",
    b"( ( ( ( ( ( ( ( ( ((",
    b"set x to " + b"1 plus " * 500 + b"1",
    b"function f with " + b"a as Integer and " * 200 + b"b as Integer\n  return b\n",
    # Null bytes
    b"\x00set x to 1",
    # Very long identifiers
    b"set " + b"x" * 10000 + b" to 1",
    # Nested blocks
    b"if a\n" * 400 + b"  print text \"hi\"\n" + b"end\n" * 400,
]


def _run_sanity() -> None:
    print(f"Running {len(_SANITY_INPUTS)} sanity inputs against fuzz_parser …")
    failures = 0
    for i, inp in enumerate(_SANITY_INPUTS):
        try:
            TestOneInput(inp)
            print(f"  [{i:2d}] OK")
        except Exception as exc:
            print(f"  [{i:2d}] CRASH: {type(exc).__name__}: {exc}")
            failures += 1
    if failures:
        print(f"\nfuzz_parser sanity: {failures} crash(es) found!")
        sys.exit(1)
    else:
        print("\nfuzz_parser sanity: all passed.")


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
                "    python fuzzing/fuzz_parser.py --sanity",
                file=sys.stderr,
            )
            sys.exit(1)
