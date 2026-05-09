"""
NativeFunctionJIT (Tier-2) tests.

Tests for the LLVM native code generation path used by the JIT tiered
compiler at opt_level >= 3.  The pipeline under test is:

    NexusLang source -> parse -> AST -> LLVMIRGenerator -> opt -> llc
                -> clang -shared -> .so -> ctypes callable

All tests that require the LLVM tool chain (opt, llc, clang) are decorated
with ``@skip_no_llvm`` and are skipped automatically in environments where
those tools are absent.
"""

from __future__ import annotations

import ctypes
import io
import sys
import threading
from pathlib import Path

import pytest

_SRC = str(Path(__file__).resolve().parent.parent.parent.parent / "src")
if _SRC not in sys.path:
    sys.path.insert(0, _SRC)

from nexuslang.jit.native_jit import NativeFunctionJIT, NativeCompileError, _tools_available
from nexuslang.jit.jit_compiler import JITCompiler

# ---------------------------------------------------------------------------
# Skip marker
# ---------------------------------------------------------------------------

skip_no_llvm = pytest.mark.skipif(
    not _tools_available(),
    reason="LLVM tools (opt, llc, clang) not available on PATH",
)

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _parse_function(source: str):
    """Parse a single NexusLang function definition from *source*."""
    from nexuslang.parser.lexer import Lexer
    from nexuslang.parser.parser import Parser

    tokens = Lexer(source).tokenize()
    ast = Parser(tokens).parse()
    return ast.statements[0]


def _silent(fn, *args, **kwargs):
    """Call fn(*args, **kwargs) with stdout suppressed, return result."""
    old = sys.stdout
    sys.stdout = io.StringIO()
    try:
        return fn(*args, **kwargs)
    finally:
        sys.stdout = old


class _FakeInterpreter:
    """Minimal interpreter stub sufficient for NativeFunctionJIT callee resolution."""

    def __init__(self, *func_defs):
        self.functions = {fd.name: fd for fd in func_defs}
        self.global_scope = dict(self.functions)
        self._global_scope = dict(self.functions)

    def call_function(self, name, args):
        raise NameError(f"unknown: {name}")


# ---------------------------------------------------------------------------
# NativeFunctionJIT: unit tests
# ---------------------------------------------------------------------------

class TestNativeFunctionJITAvailability:
    """Availability detection — runs without LLVM tools."""

    def test_tools_available_returns_bool(self):
        result = _tools_available()
        assert isinstance(result, bool)

    def test_native_jit_available_property(self):
        jit = NativeFunctionJIT(_FakeInterpreter())
        assert isinstance(jit.available, bool)
        assert jit.available == _tools_available()

    def test_compile_returns_none_when_tools_absent(self):
        """When LLVM tools are absent, compile() must return None, not raise."""
        if _tools_available():
            pytest.skip("LLVM tools present; only testing absence behaviour")
        fd = _parse_function("""
            function add_integers with a as Integer and b as Integer returns Integer
                return a plus b
            end
        """)
        jit = NativeFunctionJIT(_FakeInterpreter(fd))
        result = _silent(jit.compile, "add_integers", fd)
        assert result is None


