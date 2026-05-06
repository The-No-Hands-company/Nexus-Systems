"""
pytest tests for src/nlpl/optimizer/lto.py

Run with:
    pytest tests/unit/compiler/test_lto.py
"""

import sys
import types
from pathlib import Path

import pytest

_SRC = str(Path(__file__).resolve().parent.parent.parent.parent / "src")
if _SRC not in sys.path:
    sys.path.insert(0, _SRC)

from nexuslang.optimizer.lto import (
    LTOStats,
    LTOUnit,
    LTOContext,
    LTOPipeline,
    SymbolReferenceAnalysisPass,
    CrossModuleDCEPass,
    CrossModuleInliningPass,
    ConstantPropagationPass,
    DeadImportEliminationPass,
    RedundantExportPass,
    lto_optimize,
    lto_stats_report,
    _collect_referenced_symbols,
    _get_function_body_size,
    _get_module_constants,
)


# ---------------------------------------------------------------------------
# Minimal AST helpers
# ---------------------------------------------------------------------------

def _make_ast(stmts=None, exports=None, imports=None, constants=None):
    """Build a simple namespace-based AST node."""
    node = types.SimpleNamespace()
    node.statements = stmts or []
    if exports is not None:
        node.__exports__ = set(exports)
    if imports is not None:
        node.__imports__ = dict(imports)
    if constants is not None:
        node.__constants__ = dict(constants)
    return node


def _make_fn_call(name):
    n = types.SimpleNamespace()
    n.__class__ = type("FunctionCall", (), {"__name__": "FunctionCall"})
    n.name = name
    return n


def _make_identifier(name):
    n = types.SimpleNamespace()
    n.__class__ = type("Identifier", (), {"__name__": "Identifier"})
    n.name = name
    return n


def _make_unit(name="mod", exports=None, imports=None, constants=None, ast_stmts=None):
    return LTOUnit(
        name=name,
        ast=_make_ast(stmts=ast_stmts, exports=exports, imports=imports, constants=constants),
        exports=exports,
        imports=imports,
        constants=constants,
    )


# ---------------------------------------------------------------------------
# LTOStats
# ---------------------------------------------------------------------------

class TestLTOStats:
    def test_default_zeros(self):
        s = LTOStats()
        assert s.dead_exports_removed == 0
        assert s.functions_inlined_cross_module == 0
        assert s.constants_propagated == 0
        assert s.dead_imports_removed == 0
        assert s.redundant_exports_removed == 0
        assert s.total_passes_run == 0

    def test_add_two_stats(self):
        a = LTOStats(dead_exports_removed=3, constants_propagated=2)
        b = LTOStats(dead_exports_removed=1, constants_propagated=5)
        c = a + b
        assert c.dead_exports_removed == 4
        assert c.constants_propagated == 7

    def test_add_all_fields(self):
        a = LTOStats(
            dead_exports_removed=1,
            functions_inlined_cross_module=2,
            constants_propagated=3,
            dead_imports_removed=4,
            redundant_exports_removed=5,
            total_passes_run=6,
        )
        b = LTOStats(
            dead_exports_removed=10,
            functions_inlined_cross_module=20,
            constants_propagated=30,
            dead_imports_removed=40,
            redundant_exports_removed=50,
            total_passes_run=60,
        )
        c = a + b
        assert c.dead_exports_removed == 11
        assert c.functions_inlined_cross_module == 22
        assert c.constants_propagated == 33
        assert c.dead_imports_removed == 44
        assert c.redundant_exports_removed == 55
        assert c.total_passes_run == 66

    def test_str_contains_header(self):
        assert "LTO Statistics" in str(LTOStats())

    def test_str_contains_all_field_names(self):
        s = str(LTOStats())
        for field in ("Dead exports", "Cross-module", "Constants", "Dead imports", "Redundant", "passes"):
            assert field in s


# ---------------------------------------------------------------------------
# LTOUnit
# ---------------------------------------------------------------------------

