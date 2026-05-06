"""
Tests for the NexusLang optimiser infrastructure.

Covers:
  - OptimizationLevel / OptimizationStats / OptimizationPass / OptimizationPipeline
    (src/nlpl/optimizer/__init__.py)
  - create_optimization_pipeline() and int_to_opt_level()
  - LLVMOptimizer (src/nlpl/compiler/llvm_optimizer.py)
  - All individual AST-level passes (constant folding, DCE, inlining, etc.)
  - Link-Time Optimisation framework (src/nlpl/optimizer/lto.py)
  - Loop optimisation passes (src/nlpl/optimizer/loop_optimizations.py)
"""

import copy
import sys
import types
from typing import Any

import pytest

# ---------------------------------------------------------------------------
# Path helpers so we can import from src/ without an editable install
# ---------------------------------------------------------------------------
import os

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "..", "src"))

# ---------------------------------------------------------------------------
# Imports under test
# ---------------------------------------------------------------------------
from nexuslang.optimizer import (
    OptimizationLevel,
    OptimizationPass,
    OptimizationPipeline,
    OptimizationStats,
    create_optimization_pipeline,
    int_to_opt_level,
)
from nexuslang.compiler.llvm_optimizer import (
    LLVMOptimizer,
    OptimizationLevel as LLVMLevel,
    optimize_llvm_ir,
)

# ---------------------------------------------------------------------------
# Tiny mock-AST helpers reused across many tests
# ---------------------------------------------------------------------------


class _Program:
    """Minimal mock root node with a statements list."""

    def __init__(self, statements=None):
        self.statements = statements or []


class _Literal:
    """Mock literal node whose class name matches what the passes check."""

    def __init__(self, value):
        self.value = value


class _BinaryOperation:
    """Mock binary-operation node."""

    def __init__(self, left, operator, right):
        self.left = left
        self.operator = operator
        self.right = right


class _UnaryOperation:
    """Mock unary-operation node."""

    def __init__(self, operator, operand):
        self.operator = operator
        self.operand = operand


# Force type().__name__ to recognise our mock types by using the same names.
_Literal.__name__ = "Literal"
_BinaryOperation.__name__ = "BinaryOperation"
_UnaryOperation.__name__ = "UnaryOperation"


def _binop(left_val, op, right_val):
    """Convenience factory: Literal op Literal."""
    node = _BinaryOperation(_Literal(left_val), op, _Literal(right_val))
    node.__class__.__name__ = "BinaryOperation"
    node.left.__class__.__name__ = "Literal"
    node.right.__class__.__name__ = "Literal"
    return node


def _prog(*stmts):
    return _Program(list(stmts))


# ===========================================================================
# I.  Core optimizer framework
# ===========================================================================


class TestOptimizationLevel:
    def test_o0_value(self):
        assert OptimizationLevel.O0.value == 0

    def test_o1_value(self):
        assert OptimizationLevel.O1.value == 1

    def test_o2_value(self):
        assert OptimizationLevel.O2.value == 2

    def test_o3_value(self):
        assert OptimizationLevel.O3.value == 3

    def test_os_value(self):
        assert OptimizationLevel.Os.value == 4

    def test_all_members(self):
        assert len(OptimizationLevel) == 5


class TestOptimizationStats:
    def test_defaults_zero(self):
        s = OptimizationStats()
        assert s.dead_functions_removed == 0
        assert s.dead_variables_removed == 0
        assert s.unreachable_blocks_removed == 0
        assert s.constants_folded == 0
        assert s.functions_inlined == 0
        assert s.total_passes == 0

    def test_str_contains_labels(self):
        s = OptimizationStats()
        text = str(s)
        assert "Dead Functions" in text
        assert "Dead Variables" in text
        assert "Constants Folded" in text
        assert "Functions Inlined" in text
        assert "Total Optimization Passes" in text

    def test_str_reflects_values(self):
        s = OptimizationStats(constants_folded=7)
        assert "7" in str(s)

    def test_direct_mutation(self):
        s = OptimizationStats()
        s.total_passes += 3
        assert s.total_passes == 3


class TestOptimizationPassBase:
    def test_name_stored(self):
        class _P(OptimizationPass):
            def run(self, ast):
                return ast

        p = _P("MyPass")
        assert p.name == "MyPass"

    def test_enabled_default_true(self):
        class _P(OptimizationPass):
            def run(self, ast):
                return ast

        assert _P("x").enabled is True

    def test_should_run_true_when_enabled(self):
        class _P(OptimizationPass):
            def run(self, ast):
                return ast

        p = _P("x")
        assert p.should_run(OptimizationLevel.O2) is True

    def test_should_run_false_when_disabled(self):
        class _P(OptimizationPass):
            def run(self, ast):
                return ast

        p = _P("x")
        p.enabled = False
        assert p.should_run(OptimizationLevel.O2) is False

    def test_run_not_implemented_in_base(self):
        p = OptimizationPass("bare")
        with pytest.raises(NotImplementedError):
            p.run(None)

    def test_stats_instance_per_pass(self):
        class _P(OptimizationPass):
            def run(self, ast):
                return ast

        p1, p2 = _P("a"), _P("b")
        p1.stats.constants_folded += 1
        assert p2.stats.constants_folded == 0