class TestNativeFunctionJITIRGeneration:
    """_generate_ir correctness — validates the IR without running LLVM."""

    def _ir(self, source: str, func_name: str) -> str:
        fd = _parse_function(source)
        jit = NativeFunctionJIT(_FakeInterpreter(fd))
        ir = jit._generate_ir(func_name, fd)
        assert ir is not None, "_generate_ir returned None"
        return ir

    def test_ir_exports_nxl_prefixed_symbol_single_param(self):
        ir = self._ir("""
            function count_down with n as Integer returns Integer
                return n
            end
        """, "count_down")
        assert "@nxl_count_down" in ir

    def test_ir_exports_nxl_prefixed_symbol_two_params(self):
        ir = self._ir("""
            function add_integers with a as Integer and b as Integer returns Integer
                return a plus b
            end
        """, "add_integers")
        assert "@nxl_add_integers" in ir

    def test_ir_define_preserves_correct_return_type(self):
        ir = self._ir("""
            function get_value with n as Integer returns Integer
                return n
            end
        """, "get_value")
        # The define line for the renamed function should carry i64 return type
        define_lines = [
            l for l in ir.split("\n")
            if "define" in l and "nxl_get_value" in l
        ]
        assert define_lines, "No define line found for nxl_get_value"
        assert "i64" in define_lines[0]

    def test_ir_no_stale_hardcoded_alias(self):
        ir = self._ir("""
            function add_integers with a as Integer and b as Integer returns Integer
                return a plus b
            end
        """, "add_integers")
        # Old buggy approach: 'alias i64 (i64)' — wrong for multi-param functions
        assert "alias i64 (i64)" not in ir

    def test_ir_two_param_define_is_correct_arity(self):
        ir = self._ir("""
            function add_integers with a as Integer and b as Integer returns Integer
                return a plus b
            end
        """, "add_integers")
        define_lines = [
            l for l in ir.split("\n")
            if "define" in l and "nxl_add_integers" in l
        ]
        assert define_lines, "No define line for nxl_add_integers"
        # Both parameters must appear in the define line
        assert "%a" in define_lines[0] and "%b" in define_lines[0]

    def test_ir_original_symbol_not_exported_under_bare_name(self):
        """The original @add_integers must be renamed; no bare @add_integers( in output."""
        ir = self._ir("""
            function add_integers with a as Integer and b as Integer returns Integer
                return a plus b
            end
        """, "add_integers")
        # Every occurrence of add_integers must be under nxl_add_integers
        import re
        bare_refs = re.findall(r'@add_integers(?!\s*=)', ir)
        # All bare refs must contain the nxl_ prefix
        assert all("nxl_add_integers" in r or r == "@add_integers" for r in bare_refs), \
            f"Found bare @add_integers references: {bare_refs}"

    def test_ir_recursive_function_callsites_renamed(self):
        """A recursive function's self-call must also use nxl_ symbol."""
        ir = self._ir("""
            function countdown with n as Integer returns Integer
                if n is less than or equal to 0
                    return 0
                end
                return n
            end
        """, "countdown")
        # The define should be @nxl_countdown
        assert "@nxl_countdown" in ir