class TestLTOUnit:
    def test_basic_construction(self):
        u = LTOUnit("mymod")
        assert u.name == "mymod"

    def test_default_empty_exports(self):
        u = LTOUnit("x")
        assert isinstance(u.exports, set)
        assert len(u.exports) == 0

    def test_default_empty_imports(self):
        u = LTOUnit("x")
        assert isinstance(u.imports, dict)
        assert len(u.imports) == 0

    def test_exports_passed_explicitly(self):
        u = LTOUnit("x", exports={"foo", "bar"})
        assert "foo" in u.exports
        assert "bar" in u.exports

    def test_imports_passed_explicitly(self):
        u = LTOUnit("x", imports={"add": "math"})
        assert u.imports["add"] == "math"

    def test_constants_passed_explicitly(self):
        u = LTOUnit("x", constants={"PI": 3.14})
        assert u.constants["PI"] == 3.14

    def test_exports_from_ast(self):
        ast = _make_ast(exports={"alpha", "beta"})
        u = LTOUnit("x", ast=ast)
        assert "alpha" in u.exports
        assert "beta" in u.exports

    def test_imports_from_ast(self):
        ast = _make_ast(imports={"sqrt": "math"})
        u = LTOUnit("x", ast=ast)
        assert u.imports["sqrt"] == "math"

    def test_constants_from_ast(self):
        ast = _make_ast(constants={"VERSION": 1})
        u = LTOUnit("x", ast=ast)
        assert u.constants["VERSION"] == 1

    def test_repr_contains_name(self):
        u = LTOUnit("mod_a")
        assert "mod_a" in repr(u)

    def test_initial_stats_zero(self):
        u = LTOUnit("x")
        assert u.stats.dead_exports_removed == 0

    def test_referenced_symbols_empty_initially(self):
        u = LTOUnit("x")
        assert isinstance(u.referenced_symbols, set)

    def test_inlined_from_empty_initially(self):
        u = LTOUnit("x")
        assert isinstance(u.inlined_from, dict)

    def test_none_ast_ok(self):
        u = LTOUnit("x", ast=None)
        assert u.ast is None


# ---------------------------------------------------------------------------
# LTOContext
# ---------------------------------------------------------------------------

class TestLTOContext:
    def test_empty_context(self):
        ctx = LTOContext()
        assert ctx.units == []

    def test_add_unit(self):
        ctx = LTOContext()
        ctx.add_unit(LTOUnit("a"))
        assert len(ctx.units) == 1

    def test_units_passed_at_construction(self):
        ctx = LTOContext(units=[LTOUnit("a"), LTOUnit("b")])
        assert len(ctx.units) == 2

    def test_entry_points(self):
        ctx = LTOContext(entry_points={"main"})
        assert "main" in ctx.entry_points

    def test_build_index(self):
        u = LTOUnit("a", exports={"foo"})
        ctx = LTOContext(units=[u])
        ctx.build_index()
        assert ctx.defining_unit("foo") is u

    def test_defining_unit_none_for_unknown(self):
        ctx = LTOContext()
        ctx.build_index()
        assert ctx.defining_unit("unknown") is None

    def test_all_imported_symbols(self):
        u1 = LTOUnit("a", imports={"x": "mod1"})
        u2 = LTOUnit("b", imports={"y": "mod2"})
        ctx = LTOContext(units=[u1, u2])
        syms = ctx.all_imported_symbols()
        assert "x" in syms
        assert "y" in syms

    def test_all_exported_symbols(self):
        u1 = LTOUnit("a", exports={"foo"})
        u2 = LTOUnit("b", exports={"bar"})
        ctx = LTOContext(units=[u1, u2])
        syms = ctx.all_exported_symbols()
        assert "foo" in syms
        assert "bar" in syms

    def test_unit_by_name_found(self):
        u = LTOUnit("mymod")
        ctx = LTOContext(units=[u])
        assert ctx.unit_by_name("mymod") is u

    def test_unit_by_name_not_found(self):
        ctx = LTOContext()
        assert ctx.unit_by_name("nobody") is None

    def test_referenced_symbols_globally(self):
        u = LTOUnit("a")
        u.referenced_symbols = {"add", "sub"}
        ctx = LTOContext(units=[u])
        syms = ctx.referenced_symbols_globally()
        assert "add" in syms
        assert "sub" in syms

    def test_repr(self):
        ctx = LTOContext(units=[LTOUnit("a")])
        r = repr(ctx)
        assert "LTOContext" in r
        assert "units=1" in r