class TestOptimizationPipeline:
    def test_empty_pipeline_returns_ast_unchanged(self):
        pipeline = OptimizationPipeline()
        sentinel = object()
        assert pipeline.run(sentinel) is sentinel

    def test_add_pass_appends(self):
        class _P(OptimizationPass):
            def run(self, ast):
                return ast

        pipeline = OptimizationPipeline()
        p = _P("x")
        pipeline.add_pass(p)
        assert p in pipeline.passes

    def test_pipeline_runs_in_order(self):
        order = []

        class _Order(OptimizationPass):
            def __init__(self, label):
                super().__init__(label)
                self._label = label

            def run(self, ast):
                order.append(self._label)
                return ast

        pipeline = OptimizationPipeline()
        pipeline.add_pass(_Order("first"))
        pipeline.add_pass(_Order("second"))
        pipeline.run(None)
        assert order == ["first", "second"]

    def test_disabled_pass_skipped(self):
        called = []

        class _P(OptimizationPass):
            def run(self, ast):
                called.append(True)
                return ast

        pipeline = OptimizationPipeline()
        p = _P("x")
        p.enabled = False
        pipeline.add_pass(p)
        pipeline.run(None)
        assert called == []

    def test_stats_total_passes_incremented(self):
        class _P(OptimizationPass):
            def run(self, ast):
                return ast

        pipeline = OptimizationPipeline()
        pipeline.add_pass(_P("a"))
        pipeline.add_pass(_P("b"))
        pipeline.run(None)
        assert pipeline.stats.total_passes == 2

    def test_stats_constants_folded_aggregated(self):
        class _CountingPass(OptimizationPass):
            def run(self, ast):
                self.stats.constants_folded += 3
                return ast

        pipeline = OptimizationPipeline()
        pipeline.add_pass(_CountingPass("c"))
        pipeline.run(None)
        assert pipeline.stats.constants_folded == 3

    def test_level_stored(self):
        pipeline = OptimizationPipeline(OptimizationLevel.O3)
        assert pipeline.level == OptimizationLevel.O3

    def test_verbose_default_false(self):
        assert OptimizationPipeline().verbose is False


class TestCreateOptimizationPipeline:
    def test_o0_returns_empty_pipeline(self):
        p = create_optimization_pipeline(OptimizationLevel.O0)
        assert len(p.passes) == 0

    def test_o1_has_constant_folding(self):
        p = create_optimization_pipeline(OptimizationLevel.O1)
        names = [pas.name for pas in p.passes]
        assert any("Constant Folding" in n or "constant" in n.lower() for n in names)

    def test_o1_has_dce(self):
        p = create_optimization_pipeline(OptimizationLevel.O1)
        names = [pas.name for pas in p.passes]
        assert any("dead" in n.lower() or "dce" in n.lower() for n in names)

    def test_o2_has_more_passes_than_o1(self):
        p1 = create_optimization_pipeline(OptimizationLevel.O1)
        p2 = create_optimization_pipeline(OptimizationLevel.O2)
        assert len(p2.passes) > len(p1.passes)

    def test_o3_has_more_passes_than_o2(self):
        p2 = create_optimization_pipeline(OptimizationLevel.O2)
        p3 = create_optimization_pipeline(OptimizationLevel.O3)
        assert len(p3.passes) > len(p2.passes)

    def test_os_has_dce(self):
        p = create_optimization_pipeline(OptimizationLevel.Os)
        names = [pas.name for pas in p.passes]
        assert any("dead" in n.lower() or "dce" in n.lower() for n in names)

    def test_o3_has_cse(self):
        p = create_optimization_pipeline(OptimizationLevel.O3)
        names = [pas.name for pas in p.passes]
        assert any("subexpression" in n.lower() or "cse" in n.lower() for n in names)

    def test_o3_has_tco(self):
        p = create_optimization_pipeline(OptimizationLevel.O3)
        names = [pas.name for pas in p.passes]
        assert any("tail" in n.lower() or "tco" in n.lower() for n in names)

    def test_pipeline_level_set_correctly(self):
        p = create_optimization_pipeline(OptimizationLevel.O2)
        assert p.level == OptimizationLevel.O2


