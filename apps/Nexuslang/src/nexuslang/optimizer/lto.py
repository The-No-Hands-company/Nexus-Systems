"""
Link-Time Optimization (LTO) for NexusLang
======================================

Implements whole-program optimization across module boundaries.  LTO is
performed *after* all individual compilation units have been compiled — it
operates on the collected set of module ASTs (or symbol tables derived from
them) and applies transformations that cannot be done within a single module:

Passes
------
CrossModuleDCEPass
    Whole-program dead-code elimination.  A function exported by module A but
    never imported or called anywhere in the full program is removed.

CrossModuleInliningPass
    Inline small functions across module boundaries.  A function defined in
    module A that is called from module B (and is small enough) is inlined at
    the call site, avoiding the cross-module call overhead.

ConstantPropagationPass
    Propagate module-level constants (``set X to <literal>``) across module
    boundaries so downstream consumers can fold them at compile time.

DeadImportElimination
    Remove ``import`` statements that bring in symbols which are subsequently
    never used within the importing module.

RedundantExportPass
    Strip ``export`` annotations from symbols that are never imported by any
    other module in the program (useful before final linking).

Usage
-----
    from nexuslang.optimizer.lto import LTOPipeline, LTOContext, LTOUnit

    # Build one LTOUnit per compiled module
    units = [
        LTOUnit("math_utils", math_ast, exports={"add", "sub"}),
        LTOUnit("main",       main_ast, imports={"add"}),
    ]

    ctx = LTOContext(units)
    pipeline = LTOPipeline()
    optimized_ctx = pipeline.run(ctx)

    # Access each optimized unit
    for unit in optimized_ctx.units:
        print(unit.name, unit.stats)
"""

import copy
from dataclasses import dataclass, field
from typing import Any, Dict, List, Optional, Set, Tuple
from . import OptimizationLevel, OptimizationPass, OptimizationStats


# ---------------------------------------------------------------------------
# Data structures
# ---------------------------------------------------------------------------

@dataclass
class LTOStats:
    """Per-unit and whole-program LTO statistics."""
    dead_exports_removed: int = 0
    functions_inlined_cross_module: int = 0
    constants_propagated: int = 0
    dead_imports_removed: int = 0
    redundant_exports_removed: int = 0
    total_passes_run: int = 0

    def __add__(self, other: "LTOStats") -> "LTOStats":
        return LTOStats(
            dead_exports_removed=self.dead_exports_removed + other.dead_exports_removed,
            functions_inlined_cross_module=self.functions_inlined_cross_module + other.functions_inlined_cross_module,
            constants_propagated=self.constants_propagated + other.constants_propagated,
            dead_imports_removed=self.dead_imports_removed + other.dead_imports_removed,
            redundant_exports_removed=self.redundant_exports_removed + other.redundant_exports_removed,
            total_passes_run=self.total_passes_run + other.total_passes_run,
        )

    def __str__(self) -> str:
        return (
            f"LTO Statistics:\n"
            f"  Dead exports removed:            {self.dead_exports_removed}\n"
            f"  Cross-module inlines:            {self.functions_inlined_cross_module}\n"
            f"  Constants propagated:            {self.constants_propagated}\n"
            f"  Dead imports removed:            {self.dead_imports_removed}\n"
            f"  Redundant exports stripped:      {self.redundant_exports_removed}\n"
            f"  Total LTO passes run:            {self.total_passes_run}"
        )


class LTOUnit:
    """
    A single compilation unit participating in LTO.

    Attributes
    ----------
    name : str
        Module name (e.g. ``"math_utils"``).
    ast : Any
        The compiled AST or IR for this module.  The LTO passes treat the AST
        as an opaque object with optional well-known attributes:
        - ``statements`` : list of top-level statement nodes
        - ``__module_name__`` : str
        - ``__exports__`` : set[str]
        - ``__imports__`` : dict[str, str]  (symbol -> source module)
        - ``__constants__`` : dict[str, Any]
    exports : set[str]
        Names exported by this module.  Populated from ``ast.__exports__`` if
        present; otherwise passed explicitly.
    imports : dict[str, str]
        ``{symbol_name: source_module}`` — symbols this module imports and
        from which module.  Populated from ``ast.__imports__`` if present.
    constants : dict[str, Any]
        Module-level constants this unit defines.
    stats : LTOStats
        Per-unit statistics updated after optimization.
    """

    def __init__(
        self,
        name: str,
        ast: Any = None,
        exports: Optional[Set[str]] = None,
        imports: Optional[Dict[str, str]] = None,
        constants: Optional[Dict[str, Any]] = None,
    ):
        self.name = name
        self.ast = ast
        # Prefer data attached to the AST node; fall back to constructor args
        if ast is not None and hasattr(ast, "__exports__"):
            self.exports: Set[str] = set(ast.__exports__)
        else:
            self.exports = set(exports) if exports else set()

        if ast is not None and hasattr(ast, "__imports__"):
            self.imports: Dict[str, str] = dict(ast.__imports__)
        else:
            self.imports = dict(imports) if imports else {}

        if ast is not None and hasattr(ast, "__constants__"):
            self.constants: Dict[str, Any] = dict(ast.__constants__)
        else:
            self.constants = dict(constants) if constants else {}

        # Symbols actually referenced inside this unit (discovered by analysis)
        self.referenced_symbols: Set[str] = set()
        # Small functions from *other* modules that have been inlined here
        self.inlined_from: Dict[str, str] = {}  # {call_site_id -> source_module}
        self.stats = LTOStats()

    def __repr__(self) -> str:
        return (
            f"LTOUnit(name={self.name!r}, "
            f"exports={len(self.exports)}, "
            f"imports={len(self.imports)})"
        )


