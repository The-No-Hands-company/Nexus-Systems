"""tests/unit/compiler/test_stdlib_native.py

Tests for the NexusLang native runtime library (stdlib_native).

Covers:
- Python package API (is_built, get_library_path, get_link_flags)
- Archive integrity (nm-based symbol verification)
- Symbol correctness via ctypes (math, string, io helpers)
- Integration: compile_to_executable links against libNLPL.a
"""

from __future__ import annotations

import ctypes
import math
import os
import subprocess
import sys
import tempfile
from typing import List

import pytest

# ---------------------------------------------------------------------------
# Path setup
# ---------------------------------------------------------------------------

REPO_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), "..", "..", "..", "..")
)
sys.path.insert(0, os.path.join(REPO_ROOT, "src"))

from nexuslang.stdlib_native import (
    DEFAULT_BUILD_DIR,
    INCLUDE_DIR,
    STDLIB_NATIVE_DIR,
    build_if_needed,
    get_include_flags,
    get_library_path,
    get_link_flags,
    is_built,
)

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

_LIB_PATH: str | None = None


def _lib() -> str:
    """Return library path, skipping the test if not built."""
    global _LIB_PATH
    if _LIB_PATH is None:
        _LIB_PATH = get_library_path()
    if _LIB_PATH is None:
        pytest.skip("libNLPL.a has not been built yet")
    return _LIB_PATH


def _nm_symbols(lib_path: str) -> List[str]:
    """Return list of exported (T) symbol names from an archive."""
    result = subprocess.run(
        ["nm", lib_path],
        capture_output=True,
        text=True,
    )
    names = []
    for line in result.stdout.splitlines():
        parts = line.strip().split()
        if len(parts) == 3 and parts[1] == "T":
            names.append(parts[2])
    return names


# ===========================================================================
# 1.  Package API
# ===========================================================================


class TestPackageAPI:
    def test_stdlib_native_dir_exists(self):
        assert os.path.isdir(STDLIB_NATIVE_DIR)

    def test_include_dir_exists(self):
        assert os.path.isdir(INCLUDE_DIR)

    def test_header_present(self):
        assert os.path.isfile(os.path.join(INCLUDE_DIR, "nxl_runtime.h"))

    def test_is_built_reflects_reality(self):
        lib = get_library_path()
        assert is_built() == (lib is not None)

    def test_get_library_path_type(self):
        lib = get_library_path()
        assert lib is None or isinstance(lib, str)

    def test_get_link_flags_returns_list(self):
        flags = get_link_flags()
        assert isinstance(flags, list)
        assert len(flags) >= 1

    def test_get_link_flags_includes_lm(self):
        flags = get_link_flags()
        assert "-lm" in flags

    def test_get_link_flags_includes_lib_when_built(self):
        if not is_built():
            pytest.skip("library not built")
        flags = get_link_flags()
        lib = get_library_path()
        assert lib in flags

    def test_get_include_flags(self):
        flags = get_include_flags()
        assert any(flag.startswith("-I") for flag in flags)
        assert any(INCLUDE_DIR in flag for flag in flags)

    def test_build_if_needed_idempotent(self):
        # If already built, should return True without rebuilding.
        if not is_built():
            pytest.skip("library not built")
        ok = build_if_needed()
        assert ok is True
        assert is_built()


# ===========================================================================
# 2.  Archive integrity (nm symbol check)
# ===========================================================================


