"""
pytest tests for src/nlpl/optimizer/loop_optimizations.py

Run with:
    pytest tests/unit/compiler/test_loop_optimizations.py
"""
import sys
import types
from pathlib import Path

import pytest

_SRC = str(Path(__file__).resolve().parent.parent.parent.parent / "src")
if _SRC not in sys.path:
    sys.path.insert(0, _SRC)

from nexuslang.optimizer.loop_optimizations import (
    LoopOptimizationStats,
    LoopInfo,
    LoopAnalysisPass,
    LoopInvariantCodeMotionPass,
    LoopFusionPass,
    InductionVariableSimplificationPass,
    LoopStrengthReductionPass,
    LoopOptimizationPipeline,
    loop_optimize,
    _is_loop,
    _get_body,
    _collect_assigned_vars,
    _collect_referenced_vars,
    _analyze_loop,
    _is_invariant_stmt,
    _loops_have_same_range,
    _loops_independent,
    _is_induction_multiply,
    _reduce_strength_in_expr,
    _SyntheticLiteral,
)
from nexuslang.parser.ast import (
    ForLoop, WhileLoop, RepeatNTimesLoop, RepeatWhileLoop,
    VariableDeclaration, BinaryOperation, Identifier, Literal,
    IfStatement,
)


# ---------------------------------------------------------------------------
# Minimal AST helpers
# ---------------------------------------------------------------------------

def _lit(value):
    """Make a Literal node with an integer value."""
    return Literal(type="integer", value=value)


def _ident(name):
    return Identifier(name=name)


def _binop(op, left, right):
    return BinaryOperation(left=left, operator=op, right=right)


def _vardecl(name, value):
    return VariableDeclaration(name=name, value=value)


def _for_range(iterator, start, end, body, step=None):
    return ForLoop(
        iterator=iterator,
        start=_lit(start) if isinstance(start, int) else start,
        end=_lit(end) if isinstance(end, int) else end,
        step=_lit(step) if isinstance(step, int) else step,
        body=body,
    )


def _for_each(iterator, iterable_name, body):
    return ForLoop(iterator=iterator, iterable=_ident(iterable_name), body=body)


def _while_loop(body):
    return WhileLoop(condition=_lit(1), body=body)


def _repeat(count_val, body):
    return RepeatNTimesLoop(count=_lit(count_val), body=body)


def _program(stmts):
    """Minimal program-like namespace."""
    ns = types.SimpleNamespace()
    ns.statements = list(stmts)
    return ns


# ---------------------------------------------------------------------------
# TestLoopOptimizationStats
# ---------------------------------------------------------------------------

class TestLoopOptimizationStats:
    def test_default_values(self):
        s = LoopOptimizationStats()
        assert s.invariants_hoisted == 0
        assert s.loops_fused == 0
        assert s.induction_vars_simplified == 0
        assert s.strength_reductions == 0
        assert s.total_passes_run == 0

    def test_add_two_stats(self):
        a = LoopOptimizationStats(invariants_hoisted=3, loops_fused=1, total_passes_run=2)
        b = LoopOptimizationStats(invariants_hoisted=2, loops_fused=4, total_passes_run=3)
        c = a + b
        assert c.invariants_hoisted == 5
        assert c.loops_fused == 5
        assert c.total_passes_run == 5

    def test_str_contains_field_names(self):
        s = LoopOptimizationStats(invariants_hoisted=7, strength_reductions=2)
        text = str(s)
        assert "Invariants Hoisted" in text
        assert "7" in text
        assert "Strength Reductions" in text

    def test_add_preserves_induction_vars(self):
        a = LoopOptimizationStats(induction_vars_simplified=5)
        b = LoopOptimizationStats(induction_vars_simplified=3)
        assert (a + b).induction_vars_simplified == 8

    def test_zero_plus_zero(self):
        assert (LoopOptimizationStats() + LoopOptimizationStats()) == LoopOptimizationStats()


# ---------------------------------------------------------------------------
# TestLoopInfo
# ---------------------------------------------------------------------------

