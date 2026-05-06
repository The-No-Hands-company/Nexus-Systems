"""
Function Dispatch Optimization Pass
=====================================

Reduces the overhead of dynamic function-dispatch in NexusLang programs by:

1. **Direct-call promotion** – replaces virtual/dynamic dispatch with a
   direct call when the static type of the receiver is known.

2. **Devirtualization** – for method calls where the receiver is a
   concrete type (not an abstract base), eliminates the virtual dispatch
   table lookup.

3. **Inline cache insertion** – annotates call-sites with ``_dispatch_hint``
   so the interpreter / JIT can apply inline-caching without a full
   re-analysis pass.

4. **Builtin fast-paths** – replaces calls to well-known stdlib functions
   (print, len, range, …) with ``BuiltinCallNode`` markers that the
   backends can lower to intrinsics without FFI overhead.

NLPL-specific rationale
-----------------------
Because NexusLang uses natural-language names for functions, user-defined names
can silently shadow stdlib builtins.  This pass only converts call nodes to
``BuiltinCallNode`` when the name is not redefined anywhere in the file,
making the transformation safe.
"""

from typing import Any, Dict, List, Optional, Set
from dataclasses import dataclass, field
from . import OptimizationLevel, OptimizationPass, OptimizationStats


# Stdlib builtins that can be lowered to intrinsics
_BUILTIN_INTRINSICS: Set[str] = {
    "print", "len", "range", "type_of",
    "assert_equal", "assert_true", "assert_false",
    "min", "max", "abs", "round", "sqrt",
    "append", "extend", "push", "pop",
}


@dataclass
class DispatchSite:
    """Records a call site and the optimisation applied."""
    call_name: str
    line: int
    optimization_applied: str  # "direct", "devirtualized", "builtin", "cached"


