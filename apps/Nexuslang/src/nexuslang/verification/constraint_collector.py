"""
nlpl.verification.constraint_collector
=======================================

AST visitor that walks a parsed NexusLang program and extracts all
design-by-contract annotations as ``VerificationCondition`` objects.

Usage::

    from nexuslang.verification.constraint_collector import ConstraintCollector
    from nexuslang.parser.parser import Parser
    from nexuslang.parser.lexer import Lexer

    ast = Parser(Lexer(source).tokenize()).parse()
    spec = ConstraintCollector(path="my_file.nxl").collect(ast)
"""

from __future__ import annotations

from typing import List, Optional, Any

from nexuslang.verification.specification import (
    FormalSpec,
    VerificationCondition,
    VerificationStatus,
)


class ConstraintCollector:
    """Walk an NexusLang AST and collect all contract annotations as VCs.

    The collector performs a single-pass depth-first walk.  It tracks
    the current function context so that each VC can be attributed to
    its enclosing function.

    Args:
        path (str): Source file path label (used in ``FormalSpec.path``).
    """

    def __init__(self, path: str = "<unknown>") -> None:
        self._path = path
        self._current_function: Optional[str] = None
        self._vcs: List[VerificationCondition] = []

    # ------------------------------------------------------------------
    # Public API
    # ------------------------------------------------------------------

    def collect(self, ast_root: Any) -> FormalSpec:
        """Collect all VCs from *ast_root* and return a ``FormalSpec``.

        Args:
            ast_root: The ``Program`` AST node returned by ``Parser.parse()``.
                The root ``Program`` node itself has no ``node_type``; we
                walk its ``statements`` list directly.

        Returns:
            FormalSpec: The formal specification for *path*.
        """
        self._vcs = []
        self._current_function = None
        # Program nodes do not carry a node_type attribute, so we descend
        # into their statements list directly.
        stmts = getattr(ast_root, "statements", None)
        if stmts is not None:
            self._descend_list(stmts)
        else:
            self._walk(ast_root)
        spec = FormalSpec(path=self._path)
        spec.conditions = list(self._vcs)
        return spec

    # ------------------------------------------------------------------
    # Internal walk
    # ------------------------------------------------------------------

    def _walk(self, node: Any) -> None:
        """Recursively walk *node* dispatching on node_type."""
        if node is None or not hasattr(node, "node_type"):
            return

        ntype = node.node_type

        if ntype == "function_definition":
            self._visit_function(node)
        elif ntype == "require_statement":
            self._add_vc("precondition", node)
        elif ntype == "ensure_statement":
            self._add_vc("postcondition", node)
        elif ntype == "guarantee_statement":
            self._add_vc("guarantee", node)
        elif ntype == "invariant_statement":
            self._add_vc("invariant", node)
        elif ntype == "spec_block":
            self._visit_spec_block(node)
        elif ntype == "class_definition":
            self._visit_class(node)
        else:
            # Generic descent for all other node types
            self._descend(node)

    def _visit_function(self, node: Any) -> None:
        """Visit a function definition, tracking function context."""
        prev = self._current_function
        self._current_function = getattr(node, "name", None)
        self._descend_list(getattr(node, "body", []))
        self._current_function = prev

    def _visit_class(self, node: Any) -> None:
        """Visit a class definition, collecting class-level invariants."""
        class_name = getattr(node, "name", None)
        # Class-level invariants (stored in node.invariants or body)
        for inv in getattr(node, "invariants", []):
            prev = self._current_function
            self._current_function = f"{class_name}.<class>"
            self._add_vc("invariant", inv)
            self._current_function = prev
        # Visit methods
        prev_fn = self._current_function
        for method in getattr(node, "methods", []):
            if hasattr(method, "name"):
                self._current_function = f"{class_name}.{method.name}"
            self._descend_list(getattr(method, "body", []))
        self._current_function = prev_fn

    def _visit_spec_block(self, node: Any) -> None:
        """Visit a spec block, turning each annotation into a VC."""
        for ann in getattr(node, "annotations", []):
            kind = getattr(ann, "kind", "spec")
            # Map spec annotation kinds to VC kinds
            vc_kind_map = {
                "requires": "precondition",
                "ensures": "postcondition",
                "invariant": "invariant",
                "decreases": "termination",
            }
            vc_kind = vc_kind_map.get(kind, "spec")
            label = getattr(ann, "label", None)
            line = getattr(ann, "line_number", 0) or 0
            desc = f"spec:{kind}"
            if label:
                desc = f"{desc} ({label})"
            self._vcs.append(
                VerificationCondition(
                    kind=vc_kind,
                    description=desc,
                    function=self._current_function,
                    line=line,
                    ast_node=ann,
                    status=VerificationStatus.UNVERIFIED,
                )
            )

    def _add_vc(self, kind: str, node: Any) -> None:
        """Create a VC from a contract statement node and append it."""
        line = getattr(node, "line_number", 0) or 0
        # Build a human-readable description from the condition AST
        cond = getattr(node, "condition", None)
        desc = self._describe_condition(cond, kind)
        self._vcs.append(
            VerificationCondition(
                kind=kind,
                description=desc,
                function=self._current_function,
                line=line,
                ast_node=node,
                status=VerificationStatus.UNVERIFIED,
            )
        )

    # ------------------------------------------------------------------
    # Helpers
    # ------------------------------------------------------------------

    def _descend(self, node: Any) -> None:
        """Generic descent: walk all child attributes that are AST nodes."""
        for value in vars(node).values():
            if value is node:
                continue
            if hasattr(value, "node_type"):
                self._walk(value)
            elif isinstance(value, list):
                self._descend_list(value)

    def _descend_list(self, lst: List[Any]) -> None:
        """Walk each element of a list of AST nodes."""
        for item in lst:
            if hasattr(item, "node_type"):
                self._walk(item)

    @staticmethod
    def _describe_condition(node: Any, kind: str) -> str:
        """Build a short text description of the condition for reporting.

        Falls back to the *kind* label when the node cannot be rendered.
        """
        if node is None:
            return kind
        ntype = getattr(node, "node_type", "")

        # Binary comparison / logical nodes
        if ntype in ("binary_op", "comparison", "logical"):
            left = ConstraintCollector._describe_condition(
                getattr(node, "left", None), "?"
            )
            op = getattr(node, "operator", getattr(node, "op", "?"))
            right = ConstraintCollector._describe_condition(
                getattr(node, "right", None), "?"
            )
            return f"{left} {op} {right}"

        # Identifier / literal
        if ntype == "identifier":
            return str(getattr(node, "name", "?"))
        if ntype == "literal":
            return repr(getattr(node, "value", "?"))

        # old(expr)
        if ntype == "old_expression":
            inner = ConstraintCollector._describe_condition(
                getattr(node, "expr", None), "?"
            )
            return f"old({inner})"

        # Function call
        if ntype in ("function_call", "method_call"):
            name = getattr(node, "name", "?")
            return f"{name}(...)"

        # Fallback
        return kind