class TestLoopInfo:
    def test_loop_info_fields_accessible(self):
        loop = _for_range("i", 0, 10, [])
        info = LoopInfo(
            loop_node=loop, loop_kind="for", body=[],
            induction_var="i", iteration_count=10,
            is_count_known=True, variant_vars={"i"}, referenced_vars=set(),
        )
        assert info.loop_kind == "for"
        assert info.induction_var == "i"
        assert info.iteration_count == 10
        assert info.is_count_known is True

    def test_loop_info_unknown_count(self):
        info = LoopInfo(
            loop_node=None, loop_kind="while", body=[],
            induction_var=None, iteration_count=None,
            is_count_known=False, variant_vars=set(), referenced_vars=set(),
        )
        assert info.is_count_known is False
        assert info.iteration_count is None

    def test_variant_and_referenced_vars_are_sets(self):
        info = LoopInfo(
            loop_node=None, loop_kind="for", body=[],
            induction_var="j", iteration_count=5,
            is_count_known=True, variant_vars={"j", "x"}, referenced_vars={"a", "b"},
        )
        assert "j" in info.variant_vars
        assert "a" in info.referenced_vars

    def test_loop_info_none_body(self):
        info = LoopInfo(
            loop_node=None, loop_kind="repeat", body=[],
            induction_var=None, iteration_count=3,
            is_count_known=True, variant_vars=set(), referenced_vars=set(),
        )
        assert info.body == []


# ---------------------------------------------------------------------------
# TestIsLoop
# ---------------------------------------------------------------------------

class TestIsLoop:
    def test_for_loop_is_loop(self):
        assert _is_loop(_for_range("i", 0, 5, []))

    def test_while_loop_is_loop(self):
        assert _is_loop(_while_loop([]))

    def test_repeat_n_is_loop(self):
        assert _is_loop(_repeat(3, []))

    def test_repeat_while_is_loop(self):
        rw = RepeatWhileLoop(condition=_lit(1), body=[])
        assert _is_loop(rw)

    def test_non_loop_is_not_loop(self):
        assert not _is_loop(_vardecl("x", _lit(1)))
        assert not _is_loop(_lit(42))
        assert not _is_loop(types.SimpleNamespace(name="Program"))


# ---------------------------------------------------------------------------
# TestGetBody
# ---------------------------------------------------------------------------

class TestGetBody:
    def test_for_loop_body(self):
        stmt = _vardecl("x", _lit(1))
        loop = _for_range("i", 0, 3, [stmt])
        assert _get_body(loop) == [stmt]

    def test_while_loop_body(self):
        stmt = _vardecl("y", _lit(2))
        loop = _while_loop([stmt])
        assert _get_body(loop) == [stmt]

    def test_namespace_with_statements(self):
        ns = types.SimpleNamespace()
        ns.statements = [1, 2, 3]
        assert _get_body(ns) == [1, 2, 3]

    def test_node_without_body_returns_empty(self):
        assert _get_body(_lit(5)) == []


# ---------------------------------------------------------------------------
# TestCollectAssignedVars
# ---------------------------------------------------------------------------

class TestCollectAssignedVars:
    def test_single_declaration(self):
        stmts = [_vardecl("x", _lit(1))]
        assert _collect_assigned_vars(stmts) == {"x"}

    def test_multiple_declarations(self):
        stmts = [_vardecl("a", _lit(1)), _vardecl("b", _lit(2))]
        assert _collect_assigned_vars(stmts) == {"a", "b"}

    def test_no_declarations(self):
        assert _collect_assigned_vars([]) == set()

    def test_nested_in_loop(self):
        inner = [_vardecl("z", _lit(0))]
        loop = _for_range("i", 0, 5, inner)
        outer = [_vardecl("outer_var", _lit(10)), loop]
        result = _collect_assigned_vars(outer)
        # Nested declarations inside the loop body are collected recursively
        assert "outer_var" in result
        assert "z" in result

    def test_only_var_decl_counted(self):
        stmts = [_ident("x"), _lit(5)]
        assert _collect_assigned_vars(stmts) == set()

    def test_duplicate_assignment(self):
        stmts = [_vardecl("x", _lit(1)), _vardecl("x", _lit(2))]
        assert _collect_assigned_vars(stmts) == {"x"}


# ---------------------------------------------------------------------------
# TestCollectReferencedVars
# ---------------------------------------------------------------------------

class TestCollectReferencedVars:
    def test_identifier_in_list(self):
        result = _collect_referenced_vars([_ident("foo")])
        assert "foo" in result

    def test_binary_op_refs(self):
        expr = _binop("+", _ident("a"), _ident("b"))
        result = _collect_referenced_vars(expr)
        assert "a" in result
        assert "b" in result

    def test_literal_has_no_refs(self):
        assert _collect_referenced_vars(_lit(42)) == set()

    def test_none_returns_empty(self):
        assert _collect_referenced_vars(None) == set()

    def test_dict_values_scanned(self):
        d = {"name": "ignored_key", "ref": Identifier(name="my_var")}
        result = _collect_referenced_vars(d)
        assert "my_var" in result


