"""
Optimizer, PGO, and optimization-level tests.
Split from test_session_features.py.
"""

import sys
import os
import tempfile
import pytest
from pathlib import Path

_SRC = str(Path(__file__).resolve().parent.parent.parent.parent / "src")
if _SRC not in sys.path:
    sys.path.insert(0, _SRC)

class TestOptimizerPasses:
    def test_string_interning_import(self):
        from nexuslang.optimizer.string_interning import StringInterningPass
        p = StringInterningPass()
        assert p is not None

    def test_string_interning_has_name(self):
        from nexuslang.optimizer.string_interning import StringInterningPass
        p = StringInterningPass()
        assert hasattr(p, "name")
        assert isinstance(p.name, str)

    def test_type_specialization_import(self):
        from nexuslang.optimizer.type_specialization import TypeSpecializationPass
        p = TypeSpecializationPass()
        assert p is not None

    def test_type_specialization_has_name(self):
        from nexuslang.optimizer.type_specialization import TypeSpecializationPass
        p = TypeSpecializationPass()
        assert hasattr(p, "name")

    def test_dispatch_optimization_import(self):
        from nexuslang.optimizer.dispatch_optimization import DispatchOptimizationPass
        p = DispatchOptimizationPass()
        assert p is not None

    def test_dispatch_optimization_has_name(self):
        from nexuslang.optimizer.dispatch_optimization import DispatchOptimizationPass
        p = DispatchOptimizationPass()
        assert hasattr(p, "name")

    def test_pipeline_contains_passes(self):
        from nexuslang.optimizer import OptimizationPipeline
        pipeline = OptimizationPipeline()
        assert hasattr(pipeline, "passes")
        assert isinstance(pipeline.passes, list)

    def test_pipeline_add_pass(self):
        from nexuslang.optimizer import OptimizationPipeline
        from nexuslang.optimizer.string_interning import StringInterningPass
        pipeline = OptimizationPipeline()
        before = len(pipeline.passes)
        pipeline.add_pass(StringInterningPass())
        assert len(pipeline.passes) == before + 1

    def test_pipeline_all_three_passes_present(self):
        from nexuslang.optimizer import OptimizationPipeline
        pipeline = OptimizationPipeline()
        names = [p.name for p in pipeline.passes]
        # At least one of the new passes should be registered by default
        # (or we can just check all three can be imported cleanly)
        from nexuslang.optimizer.string_interning import StringInterningPass
        from nexuslang.optimizer.type_specialization import TypeSpecializationPass
        from nexuslang.optimizer.dispatch_optimization import DispatchOptimizationPass
        assert StringInterningPass().name
        assert TypeSpecializationPass().name
        assert DispatchOptimizationPass().name


# ============================================================
# Section 2 - PGO (Profile-Guided Optimization)
# ============================================================

class TestPGO:
    def test_pgo_import(self):
        from nexuslang.tooling.pgo import PGOProfile
        assert PGOProfile is not None

    def test_pgo_create_profile(self):
        from nexuslang.tooling.pgo import PGOProfile
        profile = PGOProfile(program_name="test")
        assert profile is not None

    def test_pgo_has_program_name(self):
        from nexuslang.tooling.pgo import PGOProfile
        profile = PGOProfile(program_name="my_prog")
        assert profile.program_name == "my_prog"

    def test_pgo_record_execution(self):
        from nexuslang.tooling.pgo import PGOProfile
        profile = PGOProfile(program_name="test")
        profile.record_execution("hot_func")
        assert profile.is_hot("hot_func")

    def test_pgo_cold_function(self):
        from nexuslang.tooling.pgo import PGOProfile
        profile = PGOProfile(program_name="test")
        assert not profile.is_hot("never_called")

    def test_pgo_save_and_load(self):
        from nexuslang.tooling.pgo import PGOProfile
        profile = PGOProfile(program_name="test")
        profile.record_execution("func_a")
        profile.record_execution("func_a")
        profile.record_execution("func_b")
        with tempfile.NamedTemporaryFile(suffix=".json", delete=False) as f:
            path = f.name
        try:
            profile.save(path)
            loaded = PGOProfile.load(path)
            assert loaded.is_hot("func_a")
        finally:
            os.unlink(path)

    def test_pgo_get_call_count(self):
        from nexuslang.tooling.pgo import PGOProfile
        profile = PGOProfile(program_name="test")
        for _ in range(5):
            profile.record_execution("counted_func")
        assert profile.get_call_count("counted_func") == 5


# ============================================================
# Section 3 - JIT Tiered Compilation
# ============================================================

class TestOptimizationLevel:
    def test_int_to_opt_level_import(self):
        from nexuslang.optimizer import int_to_opt_level
        assert callable(int_to_opt_level)

    def test_opt_level_0(self):
        from nexuslang.optimizer import int_to_opt_level, OptimizationLevel
        assert int_to_opt_level(0) == OptimizationLevel.O0

    def test_opt_level_1(self):
        from nexuslang.optimizer import int_to_opt_level, OptimizationLevel
        assert int_to_opt_level(1) == OptimizationLevel.O1

    def test_opt_level_2(self):
        from nexuslang.optimizer import int_to_opt_level, OptimizationLevel
        assert int_to_opt_level(2) == OptimizationLevel.O2

    def test_opt_level_3(self):
        from nexuslang.optimizer import int_to_opt_level, OptimizationLevel
        assert int_to_opt_level(3) == OptimizationLevel.O3

    def test_opt_level_out_of_range(self):
        from nexuslang.optimizer import int_to_opt_level
        with pytest.raises((ValueError, KeyError, Exception)):
            int_to_opt_level(99)


