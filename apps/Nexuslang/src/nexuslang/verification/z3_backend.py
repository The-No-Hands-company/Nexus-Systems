"""
nlpl.verification.z3_backend
==============================

Optional Z3 SMT solver backend for discharging verification conditions.

Z3 is a theorem prover from Microsoft Research.  It is **not** a required
dependency of NexusLang.  This module checks for the ``z3`` Python package at
import time and degrades gracefully when it is unavailable.

Install Z3 to enable formal proof::

    pip install z3-solver

Usage::

    from nexuslang.verification.z3_backend import Z3Backend, Z3_AVAILABLE

    if Z3_AVAILABLE:
        backend = Z3Backend(timeout_ms=5000)
        results = backend.verify_all(spec)
"""

from __future__ import annotations

from typing import List, Optional, Tuple, Any

from nexuslang.verification.specification import (
    FormalSpec,
    VerificationCondition,
    VerificationStatus,
)

# Optional Z3 import
try:
    import z3 as _z3  # type: ignore
    Z3_AVAILABLE: bool = True
except ImportError:  # pragma: no cover
    _z3 = None
    Z3_AVAILABLE = False


class Z3Backend:
    """Attempt to discharge verification conditions using the Z3 SMT solver.

    When Z3 is not installed every condition is left as UNVERIFIED (the
    method returns without modification).

    Args:
        timeout_ms (int): Per-condition solver timeout in milliseconds.
                          Default is 5 000 ms (5 seconds).
    """

    def __init__(self, timeout_ms: int = 5_000) -> None:
        self._timeout_ms = timeout_ms
        self._available = Z3_AVAILABLE

    @property
    def available(self) -> bool:
        """True when the Z3 Python package is installed and importable."""
        return self._available

    # ------------------------------------------------------------------
    # Public API
    # ------------------------------------------------------------------

    def verify_all(self, spec: FormalSpec) -> FormalSpec:
        """Attempt to prove/refute each condition in *spec*.

        Conditions that cannot be expressed in the currently supported
        fragment are left as UNVERIFIED.

        Args:
            spec (FormalSpec): Input spec (modified **in-place**).

        Returns:
            FormalSpec: The same *spec* with updated statuses.
        """
        if not self._available:
            return spec

        for vc in spec.conditions:
            if vc.status != VerificationStatus.UNVERIFIED:
                continue
            self._try_discharge(vc)

        return spec

    def verify_one(self, vc: VerificationCondition) -> VerificationCondition:
        """Attempt to discharge a single VC.

        Args:
            vc (VerificationCondition): Modified **in-place**.

        Returns:
            VerificationCondition: The same *vc* with updated status.
        """
        if not self._available:
            return vc
        self._try_discharge(vc)
        return vc

    # ------------------------------------------------------------------
    # Internal solver logic
    # ------------------------------------------------------------------

    def _try_discharge(self, vc: VerificationCondition) -> None:
        """Try to encode and discharge *vc* via Z3.

        Updates *vc.status*, *vc.counter_example*, and *vc.solver_output*
        in-place.  Silently skips conditions that cannot be translated.
        """
        if _z3 is None:
            return

        try:
            expr, variables = self._translate_condition(vc.ast_node)
            if expr is None:
                # Condition is not in the supported fragment
                vc.status = VerificationStatus.UNKNOWN
                vc.solver_output = "Condition not in Z3-translatable fragment"
                return

            solver = _z3.Solver()
            solver.set("timeout", self._timeout_ms)

            # To prove P always holds, check satisfiability of NOT P
            solver.add(_z3.Not(expr))
            result = solver.check()

            if result == _z3.unsat:
                # Negation is unsatisfiable → P is always true → proved
                vc.status = VerificationStatus.PROVED
                vc.solver_output = "unsat"
            elif result == _z3.sat:
                # Negation is satisfiable → counter-example exists → failed
                vc.status = VerificationStatus.FAILED
                model = solver.model()
                vc.counter_example = {
                    str(decl): model[decl]
                    for decl in model.decls()
                }
                vc.solver_output = str(model)
            else:
                # unknown (timeout)
                vc.status = VerificationStatus.UNKNOWN
                vc.solver_output = "unknown (timeout or not supported)"

        except Exception as exc:  # pragma: no cover
            vc.status = VerificationStatus.UNKNOWN
            vc.solver_output = f"Z3 translation error: {exc}"

    def _translate_condition(
        self, node: Any
    ) -> Tuple[Optional[Any], dict]:
        """Translate an NexusLang condition AST node to a Z3 expression.

        Returns:
            Tuple of (z3_expr, variable_map).
            z3_expr is None when the node cannot be translated.
        """
        if node is None or not hasattr(node, "node_type"):
            return None, {}

        ntype = node.node_type
        variables: dict = {}

        # Condition is a require/ensure/guarantee/invariant statement
        if ntype in (
            "require_statement",
            "ensure_statement",
            "guarantee_statement",
            "invariant_statement",
        ):
            return self._translate_condition(getattr(node, "condition", None))

        # Spec annotation wrapper
        if ntype == "spec_annotation":
            return self._translate_condition(getattr(node, "condition", None))

        # Integer literal
        if ntype == "literal":
            value = getattr(node, "value", None)
            if isinstance(value, int):
                return _z3.IntVal(value), {}
            if isinstance(value, bool):
                return _z3.BoolVal(value), {}
            if isinstance(value, float):
                return _z3.RealVal(value), {}
            return None, {}

        # Identifier → Z3 integer variable (conservative assumption)
        if ntype == "identifier":
            name = str(getattr(node, "name", "x"))
            var = _z3.Int(name)
            variables[name] = var
            return var, {name: var}

        # Binary operations and comparisons
        if ntype in ("binary_op", "comparison", "logical"):
            left_expr, left_vars = self._translate_condition(
                getattr(node, "left", None)
            )
            right_expr, right_vars = self._translate_condition(
                getattr(node, "right", None)
            )
            variables.update(left_vars)
            variables.update(right_vars)

            if left_expr is None or right_expr is None:
                return None, variables

            op = str(
                getattr(node, "operator", getattr(node, "op", ""))
            ).lower()

            op_map = {
                "equals": left_expr == right_expr,
                "==": left_expr == right_expr,
                "not_equal": left_expr != right_expr,
                "!=": left_expr != right_expr,
                "not equal to": left_expr != right_expr,
                "greater_than": left_expr > right_expr,
                ">": left_expr > right_expr,
                "greater than": left_expr > right_expr,
                "less_than": left_expr < right_expr,
                "<": left_expr < right_expr,
                "less than": left_expr < right_expr,
                "greater_than_or_equal": left_expr >= right_expr,
                ">=": left_expr >= right_expr,
                "greater than or equal to": left_expr >= right_expr,
                "less_than_or_equal": left_expr <= right_expr,
                "<=": left_expr <= right_expr,
                "less than or equal to": left_expr <= right_expr,
                "and": _z3.And(left_expr, right_expr),
                "or": _z3.Or(left_expr, right_expr),
                "plus": left_expr + right_expr,
                "+": left_expr + right_expr,
                "minus": left_expr - right_expr,
                "-": left_expr - right_expr,
                "times": left_expr * right_expr,
                "*": left_expr * right_expr,
            }
            if op in op_map:
                return op_map[op], variables
            return None, variables

        # Not / negation
        if ntype in ("unary_op", "not"):
            inner, inner_vars = self._translate_condition(
                getattr(node, "operand", getattr(node, "expr", None))
            )
            if inner is None:
                return None, inner_vars
            op = str(getattr(node, "operator", getattr(node, "op", ""))).lower()
            if op in ("not", "!"):
                return _z3.Not(inner), inner_vars
            return None, inner_vars

        return None, variables