# ---------------------------------------------------------------------------
# TestIsInvariantStmt
# ---------------------------------------------------------------------------

class TestIsInvariantStmt:
    def test_pure_constant_assignment_is_invariant(self):
        stmt = _vardecl("c", _lit(42))
        assert _is_invariant_stmt(stmt, variant_vars=set()) is True

    def test_assignment_depending_on_non_variant_is_invariant(self):
        stmt = _vardecl("result", _binop("+", _ident("a"), _ident("b")))
        assert _is_invariant_stmt(stmt, variant_vars={"i"}) is True

    def test_assignment_depending_on_variant_is_not_invariant(self):
        stmt = _vardecl("result", _binop("*", _ident("i"), _lit(2)))
        assert _is_invariant_stmt(stmt, variant_vars={"i"}) is False

    def test_non_declaration_is_never_invariant(self):
        assert _is_invariant_stmt(_ident("foo"), variant_vars=set()) is False
        assert _is_invariant_stmt(_lit(1), variant_vars=set()) is False

    def test_declaration_of_variant_var_is_not_invariant(self):
        stmt = _vardecl("i", _lit(0))
        assert _is_invariant_stmt(stmt, variant_vars={"i"}) is False

    def test_none_value_is_not_invariant(self):
        stmt = VariableDeclaration(name="x", value=None)
        assert _is_invariant_stmt(stmt, variant_vars=set()) is False


# ---------------------------------------------------------------------------
# TestAnalyzeLoop
# ---------------------------------------------------------------------------

class TestAnalyzeLoop:
    def test_for_range_loop_kind(self):
        loop = _for_range("i", 0, 10, [])
        info = _analyze_loop(loop)
        assert info.loop_kind == "for"

    def test_for_range_iteration_count(self):
        loop = _for_range("i", 0, 10, [])
        info = _analyze_loop(loop)
        assert info.is_count_known is True
        assert info.iteration_count == 10

    def test_for_range_with_step(self):
        loop = _for_range("i", 0, 10, [], step=2)
        info = _analyze_loop(loop)
        assert info.is_count_known is True
        assert info.iteration_count == 5

    def test_for_each_unknown_count(self):
        loop = _for_each("item", "my_list", [])
        info = _analyze_loop(loop)
        assert info.is_count_known is False
        assert info.iteration_count is None

    def test_while_loop_kind(self):
        loop = _while_loop([])
        info = _analyze_loop(loop)
        assert info.loop_kind == "while"
        assert info.is_count_known is False

    def test_repeat_n_iteration_count(self):
        loop = _repeat(7, [])
        info = _analyze_loop(loop)
        assert info.loop_kind == "repeat"
        assert info.is_count_known is True
        assert info.iteration_count == 7

    def test_induction_var_in_variant_vars(self):
        body = [_vardecl("x", _ident("i"))]
        loop = _for_range("i", 0, 5, body)
        info = _analyze_loop(loop)
        assert "i" in info.variant_vars

    def test_assigned_vars_in_variant(self):
        body = [_vardecl("acc", _lit(0))]
        loop = _for_range("i", 0, 5, body)
        info = _analyze_loop(loop)
        assert "acc" in info.variant_vars

    def test_referenced_vars_collected(self):
        body = [_vardecl("result", _binop("+", _ident("alpha"), _ident("beta")))]
        loop = _for_range("i", 0, 5, body)
        info = _analyze_loop(loop)
        assert "alpha" in info.referenced_vars
        assert "beta" in info.referenced_vars


# ---------------------------------------------------------------------------
# TestLoopsHaveSameRange
# ---------------------------------------------------------------------------