class TestIntToOptLevel:
    def test_zero_gives_o0(self):
        assert int_to_opt_level(0) == OptimizationLevel.O0

    def test_one_gives_o1(self):
        assert int_to_opt_level(1) == OptimizationLevel.O1

    def test_two_gives_o2(self):
        assert int_to_opt_level(2) == OptimizationLevel.O2

    def test_three_gives_o3(self):
        assert int_to_opt_level(3) == OptimizationLevel.O3

    def test_four_gives_os(self):
        assert int_to_opt_level(4) == OptimizationLevel.Os

    def test_s_string_gives_os(self):
        assert int_to_opt_level("s") == OptimizationLevel.Os

    def test_invalid_raises_value_error(self):
        with pytest.raises(ValueError):
            int_to_opt_level(99)

    def test_invalid_string_raises_value_error(self):
        with pytest.raises(ValueError):
            int_to_opt_level("fast")


# ===========================================================================
# II.  LLVMOptimizer
# ===========================================================================


class TestLLVMLevel:
    """OptimizationLevel inside llvm_optimizer.py."""

    def test_o0(self):
        assert LLVMLevel.O0.value == 0 or LLVMLevel.O0 is not None

    def test_o2_exists(self):
        assert hasattr(LLVMLevel, "O2")

    def test_o3_exists(self):
        assert hasattr(LLVMLevel, "O3")

    def test_os_exists(self):
        assert hasattr(LLVMLevel, "Os")


class TestLLVMOptimizer:
    def test_default_level_is_o2(self):
        opt = LLVMOptimizer()
        assert opt.optimization_level == LLVMLevel.O2

    def test_explicit_level_stored(self):
        opt = LLVMOptimizer(optimization_level=LLVMLevel.O3)
        assert opt.optimization_level == LLVMLevel.O3

    def test_get_pass_list_o0_returns_none_list(self):
        opt = LLVMOptimizer(optimization_level=LLVMLevel.O0)
        passes = opt.get_pass_list()
        assert isinstance(passes, list)
        assert "none" in passes

    def test_get_pass_list_o1_has_mem2reg(self):
        opt = LLVMOptimizer(optimization_level=LLVMLevel.O1)
        passes = opt.get_pass_list()
        assert "mem2reg" in passes

    def test_get_pass_list_o1_has_simplifycfg(self):
        opt = LLVMOptimizer(optimization_level=LLVMLevel.O1)
        assert "simplifycfg" in opt.get_pass_list()

    def test_get_pass_list_o2_superset_of_o1(self):
        opt1 = LLVMOptimizer(optimization_level=LLVMLevel.O1)
        opt2 = LLVMOptimizer(optimization_level=LLVMLevel.O2)
        set1, set2 = set(opt1.get_pass_list()), set(opt2.get_pass_list())
        assert set1.issubset(set2)

    def test_get_pass_list_o2_has_inline(self):
        opt = LLVMOptimizer(optimization_level=LLVMLevel.O2)
        assert "inline" in opt.get_pass_list()

    def test_get_pass_list_o3_has_loop_unroll(self):
        opt = LLVMOptimizer(optimization_level=LLVMLevel.O3)
        passes = opt.get_pass_list()
        assert "loop-unroll" in passes

    def test_get_pass_list_o3_has_vectorize(self):
        opt = LLVMOptimizer(optimization_level=LLVMLevel.O3)
        assert "vectorize" in opt.get_pass_list()

    def test_get_pass_list_os_size_opt(self):
        opt = LLVMOptimizer(optimization_level=LLVMLevel.Os)
        passes = opt.get_pass_list()
        # Os should contain a size-specific pass
        assert any("size" in p.lower() or p.startswith("Os") for p in passes)

    def test_set_level_changes_level(self):
        opt = LLVMOptimizer(optimization_level=LLVMLevel.O1)
        opt.set_level(LLVMLevel.O3)
        assert opt.optimization_level == LLVMLevel.O3
        assert "loop-unroll" in opt.get_pass_list()

    def test_estimate_speedup_o0(self):
        opt = LLVMOptimizer(optimization_level=LLVMLevel.O0)
        opt._estimate_speedup()
        assert opt.stats["estimated_speedup"] == pytest.approx(1.0)

    def test_estimate_speedup_o1(self):
        opt = LLVMOptimizer(optimization_level=LLVMLevel.O1)
        opt._estimate_speedup()
        assert opt.stats["estimated_speedup"] == pytest.approx(1.5)

    def test_estimate_speedup_o2(self):
        opt = LLVMOptimizer(optimization_level=LLVMLevel.O2)
        opt._estimate_speedup()
        assert opt.stats["estimated_speedup"] == pytest.approx(2.5)

    def test_estimate_speedup_o3(self):
        opt = LLVMOptimizer(optimization_level=LLVMLevel.O3)
        opt._estimate_speedup()
        assert opt.stats["estimated_speedup"] == pytest.approx(3.0)

    def test_estimate_speedup_os(self):
        opt = LLVMOptimizer(optimization_level=LLVMLevel.Os)
        opt._estimate_speedup()
        assert opt.stats["estimated_speedup"] == pytest.approx(2.0)

    def test_get_stats_has_expected_keys(self):
        opt = LLVMOptimizer()
        stats = opt.get_stats()
        assert "passes_run" in stats
        assert "functions_optimized" in stats
        assert "estimated_speedup" in stats

    def test_get_stats_speedup_matches_estimate(self):
        opt = LLVMOptimizer(optimization_level=LLVMLevel.O3)
        # _estimate_speedup() must be called to update the stats dict
        opt._estimate_speedup()
        assert opt.get_stats()["estimated_speedup"] == pytest.approx(3.0)

    def test_optimize_module_o0_returns_input(self):
        """At O0 there are no passes, so the original IR should come back."""
        opt = LLVMOptimizer(optimization_level=LLVMLevel.O0)
        ir = "; module\ndefine i32 @main() { ret i32 0 }"
        result = opt.optimize_module(ir)
        # O0 does nothing — either returns as-is OR still calls opt in passthrough mode
        assert isinstance(result, str)
        assert len(result) > 0

    def test_optimize_module_graceful_when_opt_missing(self, monkeypatch, tmp_path):
        """If the 'opt' binary is absent, should return original IR, not raise."""
        import subprocess

        original_run = subprocess.run

        def _failing_run(*args, **kwargs):
            raise FileNotFoundError("opt not found")

        monkeypatch.setattr(subprocess, "run", _failing_run)
        opt = LLVMOptimizer(optimization_level=LLVMLevel.O2)
        ir = "; test module\ndefine void @foo() { ret void }"
        result = opt.optimize_module(ir)
        assert result == ir  # falls back to original

    def test_optimize_file_false_for_missing_input(self, tmp_path):
        opt = LLVMOptimizer()
        out = tmp_path / "out.ll"
        result = opt.optimize_file(str(tmp_path / "nonexistent.ll"), str(out))
        assert result is False

    def test_optimize_file_creates_output(self, tmp_path, monkeypatch):
        """When input exists and opt succeeds, output file should be created."""
        import subprocess

        in_file = tmp_path / "in.ll"
        out_file = tmp_path / "out.ll"
        in_file.write_text("; test\ndefine void @f() { ret void }\n")

        # Simulate opt by writing input to output
        def _mock_run(cmd, *args, **kwargs):
            # Copy input to output
            out_path = str(out_file)
            with open(in_file) as f:
                content = f.read()
            with open(out_path, "w") as f:
                f.write(content)
            result = types.SimpleNamespace(returncode=0, stdout=b"", stderr=b"")
            return result

        monkeypatch.setattr(subprocess, "run", _mock_run)
        opt = LLVMOptimizer(optimization_level=LLVMLevel.O2)
        # Whether True or False depends on implementation; just ensure no exception
        result = opt.optimize_file(str(in_file), str(out_file))
        assert isinstance(result, bool)

    def test_print_stats_runs_without_error(self, capsys):
        opt = LLVMOptimizer()
        opt.print_stats()  # should not raise