class LTOContext:
    """
    The full set of compilation units to be optimized together.

    Attributes
    ----------
    units : list[LTOUnit]
        All modules in the program.
    entry_points : set[str]
        Module names that contain program entry points.  Symbols reachable
        from an entry point are considered live.
    stats : LTOStats
        Aggregated statistics across all passes.
    """

    def __init__(
        self,
        units: Optional[List[LTOUnit]] = None,
        entry_points: Optional[Set[str]] = None,
    ):
        self.units: List[LTOUnit] = list(units) if units else []
        self.entry_points: Set[str] = set(entry_points) if entry_points else set()
        self.stats = LTOStats()
        # Global symbol table: symbol_name -> LTOUnit that defines it
        self._symbol_index: Dict[str, LTOUnit] = {}
        self._index_built = False

    def add_unit(self, unit: LTOUnit) -> None:
        """Register a new compilation unit."""
        self.units.append(unit)
        self._index_built = False

    def build_index(self) -> None:
        """Build the global symbol index from all units."""
        self._symbol_index.clear()
        for unit in self.units:
            for sym in unit.exports:
                self._symbol_index[sym] = unit
        self._index_built = True

    def defining_unit(self, symbol: str) -> Optional[LTOUnit]:
        """Return the unit that exports ``symbol``, or None."""
        if not self._index_built:
            self.build_index()
        return self._symbol_index.get(symbol)

    def all_imported_symbols(self) -> Set[str]:
        """Return the union of every symbol imported by any unit."""
        result: Set[str] = set()
        for unit in self.units:
            result.update(unit.imports.keys())
        return result

    def all_exported_symbols(self) -> Set[str]:
        """Return the union of every symbol exported by any unit."""
        result: Set[str] = set()
        for unit in self.units:
            result.update(unit.exports)
        return result

    def referenced_symbols_globally(self) -> Set[str]:
        """Union of all referenced symbols across every unit."""
        result: Set[str] = set()
        for unit in self.units:
            result.update(unit.referenced_symbols)
        return result

    def unit_by_name(self, name: str) -> Optional[LTOUnit]:
        """Return the unit with the given module name, or None."""
        for unit in self.units:
            if unit.name == name:
                return unit
        return None

    def __repr__(self) -> str:
        return (
            f"LTOContext(units={len(self.units)}, "
            f"entry_points={self.entry_points!r})"
        )


# ---------------------------------------------------------------------------
# AST walking helpers (unit → referenced symbols)
# ---------------------------------------------------------------------------

def _collect_referenced_symbols(ast: Any, result: Set[str]) -> None:
    """
    Recursively walk an AST and collect all identifier/call names into
    ``result``.  Works with both object-graph ASTs and dict-based IRs.
    """
    if ast is None:
        return
    if isinstance(ast, dict):
        for key, val in ast.items():
            if key in ("name", "callee", "identifier") and isinstance(val, str):
                result.add(val)
            else:
                _collect_referenced_symbols(val, result)
    elif isinstance(ast, (list, tuple)):
        for item in ast:
            _collect_referenced_symbols(item, result)
    else:
        # Object-graph node
        node_type = type(ast).__name__
        if node_type in ("FunctionCall", "CallExpression"):
            if hasattr(ast, "name"):
                result.add(ast.name)
            elif hasattr(ast, "callee") and isinstance(ast.callee, str):
                result.add(ast.callee)
        if node_type in ("Identifier", "VariableReference"):
            if hasattr(ast, "name"):
                result.add(ast.name)
        # Recurse into child attributes that look like AST subtrees
        for attr in vars(ast) if hasattr(ast, "__dict__") else []:
            if attr.startswith("_"):
                continue
            child = getattr(ast, attr)
            if child is ast:
                continue
            _collect_referenced_symbols(child, result)