class TestLoopsHaveSameRange:
    def test_two_identical_range_loops(self):
        a = _for_range("i", 0, 10, [])
        b = _for_range("j", 0, 10, [])
        assert _loops_have_same_range(a, b)

    def test_different_end(self):
        a = _for_range("i", 0, 10, [])
        b = _for_range("i", 0, 20, [])
        assert not _loops_have_same_range(a, b)

    def test_different_start(self):
        a = _for_range("i", 0, 10, [])
        b = _for_range("i", 1, 10, [])
        assert not _loops_have_same_range(a, b)

    def test_for_each_loops_not_fusable(self):
        a = _for_each("x", "list_a", [])
        b = _for_each("y", "list_b", [])
        assert not _loops_have_same_range(a, b)

    def test_while_loop_not_fusable(self):
        a = _for_range("i", 0, 5, [])
        b = _while_loop([])
        assert not _loops_have_same_range(a, b)

    def test_same_range_with_step(self):
        a = _for_range("i", 0, 10, [], step=2)
        b = _for_range("j", 0, 10, [], step=2)
        assert _loops_have_same_range(a, b)


# ---------------------------------------------------------------------------
# TestLoopsIndependent
# ---------------------------------------------------------------------------

class TestLoopsIndependent:
    def test_disjoint_writes_are_independent(self):
        a = _for_range("i", 0, 5, [_vardecl("x", _lit(1))])
        b = _for_range("i", 0, 5, [_vardecl("y", _lit(2))])
        assert _loops_independent(a, b)

    def test_a_writes_b_reads(self):
        a = _for_range("i", 0, 5, [_vardecl("shared", _lit(1))])
        b = _for_range("i", 0, 5, [_vardecl("result", _ident("shared"))])
        assert not _loops_independent(a, b)

    def test_both_write_same_var(self):
        a = _for_range("i", 0, 5, [_vardecl("counter", _lit(1))])
        b = _for_range("i", 0, 5, [_vardecl("counter", _lit(2))])
        assert not _loops_independent(a, b)

    def test_empty_bodies_are_independent(self):
        a = _for_range("i", 0, 5, [])
        b = _for_range("j", 0, 5, [])
        assert _loops_independent(a, b)

    def test_b_writes_a_reads(self):
        a = _for_range("i", 0, 5, [_vardecl("r", _ident("dep"))])
        b = _for_range("i", 0, 5, [_vardecl("dep", _lit(0))])
        assert not _loops_independent(a, b)

    def test_unrelated_vars_independent(self):
        a = _for_range("i", 0, 5, [_vardecl("a_var", _ident("extA"))])
        b = _for_range("i", 0, 5, [_vardecl("b_var", _ident("extB"))])
        assert _loops_independent(a, b)


# ---------------------------------------------------------------------------
# TestReduceStrengthInExpr
# ---------------------------------------------------------------------------

class TestReduceStrengthInExpr:
    def test_x_squared_to_multiply(self):
        expr = _binop("**", _ident("x"), _lit(2))
        result = _reduce_strength_in_expr(expr)
        assert result is expr
        assert result.operator == "*"

    def test_x_times_one_returns_x(self):
        ident = _ident("x")
        expr = _binop("*", ident, _lit(1))
        result = _reduce_strength_in_expr(expr)
        assert result is ident

    def test_one_times_x_returns_x(self):
        ident = _ident("x")
        expr = _binop("*", _lit(1), ident)
        result = _reduce_strength_in_expr(expr)
        assert result is ident

    def test_x_times_zero_returns_literal(self):
        expr = _binop("*", _ident("x"), _lit(0))
        result = _reduce_strength_in_expr(expr)
        assert isinstance(result, _SyntheticLiteral)
        assert result.value == 0

    def test_x_plus_zero_returns_x(self):
        ident = _ident("x")
        expr = _binop("+", ident, _lit(0))
        result = _reduce_strength_in_expr(expr)
        assert result is ident

    def test_zero_plus_x_returns_x(self):
        ident = _ident("x")
        expr = _binop("+", _lit(0), ident)
        result = _reduce_strength_in_expr(expr)
        assert result is ident

    def test_x_minus_zero_returns_x(self):
        ident = _ident("x")
        expr = _binop("-", ident, _lit(0))
        result = _reduce_strength_in_expr(expr)
        assert result is ident

    def test_non_binary_op_returns_none(self):
        assert _reduce_strength_in_expr(_lit(42)) is None
        assert _reduce_strength_in_expr(_ident("x")) is None

    def test_non_reducible_returns_none(self):
        expr = _binop("+", _ident("a"), _ident("b"))
        assert _reduce_strength_in_expr(expr) is None


# ---------------------------------------------------------------------------
# TestLoopAnalysisPass
# ---------------------------------------------------------------------------