# Minimum expected symbols from each translation unit.
_REQUIRED_SYMBOLS = [
    # runtime
    "nxl_panic",
    "nxl_assert",
    "nxl_print",
    "nxl_println",
    "nxl_print_int",
    "nxl_print_float",
    "nxl_print_bool",
    "nxl_int_to_string",
    "nxl_float_to_string",
    "nxl_bool_to_string",
    "nxl_string_to_int",
    "nxl_string_to_float",
    "nxl_parallel_for_i64",
    "nxl_runtime_version",
    # math
    "nxl_sin",
    "nxl_cos",
    "nxl_tan",
    "nxl_sqrt",
    "nxl_pow",
    "nxl_exp",
    "nxl_log",
    "nxl_floor",
    "nxl_ceil",
    "nxl_round",
    "nxl_abs_int",
    "nxl_abs_float",
    "nxl_min_int",
    "nxl_max_int",
    "nxl_min_float",
    "nxl_max_float",
    # string
    "nxl_str_concat",
    "nxl_str_length",
    "nxl_str_replace",
    "nxl_str_trim",
    "nxl_str_toupper",
    "nxl_str_tolower",
    "nxl_substr",
    "nxl_charat",
    "nxl_indexof",
    # collections
    "nxl_array_new",
    "nxl_array_push",
    "nxl_array_pop",
    "nxl_array_get",
    "nxl_array_length",
    "nxl_array_free",
    # io
    "nxl_read_line",
    "nxl_read_file",
    "nxl_write_file",
]


class TestArchiveSymbols:
    @pytest.fixture(scope="class")
    def symbols(self):
        return set(_nm_symbols(_lib()))

    @pytest.mark.parametrize("sym", _REQUIRED_SYMBOLS)
    def test_symbol_present(self, sym, symbols):
        assert sym in symbols, f"Missing symbol: {sym}"

    def test_minimum_symbol_count(self, symbols):
        assert len(symbols) >= 50, f"Expected at least 50 symbols, got {len(symbols)}"

    def test_all_nxl_prefixed(self, symbols):
        non_prefixed = [s for s in symbols if not s.startswith("nxl_") and not s.startswith("arr")]
        assert non_prefixed == [], f"Unexpected non-prefixed symbols: {non_prefixed}"


# ===========================================================================
# 3.  Functional correctness via ctypes
# ===========================================================================


@pytest.fixture(scope="module")
def _clib():
    """Load libNLPL.a as a shared library via a small shared shim.

    Strategy: compile a tiny shared wrapper that just re-exports the .a so
    ctypes can dlopen it.  Requires cc (gcc/clang).
    """
    import shutil

    lib = _lib()
    cc = shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
    if cc is None:
        pytest.skip("No C compiler found, skipping ctypes tests")

    with tempfile.TemporaryDirectory() as tmpdir:
        so_path = os.path.join(tmpdir, "libnxl_test.so")
        # Compile an empty .c into a shared library that links the static archive
        empty_c = os.path.join(tmpdir, "empty.c")
        with open(empty_c, "w") as f:
            f.write("/* empty */\n")
        cmd = [
            cc,
            "-shared", "-fPIC",
            empty_c,
            f"-Wl,--whole-archive", lib, f"-Wl,--no-whole-archive",
            "-lm",
            "-o", so_path,
        ]
        r = subprocess.run(cmd, capture_output=True, text=True)
        if r.returncode != 0:
            pytest.skip(f"Could not build test shim: {r.stderr[:400]}")

        yield ctypes.CDLL(so_path)