# ---------------------------------------------------------------------------
# Helper: _collect_referenced_symbols
# ---------------------------------------------------------------------------

class TestCollectReferencedSymbols:
    def test_empty_none(self):
        refs = set()
        _collect_referenced_symbols(None, refs)
        assert refs == set()

    def test_dict_ast_name(self):
        ast = {"type": "FunctionCall", "name": "add"}
        refs = set()
        _collect_referenced_symbols(ast, refs)
        assert "add" in refs

    def test_dict_nested(self):
        ast = {"stmts": [{"name": "foo"}, {"callee": "bar"}]}
        refs = set()
        _collect_referenced_symbols(ast, refs)
        assert "foo" in refs
        assert "bar" in refs

    def test_list_ast(self):
        ast = [{"name": "x"}, {"callee": "y"}]
        refs = set()
        _collect_referenced_symbols(ast, refs)
        assert "x" in refs
        assert "y" in refs

    def test_object_with_name_attr(self):
        # Use a real class so __class__ restriction is avoided
        class FunctionCall:
            def __init__(self, name):
                self.name = name
        node = FunctionCall("compute")
        refs = set()
        _collect_referenced_symbols(node, refs)
        assert "compute" in refs

    def test_non_string_values_ignored(self):
        ast = {"count": 5, "flag": True}
        refs = set()
        _collect_referenced_symbols(ast, refs)
        assert refs == set()


# ---------------------------------------------------------------------------
# Helper: _get_function_body_size
# ---------------------------------------------------------------------------

class TestGetFunctionBodySize:
    def test_none_ast(self):
        assert _get_function_body_size(None, "foo") == 0

    def test_not_found(self):
        ast = _make_ast()
        assert _get_function_body_size(ast, "missing") == 0

    def test_dict_ast_found(self):
        ast = {
            "statements": [
                {
                    "type": "FunctionDefinition",
                    "name": "add",
                    "body": [{"type": "Return"}, {"type": "Add"}],
                }
            ]
        }
        assert _get_function_body_size(ast, "add") == 2

    def test_dict_ast_function_body_absent(self):
        ast = {
            "statements": [
                {"type": "function", "name": "noop", "body": []}
            ]
        }
        assert _get_function_body_size(ast, "noop") == 0

    def test_empty_statements(self):
        ast = _make_ast(stmts=[])
        assert _get_function_body_size(ast, "foo") == 0


# ---------------------------------------------------------------------------
# Helper: _get_module_constants
# ---------------------------------------------------------------------------

class TestGetModuleConstants:
    def test_none_ast(self):
        assert _get_module_constants(None) == {}

    def test_from_ast_attribute(self):
        ast = _make_ast(constants={"PI": 3})
        result = _get_module_constants(ast)
        assert result["PI"] == 3

    def test_from_dict_ast(self):
        ast = {
            "statements": [
                {"type": "ConstantDeclaration", "name": "MAX", "value": 100}
            ]
        }
        result = _get_module_constants(ast)
        assert result.get("MAX") == 100

    def test_empty(self):
        ast = _make_ast()
        result = _get_module_constants(ast)
        assert isinstance(result, dict)


# ---------------------------------------------------------------------------
# SymbolReferenceAnalysisPass
# ---------------------------------------------------------------------------

class TestSymbolReferenceAnalysisPass:
    def test_import(self):
        p = SymbolReferenceAnalysisPass()
        assert p is not None

    def test_name(self):
        p = SymbolReferenceAnalysisPass()
        assert "reference" in p.name or "analysis" in p.name

    def test_run_single_ast_passthrough(self):
        p = SymbolReferenceAnalysisPass()
        ast = _make_ast()
        assert p.run(ast) is ast

    def test_populates_referenced_symbols(self):
        ast = {"name": "calc", "body": [{"callee": "sqrt"}]}
        unit = LTOUnit("x", ast=ast)
        ctx = LTOContext(units=[unit])
        p = SymbolReferenceAnalysisPass()
        p.run_on_context(ctx)
        assert "calc" in unit.referenced_symbols or "sqrt" in unit.referenced_symbols

    def test_empty_ast(self):
        unit = LTOUnit("x", ast=None)
        ctx = LTOContext(units=[unit])
        p = SymbolReferenceAnalysisPass()
        p.run_on_context(ctx)
        assert isinstance(unit.referenced_symbols, set)

    def test_multiple_units(self):
        u1 = LTOUnit("a", ast={"name": "foo"})
        u2 = LTOUnit("b", ast={"name": "bar"})
        ctx = LTOContext(units=[u1, u2])
        SymbolReferenceAnalysisPass().run_on_context(ctx)
        assert "foo" in u1.referenced_symbols
        assert "bar" in u2.referenced_symbols


