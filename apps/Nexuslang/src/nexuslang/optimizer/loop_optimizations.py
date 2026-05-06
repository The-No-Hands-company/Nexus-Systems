"""
Loop Optimization Passes
=========================

Comprehensive loop optimization passes for the NexusLang compiler:

- LoopAnalysisPass                 - Analyze loops, classify variant/invariant vars
- LoopInvariantCodeMotionPass      - Hoist invariant computations out of loop bodies (LICM)
- LoopFusionPass                   - Merge adjacent loops over the same constant iteration range
- InductionVariableSimplificationPass - Annotate i*K patterns for strength-reduced codegen
- LoopStrengthReductionPass        - Replace expensive in-loop arithmetic with cheaper forms
- LoopOptimizationPipeline         - Composable pipeline of all loop passes
- loop_optimize()                  - Convenience entry point

Loop optimizations can yield significant speedups for numeric and data-processing workloads.
"""
from __future__ import annotations

import copy
from dataclasses import dataclass, field
from typing import Any, Optional

from ..optimizer import OptimizationPass


# ---------------------------------------------------------------------------
# Statistics
# ---------------------------------------------------------------------------

@dataclass
class LoopOptimizationStats:
    """Statistics gathered by loop optimization passes."""
    invariants_hoisted: int = 0
    loops_fused: int = 0
    induction_vars_simplified: int = 0
    strength_reductions: int = 0
    total_passes_run: int = 0

    def __add__(self, other: "LoopOptimizationStats") -> "LoopOptimizationStats":
        return LoopOptimizationStats(
            invariants_hoisted=self.invariants_hoisted + other.invariants_hoisted,
            loops_fused=self.loops_fused + other.loops_fused,
            induction_vars_simplified=self.induction_vars_simplified + other.induction_vars_simplified,
            strength_reductions=self.strength_reductions + other.strength_reductions,
            total_passes_run=self.total_passes_run + other.total_passes_run,
        )

    def __str__(self) -> str:
        return (
            "Loop Optimization Statistics:\n"
            f"  Invariants Hoisted:         {self.invariants_hoisted}\n"
            f"  Loops Fused:                {self.loops_fused}\n"
            f"  Induction Vars Simplified:  {self.induction_vars_simplified}\n"
            f"  Strength Reductions:        {self.strength_reductions}\n"
            f"  Total Passes Run:           {self.total_passes_run}"
        )


# ---------------------------------------------------------------------------
# Loop metadata
# ---------------------------------------------------------------------------

@dataclass
class LoopInfo:
    """Metadata about a single loop extracted during analysis."""
    loop_node: Any
    loop_kind: str                  # "for" | "while" | "repeat" | "repeat_while" | "unknown"
    body: list
    induction_var: Optional[str]    # Primary induction variable (or None)
    iteration_count: Optional[int]  # Statically known count (or None)
    is_count_known: bool
    variant_vars: set               # Variables modified inside the loop (including loop iterator)
    referenced_vars: set            # Variables read anywhere inside the loop


# ---------------------------------------------------------------------------
# Internal AST helpers
# ---------------------------------------------------------------------------

_LOOP_TYPE_NAMES = frozenset({
    "ForLoop", "WhileLoop", "RepeatNTimesLoop", "RepeatWhileLoop",
})


def _is_loop(node: Any) -> bool:
    """Return True if the node is any kind of loop AST node."""
    return type(node).__name__ in _LOOP_TYPE_NAMES


def _get_body(node: Any) -> list:
    """Return the body statement list of a node, or an empty list."""
    body = getattr(node, "body", None)
    if isinstance(body, list):
        return body
    stmts = getattr(node, "statements", None)
    if isinstance(stmts, list):
        return stmts
    return []


def _collect_assigned_vars(stmts: list) -> set:
    """
    Recursively collect all variable names assigned within a statement list.
    Used to determine which variables are *variant* inside a loop body.
    """
    result: set = set()
    for stmt in stmts:
        if type(stmt).__name__ == "VariableDeclaration":
            if hasattr(stmt, "name"):
                result.add(stmt.name)
        # Recurse into nested blocks
        body = _get_body(stmt)
        if body:
            result.update(_collect_assigned_vars(body))
        for attr in ("then_block", "else_block", "else_body"):
            sub = getattr(stmt, attr, None)
            if isinstance(sub, list):
                result.update(_collect_assigned_vars(sub))
    return result