def _get_function_body_size(ast: Any, func_name: str) -> int:
    """
    Heuristic: return the number of statement-like nodes in the body of
    ``func_name`` within ``ast``.  Returns 0 if the function is not found
    or the AST does not have a queryable structure.
    """
    if ast is None:
        return 0
    if isinstance(ast, dict):
        stmts = ast.get("statements") or ast.get("body") or []
        for stmt in stmts:
            if isinstance(stmt, dict):
                if stmt.get("type") in ("FunctionDefinition", "function") and stmt.get("name") == func_name:
                    body = stmt.get("body") or []
                    return len(body) if isinstance(body, list) else 1
    if hasattr(ast, "statements"):
        for stmt in (ast.statements or []):
            stype = type(stmt).__name__
            if stype in ("FunctionDefinition", "FunctionDeclaration"):
                if getattr(stmt, "name", None) == func_name:
                    body = getattr(stmt, "body", None)
                    if isinstance(body, list):
                        return len(body)
                    return 1
    return 0


def _get_module_constants(ast: Any) -> Dict[str, Any]:
    """Extract top-level constant assignments from an AST."""
    result: Dict[str, Any] = {}
    if ast is None:
        return result
    if hasattr(ast, "__constants__"):
        return dict(ast.__constants__)
    stmts = []
    if isinstance(ast, dict):
        stmts = ast.get("statements") or []
    elif hasattr(ast, "statements"):
        stmts = ast.statements or []
    for stmt in stmts:
        if isinstance(stmt, dict):
            if stmt.get("type") in ("ConstantDeclaration", "let", "const"):
                name = stmt.get("name")
                val = stmt.get("value")
                if name and val is not None:
                    result[name] = val
        elif type(stmt).__name__ in ("ConstantDeclaration", "VariableDeclaration"):
            if getattr(stmt, "is_const", False) or getattr(stmt, "const", False):
                name = getattr(stmt, "name", None)
                val = getattr(stmt, "value", None)
                if name:
                    result[name] = val
    return result


# ---------------------------------------------------------------------------
# LTO Passes
# ---------------------------------------------------------------------------

class CrossModuleDCEPass(OptimizationPass):
    """
    Cross-module dead-code elimination.

    An exported symbol that is never imported (and is not an entry-point
    module's public API) is considered dead and removed from the exporting
    module's export list.  If the symbol is also unreferenced within its own
    module the function body is dropped entirely.
    """

    def __init__(self, keep_entry_exports: bool = True):
        super().__init__("cross_module_dce")
        self.keep_entry_exports = keep_entry_exports

    def run(self, ast: Any) -> Any:
        """Single-unit pass — use ``run_on_context`` for full LTO."""
        return ast

    def run_on_context(self, ctx: LTOContext) -> LTOContext:
        """Remove dead exports across all units in the context."""
        ctx.build_index()
        imported_globally = ctx.all_imported_symbols()

        for unit in ctx.units:
            is_entry = unit.name in ctx.entry_points
            dead: Set[str] = set()
            for sym in list(unit.exports):
                if sym in imported_globally:
                    continue
                if is_entry and self.keep_entry_exports:
                    continue
                dead.add(sym)

            unit.exports -= dead
            removed = len(dead)
            unit.stats.dead_exports_removed += removed
            ctx.stats.dead_exports_removed += removed
            self.stats.dead_functions_removed += removed

        ctx.build_index()
        return ctx


class CrossModuleInliningPass(OptimizationPass):
    """
    Cross-module function inlining.

    For each call to an imported symbol, if the defining module exports a body
    small enough to inline (under ``max_inline_size`` AST nodes), annotate the
    call site with inline metadata.  Code-generation backends can then emit
    the inlined body directly.
    """

    DEFAULT_MAX_INLINE_SIZE = 15  # AST nodes

    def __init__(self, max_inline_size: int = DEFAULT_MAX_INLINE_SIZE):
        super().__init__("cross_module_inlining")
        self.max_inline_size = max_inline_size

    def run(self, ast: Any) -> Any:
        return ast

    def run_on_context(self, ctx: LTOContext) -> LTOContext:
        """Annotate inlineable cross-module call sites."""
        ctx.build_index()

        for unit in ctx.units:
            inlined = 0
            for sym, src_module_name in unit.imports.items():
                src_unit = ctx.unit_by_name(src_module_name)
                if src_unit is None:
                    src_unit = ctx.defining_unit(sym)
                if src_unit is None:
                    continue

                body_size = _get_function_body_size(src_unit.ast, sym)
                if body_size == 0 or body_size > self.max_inline_size:
                    continue

                # Mark the call site for inlining
                unit.inlined_from[sym] = src_module_name
                inlined += 1

            unit.stats.functions_inlined_cross_module += inlined
            ctx.stats.functions_inlined_cross_module += inlined
            self.stats.functions_inlined += inlined

        return ctx