# ---------------------------------------------------------------------------
# CrossModuleDCEPass
# ---------------------------------------------------------------------------

class TestCrossModuleDCEPass:
    def test_import(self):
        p = CrossModuleDCEPass()
        assert p is not None

    def test_removes_unused_export(self):
        # mod_a exports "helper"; nobody imports it
        a = LTOUnit("mod_a", exports={"helper"})
        ctx = LTOContext(units=[a])
        p = CrossModuleDCEPass()
        ctx = p.run_on_context(ctx)
        assert "helper" not in a.exports

    def test_keeps_imported_export(self):
        a = LTOUnit("mod_a", exports={"add"})
        b = LTOUnit("mod_b", imports={"add": "mod_a"})
        ctx = LTOContext(units=[a, b])
        CrossModuleDCEPass().run_on_context(ctx)
        assert "add" in a.exports

    def test_entry_point_exports_kept(self):
        main = LTOUnit("main", exports={"run"})
        ctx = LTOContext(units=[main], entry_points={"main"})
        CrossModuleDCEPass(keep_entry_exports=True).run_on_context(ctx)
        assert "run" in main.exports

    def test_entry_point_stripped_if_flag_off(self):
        main = LTOUnit("main", exports={"run"})
        ctx = LTOContext(units=[main], entry_points={"main"})
        CrossModuleDCEPass(keep_entry_exports=False).run_on_context(ctx)
        assert "run" not in main.exports

    def test_updates_unit_stats(self):
        a = LTOUnit("a", exports={"dead1", "dead2"})
        ctx = LTOContext(units=[a])
        CrossModuleDCEPass().run_on_context(ctx)
        assert a.stats.dead_exports_removed == 2

    def test_updates_ctx_stats(self):
        a = LTOUnit("a", exports={"dead"})
        ctx = LTOContext(units=[a])
        CrossModuleDCEPass().run_on_context(ctx)
        assert ctx.stats.dead_exports_removed >= 1

    def test_single_unit_passthrough(self):
        p = CrossModuleDCEPass()
        ast = _make_ast()
        assert p.run(ast) is ast

    def test_multiple_dead_exports(self):
        a = LTOUnit("a", exports={"x", "y", "z"})
        ctx = LTOContext(units=[a])
        CrossModuleDCEPass().run_on_context(ctx)
        assert a.exports == set()


# ---------------------------------------------------------------------------
# CrossModuleInliningPass
# ---------------------------------------------------------------------------