class TestLoopAnalysisPass:
    def test_no_loops_empty_list(self):
        prog = _program([_vardecl("x", _lit(1))])
        p = LoopAnalysisPass()
        p.run(prog)
        assert p.loop_infos == []

    def test_single_for_loop(self):
        prog = _program([_for_range("i", 0, 5, [])])
        p = LoopAnalysisPass()
        p.run(prog)
        assert len(p.loop_infos) == 1
        assert p.loop_infos[0].loop_kind == "for"

    def test_multiple_loops_in_sequence(self):
        prog = _program([
            _for_range("i", 0, 5, []),
            _for_range("j", 0, 3, []),
        ])
        p = LoopAnalysisPass()
        p.run(prog)
        assert len(p.loop_infos) == 2

    def test_nested_loops_both_recorded(self):
        inner = _for_range("j", 0, 3, [])
        outer = _for_range("i", 0, 5, [inner])
        prog = _program([outer])
        p = LoopAnalysisPass()
        p.run(prog)
        assert len(p.loop_infos) == 2

    def test_run_returns_same_ast(self):
        prog = _program([])
        p = LoopAnalysisPass()
        result = p.run(prog)
        assert result is prog

    def test_loop_info_cleared_on_second_run(self):
        prog = _program([_for_range("i", 0, 3, [])])
        p = LoopAnalysisPass()
        p.run(prog)
        p.run(_program([]))  # second run on empty program
        assert p.loop_infos == []

    def test_while_loop_recorded(self):
        prog = _program([_while_loop([])])
        p = LoopAnalysisPass()
        p.run(prog)
        assert len(p.loop_infos) == 1
        assert p.loop_infos[0].loop_kind == "while"

    def test_repeat_n_iteration_count_detected(self):
        prog = _program([_repeat(4, [])])
        p = LoopAnalysisPass()
        p.run(prog)
        assert p.loop_infos[0].iteration_count == 4
        assert p.loop_infos[0].is_count_known is True


# ---------------------------------------------------------------------------
# TestLoopInvariantCodeMotionPass
# ---------------------------------------------------------------------------

class TestLoopInvariantCodeMotionPass:
    def test_no_loops_no_hoisting(self):
        prog = _program([_vardecl("x", _lit(1))])
        p = LoopInvariantCodeMotionPass()
        p.run(prog)
        assert p.hoisted_total == 0

    def test_invariant_stmt_hoisted_before_loop(self):
        inv_stmt = _vardecl("c", _binop("+", _ident("a"), _ident("b")))
        variant_stmt = _vardecl("sum", _binop("*", _ident("i"), _lit(2)))
        loop = _for_range("i", 0, 10, [inv_stmt, variant_stmt])
        prog = _program([loop])
        p = LoopInvariantCodeMotionPass()
        p.run(prog)
        # The hoisted stmt should appear before the loop in prog.statements
        assert len(prog.statements) == 2
        assert prog.statements[0].name == "c"
        assert p.hoisted_total == 1

    def test_loop_body_shrinks_after_hoisting(self):
        inv_stmt = _vardecl("k", _lit(42))
        loop = _for_range("i", 0, 5, [inv_stmt, _vardecl("x", _ident("i"))])
        prog = _program([loop])
        p = LoopInvariantCodeMotionPass()
        p.run(prog)
        remaining_loop = prog.statements[-1]
        # Only the variant stmt should remain in the loop body
        assert len(remaining_loop.body) == 1

    def test_variant_stmt_not_hoisted(self):
        stmt = _vardecl("x", _ident("i"))  # references i which is variant
        loop = _for_range("i", 0, 5, [stmt])
        prog = _program([loop])
        p = LoopInvariantCodeMotionPass()
        p.run(prog)
        assert p.hoisted_total == 0
        assert len(prog.statements) == 1  # only the loop

    def test_hoisted_stmt_is_deep_copy(self):
        inv_stmt = _vardecl("c", _lit(99))
        loop = _for_range("i", 0, 5, [inv_stmt])
        prog = _program([loop])
        p = LoopInvariantCodeMotionPass()
        p.run(prog)
        hoisted = prog.statements[0]
        assert hoisted is not inv_stmt  # deep copy

    def test_multiple_invariants_all_hoisted(self):
        stmts = [
            _vardecl("a", _lit(1)),
            _vardecl("b", _lit(2)),
            _vardecl("x", _ident("i")),
        ]
        loop = _for_range("i", 0, 10, stmts)
        prog = _program([loop])
        p = LoopInvariantCodeMotionPass()
        p.run(prog)
        assert p.hoisted_total == 2

    def test_run_returns_ast(self):
        prog = _program([])
        p = LoopInvariantCodeMotionPass()
        assert p.run(prog) is prog

    def test_empty_loop_body_unchanged(self):
        loop = _for_range("i", 0, 10, [])
        prog = _program([loop])
        p = LoopInvariantCodeMotionPass()
        p.run(prog)
        assert p.hoisted_total == 0

    def test_hoisting_updates_stats(self):
        inv = _vardecl("k", _lit(5))
        loop = _for_range("i", 0, 5, [inv])
        prog = _program([loop])
        p = LoopInvariantCodeMotionPass()
        p.run(prog)
        assert p.stats.constants_folded == 1

    def test_no_body_attr_handled(self):
        ns = types.SimpleNamespace()
        ns.statements = []
        p = LoopInvariantCodeMotionPass()
        p.run(ns)  # Should not raise
        assert p.hoisted_total == 0

    def test_nested_loop_invariant_hoisted_to_inner_prelude(self):
        inner_inv = _vardecl("inner_c", _lit(7))
        inner_variant = _vardecl("y", _ident("j"))
        inner_loop = _for_range("j", 0, 3, [inner_inv, inner_variant])
        outer_loop = _for_range("i", 0, 5, [inner_loop])
        prog = _program([outer_loop])
        p = LoopInvariantCodeMotionPass()
        p.run(prog)
        # inner_c hoisted to before inner_loop (inside outer_loop body)
        outer_body = prog.statements[0].body
        # The hoisted stmt should prepend the inner loop in the outer body
        assert any(
            getattr(s, "name", None) == "inner_c" for s in outer_body
        )


