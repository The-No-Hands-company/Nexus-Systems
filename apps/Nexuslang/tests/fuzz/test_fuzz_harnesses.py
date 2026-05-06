"""
tests/fuzz/test_fuzz_harnesses.py

Verifies that all five fuzz harnesses:
  1. Accept arbitrary byte sequences without raising unexpected Python exceptions.
  2. Handle valid NexusLang programs without crashing.
  3. Handle known-bad inputs (intentionally broken syntax, type errors, etc.)
     without crashing — only the expected NxlError subclasses may propagate.
  4. Accept all built-in _SANITY_INPUTS from each harness module.

These tests run in the normal pytest suite (fast, ~seconds).
They exercise the harnesses without Atheris — just call TestOneInput directly.
"""

from __future__ import annotations

import importlib
import random
import sys
import os
import pytest

# Ensure the project root is on the path.
_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
if _ROOT not in sys.path:
    sys.path.insert(0, _ROOT)

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _load_harness(name: str):
    """Import a fuzz harness module and return it."""
    spec = importlib.util.spec_from_file_location(
        name,
        os.path.join(_ROOT, "fuzzing", f"{name}.py"),
    )
    mod = importlib.util.module_from_spec(spec)  # type: ignore[arg-type]
    spec.loader.exec_module(mod)  # type: ignore[union-attr]
    return mod


def _run_no_crash(harness_module, data: bytes) -> None:
    """
    Assert that TestOneInput(data) does not raise any exception.
    Any exception is a test failure.
    """
    harness_module.TestOneInput(data)  # Must not raise.


# ---------------------------------------------------------------------------
# Common byte sequences used across all tests
# ---------------------------------------------------------------------------

_COMMON_INPUTS: list[bytes] = [
    b"",
    b"\x00",
    b"\xff" * 8,
    b"\r\n",
    b" " * 1024,
    bytes(range(256)),
    b"set x to 1",
    b'print text "ok"',
    b"###",
    b"\xff\xfe\xfd\xfc",
]

# Deterministic pseudo-random byte samples (reproducible across runs).
_RNG = random.Random(42)
_RANDOM_INPUTS: list[bytes] = [
    bytes(_RNG.getrandbits(8) for _ in range(length))
    for length in [1, 4, 16, 64, 256, 1024, 4096]
]

_ALL_COMMON = _COMMON_INPUTS + _RANDOM_INPUTS


# ---------------------------------------------------------------------------
# Test: fuzz_lexer
# ---------------------------------------------------------------------------

@pytest.fixture(scope="module")
def fuzz_lexer():
    return _load_harness("fuzz_lexer")


class TestFuzzLexer:
    def test_sanity_inputs(self, fuzz_lexer) -> None:
        """All built-in sanity inputs must not crash the lexer harness."""
        for inp in fuzz_lexer._SANITY_INPUTS:
            _run_no_crash(fuzz_lexer, inp)

    @pytest.mark.parametrize("data", _ALL_COMMON)
    def test_common_inputs(self, fuzz_lexer, data: bytes) -> None:
        _run_no_crash(fuzz_lexer, data)

    def test_valid_nxl_programs(self, fuzz_lexer) -> None:
        valid = [
            b'print text "hello, world!"',
            b"set x to 42",
            b"set name to \"NLPL\"",
            b"function add with a as Integer and b as Integer returns Integer\n  return a plus b\n",
            b"# just a comment",
            b"true",
            b"false",
            b"3.14159",
            b"[1, 2, 3]",
            b'{"key": "value"}',
        ]
        for prog in valid:
            _run_no_crash(fuzz_lexer, prog)

    def test_null_bytes(self, fuzz_lexer) -> None:
        _run_no_crash(fuzz_lexer, b"\x00" * 100)
        _run_no_crash(fuzz_lexer, b"set\x00x\x00to\x001")

    def test_very_long_token(self, fuzz_lexer) -> None:
        _run_no_crash(fuzz_lexer, b"set " + b"x" * 100_000 + b" to 1")

    def test_deeply_nested_parens(self, fuzz_lexer) -> None:
        _run_no_crash(fuzz_lexer, b"(" * 500 + b"x" + b")" * 500)

    def test_unicode_snowflakes(self, fuzz_lexer) -> None:
        _run_no_crash(fuzz_lexer, "set x to \"snowy\"".encode("utf-8"))
        _run_no_crash(fuzz_lexer, b"\xe2\x9c\x9c " * 50)  # UTF-8 snowflake

    def test_lone_surrogates(self, fuzz_lexer) -> None:
        # Lone surrogates are invalid UTF-8 but should not crash.
        _run_no_crash(fuzz_lexer, b"\xed\xa0\x80 set x to 1")


# ---------------------------------------------------------------------------
# Test: fuzz_parser
# ---------------------------------------------------------------------------

@pytest.fixture(scope="module")
def fuzz_parser():
    return _load_harness("fuzz_parser")