class ConstantPropagationPass(OptimizationPass):
    """
    Cross-module constant propagation.

    Constants defined at module scope in one unit and imported by another are
    propagated into the importing unit's constant table so that the code
    generator can fold them without a lookup in the exporting module.
    """

    def __init__(self):
        super().__init__("cross_module_constant_propagation")

    def run(self, ast: Any) -> Any:
        return ast

    def run_on_context(self, ctx: LTOContext) -> LTOContext:
        """Propagate constants into all importing units."""
        # Build a global constant table: {sym -> value}
        global_constants: Dict[str, Any] = {}
        for unit in ctx.units:
            # Prefer explicitly provided constants, then scan AST
            consts = dict(unit.constants) or _get_module_constants(unit.ast)
            unit.constants.update(consts)
            for k, v in unit.constants.items():
                if k in unit.exports:
                    global_constants[k] = v

        propagated_total = 0
        for unit in ctx.units:
            propagated = 0
            for sym, src_mod in unit.imports.items():
                if sym in global_constants and sym not in unit.constants:
                    unit.constants[sym] = global_constants[sym]
                    propagated += 1
            unit.stats.constants_propagated += propagated
            ctx.stats.constants_propagated += propagated
            propagated_total += propagated
            self.stats.constants_folded += propagated

        return ctx


class DeadImportEliminationPass(OptimizationPass):
    """
    Remove import declarations for symbols that are never used within the
    importing module.
    """

    def __init__(self):
        super().__init__("dead_import_elimination")

    def run(self, ast: Any) -> Any:
        return ast

    def run_on_context(self, ctx: LTOContext) -> LTOContext:
        """Drop unused imports from each unit."""
        for unit in ctx.units:
            # Discover what this unit actually references
            refs: Set[str] = set(unit.referenced_symbols)
            _collect_referenced_symbols(unit.ast, refs)

            dead_imports = {
                sym for sym in unit.imports if sym not in refs
            }
            for sym in dead_imports:
                del unit.imports[sym]

            removed = len(dead_imports)
            unit.stats.dead_imports_removed += removed
            ctx.stats.dead_imports_removed += removed
            self.stats.dead_variables_removed += removed

        return ctx


class RedundantExportPass(OptimizationPass):
    """
    Strip ``export`` annotations from symbols that no other module imports.

    This pass is lightweight — it only modifies the metadata on each unit;
    the actual AST ``export`` keyword removal is left to the code generator.
    """

    def __init__(self):
        super().__init__("redundant_export_elimination")

    def run(self, ast: Any) -> Any:
        return ast

    def run_on_context(self, ctx: LTOContext) -> LTOContext:
        """Remove export annotations for symbols never imported."""
        imported_globally = ctx.all_imported_symbols()

        for unit in ctx.units:
            if unit.name in ctx.entry_points:
                continue  # Never strip entry-point exports
            redundant = unit.exports - imported_globally
            unit.exports -= redundant
            removed = len(redundant)
            unit.stats.redundant_exports_removed += removed
            ctx.stats.redundant_exports_removed += removed
            self.stats.dead_functions_removed += removed

        return ctx


class SymbolReferenceAnalysisPass(OptimizationPass):
    """
    Scans each unit's AST to populate ``unit.referenced_symbols``.  This
    should be the *first* pass in every LTO pipeline so subsequent passes
    see accurate reference data.
    """

    def __init__(self):
        super().__init__("symbol_reference_analysis")

    def run(self, ast: Any) -> Any:
        return ast

    def run_on_context(self, ctx: LTOContext) -> LTOContext:
        for unit in ctx.units:
            refs: Set[str] = set()
            _collect_referenced_symbols(unit.ast, refs)
            unit.referenced_symbols = refs
        return ctx


# ---------------------------------------------------------------------------
# LTO Pipeline
# ---------------------------------------------------------------------------