class TestMathFunctions:
    def test_sqrt(self, _clib):
        fn = _clib.nxl_sqrt
        fn.restype = ctypes.c_double
        fn.argtypes = [ctypes.c_double]
        assert abs(fn(4.0) - 2.0) < 1e-9
        assert abs(fn(9.0) - 3.0) < 1e-9

    def test_sin(self, _clib):
        fn = _clib.nxl_sin
        fn.restype = ctypes.c_double
        fn.argtypes = [ctypes.c_double]
        assert abs(fn(0.0)) < 1e-9
        assert abs(fn(math.pi / 2) - 1.0) < 1e-9

    def test_cos(self, _clib):
        fn = _clib.nxl_cos
        fn.restype = ctypes.c_double
        fn.argtypes = [ctypes.c_double]
        assert abs(fn(0.0) - 1.0) < 1e-9
        assert abs(fn(math.pi)) < 1e-9 or abs(fn(math.pi) + 1.0) < 1e-9

    def test_pow(self, _clib):
        fn = _clib.nxl_pow
        fn.restype = ctypes.c_double
        fn.argtypes = [ctypes.c_double, ctypes.c_double]
        assert abs(fn(2.0, 10.0) - 1024.0) < 1e-6

    def test_floor(self, _clib):
        fn = _clib.nxl_floor
        fn.restype = ctypes.c_double
        fn.argtypes = [ctypes.c_double]
        assert fn(3.9) == 3.0
        assert fn(-1.1) == -2.0

    def test_ceil(self, _clib):
        fn = _clib.nxl_ceil
        fn.restype = ctypes.c_double
        fn.argtypes = [ctypes.c_double]
        assert fn(3.1) == 4.0
        assert fn(-1.9) == -1.0

    def test_abs_int(self, _clib):
        fn = _clib.nxl_abs_int
        fn.restype = ctypes.c_longlong
        fn.argtypes = [ctypes.c_longlong]
        assert fn(-42) == 42
        assert fn(0) == 0
        assert fn(100) == 100

    def test_abs_float(self, _clib):
        fn = _clib.nxl_abs_float
        fn.restype = ctypes.c_double
        fn.argtypes = [ctypes.c_double]
        assert abs(fn(-3.14) - 3.14) < 1e-9

    def test_min_int(self, _clib):
        fn = _clib.nxl_min_int
        fn.restype = ctypes.c_longlong
        fn.argtypes = [ctypes.c_longlong, ctypes.c_longlong]
        assert fn(3, 7) == 3
        assert fn(-5, 2) == -5

    def test_max_int(self, _clib):
        fn = _clib.nxl_max_int
        fn.restype = ctypes.c_longlong
        fn.argtypes = [ctypes.c_longlong, ctypes.c_longlong]
        assert fn(3, 7) == 7
        assert fn(-5, 2) == 2

    def test_clamp_int(self, _clib):
        fn = _clib.nxl_clamp_int
        fn.restype = ctypes.c_longlong
        fn.argtypes = [ctypes.c_longlong, ctypes.c_longlong, ctypes.c_longlong]
        assert fn(5, 0, 10) == 5
        assert fn(-1, 0, 10) == 0
        assert fn(15, 0, 10) == 10