class TestCrossModuleInliningPass:
    def test_import(self):
        p = CrossModuleInliningPass()
        assert p is not None

    def test_default_threshold(self):
        p = CrossModuleInliningPass()
        assert p.max_inline_size == CrossModuleInliningPass.DEFAULT_MAX_INLINE_SIZE

    def test_custom_threshold(self):
        p = CrossModuleInliningPass(max_inline_size=5)
        assert p.max_inline_size == 5

    def test_small_function_inlined(self):
        # mod_a exports "tiny" with body size 1
        tiny_ast = {
            "statements": [
                {"type": "function", "name": "tiny", "body": [{"type": "Return"}]}
            ]
        }
        a = LTOUnit("mod_a", ast=tiny_ast, exports={"tiny"})
        b = LTOUnit("mod_b", imports={"tiny": "mod_a"})
        ctx = LTOContext(units=[a, b])
        CrossModuleInliningPass(max_inline_size=5).run_on_context(ctx)
        assert "tiny" in b.inlined_from

    def test_large_function_not_inlined(self):
        big_ast = {
            "statements": [
                {
                    "type": "function",
                    "name": "big",
                    "body": [{"type": "stmt"}] * 50,
                }
            ]
        }
        a = LTOUnit("mod_a", ast=big_ast, exports={"big"})
        b = LTOUnit("mod_b", imports={"big": "mod_a"})
        ctx = LTOContext(units=[a, b])
        CrossModuleInliningPass(max_inline_size=10).run_on_context(ctx)
        assert "big" not in b.inlined_from

    def test_updates_inline_stats(self):
        tiny_ast = {
            "statements": [
                {"type": "function", "name": "f", "body": [{"type": "Return"}]}
            ]
        }
        a = LTOUnit("a", ast=tiny_ast, exports={"f"})
        b = LTOUnit("b", imports={"f": "a"})
        ctx = LTOContext(units=[a, b])
        CrossModuleInliningPass(max_inline_size=5).run_on_context(ctx)
        assert b.stats.functions_inlined_cross_module >= 1

    def test_no_imports_no_inlining(self):
        a = LTOUnit("a", exports={"f"})
        b = LTOUnit("b")
        ctx = LTOContext(units=[a, b])
        CrossModuleInliningPass().run_on_context(ctx)
        assert len(b.inlined_from) == 0

    def test_single_unit_passthrough(self):
        p = CrossModuleInliningPass()
        ast = _make_ast()
        assert p.run(ast) is ast


# ---------------------------------------------------------------------------
# ConstantPropagationPass
# ---------------------------------------------------------------------------

class TestConstantPropagationPass:
    def test_import(self):
        p = ConstantPropagationPass()
        assert p is not None

    def test_propagates_into_importer(self):
        a = LTOUnit("a", exports={"PI"}, constants={"PI": 3.14159})
        b = LTOUnit("b", imports={"PI": "a"})
        ctx = LTOContext(units=[a, b])
        ConstantPropagationPass().run_on_context(ctx)
        assert "PI" in b.constants
        assert b.constants["PI"] == 3.14159

    def test_does_not_duplicate_existing(self):
        a = LTOUnit("a", exports={"K"}, constants={"K": 42})
        b = LTOUnit("b", imports={"K": "a"}, constants={"K": 99})
        ctx = LTOContext(units=[a, b])
        ConstantPropagationPass().run_on_context(ctx)
        # b already has K; should not be overwritten
        assert b.constants["K"] == 99

    def test_updates_stats(self):
        a = LTOUnit("a", exports={"X"}, constants={"X": 1})
        b = LTOUnit("b", imports={"X": "a"})
        ctx = LTOContext(units=[a, b])
        ConstantPropagationPass().run_on_context(ctx)
        assert b.stats.constants_propagated >= 1

    def test_non_exported_constant_not_propagated(self):
        a = LTOUnit("a", constants={"INTERNAL": 0})  # not exported
        b = LTOUnit("b", imports={"INTERNAL": "a"})
        ctx = LTOContext(units=[a, b])
        ConstantPropagationPass().run_on_context(ctx)
        # INTERNAL is not in a.exports so not added to global_constants
        assert "INTERNAL" not in b.constants

    def test_single_unit_passthrough(self):
        p = ConstantPropagationPass()
        ast = _make_ast()
        assert p.run(ast) is ast

    def test_reads_constants_from_ast(self):
        ast = _make_ast(constants={"E": 2.718}, exports={"E"})
        a = LTOUnit("a", ast=ast)
        b = LTOUnit("b", imports={"E": "a"})
        ctx = LTOContext(units=[a, b])
        ConstantPropagationPass().run_on_context(ctx)
        assert "E" in b.constants


# ---------------------------------------------------------------------------
# DeadImportEliminationPass
# ---------------------------------------------------------------------------