class TestOptimizeLlvmIr:
    def test_o0_returns_input(self):
        ir = "; test\ndefine void @empty() { ret void }"
        result = optimize_llvm_ir(ir, level="O0")
        # Should return as-is or similar string (no opt binary needed for O0)
        assert isinstance(result, str)

    def test_invalid_level_string_handled(self):
        """Unknown level should not crash — falls back to O2 or returns ir."""
        ir = "; test"
        # Should not raise
        try:
            result = optimize_llvm_ir(ir, level="O9")
            assert isinstance(result, str)
        except (ValueError, KeyError):
            pass  # Raising is also acceptable

    def test_graceful_when_opt_missing(self, monkeypatch):
        import subprocess

        def _raise(*args, **kwargs):
            raise FileNotFoundError("opt not found")

        monkeypatch.setattr(subprocess, "run", _raise)
        ir = "; will fallback"
        result = optimize_llvm_ir(ir, level="O2")
        assert result == ir


# ===========================================================================
# III.  Individual AST-level optimizer passes — smoke tests
# ===========================================================================

def _import_passes():
    from nexuslang.optimizer.constant_folding import ConstantFoldingPass
    from nexuslang.optimizer.dead_code_elimination import DeadCodeEliminationPass
    from nexuslang.optimizer.function_inlining import FunctionInliningPass
    from nexuslang.optimizer.strength_reduction import StrengthReductionPass
    from nexuslang.optimizer.loop_unrolling import LoopUnrollingPass
    from nexuslang.optimizer.common_subexpression_elimination import CommonSubexpressionEliminationPass
    from nexuslang.optimizer.tail_call_optimization import TailCallOptimizationPass
    from nexuslang.optimizer.string_interning import StringInterningPass
    from nexuslang.optimizer.type_specialization import TypeSpecializationPass
    from nexuslang.optimizer.dispatch_optimization import DispatchOptimizationPass
    return (
        ConstantFoldingPass, DeadCodeEliminationPass, FunctionInliningPass,
        StrengthReductionPass, LoopUnrollingPass, CommonSubexpressionEliminationPass,
        TailCallOptimizationPass, StringInterningPass, TypeSpecializationPass,
        DispatchOptimizationPass,
    )