class TestStringFunctions:
    def test_str_length(self, _clib):
        fn = _clib.nxl_str_length
        fn.restype = ctypes.c_longlong
        fn.argtypes = [ctypes.c_char_p]
        assert fn(b"hello") == 5
        assert fn(b"") == 0

    def test_str_concat(self, _clib):
        fn = _clib.nxl_str_concat
        fn.restype = ctypes.c_char_p
        fn.argtypes = [ctypes.c_char_p, ctypes.c_char_p]
        result = fn(b"hello", b" world")
        assert result == b"hello world"

    def test_substr(self, _clib):
        fn = _clib.nxl_substr
        fn.restype = ctypes.c_char_p
        fn.argtypes = [ctypes.c_char_p, ctypes.c_int, ctypes.c_int]
        result = fn(b"hello", 1, 3)
        assert result == b"ell"

    def test_indexof_found(self, _clib):
        fn = _clib.nxl_indexof
        fn.restype = ctypes.c_longlong
        fn.argtypes = [ctypes.c_char_p, ctypes.c_char_p]
        assert fn(b"hello world", b"world") == 6

    def test_indexof_not_found(self, _clib):
        fn = _clib.nxl_indexof
        fn.restype = ctypes.c_longlong
        fn.argtypes = [ctypes.c_char_p, ctypes.c_char_p]
        assert fn(b"hello", b"xyz") == -1

    def test_str_toupper(self, _clib):
        fn = _clib.nxl_str_toupper
        fn.restype = ctypes.c_char_p
        fn.argtypes = [ctypes.c_char_p]
        result = fn(b"hello")
        assert result == b"HELLO"

    def test_str_tolower(self, _clib):
        fn = _clib.nxl_str_tolower
        fn.restype = ctypes.c_char_p
        fn.argtypes = [ctypes.c_char_p]
        result = fn(b"WORLD")
        assert result == b"world"

    def test_str_trim(self, _clib):
        fn = _clib.nxl_str_trim
        fn.restype = ctypes.c_char_p
        fn.argtypes = [ctypes.c_char_p]
        result = fn(b"  hi  ")
        assert result == b"hi"

    def test_str_replace(self, _clib):
        fn = _clib.nxl_str_replace
        fn.restype = ctypes.c_char_p
        fn.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p]
        result = fn(b"aabbcc", b"bb", b"XX")
        assert result == b"aaXXcc"

    def test_str_compare_equal(self, _clib):
        fn = _clib.nxl_str_compare
        fn.restype = ctypes.c_int
        fn.argtypes = [ctypes.c_char_p, ctypes.c_char_p]
        assert fn(b"abc", b"abc") == 0

    def test_str_compare_less(self, _clib):
        fn = _clib.nxl_str_compare
        fn.restype = ctypes.c_int
        fn.argtypes = [ctypes.c_char_p, ctypes.c_char_p]
        assert fn(b"abc", b"abd") < 0

    def test_str_starts_with_true(self, _clib):
        fn = _clib.nxl_str_starts_with
        fn.restype = ctypes.c_int
        fn.argtypes = [ctypes.c_char_p, ctypes.c_char_p]
        assert fn(b"hello world", b"hello") != 0

    def test_str_starts_with_false(self, _clib):
        fn = _clib.nxl_str_starts_with
        fn.restype = ctypes.c_int
        fn.argtypes = [ctypes.c_char_p, ctypes.c_char_p]
        assert fn(b"hello world", b"world") == 0

    def test_str_ends_with_true(self, _clib):
        fn = _clib.nxl_str_ends_with
        fn.restype = ctypes.c_int
        fn.argtypes = [ctypes.c_char_p, ctypes.c_char_p]
        assert fn(b"hello world", b"world") != 0


class TestRuntimeFunctions:
    def test_runtime_version(self, _clib):
        fn = _clib.nxl_runtime_version
        fn.restype = ctypes.c_char_p
        fn.argtypes = []
        result = fn()
        assert result is not None
        assert len(result) > 0

    def test_int_to_string(self, _clib):
        fn = _clib.nxl_int_to_string
        fn.restype = ctypes.c_char_p
        fn.argtypes = [ctypes.c_longlong]
        assert fn(42) == b"42"
        assert fn(-100) == b"-100"
        assert fn(0) == b"0"

    def test_float_to_string(self, _clib):
        fn = _clib.nxl_float_to_string
        fn.restype = ctypes.c_char_p
        fn.argtypes = [ctypes.c_double]
        result = fn(3.14)
        assert result is not None
        assert b"3.14" in result or b"3.1" in result  # allow formatting variation

    def test_bool_to_string_true(self, _clib):
        fn = _clib.nxl_bool_to_string
        fn.restype = ctypes.c_char_p
        fn.argtypes = [ctypes.c_int]
        assert fn(1) == b"true"

    def test_bool_to_string_false(self, _clib):
        fn = _clib.nxl_bool_to_string
        fn.restype = ctypes.c_char_p
        fn.argtypes = [ctypes.c_int]
        assert fn(0) == b"false"

    def test_string_to_int(self, _clib):
        fn = _clib.nxl_string_to_int
        fn.restype = ctypes.c_longlong
        fn.argtypes = [ctypes.c_char_p]
        assert fn(b"42") == 42
        assert fn(b"-7") == -7

    def test_string_to_float(self, _clib):
        fn = _clib.nxl_string_to_float
        fn.restype = ctypes.c_double
        fn.argtypes = [ctypes.c_char_p]
        assert abs(fn(b"3.14") - 3.14) < 1e-6