# ---------------------------------------------------------------------------
# TestLoopFusionPass
# ---------------------------------------------------------------------------

class TestLoopFusionPass:
    def test_no_loops_unchanged(self):
        prog = _program([_vardecl("x", _lit(1))])
        p = LoopFusionPass()
        p.run(prog)
        assert p.fused_total == 0

    def test_adjacent_independent_same_range_fused(self):
        loop_a = _for_range("i", 0, 5, [_vardecl("a", _lit(1))])
        loop_b = _for_range("j", 0, 5, [_vardecl("b", _lit(2))])
        prog = _program([loop_a, loop_b])
        p = LoopFusionPass()
        p.run(prog)
        assert p.fused_total == 1
        assert len(prog.statements) == 1

    def test_fused_body_contains_both(self):
        s1 = _vardecl("x", _lit(1))
        s2 = _vardecl("y", _lit(2))
        loop_a = _for_range("i", 0, 5, [s1])
        loop_b = _for_range("j", 0, 5, [s2])
        prog = _program([loop_a, loop_b])
        p = LoopFusionPass()
        p.run(prog)
        merged_body = prog.statements[0].body
        assert len(merged_body) == 2

    def test_different_ranges_not_fused(self):
        loop_a = _for_range("i", 0, 5, [_vardecl("a", _lit(1))])
        loop_b = _for_range("j", 0, 10, [_vardecl("b", _lit(2))])
        prog = _program([loop_a, loop_b])
        p = LoopFusionPass()
        p.run(prog)
        assert p.fused_total == 0

    def test_dependent_loops_not_fused(self):
        loop_a = _for_range("i", 0, 5, [_vardecl("shared", _lit(1))])
        loop_b = _for_range("j", 0, 5, [_vardecl("r", _ident("shared"))])
        prog = _program([loop_a, loop_b])
        p = LoopFusionPass()
        p.run(prog)
        assert p.fused_total == 0

    def test_three_fusable_loops(self):
        loops = [_for_range("i", 0, 5, [_vardecl(f"v{i}", _lit(i))]) for i in range(3)]
        prog = _program(loops)
        p = LoopFusionPass()
        p.run(prog)
        assert len(prog.statements) == 1
        assert len(prog.statements[0].body) == 3

    def test_run_returns_ast(self):
        prog = _program([])
        p = LoopFusionPass()
        assert p.run(prog) is prog

    def test_while_loops_not_fused(self):
        a = _while_loop([_vardecl("x", _lit(1))])
        b = _while_loop([_vardecl("y", _lit(2))])
        prog = _program([a, b])
        p = LoopFusionPass()
        p.run(prog)
        assert p.fused_total == 0

    def test_stats_updated(self):
        loop_a = _for_range("i", 0, 5, [_vardecl("a", _lit(1))])
        loop_b = _for_range("j", 0, 5, [_vardecl("b", _lit(2))])
        prog = _program([loop_a, loop_b])
        p = LoopFusionPass()
        p.run(prog)
        assert p.stats.dead_functions_removed == 1