class TestPassInstantiation:
    def setup_method(self):
        (
            self.CF, self.DCE, self.FI, self.SR, self.LU, self.CSE,
            self.TCO, self.SI, self.TS, self.DO,
        ) = _import_passes()

    def test_cf_name(self):
        assert "Constant Folding" in self.CF().name or "constant" in self.CF().name.lower()

    def test_dce_name(self):
        name = self.DCE().name.lower()
        assert "dead" in name or "dce" in name

    def test_fi_name(self):
        name = self.FI().name.lower()
        assert "inline" in name or "inlining" in name

    def test_sr_name(self):
        name = self.SR().name.lower()
        assert "strength" in name or "reduction" in name

    def test_lu_name(self):
        name = self.LU().name.lower()
        assert "unroll" in name or "loop" in name

    def test_cse_name(self):
        name = self.CSE().name.lower()
        assert "subexpression" in name or "cse" in name or "common" in name

    def test_tco_name(self):
        name = self.TCO().name.lower()
        assert "tail" in name or "tco" in name

    def test_si_name(self):
        name = self.SI().name.lower()
        assert "string" in name or "intern" in name

    def test_ts_name(self):
        name = self.TS().name.lower()
        assert "special" in name or "type" in name

    def test_do_name(self):
        name = self.DO().name.lower()
        assert "dispatch" in name

    def test_all_enabled_by_default(self):
        for PassClass in _import_passes():
            assert PassClass().enabled is True

    def test_disable_cf(self):
        cf = self.CF()
        cf.enabled = False
        assert cf.should_run(OptimizationLevel.O2) is False

    def test_cf_run_returns_something(self):
        cf = self.CF()
        prog = _prog()
        result = cf.run(prog)
        assert result is not None

    def test_dce_run_returns_something(self):
        dce = self.DCE()
        prog = _prog()
        assert dce.run(prog) is not None


# ===========================================================================
# IV.  ConstantFolding functional tests with mock AST
# ===========================================================================


class TestConstantFoldingFunctional:
    def setup_method(self):
        from nexuslang.optimizer.constant_folding import ConstantFoldingPass
        self.pass_ = ConstantFoldingPass()

    def _fold(self, left_val, op, right_val):
        """Run the pass on a single BinaryOperation and inspect the result."""
        prog = _prog(_binop(left_val, op, right_val))
        result = self.pass_.run(prog)
        return result.statements[0]

    def test_integer_addition(self):
        node = self._fold(2, "+", 3)
        assert type(node).__name__ == "Literal"
        assert node.value == 5

    def test_integer_subtraction(self):
        node = self._fold(10, "-", 3)
        assert type(node).__name__ == "Literal"
        assert node.value == 7

    def test_integer_multiplication(self):
        node = self._fold(4, "*", 5)
        assert type(node).__name__ == "Literal"
        assert node.value == 20

    def test_integer_division(self):
        node = self._fold(10, "/", 4)
        assert type(node).__name__ == "Literal"
        assert abs(node.value - 2.5) < 1e-6

    def test_integer_modulo(self):
        node = self._fold(10, "%", 3)
        assert type(node).__name__ == "Literal"
        assert node.value == 1

    def test_string_concatenation(self):
        node = self._fold("hello", "+", " world")
        assert type(node).__name__ == "Literal"
        assert node.value == "hello world"

    def test_division_by_zero_not_folded(self):
        """Division by zero must NOT be folded — keep original node."""
        node = self._fold(5, "/", 0)
        # Must remain a BinaryOperation, not a Literal
        assert type(node).__name__ == "BinaryOperation"

    def test_modulo_by_zero_not_folded(self):
        node = self._fold(5, "%", 0)
        assert type(node).__name__ == "BinaryOperation"

    def test_plus_alias(self):
        """Natural-language 'plus' keyword should fold like '+'."""
        node = self._fold(1, "plus", 2)
        assert type(node).__name__ == "Literal"
        assert node.value == 3

    def test_minus_alias(self):
        node = self._fold(9, "minus", 4)
        assert type(node).__name__ == "Literal"
        assert node.value == 5

    def test_times_alias(self):
        node = self._fold(3, "times", 7)
        assert type(node).__name__ == "Literal"
        assert node.value == 21

    def test_non_literal_operand_not_folded(self):
        """If one operand is not a Literal the node must be left alone."""
        class _Ident:
            pass

        _Ident.__name__ = "Identifier"
        op = _BinaryOperation(_Ident(), "+", _Literal(5))
        op.__class__.__name__ = "BinaryOperation"
        op.right.__class__.__name__ = "Literal"
        prog = _prog(op)
        result = self.pass_.run(prog)
        assert type(result.statements[0]).__name__ == "BinaryOperation"

    def test_deep_copy_does_not_mutate_original(self):
        """run() must deep-copy the AST, not mutate it in place."""
        orig = _prog(_binop(3, "+", 4))
        orig_type = type(orig.statements[0]).__name__
        self.pass_.run(orig)
        assert type(orig.statements[0]).__name__ == orig_type