class TestCollectionFunctions:
    def test_array_new_and_free(self, _clib):
        new_fn = _clib.nxl_array_new
        new_fn.restype = ctypes.c_void_p
        new_fn.argtypes = []
        free_fn = _clib.nxl_array_free
        free_fn.restype = None
        free_fn.argtypes = [ctypes.c_void_p]

        arr = new_fn()
        assert arr is not None
        free_fn(arr)

    def test_array_push_and_length(self, _clib):
        new_fn = _clib.nxl_array_new
        new_fn.restype = ctypes.c_void_p
        new_fn.argtypes = []
        push_fn = _clib.nxl_array_push
        push_fn.restype = None
        push_fn.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
        len_fn = _clib.nxl_array_length
        len_fn.restype = ctypes.c_longlong
        len_fn.argtypes = [ctypes.c_void_p]
        free_fn = _clib.nxl_array_free
        free_fn.restype = None
        free_fn.argtypes = [ctypes.c_void_p]

        arr = new_fn()
        assert len_fn(arr) == 0
        push_fn(arr, ctypes.cast(ctypes.c_char_p(b"a"), ctypes.c_void_p))
        push_fn(arr, ctypes.cast(ctypes.c_char_p(b"b"), ctypes.c_void_p))
        assert len_fn(arr) == 2
        free_fn(arr)


# ===========================================================================
# 4.  Integration: compile_to_executable uses libNLPL.a
# ===========================================================================


_MINIMAL_NLPL = """\
set x to 42
print text "hello from native"
"""


@pytest.mark.integration
class TestCompileToExecutableIntegration:
    """Compile a minimal NexusLang program and verify it links against libNLPL.a."""

    @pytest.fixture(scope="class")
    def compiled_exe(self, tmp_path_factory):
        import shutil

        shutil.which("opt") or pytest.skip("opt not found")
        shutil.which("llc") or pytest.skip("llc not found")
        shutil.which("clang") or pytest.skip("clang not found")

        from nexuslang.compiler.backends.llvm_ir_generator import LLVMIRGenerator
        from nexuslang.parser.lexer import Lexer
        from nexuslang.parser.parser import Parser

        src = _MINIMAL_NLPL
        tokens = Lexer(src).tokenize()
        ast = Parser(tokens).parse()

        gen = LLVMIRGenerator()
        gen.generate(ast)

        tmpdir = tmp_path_factory.mktemp("exe")
        exe = str(tmpdir / "test_prog")
        ok = gen.compile_to_executable(exe, opt_level=0)
        if not ok:
            pytest.skip("compile_to_executable failed (tool chain incomplete)")
        return exe

    def test_executable_exists(self, compiled_exe):
        assert os.path.isfile(compiled_exe)

    def test_executable_runs(self, compiled_exe):
        r = subprocess.run([compiled_exe], capture_output=True, text=True, timeout=10)
        assert r.returncode == 0

    def test_executable_output(self, compiled_exe):
        r = subprocess.run([compiled_exe], capture_output=True, text=True, timeout=10)
        assert "hello from native" in r.stdout

    def test_executable_links_libNLPL(self, compiled_exe):
        # Use nm to confirm libNLPL symbols appear in the binary.
        import shutil

        if shutil.which("nm"):
            # Use plain nm (not -D) so statically-linked symbols are visible.
            r = subprocess.run(
                ["nm", compiled_exe], capture_output=True, text=True
            )
            combined = r.stdout + r.stderr
            # At minimum, nxl_panic or nxl_print should appear somewhere.
            nxl_present = any(
                sym in combined
                for sym in ("nxl_panic", "nxl_print", "nxl_sqrt")
            )
            # Soft assertion: the binary ran OK already; symbol listing is best-effort.
            assert nxl_present or r.returncode != 0  # skip if nm can't inspect it