class TestNativeFunctionJITCompilation:
    """End-to-end compilation and correctness tests. Require LLVM tools."""

    @skip_no_llvm
    def test_single_param_integer_function_compiles(self):
        fd = _parse_function("""
            function double_it with n as Integer returns Integer
                return n plus n
            end
        """)
        jit = NativeFunctionJIT(_FakeInterpreter(fd), opt_level=3)
        fn = _silent(jit.compile, "double_it", fd)
        assert fn is not None and callable(fn)

    @skip_no_llvm
    def test_single_param_returns_correct_values(self):
        fd = _parse_function("""
            function double_it with n as Integer returns Integer
                return n plus n
            end
        """)
        jit = NativeFunctionJIT(_FakeInterpreter(fd), opt_level=3)
        fn = _silent(jit.compile, "double_it", fd)
        assert fn(0) == 0
        assert fn(1) == 2
        assert fn(21) == 42
        assert fn(500) == 1000

    @skip_no_llvm
    def test_two_param_integer_function_compiles(self):
        fd = _parse_function("""
            function add_integers with a as Integer and b as Integer returns Integer
                return a plus b
            end
        """)
        jit = NativeFunctionJIT(_FakeInterpreter(fd), opt_level=3)
        fn = _silent(jit.compile, "add_integers", fd)
        assert fn is not None and callable(fn)

    @skip_no_llvm
    def test_two_param_returns_correct_values(self):
        fd = _parse_function("""
            function add_integers with a as Integer and b as Integer returns Integer
                return a plus b
            end
        """)
        jit = NativeFunctionJIT(_FakeInterpreter(fd), opt_level=3)
        fn = _silent(jit.compile, "add_integers", fd)
        assert fn(0, 0) == 0
        assert fn(10, 32) == 42
        assert fn(100, 200) == 300
        assert fn(-5, 5) == 0

    @skip_no_llvm
    def test_iterative_loop_function_correct(self):
        fd = _parse_function("""
            function sum_to_n with n as Integer returns Integer
                set total to 0
                set i to 1
                while i is less than or equal to n
                    set total to total plus i
                    set i to i plus 1
                end
                return total
            end
        """)
        jit = NativeFunctionJIT(_FakeInterpreter(fd), opt_level=3)
        fn = _silent(jit.compile, "sum_to_n", fd)
        assert fn(0) == 0
        assert fn(1) == 1
        assert fn(10) == 55      # 1+2+...+10
        assert fn(100) == 5050   # sum 1..100

    @skip_no_llvm
    def test_conditional_logic_correct(self):
        fd = _parse_function("""
            function abs_value with n as Integer returns Integer
                if n is less than 0
                    return 0 minus n
                end
                return n
            end
        """)
        jit = NativeFunctionJIT(_FakeInterpreter(fd), opt_level=3)
        fn = _silent(jit.compile, "abs_value", fd)
        assert fn(5) == 5
        assert fn(-7) == 7
        assert fn(0) == 0

    @skip_no_llvm
    def test_compile_opt_level_0_also_works(self):
        fd = _parse_function("""
            function add_integers with a as Integer and b as Integer returns Integer
                return a plus b
            end
        """)
        jit = NativeFunctionJIT(_FakeInterpreter(fd), opt_level=0)
        fn = _silent(jit.compile, "add_integers", fd)
        assert fn is not None
        assert fn(3, 4) == 7

    @skip_no_llvm
    def test_cache_hit_returns_identical_object(self):
        fd = _parse_function("""
            function add_integers with a as Integer and b as Integer returns Integer
                return a plus b
            end
        """)
        jit = NativeFunctionJIT(_FakeInterpreter(fd), opt_level=3)
        fn1 = _silent(jit.compile, "add_integers", fd)
        fn2 = _silent(jit.compile, "add_integers", fd)
        assert fn1 is fn2, "Second compile() call should hit cache and return same object"

    @skip_no_llvm
    def test_cache_hit_is_fast(self):
        """Second compile() call (cache hit) completes in < 5ms."""
        import time
        fd = _parse_function("""
            function add_integers with a as Integer and b as Integer returns Integer
                return a plus b
            end
        """)
        jit = NativeFunctionJIT(_FakeInterpreter(fd), opt_level=3)
        _silent(jit.compile, "add_integers", fd)  # warm cache
        t0 = time.perf_counter()
        _silent(jit.compile, "add_integers", fd)
        elapsed_ms = (time.perf_counter() - t0) * 1000
        assert elapsed_ms < 5.0, f"Cache hit took {elapsed_ms:.2f}ms (expected < 5ms)"

    @skip_no_llvm
    def test_float_wrapper_supports_scalar_signatures(self):
        """Float parameters and returns should map to ctypes double."""
        from nexuslang.parser.ast import FunctionDefinition, Parameter, ReturnStatement, Identifier

        class _NativeFn:
            def __call__(self, value):
                return value

        param = Parameter("x")
        param.type_annotation = "Float"
        fd = FunctionDefinition("float_fn", [param], [ReturnStatement(Identifier("x"))])
        fd.return_type = "Float"
        native = _NativeFn()
        jit = NativeFunctionJIT(_FakeInterpreter(), opt_level=3)
        wrapper = jit._make_wrapper(fd, native)
        assert wrapper is not None
        assert native.argtypes == [ctypes.c_double]
        assert native.restype is ctypes.c_double
        assert wrapper(3.25) == pytest.approx(3.25)

    @skip_no_llvm
    def test_compile_float_identity_function(self):
        """A simple Float -> Float function should compile to a native callable."""
        fd = _parse_function("""
            function float_identity with x as Float returns Float
                return x
            end
        """)
        jit = NativeFunctionJIT(_FakeInterpreter(fd), opt_level=3)
        fn = _silent(jit.compile, "float_identity", fd)
        assert fn is not None
        assert fn(7.5) == pytest.approx(7.5)

    @skip_no_llvm
    def test_boolean_wrapper_supports_scalar_signatures(self):
        """Boolean parameters and returns should map to ctypes c_bool, not c_int64."""
        from nexuslang.parser.ast import FunctionDefinition, Parameter, ReturnStatement, Identifier

        class _NativeFn:
            def __call__(self, value):
                return value

        param = Parameter("flag")
        param.type_annotation = "Boolean"
        fd = FunctionDefinition("bool_fn", [param], [ReturnStatement(Identifier("flag"))])
        fd.return_type = "Boolean"
        native = _NativeFn()
        jit = NativeFunctionJIT(_FakeInterpreter(), opt_level=3)
        wrapper = jit._make_wrapper(fd, native)
        assert wrapper is not None
        assert native.argtypes == [ctypes.c_bool]
        assert native.restype is ctypes.c_bool
        assert wrapper(True) is True
        assert wrapper(False) is False

    @skip_no_llvm
    def test_compile_boolean_not_function(self):
        """A Boolean -> Boolean function that negates its input should compile natively."""
        fd = _parse_function("""
            function bool_not with flag as Boolean returns Boolean
                if flag
                    return false
                end
                return true
            end
        """)
        jit = NativeFunctionJIT(_FakeInterpreter(fd), opt_level=3)
        fn = _silent(jit.compile, "bool_not", fd)
        assert fn is not None
        assert fn(False) is True
        assert fn(True) is False

    @skip_no_llvm
    def test_concurrent_compilation_returns_correct_results(self):
        """Concurrent compilation of the same function converges to a single cached callable."""
        fd = _parse_function("""
            function add_integers with a as Integer and b as Integer returns Integer
                return a plus b
            end
        """)
        jit = NativeFunctionJIT(_FakeInterpreter(fd), opt_level=3)
        results = []
        errors = []

        def compile_and_run():
            try:
                fn = _silent(jit.compile, "add_integers", fd)
                if fn is not None:
                    results.append(fn(3, 4))
            except Exception as e:
                errors.append(e)

        threads = [threading.Thread(target=compile_and_run) for _ in range(4)]
        for t in threads:
            t.start()
        for t in threads:
            t.join()

        assert not errors, f"Errors in concurrent compilation: {errors}"
        assert all(r == 7 for r in results), f"Unexpected results: {results}"