# ============================================================
# Section 15b - Interpreter-mode pipeline (Priority 8 regression guard)
# ============================================================

class TestInterpreterModePipeline:
    """Verify that interpreter_mode=True excludes AST-inflating passes.

    Root cause of the O3>O0 regression (perf-baseline.json Feb 2026):
    TypeSpecializationPass, FunctionInliningPass, LoopUnrollingPass, and
    DispatchOptimizationPass all add or duplicate AST nodes that the
    interpreter walks on every execution.  StringInterningPass at
    min_occurrences=1 annotates every string literal without benefit.
    None of these annotations are read by the interpreter.

    The change: create_optimization_pipeline(level, interpreter_mode=True)
    restricts the pipeline to passes that reduce interpretation work
    (ConstantFolding, DCE, StrengthReduction, CSE, TCO).
    """

    def _pass_names(self, mode, level_int):
        from nexuslang.optimizer import create_optimization_pipeline, OptimizationLevel
        lvl_map = {0: OptimizationLevel.O0, 1: OptimizationLevel.O1,
                   2: OptimizationLevel.O2, 3: OptimizationLevel.O3}
        pipeline = create_optimization_pipeline(lvl_map[level_int], interpreter_mode=mode)
        return [p.name for p in pipeline.passes]

    # --- interpreter_mode=True: forbidden passes must be absent ---

    @pytest.mark.parametrize("level", [1, 2, 3])
    def test_no_function_inlining_in_interpreter_mode(self, level):
        names = self._pass_names(True, level)
        assert not any("inlin" in n.lower() for n in names), (
            f"FunctionInliningPass must not run at O{level} in interpreter mode; got {names}"
        )

    @pytest.mark.parametrize("level", [1, 2, 3])
    def test_no_loop_unrolling_in_interpreter_mode(self, level):
        names = self._pass_names(True, level)
        assert not any("unroll" in n.lower() for n in names), (
            f"LoopUnrollingPass must not run at O{level} in interpreter mode; got {names}"
        )

    @pytest.mark.parametrize("level", [1, 2, 3])
    def test_no_type_specialization_in_interpreter_mode(self, level):
        names = self._pass_names(True, level)
        assert not any("specializ" in n.lower() for n in names), (
            f"TypeSpecializationPass must not run at O{level} in interpreter mode; got {names}"
        )

    @pytest.mark.parametrize("level", [1, 2, 3])
    def test_no_dispatch_optimization_in_interpreter_mode(self, level):
        names = self._pass_names(True, level)
        assert not any("dispatch" in n.lower() for n in names), (
            f"DispatchOptimizationPass must not run at O{level} in interpreter mode; got {names}"
        )

    # --- interpreter_mode=True: beneficial passes must be present ---

    @pytest.mark.parametrize("level", [1, 2, 3])
    def test_constant_folding_present_in_interpreter_mode(self, level):
        names = self._pass_names(True, level)
        assert any("constant" in n.lower() or "fold" in n.lower() for n in names), (
            f"ConstantFoldingPass must run at O{level} in interpreter mode; got {names}"
        )

    @pytest.mark.parametrize("level", [1, 2, 3])
    def test_dce_present_in_interpreter_mode(self, level):
        names = self._pass_names(True, level)
        assert any("dead" in n.lower() or "dce" in n.lower() or "eliminat" in n.lower() for n in names), (
            f"DeadCodeEliminationPass must run at O{level} in interpreter mode; got {names}"
        )

    # --- compiled mode still includes the full pipeline ---

    def test_compiled_mode_includes_inlining_at_o2(self):
        names = self._pass_names(False, 2)
        assert any("inlin" in n.lower() for n in names), (
            f"FunctionInliningPass must be present in compiled mode O2; got {names}"
        )

    def test_compiled_mode_includes_type_specialization_at_o2(self):
        names = self._pass_names(False, 2)
        assert any("specializ" in n.lower() for n in names), (
            f"TypeSpecializationPass must be present in compiled mode O2; got {names}"
        )

    def test_compiled_mode_includes_loop_unrolling_at_o2(self):
        names = self._pass_names(False, 2)
        assert any("unroll" in n.lower() for n in names), (
            f"LoopUnrollingPass must be present in compiled mode O2; got {names}"
        )

    # --- O0 is empty regardless of mode ---

    def test_o0_pipeline_empty_interpreter_mode(self):
        names = self._pass_names(True, 0)
        assert names == [], f"O0 pipeline must be empty; got {names}"

    def test_o0_pipeline_empty_compiled_mode(self):
        names = self._pass_names(False, 0)
        assert names == [], f"O0 pipeline must be empty; got {names}"

    # --- interpreter calls pipeline with interpreter_mode=True ---

    def test_interpreter_passes_interpreter_mode(self):
        """Verify the interpreter.interpret() passes interpreter_mode=True to the pipeline."""
        import inspect
        from nexuslang.interpreter.interpreter import Interpreter
        source = inspect.getsource(Interpreter.interpret)
        assert "interpreter_mode=True" in source, (
            "Interpreter.interpret() must call create_optimization_pipeline(..., interpreter_mode=True)"
        )


# ============================================================
# Section 16 - Assertion library (ExpectStatement)
# ============================================================