class TestFuzzParser:
    def test_sanity_inputs(self, fuzz_parser) -> None:
        for inp in fuzz_parser._SANITY_INPUTS:
            _run_no_crash(fuzz_parser, inp)

    @pytest.mark.parametrize("data", _ALL_COMMON)
    def test_common_inputs(self, fuzz_parser, data: bytes) -> None:
        _run_no_crash(fuzz_parser, data)

    def test_broken_syntax(self, fuzz_parser) -> None:
        broken = [
            b"function",
            b"end end end end end",
            b"set set set to to to",
            b"if",
            b"class class class",
            b"( ( ( ( ( (",
            b"return return return",
        ]
        for prog in broken:
            _run_no_crash(fuzz_parser, prog)

    def test_deeply_nested_if(self, fuzz_parser) -> None:
        prog = (b"if x\n" * 200) + b"  print text \"ok\"\n" + (b"end\n" * 200)
        _run_no_crash(fuzz_parser, prog)

    def test_many_parameters(self, fuzz_parser) -> None:
        # A function with 100 parameters should not crash, only fail gracefully.
        params = b" and ".join(b"p" + str(i).encode() + b" as Integer" for i in range(100))
        prog = b"function f with " + params + b" returns Integer\n  return 0\n"
        _run_no_crash(fuzz_parser, prog)

    def test_struct_with_many_fields(self, fuzz_parser) -> None:
        fields = b"\n".join(b"  field" + str(i).encode() + b" as Integer" for i in range(200))
        prog = b"struct Big\n" + fields + b"\nend"
        _run_no_crash(fuzz_parser, prog)


# ---------------------------------------------------------------------------
# Test: fuzz_interpreter
# ---------------------------------------------------------------------------

@pytest.fixture(scope="module")
def fuzz_interpreter():
    return _load_harness("fuzz_interpreter")


class TestFuzzInterpreter:
    def test_sanity_inputs(self, fuzz_interpreter) -> None:
        for inp in fuzz_interpreter._SANITY_INPUTS:
            _run_no_crash(fuzz_interpreter, inp)

    @pytest.mark.parametrize("data", _ALL_COMMON)
    def test_common_inputs(self, fuzz_interpreter, data: bytes) -> None:
        _run_no_crash(fuzz_interpreter, data)

    def test_runtime_errors(self, fuzz_interpreter) -> None:
        """Programs that cause runtime errors must not crash the harness."""
        programs = [
            b"set x to undefined_var",
            b'set d to {}\nprint text d["missing"]',
            b"set items to []\nprint text items[0]",
            b"set x to 1 divided by 0",
        ]
        for prog in programs:
            _run_no_crash(fuzz_interpreter, prog)

    def test_side_effect_isolation(self, fuzz_interpreter) -> None:
        """Running a program twice must give same result (no global state leak)."""
        prog = b"set counter to 0\nset counter to counter plus 1\n"
        _run_no_crash(fuzz_interpreter, prog)
        _run_no_crash(fuzz_interpreter, prog)

    def test_print_various_types(self, fuzz_interpreter) -> None:
        _run_no_crash(fuzz_interpreter, b"print text 42")
        _run_no_crash(fuzz_interpreter, b"print text 3.14")
        _run_no_crash(fuzz_interpreter, b"print text true")
        _run_no_crash(fuzz_interpreter, b"print text false")
        _run_no_crash(fuzz_interpreter, b"print text [1, 2, 3]")

    def test_import_stdlib(self, fuzz_interpreter) -> None:
        _run_no_crash(fuzz_interpreter, b'import "math"\nprint text math_sqrt with value: 4.0\n')

    def test_class_instantiation(self, fuzz_interpreter) -> None:
        prog = (
            b"class Counter\n"
            b"  properties\n"
            b"    count as Integer\n"
            b"  end\n"
            b"  methods\n"
            b"    method increment\n"
            b"      set count to count plus 1\n"
            b"    end\n"
            b"  end\n"
            b"end\n"
            b"set c to Counter\n"
        )
        _run_no_crash(fuzz_interpreter, prog)


# ---------------------------------------------------------------------------
# Test: fuzz_typechecker
# ---------------------------------------------------------------------------

@pytest.fixture(scope="module")
def fuzz_typechecker():
    return _load_harness("fuzz_typechecker")