# ---------------------------------------------------------------------------
# TestInductionVariableSimplificationPass
# ---------------------------------------------------------------------------

class TestInductionVariableSimplificationPass:
    def test_no_loops_no_change(self):
        prog = _program([_vardecl("x", _lit(1))])
        p = InductionVariableSimplificationPass()
        p.run(prog)
        assert p.simplified_total == 0

    def test_i_times_k_detected(self):
        mul = _binop("*", _ident("i"), _lit(4))
        stmt = _vardecl("offset", mul)
        loop = _for_range("i", 0, 10, [stmt])
        prog = _program([loop])
        p = InductionVariableSimplificationPass()
        p.run(prog)
        assert p.simplified_total == 1

    def test_k_times_i_detected(self):
        mul = _binop("*", _lit(8), _ident("i"))
        stmt = _vardecl("stride", mul)
        loop = _for_range("i", 0, 10, [stmt])
        prog = _program([loop])
        p = InductionVariableSimplificationPass()
        p.run(prog)
        assert p.simplified_total == 1

    def test_annotation_added_to_loop(self):
        mul = _binop("*", _ident("i"), _lit(4))
        stmt = _vardecl("offset", mul)
        loop = _for_range("i", 0, 10, [stmt])
        prog = _program([loop])
        p = InductionVariableSimplificationPass()
        p.run(prog)
        assert hasattr(loop, "_iv_simplifications")
        lhs, aux, step = loop._iv_simplifications[0]
        assert lhs == "offset"
        assert step == 4

    def test_non_multiply_not_simplified(self):
        stmt = _vardecl("x", _binop("+", _ident("i"), _lit(1)))
        loop = _for_range("i", 0, 10, [stmt])
        prog = _program([loop])
        p = InductionVariableSimplificationPass()
        p.run(prog)
        assert p.simplified_total == 0

    def test_run_returns_ast(self):
        prog = _program([])
        p = InductionVariableSimplificationPass()
        assert p.run(prog) is prog

    def test_for_each_no_induction_var(self):
        mul = _binop("*", _ident("item"), _lit(2))
        stmt = _vardecl("doubled", mul)
        loop = _for_each("item", "my_list", [stmt])
        prog = _program([loop])
        p = InductionVariableSimplificationPass()
        p.run(prog)
        assert p.simplified_total == 0


# ---------------------------------------------------------------------------
# TestLoopStrengthReductionPass
# ---------------------------------------------------------------------------

class TestLoopStrengthReductionPass:
    def test_no_loops_no_change(self):
        prog = _program([_vardecl("x", _lit(1))])
        p = LoopStrengthReductionPass()
        p.run(prog)
        assert p.reduced_total == 0

    def test_x_squared_reduced_in_loop(self):
        expr = _binop("**", _ident("x"), _lit(2))
        stmt = _vardecl("sq", expr)
        loop = _for_range("i", 0, 5, [stmt])
        prog = _program([loop])
        p = LoopStrengthReductionPass()
        p.run(prog)
        assert p.reduced_total == 1
        assert stmt.value.operator == "*"

    def test_x_times_one_reduced(self):
        ident = _ident("x")
        stmt = _vardecl("result", _binop("*", ident, _lit(1)))
        loop = _for_range("i", 0, 5, [stmt])
        prog = _program([loop])
        p = LoopStrengthReductionPass()
        p.run(prog)
        assert p.reduced_total == 1
        assert stmt.value is ident

    def test_x_plus_zero_reduced(self):
        ident = _ident("val")
        stmt = _vardecl("out", _binop("+", ident, _lit(0)))
        loop = _for_range("i", 0, 5, [stmt])
        prog = _program([loop])
        p = LoopStrengthReductionPass()
        p.run(prog)
        assert stmt.value is ident

    def test_non_reducible_unchanged(self):
        stmt = _vardecl("s", _binop("+", _ident("a"), _ident("b")))
        loop = _for_range("i", 0, 5, [stmt])
        prog = _program([loop])
        p = LoopStrengthReductionPass()
        p.run(prog)
        assert p.reduced_total == 0

    def test_run_returns_ast(self):
        prog = _program([])
        p = LoopStrengthReductionPass()
        assert p.run(prog) is prog

    def test_stats_updated(self):
        stmt = _vardecl("v", _binop("**", _ident("x"), _lit(2)))
        loop = _for_range("i", 0, 5, [stmt])
        prog = _program([loop])
        p = LoopStrengthReductionPass()
        p.run(prog)
        assert p.stats.dead_variables_removed == 1


