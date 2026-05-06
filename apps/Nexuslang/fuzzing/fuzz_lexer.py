#!/usr/bin/env python3
"""
fuzzing/fuzz_lexer.py  --  Atheris/libFuzzer fuzz target for the NexusLang lexer.

The lexer MUST:
  - Never raise an unhandled Python exception (no AttributeError, IndexError,
    RecursionError, or similar) for ANY byte sequence.
  - Either return a list of tokens or raise a well-typed NxlSyntaxError.

Running with Atheris (libFuzzer backend):
    python -m atheris fuzz_lexer.py corpus/lexer/  # basic run
    python -m atheris fuzz_lexer.py corpus/lexer/ -max_total_time=60

Running manually for a quick sanity check (no Atheris required):
    python fuzzing/fuzz_lexer.py --sanity
"""

from __future__ import annotations

import os
import sys

# Allow running from the project root without installation.
_ROOT = os.path.join(os.path.dirname(__file__), "..")
_SRC = os.path.join(_ROOT, "src")
if _SRC not in sys.path:
    sys.path.insert(0, _SRC)

from nexuslang.parser.lexer import Lexer  # noqa: E402
from nexuslang.errors import NxlSyntaxError, NxlError  # noqa: E402

# ---------------------------------------------------------------------------
# Accepted exceptions: these are expected when the lexer sees invalid input.
# Any OTHER exception is a bug and will crash the fuzzer (triggering a report).
# ---------------------------------------------------------------------------
_EXPECTED = (
    NxlError,
    NxlSyntaxError,
    ValueError,
    UnicodeDecodeError,
    OverflowError,
)


def TestOneInput(data: bytes) -> None:
    """
    Fuzz entry point.  Called by Atheris (or directly by tests).

    Invariant: must not raise any exception outside of _EXPECTED.
    """
    # Decode bytes to a string.  We use 'replace' so that every byte sequence
    # maps to a valid string — this is intentional: we want to test the lexer
    # against arbitrary Unicode characters, not waste cycles on decode errors.
    try:
        source = data.decode("utf-8", errors="replace")
    except Exception:
        return  # Should never happen with errors='replace', but be defensive.

    try:
        lexer = Lexer(source)
        _tokens = lexer.tokenize()
        # Basic invariant: tokenize() must return a list.
        assert isinstance(_tokens, list), (
            f"Lexer.tokenize() returned {type(_tokens).__name__}, expected list"
        )
    except _EXPECTED:
        # Expected: malformed input, unknown character, etc.
        pass
    except RecursionError:
        # Deep nesting in the source can cause recursion in the lexer.
        # Treat it as a "crash" bug to encourage fixing.
        raise
    # All other exceptions (AttributeError, IndexError, TypeError, …) propagate
    # unhandled — Atheris will report them as crashes.


# ---------------------------------------------------------------------------
# Atheris integration
# ---------------------------------------------------------------------------
def _run_with_atheris() -> None:
    import atheris  # type: ignore[import]

    atheris.instrument_imports()
    atheris.Setup(sys.argv, TestOneInput)
    atheris.Fuzz()


# ---------------------------------------------------------------------------
# Sanity runner (no Atheris required)
# ---------------------------------------------------------------------------
_SANITY_INPUTS: list[bytes] = [
    b"",
    b"print text \"hello\"",
    b"set x to 42",
    b"function greet with name as String\n  print text name\n",
    b"if x is greater than 0\n  print text \"pos\"\n",
    b"\x00\x01\x02\x03",
    b"\xff\xfe\xfd",
    b"set x to " + b"9" * 100_000,
    b"###",
    b'"unclosed string',
    b"set x to ((((",
    b"\r\n\t" * 500,
    b"\\u0000\\u0001",
    b"end end end end end",
]


def _run_sanity() -> None:
    print(f"Running {len(_SANITY_INPUTS)} sanity inputs against fuzz_lexer …")
    failures = 0
    for i, inp in enumerate(_SANITY_INPUTS):
        try:
            TestOneInput(inp)
            print(f"  [{i:2d}] OK")
        except Exception as exc:
            print(f"  [{i:2d}] CRASH: {type(exc).__name__}: {exc}")
            failures += 1
    if failures:
        print(f"\nfuzz_lexer sanity: {failures} crash(es) found!")
        sys.exit(1)
    else:
        print("\nfuzz_lexer sanity: all passed.")


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
                "    python fuzzing/fuzz_lexer.py --sanity",
                file=sys.stderr,
            )
            sys.exit(1)