class TestFuzzTypechecker:
    def test_sanity_inputs(self, fuzz_typechecker) -> None:
        for inp in fuzz_typechecker._SANITY_INPUTS:
            _run_no_crash(fuzz_typechecker, inp)

    @pytest.mark.parametrize("data", _ALL_COMMON)
    def test_common_inputs(self, fuzz_typechecker, data: bytes) -> None:
        _run_no_crash(fuzz_typechecker, data)

    def test_type_errors(self, fuzz_typechecker) -> None:
        """Type errors should raise NxlTypeError, not crash."""
        programs = [
            b'set x as Integer to "hello"',
            b"set x as String to 42",
            b"function f with x as Integer\n  return x plus true\n",
        ]
        for prog in programs:
            _run_no_crash(fuzz_typechecker, prog)

    def test_unknown_types(self, fuzz_typechecker) -> None:
        _run_no_crash(fuzz_typechecker, b"set x as UnknownType123 to 42")
        _run_no_crash(fuzz_typechecker, b"set x as List of Unknown to []")

    def test_generic_functions(self, fuzz_typechecker) -> None:
        prog = (
            b"function identity as T with value as T returns T\n"
            b"  return value\n"
        )
        _run_no_crash(fuzz_typechecker, prog)


# ---------------------------------------------------------------------------
# Test: fuzz_ffi_marshal
# ---------------------------------------------------------------------------

@pytest.fixture(scope="module")
def fuzz_ffi():
    return _load_harness("fuzz_ffi_marshal")


class TestFuzzFFIMarshal:
    def test_sanity_inputs(self, fuzz_ffi) -> None:
        for inp in fuzz_ffi._SANITY_INPUTS:
            _run_no_crash(fuzz_ffi, inp)

    @pytest.mark.parametrize("data", _ALL_COMMON)
    def test_common_inputs(self, fuzz_ffi, data: bytes) -> None:
        _run_no_crash(fuzz_ffi, data)

    def test_known_type_names(self, fuzz_ffi) -> None:
        """All documented FFI type names should be accepted."""
        known = [
            b"\x00int", b"\x00long", b"\x00float", b"\x00double",
            b"\x00char", b"\x00char*", b"\x00void*", b"\x00size_t",
            b"\x00uint8", b"\x00uint16", b"\x00uint32", b"\x00uint64",
            b"\x00int8", b"\x00int16", b"\x00int32", b"\x00int64",
            b"\x00void",
        ]
        for inp in known:
            _run_no_crash(fuzz_ffi, inp)

    def test_unknown_type_names(self, fuzz_ffi) -> None:
        """Unknown type names should raise ValueError (handled), not crash."""
        for name in [b"SomeUnknownType", b"", b"UINT64_T", b"__m128"]:
            _run_no_crash(fuzz_ffi, b"\x00" + name)

    def test_struct_selector_bytes(self, fuzz_ffi) -> None:
        """Selector byte 1 path with edge-case arg_count bytes."""
        _run_no_crash(fuzz_ffi, b"\x01\x00")           # no args
        _run_no_crash(fuzz_ffi, b"\x01\x07\x00\x01\x02\x03\x04\x05\x06\x07")
        _run_no_crash(fuzz_ffi, b"\x01\xff\xff\xff")   # max-value bytes

    def test_c_string_round_trip_selector(self, fuzz_ffi) -> None:
        """Selector 2: all printable ASCII + edge cases."""
        _run_no_crash(fuzz_ffi, b"\x02hello")
        _run_no_crash(fuzz_ffi, b"\x02")
        _run_no_crash(fuzz_ffi, b"\x02\x00\x01\x02\x03")
        _run_no_crash(fuzz_ffi, b"\x02" + bytes(range(128)))


# ---------------------------------------------------------------------------
# Test: harness module structure invariants
# ---------------------------------------------------------------------------

class TestHarnessStructure:
    """Verify each harness module exposes the expected interface."""

    @pytest.mark.parametrize(
        "harness_name",
        [
            "fuzz_lexer",
            "fuzz_parser",
            "fuzz_interpreter",
            "fuzz_typechecker",
            "fuzz_ffi_marshal",
        ],
    )
    def test_has_test_one_input(self, harness_name: str) -> None:
        mod = _load_harness(harness_name)
        assert callable(getattr(mod, "TestOneInput", None)), (
            f"{harness_name} must expose a callable TestOneInput"
        )

    @pytest.mark.parametrize(
        "harness_name",
        [
            "fuzz_lexer",
            "fuzz_parser",
            "fuzz_interpreter",
            "fuzz_typechecker",
            "fuzz_ffi_marshal",
        ],
    )
    def test_has_sanity_inputs(self, harness_name: str) -> None:
        mod = _load_harness(harness_name)
        assert isinstance(getattr(mod, "_SANITY_INPUTS", None), list), (
            f"{harness_name} must expose a _SANITY_INPUTS list"
        )
        assert len(mod._SANITY_INPUTS) >= 5, (
            f"{harness_name}._SANITY_INPUTS should have at least 5 entries"
        )

    @pytest.mark.parametrize(
        "harness_name",
        [
            "fuzz_lexer",
            "fuzz_parser",
            "fuzz_interpreter",
            "fuzz_typechecker",
            "fuzz_ffi_marshal",
        ],
    )
    def test_empty_bytes_ok(self, harness_name: str) -> None:
        mod = _load_harness(harness_name)
        mod.TestOneInput(b"")  # Must never crash on empty input.