class TestDeadImportEliminationPass:
    def test_import(self):
        p = DeadImportEliminationPass()
        assert p is not None

    def test_removes_unused_import(self):
        # b imports "unused" but never references it
        b = LTOUnit("b", imports={"unused": "a"})
        b.referenced_symbols = set()
        ctx = LTOContext(units=[b])
        DeadImportEliminationPass().run_on_context(ctx)
        assert "unused" not in b.imports

    def test_keeps_used_import(self):
        b = LTOUnit("b", imports={"sqrt": "math"})
        b.referenced_symbols = {"sqrt"}
        ctx = LTOContext(units=[b])
        DeadImportEliminationPass().run_on_context(ctx)
        assert "sqrt" in b.imports

    def test_updates_stats(self):
        b = LTOUnit("b", imports={"dead": "x"})
        b.referenced_symbols = set()
        ctx = LTOContext(units=[b])
        DeadImportEliminationPass().run_on_context(ctx)
        assert b.stats.dead_imports_removed >= 1

    def test_empty_imports_no_error(self):
        b = LTOUnit("b")
        ctx = LTOContext(units=[b])
        DeadImportEliminationPass().run_on_context(ctx)
        assert b.imports == {}

    def test_multiple_units(self):
        a = LTOUnit("a", imports={"foo": "x"})
        a.referenced_symbols = set()
        b = LTOUnit("b", imports={"bar": "y"})
        b.referenced_symbols = {"bar"}
        ctx = LTOContext(units=[a, b])
        DeadImportEliminationPass().run_on_context(ctx)
        assert "foo" not in a.imports
        assert "bar" in b.imports

    def test_single_unit_passthrough(self):
        p = DeadImportEliminationPass()
        ast = _make_ast()
        assert p.run(ast) is ast


# ---------------------------------------------------------------------------
# RedundantExportPass
# ---------------------------------------------------------------------------

class TestRedundantExportPass:
    def test_import(self):
        p = RedundantExportPass()
        assert p is not None

    def test_strips_unimported_export(self):
        a = LTOUnit("a", exports={"orphan"})
        ctx = LTOContext(units=[a])
        RedundantExportPass().run_on_context(ctx)
        assert "orphan" not in a.exports

    def test_keeps_imported_export(self):
        a = LTOUnit("a", exports={"used"})
        b = LTOUnit("b", imports={"used": "a"})
        ctx = LTOContext(units=[a, b])
        RedundantExportPass().run_on_context(ctx)
        assert "used" in a.exports

    def test_entry_point_not_stripped(self):
        main = LTOUnit("main", exports={"run"})
        ctx = LTOContext(units=[main], entry_points={"main"})
        RedundantExportPass().run_on_context(ctx)
        assert "run" in main.exports

    def test_updates_stats(self):
        a = LTOUnit("a", exports={"r1", "r2"})
        ctx = LTOContext(units=[a])
        RedundantExportPass().run_on_context(ctx)
        assert a.stats.redundant_exports_removed == 2

    def test_single_unit_passthrough(self):
        p = RedundantExportPass()
        ast = _make_ast()
        assert p.run(ast) is ast


# ---------------------------------------------------------------------------
# LTOPipeline
# ---------------------------------------------------------------------------

class TestLTOPipeline:
    def test_empty_pipeline(self):
        p = LTOPipeline()
        assert p.passes == []

    def test_add_pass(self):
        p = LTOPipeline()
        p.add_pass(SymbolReferenceAnalysisPass())
        assert len(p.passes) == 1

    def test_run_empty(self):
        ctx = LTOContext(units=[LTOUnit("a")])
        ctx2 = LTOPipeline().run(ctx)
        assert ctx2 is ctx

    def test_default_pipeline_standard(self):
        p = LTOPipeline.default(aggressive=False)
        names = [pass_.name for pass_ in p.passes]
        assert any("reference" in n or "analysis" in n for n in names)

    def test_default_pipeline_has_dce(self):
        p = LTOPipeline.default()
        names = [pass_.name for pass_ in p.passes]
        assert any("dce" in n or "dead" in n for n in names)

    def test_default_pipeline_has_const_prop(self):
        p = LTOPipeline.default()
        names = [pass_.name for pass_ in p.passes]
        assert any("constant" in n or "propagation" in n for n in names)

    def test_default_pipeline_aggressive_has_inlining(self):
        p = LTOPipeline.default(aggressive=True)
        names = [pass_.name for pass_ in p.passes]
        assert any("inline" in n or "inlining" in n for n in names)

    def test_default_pipeline_standard_has_inlining(self):
        # Standard also includes inlining (just smaller threshold)
        p = LTOPipeline.default(aggressive=False)
        names = [pass_.name for pass_ in p.passes]
        assert any("inline" in n or "inlining" in n for n in names)

    def test_pipeline_increments_passes_run(self):
        ctx = LTOContext(units=[LTOUnit("a")])
        p = LTOPipeline()
        p.add_pass(SymbolReferenceAnalysisPass())
        p.add_pass(CrossModuleDCEPass())
        ctx = p.run(ctx)
        assert ctx.stats.total_passes_run == 2

    def test_verbose_flag(self, capsys):
        p = LTOPipeline()
        p.add_pass(SymbolReferenceAnalysisPass())
        p.verbose = True
        ctx = LTOContext(units=[LTOUnit("a")])
        p.run(ctx)
        captured = capsys.readouterr()
        assert "[LTO]" in captured.out

    def test_pipeline_returns_context(self):
        ctx = LTOContext(units=[LTOUnit("a")])
        result = LTOPipeline().run(ctx)
        assert isinstance(result, LTOContext)

    def test_multiple_passes_all_run(self):
        ctx = LTOContext(units=[LTOUnit("a")])
        p = LTOPipeline()
        for _ in range(5):
            p.add_pass(SymbolReferenceAnalysisPass())
        ctx = p.run(ctx)
        assert ctx.stats.total_passes_run == 5