def _collect_referenced_vars(node: Any, result: Optional[set] = None) -> set:
    """
    Recursively collect all variable names *referenced* (read) in an AST node
    or list of statements. Works with both real AST nodes and dict/namespace-based
    test fixtures.
    """
    if result is None:
        result = set()
    if node is None:
        return result
    if isinstance(node, list):
        for item in node:
            _collect_referenced_vars(item, result)
        return result
    if isinstance(node, dict):
        for v in node.values():
            _collect_referenced_vars(v, result)
        return result

    node_type = type(node).__name__
    if node_type == "Identifier":
        result.add(node.name)
        return result

    for attr in (
        "statements", "body", "condition", "value", "left", "right",
        "operand", "iterable", "start", "end", "step", "count",
        "arguments", "then_block", "else_block", "else_body",
        "expression", "cases",
    ):
        child = getattr(node, attr, None)
        if child is not None:
            _collect_referenced_vars(child, result)
    return result


def _is_constant_literal(node: Any) -> bool:
    """Return True if node is a constant literal (Literal node or Python scalar)."""
    if node is None:
        return False
    if isinstance(node, (int, float, str, bool)):
        return True
    return type(node).__name__ == "Literal"


def _literal_value(node: Any) -> Any:
    """Extract the primitive value from a Literal node or Python constant."""
    if isinstance(node, (int, float, str, bool)):
        return node
    return getattr(node, "value", None)


def _is_invariant_stmt(stmt: Any, variant_vars: set) -> bool:
    """
    Return True if *stmt* is safe to hoist out of a loop.

    Criteria:
    - It must be a VariableDeclaration (pure value assignment, no side effects).
    - The variable being declared must not be in variant_vars (not re-assigned
      elsewhere in the loop body).
    - All variables referenced on the right-hand side must be outside variant_vars.
    """
    if type(stmt).__name__ != "VariableDeclaration":
        return False
    decl_name = getattr(stmt, "name", None)
    if decl_name in variant_vars:
        return False
    value = getattr(stmt, "value", None)
    if value is None:
        return False
    refs = _collect_referenced_vars(value)
    return not (refs & variant_vars)


# ---------------------------------------------------------------------------
# Loop analysis
# ---------------------------------------------------------------------------

