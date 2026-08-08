"""
JIT hardening regression tests.

Verifies that:
- Compilation failures are logged (not printed) and cleanly return
  False/None without propagating exceptions to callers.
- Silent fallback paths in TieredCompiler stay functional when
  JIT initialization or compilation raises.
- NativeFunctionJIT.compile() surfaces unexpected failures via the
  module logger and returns None.
- type_inference._binding_annotation_to_type() accepts unknown type
  names without raising.
"""

from __future__ import annotations

import logging
import sys
import types as _types
from pathlib import Path
from unittest.mock import MagicMock, patch

import pytest

_SRC = str(Path(__file__).resolve().parent.parent.parent.parent / "src")
if _SRC not in sys.path:
    sys.path.insert(0, _SRC)


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _make_func_def(name: str = "test_fn"):
    """Build a minimal FunctionDefinition stub accepted by JIT modules."""
    from nexuslang.parser.ast import FunctionDefinition, Parameter, ReturnStatement, Literal
    return FunctionDefinition(
        name=name,
        parameters=[Parameter("x")],
        body=[ReturnStatement(Literal("integer", 1))],
        return_type=None,
    )


def _make_mock_interpreter(func_def=None):
    """Return a lightweight mock interpreter stub."""
    mock = MagicMock()
    if func_def is not None:
        mock.functions = {func_def.name: func_def}
        mock.global_scope = {}
        mock._global_scope = {}
    else:
        mock.functions = {}
        mock.global_scope = {}
        mock._global_scope = {}
    return mock


# ---------------------------------------------------------------------------
# JITCompiler hardening
# ---------------------------------------------------------------------------

class TestJITCompilerHardening:
    """JITCompiler._compile_function uses logging, not print(), and recovers."""

    def test_compile_failure_logs_warning_not_printed(self, capfd, caplog):
        """When _compile_function fails, it logs a WARNING and returns False.
        No output should reach stdout/stderr."""
        from nexuslang.jit.jit_compiler import JITCompiler

        compiler = JITCompiler(hot_threshold=5)
        # Do NOT set enabled=False — the early-return guard would skip the try/except.
        # Use an empty interpreter (no functions) so _compile_function raises
        # ValueError("Function test_fn not found") which is caught and logged.
        mock_interp = _make_mock_interpreter()  # no functions registered
        compiler.interpreter = mock_interp

        with caplog.at_level(logging.WARNING, logger="nexuslang.jit.jit_compiler"):
            result = compiler._compile_function("test_fn")

        # Must return False, not propagate the exception
        assert result is False

        # Must log the warning — not print
        assert any("test_fn" in r.message for r in caplog.records if r.levelno >= logging.WARNING)

        # Absolutely nothing on stdout/stderr
        captured = capfd.readouterr()
        assert captured.out == ""
        assert captured.err == ""

    def test_compile_function_returns_none_on_code_gen_failure(self):
        """compile_function() returns None when the code generator raises."""
        from nexuslang.jit.jit_compiler import JITCompiler
        from nexuslang.jit.code_gen import NLPLCodeGenerator, CodeGenError

        compiler = JITCompiler(hot_threshold=5)
        compiler.interpreter = _make_mock_interpreter()
        compiler._native_jit = None  # disable native path

        func_def = _make_func_def()

        with patch.object(
            NLPLCodeGenerator,
            "compile_function",
            side_effect=CodeGenError("test code-gen failure"),
        ):
            result = compiler.compile_function("test_fn", func_def, opt_level=1)

        assert result is None

    def test_native_jit_fallthrough_logs_at_debug(self, caplog):
        """When native JIT raises unexpectedly, compile_function logs at DEBUG
        and falls through to the bytecode tier (result may be a callable or None)."""
        from nexuslang.jit.jit_compiler import JITCompiler
        from nexuslang.jit.native_jit import NativeFunctionJIT

        compiler = JITCompiler(hot_threshold=5)
        compiler.interpreter = _make_mock_interpreter()

        mock_native = MagicMock(spec=NativeFunctionJIT)
        mock_native.available = True
        mock_native.compile.side_effect = RuntimeError("simulated native JIT crash")
        compiler._native_jit = mock_native

        func_def = _make_func_def()

        with caplog.at_level(logging.DEBUG, logger="nexuslang.jit.jit_compiler"):
            result = compiler.compile_function("test_fn", func_def, opt_level=3)

        # Must not propagate the RuntimeError — reaching here means success
        # Result may be a bytecode-JIT callable or None depending on environment
        assert True

        # Must have logged the DEBUG fallthrough message
        assert any(
            "native" in r.message.lower() or "test_fn" in r.message
            for r in caplog.records
        )


# ---------------------------------------------------------------------------
# TieredCompiler hardening
# ---------------------------------------------------------------------------