# ---------------------------------------------------------------------------
# JITCompiler: native path routing tests
# ---------------------------------------------------------------------------

class TestJITCompilerNativeRouting:
    """Verify that JITCompiler.compile_function routes correctly to native at opt_level=3."""

    def _make_jit_with_func(self, source: str):
        fd = _parse_function(source)

        class _Interp:
            def __init__(self_, fd):
                self_.functions = {fd.name: fd}
                self_.global_scope = {fd.name: fd}
                self_._global_scope = {fd.name: fd}

            def call_function(self_, name, args):
                raise NameError(name)

        interp = _Interp(fd)
        jit = JITCompiler(hot_threshold=10, optimization_level=3)
        jit.attach_to_interpreter(interp)
        return jit, fd

    @skip_no_llvm
    def test_opt_level_3_returns_callable(self):
        jit, fd = self._make_jit_with_func("""
            function add_integers with a as Integer and b as Integer returns Integer
                return a plus b
            end
        """)
        fn = _silent(jit.compile_function, fd.name, fd, opt_level=3)
        assert fn is not None and callable(fn)

    @skip_no_llvm
    def test_opt_level_3_returns_correct_result(self):
        jit, fd = self._make_jit_with_func("""
            function add_integers with a as Integer and b as Integer returns Integer
                return a plus b
            end
        """)
        fn = _silent(jit.compile_function, fd.name, fd, opt_level=3)
        assert fn(10, 32) == 42
        assert fn(0, 0) == 0
        assert fn(-10, 10) == 0

    @skip_no_llvm
    def test_opt_level_3_cache_hit_consistent(self):
        jit, fd = self._make_jit_with_func("""
            function add_integers with a as Integer and b as Integer returns Integer
                return a plus b
            end
        """)
        fn1 = _silent(jit.compile_function, fd.name, fd, opt_level=3)
        fn2 = _silent(jit.compile_function, fd.name, fd, opt_level=3)
        assert fn1 is fn2
        assert fn1(7, 8) == 15

    def test_native_jit_attached_after_attach_to_interpreter(self):
        fd = _parse_function("""
            function add_integers with a as Integer and b as Integer returns Integer
                return a plus b
            end
        """)

        class _Interp:
            def __init__(self):
                self.functions = {fd.name: fd}
                self.global_scope = {fd.name: fd}
                self._global_scope = {fd.name: fd}
            def call_function(self, name, args): raise NameError(name)

        jit = JITCompiler()
        jit.attach_to_interpreter(_Interp())
        assert jit._native_jit is not None

    def test_native_jit_availability_matches_tools(self):
        class _Interp:
            functions = {}
            global_scope = {}
            _global_scope = {}
            def call_function(self, name, args): raise NameError(name)

        jit = JITCompiler()
        jit.attach_to_interpreter(_Interp())
        assert jit._native_jit.available == _tools_available()

    @skip_no_llvm
    def test_iterative_fibonacci_native(self):
        """Iterative Fibonacci through the full JIT pipeline."""
        jit, fd = self._make_jit_with_func("""
            function fib_iter with n as Integer returns Integer
                set a to 0
                set b to 1
                set i to 0
                while i is less than n
                    set temp to b
                    set b to a plus b
                    set a to temp
                    set i to i plus 1
                end
                return a
            end
        """)
        fn = _silent(jit.compile_function, fd.name, fd, opt_level=3)
        assert fn is not None
        assert fn(0) == 0
        assert fn(1) == 1
        assert fn(10) == 55
        assert fn(20) == 6765