# ---------------------------------------------------------------------------
# lto_optimize convenience function
# ---------------------------------------------------------------------------

class TestLtoOptimize:
    def test_returns_context(self):
        units = [LTOUnit("a"), LTOUnit("main")]
        result = lto_optimize(units)
        assert isinstance(result, LTOContext)

    def test_default_entry_point(self):
        units = [LTOUnit("a"), LTOUnit("main")]
        ctx = lto_optimize(units)
        assert "main" in ctx.entry_points

    def test_custom_entry_point(self):
        units = [LTOUnit("a"), LTOUnit("b")]
        ctx = lto_optimize(units, entry_points={"a"})
        assert "a" in ctx.entry_points

    def test_removes_dead_exports(self):
        lib = LTOUnit("lib", exports={"unused_fn"})
        main = LTOUnit("main")
        ctx = lto_optimize([lib, main], entry_points={"main"})
        # lib's unused_fn should be removed (not imported by main)
        lib_unit = ctx.unit_by_name("lib")
        assert "unused_fn" not in lib_unit.exports

    def test_keeps_used_exports(self):
        lib = LTOUnit("lib", exports={"used_fn"})
        # Give main an AST that calls used_fn so SymbolReferenceAnalysisPass
        # populates referenced_symbols and DeadImportElimination keeps the import
        main_ast = {"statements": [{"type": "FunctionCall", "name": "used_fn"}]}
        main = LTOUnit("main", ast=main_ast, imports={"used_fn": "lib"})
        ctx = lto_optimize([lib, main], entry_points={"main"})
        lib_unit = ctx.unit_by_name("lib")
        assert "used_fn" in lib_unit.exports

    def test_aggressive_flag_accepted(self):
        units = [LTOUnit("a"), LTOUnit("main")]
        ctx = lto_optimize(units, aggressive=True)
        assert isinstance(ctx, LTOContext)

    def test_empty_units(self):
        ctx = lto_optimize([])
        assert ctx.units == []

    def test_single_unit(self):
        units = [LTOUnit("only")]
        ctx = lto_optimize(units)
        assert len(ctx.units) == 1


# ---------------------------------------------------------------------------
# lto_stats_report
# ---------------------------------------------------------------------------

class TestLtoStatsReport:
    def test_returns_str(self):
        units = [LTOUnit("a"), LTOUnit("main")]
        ctx = lto_optimize(units)
        report = lto_stats_report(ctx)
        assert isinstance(report, str)

    def test_contains_header(self):
        ctx = LTOContext(units=[LTOUnit("a")])
        report = lto_stats_report(ctx)
        assert "Link-Time Optimization" in report

    def test_contains_per_unit(self):
        ctx = LTOContext(units=[LTOUnit("myunit")])
        report = lto_stats_report(ctx)
        assert "myunit" in report

    def test_contains_stats_fields(self):
        ctx = LTOContext(units=[LTOUnit("a")])
        report = lto_stats_report(ctx)
        assert "dce=" in report