# ---------------------------------------------------------------------------
# TestLoopOptimizationPipeline
# ---------------------------------------------------------------------------

class TestLoopOptimizationPipeline:
    def test_default_pipeline_has_five_passes(self):
        pipeline = LoopOptimizationPipeline.default()
        assert len(pipeline.passes) == 5

    def test_run_returns_ast(self):
        prog = _program([])
        pipeline = LoopOptimizationPipeline.default()
        result = pipeline.run(prog)
        assert result is prog

    def test_total_passes_run_incremented(self):
        prog = _program([])
        pipeline = LoopOptimizationPipeline.default()
        pipeline.run(prog)
        assert pipeline.aggregated_stats.total_passes_run == 5

    def test_add_pass_chaining(self):
        pipeline = LoopOptimizationPipeline()
        p1 = LoopAnalysisPass()
        p2 = LoopInvariantCodeMotionPass()
        result = pipeline.add_pass(p1).add_pass(p2)
        assert result is pipeline
        assert len(pipeline.passes) == 2

    def test_verbose_flag_accepted(self):
        pipeline = LoopOptimizationPipeline(verbose=True)
        assert pipeline.verbose is True

    def test_empty_pipeline_returns_ast(self):
        prog = _program([])
        pipeline = LoopOptimizationPipeline()
        assert pipeline.run(prog) is prog

    def test_stats_reset_on_each_run(self):
        prog = _program([])
        pipeline = LoopOptimizationPipeline.default()
        pipeline.run(prog)
        first = pipeline.aggregated_stats.total_passes_run
        pipeline.run(prog)
        assert pipeline.aggregated_stats.total_passes_run == first


# ---------------------------------------------------------------------------
# TestLoopOptimize
# ---------------------------------------------------------------------------

class TestLoopOptimize:
    def test_returns_tuple(self):
        prog = _program([])
        result = loop_optimize(prog)
        assert isinstance(result, tuple)
        assert len(result) == 2

    def test_returns_ast_and_stats(self):
        prog = _program([])
        ast, stats = loop_optimize(prog)
        assert ast is prog
        assert isinstance(stats, LoopOptimizationStats)

    def test_hoisting_via_loop_optimize(self):
        inv = _vardecl("c", _lit(5))
        variant = _vardecl("x", _ident("i"))
        loop = _for_range("i", 0, 10, [inv, variant])
        prog = _program([loop])
        _, stats = loop_optimize(prog)
        assert stats.total_passes_run == 5

    def test_fusion_via_loop_optimize(self):
        # Use stmts that depend on the induction variable so LICM will not hoist
        # them.  After fusion the two loops should be merged into a single loop,
        # giving exactly 1 statement in the program.
        a = _for_range("i", 0, 5, [_vardecl("a_v", _ident("i"))])
        b = _for_range("j", 0, 5, [_vardecl("b_v", _ident("j"))])
        prog = _program([a, b])
        ast_out, _ = loop_optimize(prog)
        assert len(ast_out.statements) == 1


# ---------------------------------------------------------------------------
# TestLazyImportViaPackage
# ---------------------------------------------------------------------------

class TestLazyImportViaPackage:
    def test_loop_optimize_importable_via_optimizer(self):
        from nexuslang.optimizer import loop_optimize as lo
        assert callable(lo)

    def test_loop_analysis_pass_importable_via_optimizer(self):
        from nexuslang.optimizer import LoopAnalysisPass as LAP
        assert LAP is LoopAnalysisPass

    def test_loop_pipeline_importable_via_optimizer(self):
        from nexuslang.optimizer import LoopOptimizationPipeline as LOP
        assert LOP is LoopOptimizationPipeline

    def test_loop_fusion_pass_importable(self):
        from nexuslang.optimizer import LoopFusionPass as LFP
        assert LFP is LoopFusionPass

    def test_licm_pass_importable(self):
        from nexuslang.optimizer import LoopInvariantCodeMotionPass as LICM
        assert LICM is LoopInvariantCodeMotionPass