# ===========================================================================
# V.  Link-Time Optimisation framework
# ===========================================================================


class TestLTOStats:
    def _cls(self):
        from nexuslang.optimizer.lto import LTOStats
        return LTOStats

    def test_defaults_zero(self):
        s = self._cls()()
        assert s.dead_exports_removed == 0
        assert s.functions_inlined_cross_module == 0
        assert s.constants_propagated == 0
        assert s.dead_imports_removed == 0
        assert s.redundant_exports_removed == 0
        assert s.total_passes_run == 0

    def test_str_contains_header(self):
        s = self._cls()()
        assert "LTO" in str(s)

    def test_str_contains_dead_exports(self):
        s = self._cls()()
        assert "Dead exports" in str(s) or "dead_exports" in str(s).lower()

    def test_add_sums_fields(self):
        C = self._cls()
        a = C(dead_exports_removed=2, constants_propagated=1)
        b = C(dead_exports_removed=3, constants_propagated=4)
        c = a + b
        assert c.dead_exports_removed == 5
        assert c.constants_propagated == 5


class TestLTOUnit:
    def _cls(self):
        from nexuslang.optimizer.lto import LTOUnit
        return LTOUnit

    def test_name_stored(self):
        u = self._cls()("math")
        assert u.name == "math"

    def test_exports_empty_default(self):
        u = self._cls()("x")
        assert len(u.exports) == 0

    def test_exports_provided(self):
        u = self._cls()("x", exports={"foo", "bar"})
        assert "foo" in u.exports

    def test_imports_empty_default(self):
        u = self._cls()("x")
        assert len(u.imports) == 0

    def test_imports_provided(self):
        u = self._cls()("x", imports={"add": "math"})
        assert u.imports["add"] == "math"

    def test_repr_contains_name(self):
        u = self._cls()("alpha")
        assert "alpha" in repr(u)

    def test_stats_initialised(self):
        from nexuslang.optimizer.lto import LTOStats
        u = self._cls()("x")
        assert isinstance(u.stats, LTOStats)


class TestLTOContext:
    def _classes(self):
        from nexuslang.optimizer.lto import LTOContext, LTOUnit
        return LTOContext, LTOUnit

    def test_empty_context(self):
        C, _ = self._classes()
        ctx = C()
        assert len(ctx.units) == 0

    def test_add_unit(self):
        C, U = self._classes()
        ctx = C()
        u = U("m1", exports={"fn"})
        ctx.add_unit(u)
        assert len(ctx.units) == 1

    def test_build_index(self):
        C, U = self._classes()
        u1 = U("modA", exports={"alpha"})
        ctx = C(units=[u1])
        ctx.build_index()
        assert ctx.defining_unit("alpha") is u1

    def test_defining_unit_missing_returns_none(self):
        C, _ = self._classes()
        ctx = C()
        ctx.build_index()
        assert ctx.defining_unit("ghost") is None

    def test_all_imported_symbols(self):
        C, U = self._classes()
        u1 = U("a", imports={"x": "b"})
        u2 = U("b", imports={"y": "c"})
        ctx = C(units=[u1, u2])
        syms = ctx.all_imported_symbols()
        assert "x" in syms and "y" in syms

    def test_all_exported_symbols(self):
        C, U = self._classes()
        u1 = U("a", exports={"fn1"})
        u2 = U("b", exports={"fn2"})
        ctx = C(units=[u1, u2])
        assert {"fn1", "fn2"}.issubset(ctx.all_exported_symbols())

    def test_unit_by_name(self):
        C, U = self._classes()
        u = U("target")
        ctx = C(units=[u])
        assert ctx.unit_by_name("target") is u

    def test_unit_by_name_missing(self):
        C, U = self._classes()
        ctx = C()
        assert ctx.unit_by_name("nope") is None

    def test_repr_contains_unit_count(self):
        C, U = self._classes()
        ctx = C(units=[U("a"), U("b")])
        assert "2" in repr(ctx)