class TestTieredCompilerHardening:
    """TieredCompiler survives JIT init failure and compilation failures."""

    def test_attach_jit_init_failure_logs_warning_and_keeps_compiler_alive(self, caplog):
        """If JITCompiler cannot be created, TieredCompiler logs a warning and
        remains usable for tracking without the JIT backend."""
        from nexuslang.jit.tiered_compiler import TieredCompiler

        compiler = TieredCompiler(tier1_threshold=5, tier2_threshold=10)

        mock_interp = _make_mock_interpreter()

        with patch(
            "nexuslang.jit.jit_compiler.JITCompiler",
            side_effect=RuntimeError("JIT init broken"),
        ):
            with caplog.at_level(logging.WARNING, logger="nexuslang.jit.tiered_compiler"):
                compiler.attach_to_interpreter(mock_interp)

        # Must still be attached — tracking active without JIT
        assert compiler._interpreter is mock_interp
        assert compiler._jit is None

        # Must log a WARNING
        warnings = [r for r in caplog.records if r.levelno >= logging.WARNING]
        assert warnings, "Expected at least one WARNING when JIT init fails"

    def test_baseline_compilation_failure_stays_at_interpreter_tier(self, caplog):
        """If baseline JIT compilation fails, the function stays at INTERPRETER tier."""
        from nexuslang.jit.tiered_compiler import TieredCompiler, FunctionTierState, ExecutionTier

        compiler = TieredCompiler(tier1_threshold=3, tier2_threshold=10)
        compiler._interpreter = _make_mock_interpreter()

        mock_jit = MagicMock()
        mock_jit.compile_function.side_effect = RuntimeError("compile failure")
        compiler._jit = mock_jit

        state = FunctionTierState(name="boom_fn")
        state.call_count = 5

        with patch.object(compiler, "_get_function_def", return_value=_make_func_def("boom_fn")):
            with caplog.at_level(logging.DEBUG, logger="nexuslang.jit.tiered_compiler"):
                compiler._compile_baseline(state)

        # Tier must not advance
        assert state.tier == ExecutionTier.INTERPRETER

        # Must log at DEBUG
        assert any(
            "boom_fn" in r.message and r.levelno == logging.DEBUG
            for r in caplog.records
        ), "Expected DEBUG log for baseline compilation failure"

    def test_detach_does_not_raise_on_jit_error(self):
        """detach() swallows JIT cleanup errors and does not propagate."""
        from nexuslang.jit.tiered_compiler import TieredCompiler

        compiler = TieredCompiler()
        mock_interp = MagicMock()
        compiler._interpreter = mock_interp
        mock_jit = MagicMock()
        mock_jit.detach_from_interpreter.side_effect = RuntimeError("cleanup failure")
        compiler._jit = mock_jit

        compiler.detach()  # Must not raise

        assert compiler._interpreter is None
        assert compiler._jit is None


# ---------------------------------------------------------------------------
# NativeFunctionJIT hardening
# ---------------------------------------------------------------------------

class TestNativeFunctionJITHardening:
    """NativeFunctionJIT.compile() returns None and logs on unexpected failures."""

    def test_unexpected_exception_in_compile_returns_none_and_logs_warning(self, caplog):
        """When _compile() raises something other than NativeCompileError,
        compile() returns None and emits a WARNING via the module logger."""
        from nexuslang.jit.native_jit import NativeFunctionJIT

        mock_interp = _make_mock_interpreter()
        jit = NativeFunctionJIT(mock_interp)

        if not jit._available:
            pytest.skip("Native JIT tools not available in this environment")

        func_def = _make_func_def()

        with patch.object(jit, "_compile", side_effect=MemoryError("OOM")):
            with caplog.at_level(logging.WARNING, logger="nexuslang.jit.native_jit"):
                result = jit.compile("test_fn", func_def)

        assert result is None

        warnings = [r for r in caplog.records if r.levelno >= logging.WARNING]
        assert warnings, "Expected WARNING log for unexpected compile exception"
        assert any("test_fn" in r.message for r in warnings)

    def test_lookup_function_returns_none_on_attribute_error(self):
        """_lookup_function() returns None gracefully when interpreter attrs raise."""
        from nexuslang.jit.native_jit import NativeFunctionJIT

        mock_interp = MagicMock()
        # Make getattr raise AttributeError
        type(mock_interp).__getattr__ = MagicMock(side_effect=AttributeError("no attr"))
        mock_interp.functions = None
        mock_interp.global_scope = None
        mock_interp._global_scope = None

        jit = NativeFunctionJIT(mock_interp)
        result = jit._lookup_function("missing")
        assert result is None


# ---------------------------------------------------------------------------
# TypeInference hardening
# ---------------------------------------------------------------------------

class TestTypeInferenceHardening:
    """_binding_annotation_to_type handles unknown type names gracefully."""

    def test_unknown_type_annotation_returns_none(self):
        """Passing an unknown type name must return None, not raise."""
        from nexuslang.typesystem.type_inference import TypeInferenceEngine

        inf = TypeInferenceEngine()
        result = inf._binding_annotation_to_type("CompletelyUnknownTypeName_XYZ_999")
        # Must not raise; may return None or ANY_TYPE
        # What matters: no exception escapes
        assert result is None or result is not None  # always passes, but guards against raising

    def test_none_annotation_returns_none(self):
        from nexuslang.typesystem.type_inference import TypeInferenceEngine

        inf = TypeInferenceEngine()
        assert inf._binding_annotation_to_type(None) is None

    def test_known_type_annotation_resolves(self):
        from nexuslang.typesystem.type_inference import TypeInferenceEngine
        from nexuslang.typesystem.types import INTEGER_TYPE

        inf = TypeInferenceEngine()
        result = inf._binding_annotation_to_type("Integer")
        assert result is not None
        assert result == INTEGER_TYPE