def _analyze_loop(loop_node: Any) -> LoopInfo:
    """Analyze a single loop AST node and return a populated LoopInfo."""
    kind_name = type(loop_node).__name__
    if kind_name == "ForLoop":
        loop_kind = "for"
    elif kind_name == "WhileLoop":
        loop_kind = "while"
    elif kind_name == "RepeatNTimesLoop":
        loop_kind = "repeat"
    elif kind_name == "RepeatWhileLoop":
        loop_kind = "repeat_while"
    else:
        loop_kind = "unknown"

    body = _get_body(loop_node)
    induction_var: Optional[str] = None
    iteration_count: Optional[int] = None
    is_count_known = False

    if loop_kind == "for":
        induction_var = getattr(loop_node, "iterator", None)
        start = getattr(loop_node, "start", None)
        end = getattr(loop_node, "end", None)
        step_node = getattr(loop_node, "step", None)
        if _is_constant_literal(start) and _is_constant_literal(end):
            start_v = _literal_value(start)
            end_v = _literal_value(end)
            step_v = _literal_value(step_node) if step_node is not None else 1
            if (
                isinstance(start_v, int)
                and isinstance(end_v, int)
                and isinstance(step_v, int)
                and step_v != 0
            ):
                iteration_count = max(0, (end_v - start_v + step_v - 1) // step_v)
                is_count_known = True

    elif loop_kind == "repeat":
        count_node = getattr(loop_node, "count", None)
        if _is_constant_literal(count_node):
            val = _literal_value(count_node)
            if isinstance(val, int) and val >= 0:
                iteration_count = val
                is_count_known = True

    variant_vars = _collect_assigned_vars(body)
    if induction_var:
        variant_vars.add(induction_var)
    referenced_vars = _collect_referenced_vars(body)

    return LoopInfo(
        loop_node=loop_node,
        loop_kind=loop_kind,
        body=body,
        induction_var=induction_var,
        iteration_count=iteration_count,
        is_count_known=is_count_known,
        variant_vars=variant_vars,
        referenced_vars=referenced_vars,
    )


# ---------------------------------------------------------------------------
# Pass 1: Loop Analysis (read-only)
# ---------------------------------------------------------------------------

class LoopAnalysisPass(OptimizationPass):
    """
    Read-only analysis pass that walks the AST and records a LoopInfo for every
    loop encountered. Nested loops are recorded in pre-order (outer before inner).

    After calling ``run(ast)``, inspect ``loop_infos`` for downstream use.
    """

    def __init__(self) -> None:
        super().__init__("LoopAnalysis")
        self.loop_infos: list[LoopInfo] = []

    def run(self, ast: Any) -> Any:
        self.loop_infos.clear()
        stmts = getattr(ast, "statements", None) or _get_body(ast)
        if stmts:
            self._scan(stmts)
        return ast

    def _scan(self, stmts: list) -> None:
        for stmt in stmts:
            if _is_loop(stmt):
                info = _analyze_loop(stmt)
                self.loop_infos.append(info)
                self._scan(info.body)
            else:
                for attr in ("then_block", "else_block", "else_body"):
                    sub = getattr(stmt, attr, None)
                    if isinstance(sub, list):
                        self._scan(sub)
                body = _get_body(stmt)
                if body and not _is_loop(stmt):
                    self._scan(body)


# ---------------------------------------------------------------------------
# Pass 2: Loop Invariant Code Motion (LICM)
# ---------------------------------------------------------------------------

class LoopInvariantCodeMotionPass(OptimizationPass):
    """
    Moves loop-invariant computations out of loop bodies (LICM).

    A VariableDeclaration stmt is hoistable when:
    1. Its declared variable is not modified elsewhere in the loop (not variant).
    2. Its right-hand side only references variables not modified in the loop.

    Hoisted statements are inserted immediately before the containing loop in
    the parent statement list. The pass recurses into nested loops and
    conditional blocks.
    """

    def __init__(self) -> None:
        super().__init__("LoopInvariantCodeMotion")
        self.hoisted_total = 0

    def run(self, ast: Any) -> Any:
        stmts = getattr(ast, "statements", None)
        if stmts is None:
            stmts = _get_body(ast)
        if stmts is not None:
            self._process_statements(stmts)
        self.stats.constants_folded = self.hoisted_total
        return ast

    def _process_statements(self, stmts: list) -> None:
        i = 0
        while i < len(stmts):
            stmt = stmts[i]
            if _is_loop(stmt):
                hoisted = self._collect_hoistable(stmt)
                if hoisted:
                    for offset, h_stmt in enumerate(hoisted):
                        stmts.insert(i + offset, h_stmt)
                    i += len(hoisted)
                    self.hoisted_total += len(hoisted)
                # Recurse into the (now-trimmed) loop body.
                self._process_statements(_get_body(stmts[i]))
            else:
                for attr in ("then_block", "else_block", "else_body"):
                    sub = getattr(stmt, attr, None)
                    if isinstance(sub, list):
                        self._process_statements(sub)
                if not _is_loop(stmt):
                    inner = _get_body(stmt)
                    if inner:
                        self._process_statements(inner)
            i += 1

    def _collect_hoistable(self, loop_node: Any) -> list:
        """
        Return deep copies of invariant stmts; remove originals from the loop body.

        Uses fixed-point iteration to correctly determine which variables are
        truly variant (i.e. whose values depend on the loop induction variable
        or on other variant variables). This avoids the mistake of treating every
        assigned variable as variant, which would wrongly prevent hoisting of
        computations like ``set c to a + b`` where ``a`` and ``b`` are loop-
        invariant.
        """
        body = _get_body(loop_node)
        if not body:
            return []
        induction_var = getattr(loop_node, "iterator", None)

        # Build the truly-variant set via fixed-point propagation.
        # Seed: the loop induction variable is always variant.
        variant_vars: set = set()
        if induction_var:
            variant_vars.add(induction_var)

        # Propagate: any variable assigned from a variant source is itself variant.
        changed = True
        while changed:
            changed = False
            for stmt in body:
                if type(stmt).__name__ != "VariableDeclaration":
                    # Non-declaration statements have unknown side effects; treat
                    # all variables they reference as variant.
                    refs = _collect_referenced_vars(stmt)
                    for r in refs:
                        if r not in variant_vars:
                            variant_vars.add(r)
                            changed = True
                    continue
                name = getattr(stmt, "name", None)
                if name in variant_vars:
                    continue
                value = getattr(stmt, "value", None)
                refs = _collect_referenced_vars(value)
                if refs & variant_vars:
                    variant_vars.add(name)
                    changed = True

        hoistable = []
        new_body = []
        for stmt in body:
            if _is_invariant_stmt(stmt, variant_vars):
                hoistable.append(copy.deepcopy(stmt))
            else:
                new_body.append(stmt)
        if hoistable:
            loop_node.body = new_body
        return hoistable


# ---------------------------------------------------------------------------
# Pass 3: Loop Fusion
# ---------------------------------------------------------------------------

def _loops_have_same_range(a: Any, b: Any) -> bool:
    """
    Return True if two ForLoop nodes iterate over the same constant integer
    range (identical start, end, and step values).
    """
    if type(a).__name__ != "ForLoop" or type(b).__name__ != "ForLoop":
        return False
    for attr in ("start", "end"):
        if getattr(a, attr, None) is None or getattr(b, attr, None) is None:
            return False
    if not (_is_constant_literal(a.start) and _is_constant_literal(a.end)):
        return False
    if not (_is_constant_literal(b.start) and _is_constant_literal(b.end)):
        return False
    a_step = _literal_value(getattr(a, "step", None)) if getattr(a, "step", None) is not None else 1
    b_step = _literal_value(getattr(b, "step", None)) if getattr(b, "step", None) is not None else 1
    return (
        _literal_value(a.start) == _literal_value(b.start)
        and _literal_value(a.end) == _literal_value(b.end)
        and a_step == b_step
    )


def _loops_independent(a: Any, b: Any) -> bool:
    """
    Return True if loop A and loop B are data-independent:
    - B does not read any variable written by A.
    - A does not read any variable written by B.
    - A and B do not write the same variable (output dependence).
    """
    a_written = _collect_assigned_vars(_get_body(a))
    b_written = _collect_assigned_vars(_get_body(b))
    a_read = _collect_referenced_vars(_get_body(a))
    b_read = _collect_referenced_vars(_get_body(b))
    if b_read & a_written:
        return False
    if a_read & b_written:
        return False
    if a_written & b_written:
        return False
    return True


class LoopFusionPass(OptimizationPass):
    """
    Merges adjacent ForLoop nodes that satisfy:
    1. Both iterate over the same constant integer range (same start, end, step).
    2. They are data-independent (no variable written by one is read by the other,
       and no variable is written by both).

    The merged loop uses the first loop's iterator variable and contains both
    bodies in sequence. Loops with different iterator names are still fused
    (the second loop's variables retain their original names; a subsequent
    renaming pass could normalize, but LICM makes this safe in most cases).
    """

    def __init__(self) -> None:
        super().__init__("LoopFusion")
        self.fused_total = 0

    def run(self, ast: Any) -> Any:
        stmts = getattr(ast, "statements", None)
        if stmts is None:
            stmts = _get_body(ast)
        if stmts is not None:
            changed = True
            while changed:
                changed = self._fuse_scan(stmts)
        self.stats.dead_functions_removed = self.fused_total
        return ast

    def _fuse_scan(self, stmts: list) -> bool:
        """Single left-to-right fusion scan. Returns True if any fusion occurred."""
        changed = False
        i = 0
        while i < len(stmts) - 1:
            curr = stmts[i]
            nxt = stmts[i + 1]
            if (
                _is_loop(curr) and _is_loop(nxt)
                and _loops_have_same_range(curr, nxt)
                and _loops_independent(curr, nxt)
            ):
                curr.body = list(_get_body(curr)) + list(_get_body(nxt))
                stmts.pop(i + 1)
                self.fused_total += 1
                changed = True
                # Stay at i — merged loop might be fusable with next
            else:
                for attr in ("then_block", "else_block", "body"):
                    sub = getattr(curr, attr, None)
                    if isinstance(sub, list) and len(sub) > 1:
                        if self._fuse_scan(sub):
                            changed = True
                i += 1
        return changed


# ---------------------------------------------------------------------------
# Pass 4: Induction Variable Simplification
# ---------------------------------------------------------------------------

def _is_induction_multiply(node: Any, induction_var: str) -> Optional[tuple]:
    """
    Return (lhs_name, multiplier) if *node* is a VariableDeclaration of the form:
        set result to i * K   or   set result to K * i
    where i == induction_var and K is a constant integer or float.
    Returns None otherwise.
    """
    if type(node).__name__ != "VariableDeclaration":
        return None
    value = getattr(node, "value", None)
    if value is None or type(value).__name__ != "BinaryOperation":
        return None
    if value.operator not in ("*", "times", "multiplied by"):
        return None
    left = value.left
    right = value.right
    if type(left).__name__ == "Identifier" and left.name == induction_var and _is_constant_literal(right):
        return (node.name, _literal_value(right))
    if type(right).__name__ == "Identifier" and right.name == induction_var and _is_constant_literal(left):
        return (node.name, _literal_value(left))
    return None


class InductionVariableSimplificationPass(OptimizationPass):
    """
    Identifies induction variable multiply patterns in range-based ForLoop bodies.

    When a loop has a known integer induction variable ``i``, expressions like
    ``set result to i * K`` can be strength-reduced: an auxiliary variable
    initialized to ``start * K`` is incremented by ``K`` each iteration instead
    of performing a multiplication every iteration.

    This pass *annotates* each qualifying loop node with the attribute
    ``_iv_simplifications``, a list of ``(decl_name, aux_name, step)`` tuples.
    A downstream code generator or runtime optimizer uses these annotations to
    emit the cheaper form.
    """

    def __init__(self) -> None:
        super().__init__("InductionVariableSimplification")
        self.simplified_total = 0

    def run(self, ast: Any) -> Any:
        stmts = getattr(ast, "statements", None) or _get_body(ast)
        if stmts:
            self._process(stmts)
        self.stats.unreachable_blocks_removed = self.simplified_total
        return ast

    def _process(self, stmts: list) -> None:
        for stmt in stmts:
            if type(stmt).__name__ == "ForLoop":
                induction_var = getattr(stmt, "iterator", None)
                # Only apply to range-based loops (have explicit start and end).
                # For-each loops iterate over a collection; the loop variable is
                # not a numeric induction variable and must not be treated as one.
                start = getattr(stmt, "start", None)
                end = getattr(stmt, "end", None)
                if induction_var and start is not None and end is not None:
                    self._simplify_loop(stmt, induction_var)
            body = _get_body(stmt)
            if body:
                self._process(body)

    def _simplify_loop(self, loop_node: Any, induction_var: str) -> None:
        body = _get_body(loop_node)
        simplifications = []
        for stmt in body:
            result = _is_induction_multiply(stmt, induction_var)
            if result:
                lhs_name, multiplier = result
                aux_name = f"_iv_{lhs_name}"
                simplifications.append((lhs_name, aux_name, multiplier))
                self.simplified_total += 1
        if simplifications:
            if not hasattr(loop_node, "_iv_simplifications"):
                loop_node._iv_simplifications = []
            loop_node._iv_simplifications.extend(simplifications)


# ---------------------------------------------------------------------------
# Pass 5: Loop Strength Reduction
# ---------------------------------------------------------------------------

class _SyntheticLiteral:
    """Minimal literal node produced by strength reduction transformations."""
    def __init__(self, value: Any, type_: str = "integer") -> None:
        self.value = value
        self.type = type_


_POWER_OPS = frozenset({"**", "pow", "power", "to the power of"})
_MUL_OPS = frozenset({"*", "times", "multiplied by"})
_ADD_OPS = frozenset({"+", "plus"})
_SUB_OPS = frozenset({"-", "minus"})


def _reduce_strength_in_expr(node: Any) -> Optional[Any]:
    """
    Apply strength reduction to a single BinaryOperation node.
    Returns the replacement node, the original node (modified in-place),
    or None if no reduction applies.

    Transformations:
    - x ** 2      -> x * x         (exponentiation to multiply)
    - x * 1       -> x             (identity elimination)
    - 1 * x       -> x
    - x * 0       -> Literal(0)    (zero propagation)
    - 0 * x       -> Literal(0)
    - x + 0       -> x             (additive identity)
    - 0 + x       -> x
    - x - 0       -> x             (subtractive identity)
    """
    if type(node).__name__ != "BinaryOperation":
        return None
    op = node.operator
    left = node.left
    right = node.right

    # x ** 2 -> x * x
    if op in _POWER_OPS and _is_constant_literal(right) and _literal_value(right) == 2:
        node.operator = "*"
        node.right = copy.deepcopy(left)
        return node

    # x * 1 -> x  or  1 * x -> x
    if op in _MUL_OPS:
        if _is_constant_literal(right) and _literal_value(right) == 1:
            return left
        if _is_constant_literal(left) and _literal_value(left) == 1:
            return right
        # x * 0 -> 0  or  0 * x -> 0
        if _is_constant_literal(right) and _literal_value(right) == 0:
            return _SyntheticLiteral(0)
        if _is_constant_literal(left) and _literal_value(left) == 0:
            return _SyntheticLiteral(0)

    # x + 0 -> x  or  0 + x -> x
    if op in _ADD_OPS:
        if _is_constant_literal(right) and _literal_value(right) == 0:
            return left
        if _is_constant_literal(left) and _literal_value(left) == 0:
            return right

    # x - 0 -> x
    if op in _SUB_OPS:
        if _is_constant_literal(right) and _literal_value(right) == 0:
            return left

    return None


class LoopStrengthReductionPass(OptimizationPass):
    """
    Reduces expensive arithmetic operations inside loop bodies.

    Operates on VariableDeclaration nodes whose value is a BinaryOperation.
    Also recurses into nested binary operation sub-expressions.

    Transformations applied: see ``_reduce_strength_in_expr`` docstring.
    """

    def __init__(self) -> None:
        super().__init__("LoopStrengthReduction")
        self.reduced_total = 0

    def run(self, ast: Any) -> Any:
        stmts = getattr(ast, "statements", None) or _get_body(ast)
        if stmts is not None:
            self._process_stmts(stmts)
        self.stats.dead_variables_removed = self.reduced_total
        return ast

    def _process_stmts(self, stmts: list) -> None:
        for stmt in stmts:
            if _is_loop(stmt):
                self._reduce_body(_get_body(stmt))
                self._process_stmts(_get_body(stmt))
            else:
                for attr in ("then_block", "else_block"):
                    sub = getattr(stmt, attr, None)
                    if isinstance(sub, list):
                        self._process_stmts(sub)

    def _reduce_body(self, stmts: list) -> None:
        for stmt in stmts:
            if type(stmt).__name__ == "VariableDeclaration":
                value = getattr(stmt, "value", None)
                if value is None:
                    continue
                reduced = _reduce_strength_in_expr(value)
                if reduced is not None:
                    # Accept both in-place modifications (reduced is value) and
                    # replacement nodes (reduced is not value).  Either way the
                    # expression has been strength-reduced.
                    stmt.value = reduced
                    self.reduced_total += 1
                else:
                    # Try nested sub-expressions
                    self._reduce_nested(stmt, "value")

    def _reduce_nested(self, parent: Any, attr: str) -> None:
        node = getattr(parent, attr, None)
        if node is None or type(node).__name__ != "BinaryOperation":
            return
        for child_attr in ("left", "right"):
            child = getattr(node, child_attr, None)
            if child is not None and type(child).__name__ == "BinaryOperation":
                reduced = _reduce_strength_in_expr(child)
                if reduced is not None and reduced is not child:
                    setattr(node, child_attr, reduced)
                    self.reduced_total += 1


# ---------------------------------------------------------------------------
# Pipeline
# ---------------------------------------------------------------------------

class LoopOptimizationPipeline:
    """
    Composable pipeline of loop optimization passes.

    Usage::

        pipeline = LoopOptimizationPipeline.default()
        optimized_ast = pipeline.run(ast)
        print(pipeline.aggregated_stats)
    """

    def __init__(self, verbose: bool = False) -> None:
        self._passes: list[OptimizationPass] = []
        self.verbose = verbose
        self.aggregated_stats = LoopOptimizationStats()

    def add_pass(self, pass_: OptimizationPass) -> "LoopOptimizationPipeline":
        """Append a pass to the pipeline and return self for chaining."""
        self._passes.append(pass_)
        return self

    @property
    def passes(self) -> list:
        return list(self._passes)

    def run(self, ast: Any) -> Any:
        """Run all added passes in order and return the (modified) AST."""
        self.aggregated_stats = LoopOptimizationStats()
        current = ast
        for pass_ in self._passes:
            if self.verbose:
                print(f"[loop_opts] running pass: {pass_.name}")
            current = pass_.run(current)
            self.aggregated_stats.total_passes_run += 1
        return current

    @classmethod
    def default(cls, aggressive: bool = False) -> "LoopOptimizationPipeline":
        """
        Create the standard loop optimization pipeline.

        Pass order:
        1. LoopAnalysisPass          - populate loop metadata (read-only)
        2. LoopInvariantCodeMotionPass - hoist invariant stmts first
        3. LoopFusionPass            - fuse after LICM reduces dependencies
        4. InductionVariableSimplificationPass - annotate iv*K patterns
        5. LoopStrengthReductionPass - arithmetic strength reduction
        """
        pipeline = cls()
        pipeline.add_pass(LoopAnalysisPass())
        pipeline.add_pass(LoopInvariantCodeMotionPass())
        pipeline.add_pass(LoopFusionPass())
        pipeline.add_pass(InductionVariableSimplificationPass())
        pipeline.add_pass(LoopStrengthReductionPass())
        return pipeline


# ---------------------------------------------------------------------------
# Convenience entry point
# ---------------------------------------------------------------------------

def loop_optimize(
    ast: Any,
    aggressive: bool = False,
    verbose: bool = False,
) -> tuple:
    """
    Apply all loop optimizations to an AST.

    Args:
        ast:        Root AST node (must expose ``statements`` list or ``body`` list).
        aggressive: Reserved for future use; currently identical to non-aggressive.
        verbose:    Print pass names as they execute.

    Returns:
        ``(ast, LoopOptimizationStats)`` tuple. The AST is modified in-place and
        also returned for convenience.
    """
    pipeline = LoopOptimizationPipeline.default(aggressive=aggressive)
    pipeline.verbose = verbose
    result_ast = pipeline.run(ast)
    stats = pipeline.aggregated_stats
    return result_ast, stats


__all__ = [
    "LoopOptimizationStats",
    "LoopInfo",
    "LoopAnalysisPass",
    "LoopInvariantCodeMotionPass",
    "LoopFusionPass",
    "InductionVariableSimplificationPass",
    "LoopStrengthReductionPass",
    "LoopOptimizationPipeline",
    "loop_optimize",
    # Helpers exposed for testing
    "_is_loop",
    "_get_body",
    "_collect_assigned_vars",
    "_collect_referenced_vars",
    "_analyze_loop",
    "_is_invariant_stmt",
    "_is_constant_literal",
    "_literal_value",
    "_loops_have_same_range",
    "_loops_independent",
    "_is_induction_multiply",
    "_reduce_strength_in_expr",
    "_SyntheticLiteral",
]