# ---------------------------------------------------------------------------
# Integration scenarios
# ---------------------------------------------------------------------------

class TestLTOIntegration:
    def test_three_module_pipeline(self):
        math = LTOUnit("math", exports={"add", "internal"})
        # Give utils and main ASTs that reference the imported symbols so
        # SymbolReferenceAnalysisPass populates referenced_symbols correctly
        utils_ast = {"statements": [{"name": "add"}]}
        utils = LTOUnit("utils", ast=utils_ast, exports={"fmt"}, imports={"add": "math"})
        main_ast = {"statements": [{"name": "add"}, {"name": "fmt"}]}
        main = LTOUnit("main", ast=main_ast, imports={"add": "math", "fmt": "utils"})
        ctx = lto_optimize([math, utils, main], entry_points={"main"})

        math_u = ctx.unit_by_name("math")
        utils_u = ctx.unit_by_name("utils")
        # "add" and "fmt" are used; "internal" is not
        assert "add" in math_u.exports
        assert "fmt" in utils_u.exports
        assert "internal" not in math_u.exports

    def test_constant_flows_through_two_hops(self):
        a = LTOUnit("a", exports={"K"}, constants={"K": 42})
        b = LTOUnit("b", imports={"K": "a"}, exports={"K"})
        c = LTOUnit("c", imports={"K": "b"})
        ctx = lto_optimize([a, b, c], entry_points={"c"})
        c_unit = ctx.unit_by_name("c")
        # K should be in c's constants after propagation
        assert "K" in c_unit.constants

    def test_stats_accumulated(self):
        a = LTOUnit("a", exports={"dead1", "dead2", "keep"})
        b = LTOUnit("b", imports={"keep": "a"})
        ctx = lto_optimize([a, b], entry_points={"b"})
        assert ctx.stats.dead_exports_removed >= 2

    def test_pipeline_default_all_passes_run(self):
        ctx = LTOContext(units=[LTOUnit("a"), LTOUnit("b")])
        pipeline = LTOPipeline.default()
        ctx = pipeline.run(ctx)
        # All 6 passes should have run
        assert ctx.stats.total_passes_run == len(pipeline.passes)

    def test_no_cross_contamination_between_units(self):
        # each unit should only get its own imports' constants
        a = LTOUnit("a", exports={"X"}, constants={"X": 1})
        b = LTOUnit("b", exports={"Y"}, constants={"Y": 2})
        c = LTOUnit("c", imports={"X": "a"})
        d = LTOUnit("d", imports={"Y": "b"})
        ctx = lto_optimize([a, b, c, d], entry_points={"c", "d"})
        c_unit = ctx.unit_by_name("c")
        d_unit = ctx.unit_by_name("d")
        assert "X" in c_unit.constants
        assert "Y" in d_unit.constants
        assert "Y" not in c_unit.constants
        assert "X" not in d_unit.constants


# ---------------------------------------------------------------------------
# Lazy imports via optimizer __init__
# ---------------------------------------------------------------------------

class TestLazyImportViaPackage:
    def test_lto_unit_accessible(self):
        from nexuslang.optimizer import LTOUnit
        assert LTOUnit is not None

    def test_lto_context_accessible(self):
        from nexuslang.optimizer import LTOContext
        assert LTOContext is not None

    def test_lto_pipeline_accessible(self):
        from nexuslang.optimizer import LTOPipeline
        assert LTOPipeline is not None

    def test_lto_optimize_accessible(self):
        from nexuslang.optimizer import lto_optimize
        assert callable(lto_optimize)

    def test_lto_stats_report_accessible(self):
        from nexuslang.optimizer import lto_stats_report
        assert callable(lto_stats_report)

    def test_lto_stats_accessible(self):
        from nexuslang.optimizer import LTOStats
        assert LTOStats is not None

    def test_lto_names_in_all(self):
        import nexuslang.optimizer as opt
        assert "LTOUnit" in opt.__all__
        assert "LTOContext" in opt.__all__
        assert "LTOPipeline" in opt.__all__