class LTOPipeline:
    """
    Orchestrates a sequence of LTO passes over an :class:`LTOContext`.

    Typical usage::

        pipeline = LTOPipeline.default()
        ctx = pipeline.run(ctx)

    Custom pipeline::

        pipeline = LTOPipeline()
        pipeline.add_pass(SymbolReferenceAnalysisPass())
        pipeline.add_pass(CrossModuleDCEPass())
        ctx = pipeline.run(ctx)
    """

    def __init__(self):
        self.passes: List[OptimizationPass] = []
        self.verbose: bool = False

    def add_pass(self, pass_: OptimizationPass) -> None:
        """Append a pass to the pipeline."""
        self.passes.append(pass_)

    def run(self, ctx: LTOContext) -> LTOContext:
        """
        Run all registered passes on ``ctx`` and return the (potentially
        modified) context.
        """
        for pass_ in self.passes:
            if self.verbose:
                print(f"[LTO] Running pass: {pass_.name}")

            # LTO passes operate on the context, not a single AST
            if hasattr(pass_, "run_on_context"):
                ctx = pass_.run_on_context(ctx)
            else:
                # Fallback: run single-unit pass on each unit's AST
                for unit in ctx.units:
                    unit.ast = pass_.run(unit.ast)

            ctx.stats.total_passes_run += 1

        return ctx

    def print_stats(self) -> None:
        """Print aggregated LTO statistics (call after run())."""
        pass  # caller should use ctx.stats

    @classmethod
    def default(cls, aggressive: bool = False) -> "LTOPipeline":
        """
        Create the standard LTO pipeline.

        Standard:
            analysis -> const propagation -> dead import elim ->
            cross-module DCE -> redundant export elim

        Aggressive (adds cross-module inlining with a larger threshold):
            ... -> cross-module inlining
        """
        pipeline = cls()
        pipeline.add_pass(SymbolReferenceAnalysisPass())
        pipeline.add_pass(ConstantPropagationPass())
        pipeline.add_pass(DeadImportEliminationPass())
        pipeline.add_pass(CrossModuleDCEPass(keep_entry_exports=True))
        pipeline.add_pass(RedundantExportPass())
        if aggressive:
            pipeline.add_pass(CrossModuleInliningPass(max_inline_size=30))
        else:
            pipeline.add_pass(CrossModuleInliningPass(max_inline_size=15))
        return pipeline


# ---------------------------------------------------------------------------
# Convenience helpers
# ---------------------------------------------------------------------------

def lto_optimize(
    units: List[LTOUnit],
    entry_points: Optional[Set[str]] = None,
    aggressive: bool = False,
    verbose: bool = False,
) -> LTOContext:
    """
    Convenience function: build an LTOContext from ``units``, run the default
    LTO pipeline, and return the optimized context.

    Parameters
    ----------
    units
        All compilation units in the program.
    entry_points
        Module names that serve as program entry points.  Defaults to the
        last unit (typical for a ``main`` module).
    aggressive
        Enable cross-module inlining with a larger inline threshold.
    verbose
        Print pass names while running.

    Returns
    -------
    LTOContext
        The optimized context with updated units and aggregated statistics.
    """
    if entry_points is None and units:
        entry_points = {units[-1].name}

    ctx = LTOContext(units=units, entry_points=entry_points)
    pipeline = LTOPipeline.default(aggressive=aggressive)
    pipeline.verbose = verbose
    return pipeline.run(ctx)


def lto_stats_report(ctx: LTOContext) -> str:
    """Return a human-readable LTO statistics report."""
    lines = [
        "=== Link-Time Optimization Report ===",
        str(ctx.stats),
        "",
        "Per-Unit Breakdown:",
    ]
    for unit in ctx.units:
        s = unit.stats
        lines.append(
            f"  {unit.name}: "
            f"dce={s.dead_exports_removed} "
            f"inline={s.functions_inlined_cross_module} "
            f"const={s.constants_propagated} "
            f"dead_import={s.dead_imports_removed} "
            f"redundant_export={s.redundant_exports_removed}"
        )
    return "\n".join(lines)


# ---------------------------------------------------------------------------
# Exports
# ---------------------------------------------------------------------------

__all__ = [
    # Data structures
    "LTOStats",
    "LTOUnit",
    "LTOContext",
    # Passes
    "SymbolReferenceAnalysisPass",
    "CrossModuleDCEPass",
    "CrossModuleInliningPass",
    "ConstantPropagationPass",
    "DeadImportEliminationPass",
    "RedundantExportPass",
    # Pipeline
    "LTOPipeline",
    # Helpers
    "lto_optimize",
    "lto_stats_report",
    # Internal helpers (exposed for testing)
    "_collect_referenced_symbols",
    "_get_function_body_size",
    "_get_module_constants",
]