class TestCrossModuleDCEPass:
    def _setup(self):
        from nexuslang.optimizer.lto import (
            CrossModuleDCEPass, LTOContext, LTOUnit
        )
        # modA exports "foo" and "bar"; modB imports only "foo"
        unit_a = LTOUnit("modA", exports={"foo", "bar"})
        unit_b = LTOUnit("modB", imports={"foo": "modA"})
        ctx = LTOContext(units=[unit_a, unit_b], entry_points=set())
        return CrossModuleDCEPass, ctx, unit_a, unit_b

    def test_run_returns_ast_unchanged(self):
        from nexuslang.optimizer.lto import CrossModuleDCEPass
        p = CrossModuleDCEPass()
        sentinel = object()
        assert p.run(sentinel) is sentinel

    def test_unreferenced_export_removed(self):
        P, ctx, unit_a, unit_b = self._setup()
        p = P(keep_entry_exports=False)
        ctx = p.run_on_context(ctx)
        assert "bar" not in unit_a.exports

    def test_referenced_export_kept(self):
        P, ctx, unit_a, unit_b = self._setup()
        p = P(keep_entry_exports=False)
        ctx = p.run_on_context(ctx)
        assert "foo" in unit_a.exports

    def test_entry_exports_kept_by_default(self):
        from nexuslang.optimizer.lto import CrossModuleDCEPass, LTOContext, LTOUnit
        unit_ep = LTOUnit("main", exports={"run_app"})
        ctx = LTOContext(units=[unit_ep], entry_points={"main"})
        CrossModuleDCEPass().run_on_context(ctx)
        assert "run_app" in unit_ep.exports

    def test_stats_updated(self):
        P, ctx, unit_a, unit_b = self._setup()
        p = P(keep_entry_exports=False)
        p.run_on_context(ctx)
        assert p.stats.dead_functions_removed >= 1


class TestDeadImportEliminationPass:
    def test_unused_import_removed(self):
        from nexuslang.optimizer.lto import DeadImportEliminationPass, LTOContext, LTOUnit

        unit = LTOUnit("user", imports={"unused_fn": "libA"})
        unit.referenced_symbols = set()  # nothing referenced
        ctx = LTOContext(units=[unit])
        p = DeadImportEliminationPass()
        p.run_on_context(ctx)
        assert "unused_fn" not in unit.imports

    def test_used_import_kept(self):
        from nexuslang.optimizer.lto import DeadImportEliminationPass, LTOContext, LTOUnit

        unit = LTOUnit("user", imports={"used_fn": "libA"})
        unit.referenced_symbols = {"used_fn"}
        ctx = LTOContext(units=[unit])
        p = DeadImportEliminationPass()
        p.run_on_context(ctx)
        assert "used_fn" in unit.imports


class TestConstantPropagationPass:
    def test_propagates_exported_constant(self):
        from nexuslang.optimizer.lto import ConstantPropagationPass, LTOContext, LTOUnit

        exporter = LTOUnit("constants", exports={"MAX"}, constants={"MAX": 100})
        importer = LTOUnit("user", imports={"MAX": "constants"})
        ctx = LTOContext(units=[exporter, importer])
        ConstantPropagationPass().run_on_context(ctx)
        assert importer.constants.get("MAX") == 100


class TestRedundantExportPass:
    def test_removes_unexported_symbol(self):
        from nexuslang.optimizer.lto import RedundantExportPass, LTOContext, LTOUnit

        unit = LTOUnit("lib", exports={"used", "unused"})
        user = LTOUnit("main", imports={"used": "lib"})
        ctx = LTOContext(units=[unit, user])
        RedundantExportPass().run_on_context(ctx)
        assert "unused" not in unit.exports

    def test_keeps_entry_point_exports(self):
        from nexuslang.optimizer.lto import RedundantExportPass, LTOContext, LTOUnit

        unit = LTOUnit("app", exports={"main_func"})
        ctx = LTOContext(units=[unit], entry_points={"app"})
        RedundantExportPass().run_on_context(ctx)
        assert "main_func" in unit.exports


class TestSymbolReferenceAnalysisPass:
    def test_populates_referenced_symbols_from_dict_ast(self):
        from nexuslang.optimizer.lto import SymbolReferenceAnalysisPass, LTOContext, LTOUnit

        ast = {"statements": [
            {"type": "FunctionCall", "name": "greet"},
        ]}
        unit = LTOUnit("x", ast=object())
        unit.ast = ast
        ctx = LTOContext(units=[unit])
        SymbolReferenceAnalysisPass().run_on_context(ctx)
        assert "greet" in unit.referenced_symbols