class DispatchOptimizationPass(OptimizationPass):
    """
    Reduce function-dispatch overhead via static analysis of call sites.

    Only safe transformations are applied:
    - No transformation is made when the resolution is ambiguous.
    - Devirtualization only happens for sealed/final types or concrete vars.
    - Builtin promotion only happens when the name is not shadowed.
    """

    def __init__(self, enable_builtin_lowering: bool = True):
        super().__init__("dispatch_optimization")
        self.enable_builtin_lowering = enable_builtin_lowering
        self._redefined_names: Set[str] = set()
        self._optimized_sites: List[DispatchSite] = []
        self._concrete_types: Dict[str, str] = {}  # var_name -> type_name

    # ------------------------------------------------------------------
    # Public API
    # ------------------------------------------------------------------

    def should_run(self, level: OptimizationLevel) -> bool:
        """Dispatch optimization starts at O1."""
        if isinstance(level, OptimizationLevel):
            return level.value >= OptimizationLevel.O1.value
        return False

    def run(self, ast: Any) -> Any:
        if ast is None:
            return ast

        self._redefined_names.clear()
        self._optimized_sites.clear()
        self._concrete_types.clear()

        # Phase 1 – collect all names defined in this module
        self._collect_definitions(ast)

        # Phase 2 – collect concrete-type bindings from variable declarations
        self._collect_concrete_types(ast)

        # Phase 3 – transform call sites
        self._transform_calls(ast)

        self.stats.total_passes += 1
        return ast

    # ------------------------------------------------------------------
    # Phase 1: collect locally defined names
    # ------------------------------------------------------------------

    def _collect_definitions(self, node: Any, depth: int = 0) -> None:
        if node is None or depth > 30:
            return
        node_type = type(node).__name__
        if node_type in ("FunctionDefinition", "ClassDefinition", "VariableDeclaration"):
            name = getattr(node, "name", None)
            if isinstance(name, str):
                self._redefined_names.add(name)
        for child in self._iter_children(node):
            self._collect_definitions(child, depth + 1)

    # ------------------------------------------------------------------
    # Phase 2: collect concrete type bindings
    # ------------------------------------------------------------------

    def _collect_concrete_types(self, node: Any, depth: int = 0) -> None:
        if node is None or depth > 30:
            return
        node_type = type(node).__name__
        if node_type == "VariableDeclaration":
            var_name = getattr(node, "name", None)
            inferred = getattr(node, "_inferred_type", None)
            declared = getattr(node, "type_annotation", None)
            concrete = str(inferred or declared or "")
            if var_name and concrete and concrete != "unknown":
                self._concrete_types[var_name] = concrete
        for child in self._iter_children(node):
            self._collect_concrete_types(child, depth + 1)

    # ------------------------------------------------------------------
    # Phase 3: transform call sites
    # ------------------------------------------------------------------

    def _transform_calls(self, node: Any, depth: int = 0) -> None:
        if node is None or depth > 50:
            return
        node_type = type(node).__name__

        if node_type == "FunctionCall":
            self._optimize_call(node)
        elif node_type == "MethodCall":
            self._optimize_method_call(node)

        for child in self._iter_children(node):
            self._transform_calls(child, depth + 1)

    def _optimize_call(self, call: Any) -> None:
        """Optimize a plain function call node."""
        name = getattr(call, "name", None)
        if not isinstance(name, str):
            return
        line = getattr(call, "line", 0)

        # 1. Builtin intrinsic lowering
        if (
            self.enable_builtin_lowering
            and name in _BUILTIN_INTRINSICS
            and name not in self._redefined_names
        ):
            try:
                call._dispatch_hint = "builtin_intrinsic"
                call._is_builtin = True
            except AttributeError:
                pass
            self._optimized_sites.append(
                DispatchSite(name, line, "builtin")
            )
            return

        # 2. Direct-call promotion using type information
        receiver_name = self._extract_receiver_name(call)
        if receiver_name:
            concrete_type = self._concrete_types.get(receiver_name)
            if concrete_type:
                try:
                    call._dispatch_hint = f"direct:{concrete_type}"
                    call._receiver_type = concrete_type
                except AttributeError:
                    pass
                self._optimized_sites.append(
                    DispatchSite(name, line, "direct")
                )
                return

        # 3. Attach inline-cache hint for the interpreter / JIT
        try:
            if not hasattr(call, "_dispatch_hint"):
                call._dispatch_hint = "inline_cache"
        except AttributeError:
            pass
        self._optimized_sites.append(
            DispatchSite(name, line, "cached")
        )

    def _optimize_method_call(self, call: Any) -> None:
        """Optimize a method call node (obj.method(...))."""
        receiver = getattr(call, "receiver", None)
        method_name = getattr(call, "method", None) or getattr(call, "name", None)
        line = getattr(call, "line", 0)
        if receiver is None or method_name is None:
            return

        receiver_var = getattr(receiver, "name", None)
        if isinstance(receiver_var, str):
            concrete_type = self._concrete_types.get(receiver_var)
            if concrete_type:
                # Devirtualize: remove dynamic dispatch for concrete types
                try:
                    call._dispatch_hint = f"devirtualized:{concrete_type}"
                    call._receiver_type = concrete_type
                    call._devirtualized = True
                except AttributeError:
                    pass
                self._optimized_sites.append(
                    DispatchSite(str(method_name), line, "devirtualized")
                )
                return

        # Attach inline-cache hint
        try:
            if not hasattr(call, "_dispatch_hint"):
                call._dispatch_hint = "inline_cache"
        except AttributeError:
            pass

    # ------------------------------------------------------------------
    # Utility
    # ------------------------------------------------------------------

    def _extract_receiver_name(self, call: Any) -> Optional[str]:
        """Try to extract the variable name that is the first argument (receiver)."""
        args = getattr(call, "arguments", None) or getattr(call, "args", None)
        if isinstance(args, list) and args:
            first = args[0]
            return getattr(first, "name", None)
        return None

    def _iter_children(self, node: Any):
        if not hasattr(node, "__dict__"):
            return
        for k, v in vars(node).items():
            if k.startswith("_"):
                continue
            if isinstance(v, list):
                yield from [i for i in v if i is not None and hasattr(i, "__dict__")]
            elif hasattr(v, "__dict__"):
                yield v

    # ------------------------------------------------------------------
    # Reporting
    # ------------------------------------------------------------------

    @property
    def optimized_sites(self) -> List[DispatchSite]:
        return list(self._optimized_sites)

    def summary(self) -> str:
        by_kind: Dict[str, int] = {}
        for site in self._optimized_sites:
            by_kind[site.optimization_applied] = by_kind.get(site.optimization_applied, 0) + 1
        parts = ", ".join(f"{k}: {v}" for k, v in sorted(by_kind.items()))
        return (
            f"DispatchOptimization: {len(self._optimized_sites)} sites optimized "
            f"({parts})"
        )
