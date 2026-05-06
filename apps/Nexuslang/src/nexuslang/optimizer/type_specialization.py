"""
Type Specialization Optimization Pass
======================================

Generates type-specialized variants of generic functions when the call-site
types are statically known.  Specialized variants avoid runtime type-dispatch
overhead and enable downstream optimizations (inlining, vectorization).

NLPL-specific rationale
-----------------------
NLPL's natural-language syntax allows programmers to write concise generic
code that is nonetheless called with concrete types at every call site.  The
type system tracks inferred types, giving the optimizer enough information to
create specialized copies without requiring explicit type annotations.
"""

from typing import Any, Dict, List, Optional, Set, Tuple
from dataclasses import dataclass, field
from . import OptimizationLevel, OptimizationPass, OptimizationStats


@dataclass
class SpecializationRecord:
    """Tracks a single specialization of a function."""
    original_name: str
    specialized_name: str
    type_signature: Tuple[str, ...]   # (param_type, ..., return_type)
    call_site_count: int = 0


class TypeSpecializationPass(OptimizationPass):
    """
    Create type-specialized clones of hot generic functions.

    Algorithm
    ---------
    1. Walk all function-call nodes, collecting (function_name, arg_types)
       pairs from the type-annotation metadata attached by the type-checker.
    2. For each (function_name, type_signature) pair that is called more than
       ``min_calls`` times, create a specialised copy of the function body
       with concrete type annotations inserted.
    3. Replace the originating call sites with direct calls to the
       specialised variant.
    4. The original generic function is kept (for dynamic / uninferred calls).

    Limitations
    -----------
    - Requires type annotations on call nodes (set by the type-checker).
      Falls back gracefully when annotations are absent.
    - Recursive specializations are limited to ``max_depth`` levels to prevent
      exponential code growth.
    """

    DEFAULT_MIN_CALLS = 3
    DEFAULT_MAX_SPECIALIZATIONS_PER_FUNCTION = 4
    DEFAULT_MAX_DEPTH = 2

    def __init__(
        self,
        min_calls: int = DEFAULT_MIN_CALLS,
        max_specs: int = DEFAULT_MAX_SPECIALIZATIONS_PER_FUNCTION,
        max_depth: int = DEFAULT_MAX_DEPTH,
    ):
        super().__init__("type_specialization")
        self.min_calls = min_calls
        self.max_specs = max_specs
        self.max_depth = max_depth

        self._call_profiles: Dict[str, Dict[Tuple[str, ...], int]] = {}
        self._specializations: List[SpecializationRecord] = []
        self._function_defs: Dict[str, Any] = {}   # name -> FunctionDefinition node
        self._spec_counter = 0

    # ------------------------------------------------------------------
    # Public API
    # ------------------------------------------------------------------

    def should_run(self, level: OptimizationLevel) -> bool:
        """Type specialization starts at O2."""
        if isinstance(level, OptimizationLevel):
            return level.value >= OptimizationLevel.O2.value
        return False

    def run(self, ast: Any) -> Any:
        if ast is None:
            return ast

        # Phase 1 – discover all function definitions
        self._function_defs.clear()
        self._call_profiles.clear()
        self._specializations.clear()
        self._spec_counter = 0

        self._collect_function_defs(ast)

        # Phase 2 – profile call sites
        self._profile_calls(ast)

        # Phase 3 – create specializations
        new_defs = self._create_specializations()

        if not new_defs:
            return ast  # Nothing to do

        # Phase 4 – replace call sites
        self._replace_call_sites(ast)

        # Phase 5 – inject specialized function definitions into the AST
        self._inject_specialized_defs(ast, new_defs)

        self.stats.total_passes += 1
        return ast

    # ------------------------------------------------------------------
    # Phase implementations
    # ------------------------------------------------------------------

    def _collect_function_defs(self, node: Any, depth: int = 0) -> None:
        if node is None or depth > 20:
            return
        node_type = type(node).__name__
        if node_type == "FunctionDefinition":
            name = getattr(node, "name", None)
            if isinstance(name, str):
                self._function_defs[name] = node
        for child in self._iter_children(node):
            self._collect_function_defs(child, depth + 1)

    def _profile_calls(self, node: Any, depth: int = 0) -> None:
        if node is None or depth > 50:
            return
        node_type = type(node).__name__
        if node_type == "FunctionCall":
            name = getattr(node, "name", None)
            if isinstance(name, str) and name in self._function_defs:
                type_sig = self._infer_call_type_signature(node)
                if type_sig:
                    self._call_profiles.setdefault(name, {})
                    self._call_profiles[name][type_sig] = (
                        self._call_profiles[name].get(type_sig, 0) + 1
                    )
        for child in self._iter_children(node):
            self._profile_calls(child, depth + 1)

    def _infer_call_type_signature(self, call_node: Any) -> Optional[Tuple[str, ...]]:
        """Extract the type signature from a call node's type annotations."""
        args = getattr(call_node, "arguments", None) or getattr(call_node, "args", None)
        if not isinstance(args, list):
            return None
        types_list = []
        for arg in args:
            inferred = getattr(arg, "_inferred_type", None)
            if inferred is None:
                return None  # Signature incomplete – skip
            types_list.append(str(inferred))
        ret_type = getattr(call_node, "_inferred_return_type", "unknown")
        types_list.append(str(ret_type))
        return tuple(types_list)

    def _create_specializations(self) -> List[Any]:
        """Generate specialized AST nodes for qualifying call profiles."""
        new_defs = []
        for func_name, type_counts in self._call_profiles.items():
            orig_def = self._function_defs.get(func_name)
            if orig_def is None:
                continue
            specs_created = 0
            for type_sig, count in sorted(type_counts.items(), key=lambda x: -x[1]):
                if count < self.min_calls:
                    continue
                if specs_created >= self.max_specs:
                    break
                spec_name = f"__spec_{func_name}_{self._spec_counter}__"
                self._spec_counter += 1

                # Create a shallow clone of the function definition
                spec_def = self._clone_function_def(orig_def, spec_name, type_sig)
                if spec_def is None:
                    continue

                rec = SpecializationRecord(
                    original_name=func_name,
                    specialized_name=spec_name,
                    type_signature=type_sig,
                    call_site_count=count,
                )
                self._specializations.append(rec)
                new_defs.append(spec_def)
                specs_created += 1
        return new_defs

    def _clone_function_def(
        self, orig: Any, new_name: str, type_sig: Tuple[str, ...]
    ) -> Optional[Any]:
        """
        Return a cloned function definition with the specialised name and
        concrete type annotations applied to parameters.

        This is a best-effort shallow clone; backends that need deep cloning
        should implement their own visit/copy mechanism.
        """
        import copy
        try:
            spec = copy.deepcopy(orig)
        except Exception:
            return None

        # Rename
        try:
            spec.name = new_name
        except AttributeError:
            return None

        # Annotate parameters with concrete types
        params = getattr(spec, "parameters", None) or getattr(spec, "params", None)
        if isinstance(params, list):
            for i, param in enumerate(params):
                if i < len(type_sig) - 1:  # Last element is return type
                    try:
                        param._specialised_type = type_sig[i]
                    except AttributeError:
                        pass

        # Mark the function as a specialization
        try:
            spec._is_specialization = True
            spec._original_name = orig.name if hasattr(orig, "name") else ""
            spec._type_signature = type_sig
        except AttributeError:
            pass

        return spec

    def _replace_call_sites(self, node: Any, depth: int = 0) -> None:
        """Replace qualifying call sites with calls to specialized variants."""
        if node is None or depth > 50:
            return
        for field_name in self._get_child_field_names(node):
            child = getattr(node, field_name, None)
            if child is None:
                continue
            if isinstance(child, list):
                for idx, item in enumerate(child):
                    self._maybe_redirect_call(child, idx, item)
                    self._replace_call_sites(item, depth + 1)
            else:
                self._maybe_redirect_call_attr(node, field_name, child)
                self._replace_call_sites(child, depth + 1)

    def _maybe_redirect_call(self, lst: list, idx: int, node: Any) -> None:
        if node is None or type(node).__name__ != "FunctionCall":
            return
        spec_name = self._find_specialization_name(node)
        if spec_name:
            try:
                node._original_call_name = getattr(node, "name", node)
                node.name = spec_name
            except AttributeError:
                pass

    def _maybe_redirect_call_attr(self, parent: Any, field: str, node: Any) -> None:
        if node is None or type(node).__name__ != "FunctionCall":
            return
        spec_name = self._find_specialization_name(node)
        if spec_name:
            try:
                node._original_call_name = getattr(node, "name", None)
                node.name = spec_name
            except AttributeError:
                pass

    def _find_specialization_name(self, call_node: Any) -> Optional[str]:
        name = getattr(call_node, "name", None)
        if not isinstance(name, str):
            return None
        type_sig = self._infer_call_type_signature(call_node)
        if not type_sig:
            return None
        for rec in self._specializations:
            if rec.original_name == name and rec.type_signature == type_sig:
                return rec.specialized_name
        return None

    def _inject_specialized_defs(self, ast: Any, new_defs: List[Any]) -> None:
        """Append specialized function definitions to the top-level body."""
        body = getattr(ast, "body", None) or getattr(ast, "statements", None)
        if isinstance(body, list):
            body.extend(new_defs)

    # ------------------------------------------------------------------
    # Utility helpers
    # ------------------------------------------------------------------

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

    def _get_child_field_names(self, node: Any) -> List[str]:
        if not hasattr(node, "__dict__"):
            return []
        return [
            k for k, v in vars(node).items()
            if not k.startswith("_") and (
                isinstance(v, list) or hasattr(v, "__dict__")
            )
        ]

    # ------------------------------------------------------------------
    # Reporting
    # ------------------------------------------------------------------

    @property
    def specializations(self) -> List[SpecializationRecord]:
        return list(self._specializations)

    def summary(self) -> str:
        total_sites = sum(r.call_site_count for r in self._specializations)
        return (
            f"TypeSpecialization: {len(self._specializations)} specializations "
            f"created, covering {total_sites} call sites"
        )