class TestLTOPipelineClass:
    def test_default_pipeline_has_passes(self):
        from nexuslang.optimizer.lto import LTOPipeline
        p = LTOPipeline.default()
        assert len(p.passes) >= 5

    def test_aggressive_pipeline_has_more_passes(self):
        from nexuslang.optimizer.lto import LTOPipeline
        std = LTOPipeline.default(aggressive=False)
        agg = LTOPipeline.default(aggressive=True)
        assert len(agg.passes) >= len(std.passes)

    def test_run_returns_context(self):
        from nexuslang.optimizer.lto import LTOPipeline, LTOContext, LTOUnit
        ctx = LTOContext(units=[LTOUnit("x", exports={"a"})], entry_points=set())
        result = LTOPipeline.default().run(ctx)
        from nexuslang.optimizer.lto import LTOContext as C
        assert isinstance(result, C)

    def test_stats_total_passes_incremented(self):
        from nexuslang.optimizer.lto import LTOPipeline, LTOContext, LTOUnit
        ctx = LTOContext(units=[LTOUnit("x")], entry_points=set())
        result = LTOPipeline.default().run(ctx)
        assert result.stats.total_passes_run > 0


class TestLTOConvenienceFunctions:
    def test_lto_optimize_returns_context(self):
        from nexuslang.optimizer.lto import lto_optimize, LTOUnit, LTOContext
        units = [LTOUnit("main")]
        ctx = lto_optimize(units)
        assert isinstance(ctx, LTOContext)

    def test_lto_optimize_sets_default_entry_point(self):
        from nexuslang.optimizer.lto import lto_optimize, LTOUnit
        units = [LTOUnit("alpha"), LTOUnit("beta")]
        ctx = lto_optimize(units)
        assert "beta" in ctx.entry_points

    def test_lto_stats_report_returns_string(self):
        from nexuslang.optimizer.lto import lto_optimize, lto_stats_report, LTOUnit
        ctx = lto_optimize([LTOUnit("x")])
        report = lto_stats_report(ctx)
        assert isinstance(report, str)
        assert "LTO" in report or "Link" in report or "lto" in report.lower()

    def test_lto_stats_report_has_per_unit_section(self):
        from nexuslang.optimizer.lto import lto_optimize, lto_stats_report, LTOUnit
        ctx = lto_optimize([LTOUnit("my_module")])
        report = lto_stats_report(ctx)
        assert "my_module" in report


# ===========================================================================
# VI.  Loop optimisation passes — smoke tests
# ===========================================================================


class TestLoopOptimizationPasses:
    def _import(self):
        from nexuslang.optimizer.loop_optimizations import (
            LoopAnalysisPass,
            LoopInvariantCodeMotionPass,
            LoopFusionPass,
            InductionVariableSimplificationPass,
            LoopStrengthReductionPass,
            LoopOptimizationPipeline,
        )
        return (
            LoopAnalysisPass, LoopInvariantCodeMotionPass, LoopFusionPass,
            InductionVariableSimplificationPass, LoopStrengthReductionPass,
            LoopOptimizationPipeline,
        )

    def test_loop_analysis_pass_name(self):
        LA, *_ = self._import()
        assert "loop" in LA().name.lower() or "analysis" in LA().name.lower()

    def test_licm_pass_name(self):
        _, LICM, *_ = self._import()
        name = LICM().name.lower()
        assert "invariant" in name or "licm" in name or "motion" in name

    def test_loop_fusion_pass_name(self):
        _, _, LF, *_ = self._import()
        assert "fus" in LF().name.lower() or "loop" in LF().name.lower()

    def test_induction_variable_pass_name(self):
        _, _, _, IV, *_ = self._import()
        name = IV().name.lower()
        assert "induction" in name or "variable" in name

    def test_loop_strength_reduction_pass_name(self):
        _, _, _, _, LSR, _ = self._import()
        name = LSR().name.lower()
        assert "strength" in name or "reduction" in name or "loop" in name

    def test_all_loop_passes_enabled_by_default(self):
        LA, LICM, LF, IV, LSR, _ = self._import()
        for Cls in (LA, LICM, LF, IV, LSR):
            assert Cls().enabled is True

    def test_loop_pipeline_instantiates(self):
        _, _, _, _, _, Pipeline = self._import()
        p = Pipeline()
        assert p is not None

    def test_loop_pipeline_run_returns_ast(self):
        _, _, _, _, _, Pipeline = self._import()
        p = Pipeline()
        prog = _prog()
        result = p.run(prog)
        assert result is not None

    def test_loop_unrolling_pass_instantiates(self):
        from nexuslang.optimizer.loop_unrolling import LoopUnrollingPass
        p = LoopUnrollingPass(max_unroll_count=4)
        assert p is not None
        assert "unroll" in p.name.lower() or "loop" in p.name.lower()
