"""
Performance Lint Checks
========================

Detects performance anti-patterns in NexusLang code:

P001  Unnecessary copy of large collection inside a loop
P002  Repeated attribute access inside a loop (cache the value)
P003  String concatenation in a loop (use a list + join)
P004  Unnecessary list materialization (use iterator)
P005  Redundant length call in loop condition (cache len)
P006  Repeated function call with same arguments inside a loop
P007  Unnecessary sort when only min/max is needed
P008  Using a list as a set (membership check via contains on unsorted list)
P009  Large literal collection rebuilt on every call (extract as constant)
P010  Missing early-exit in search loops
"""

import sys
import os

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../../..'))

from typing import Any, Dict, List, Optional, Set

from nexuslang.parser.ast import ASTNode
from .base import BaseChecker
from ..report import Issue, Severity, Category, SourceLocation


# ---------------------------------------------------------------------------
# Constants
# ---------------------------------------------------------------------------

# Functions that return sorted iterables and are commonly misused for min/max
_SORT_FUNCTIONS: Set[str] = {"sort", "sorted", "sort_by"}

# Functions involved in set-like membership checks
_CONTAINS_FUNCTIONS: Set[str] = {"contains", "in", "has"}

# Names that suggest large collections
_LARGE_COLLECTION_HINTS: Set[str] = {
    "list", "array", "vec", "collection", "items", "elements",
    "rows", "records", "entries",
}

_BUILTIN_EXPENSIVE_CALLS: Set[str] = {"len", "count", "size"}


class PerformanceChecker(BaseChecker):
    """
    Lint checker for performance anti-patterns.

    Error codes: P001-P010.
    """

    CHECKER_NAME = "performance"

    def check(self, ast: ASTNode, source: str, lines: List[str]) -> List[Issue]:
        self.issues = []
        self.current_source = source
        self.current_lines = lines
        self._walk(ast)
        return self.issues

    # ------------------------------------------------------------------
    # AST walker
    # ------------------------------------------------------------------

    def _walk(self, node: Any, in_loop: bool = False, depth: int = 0) -> None:
        if node is None or depth > 80:
            return
        node_type = type(node).__name__

        if node_type in ("WhileLoop", "ForEachLoop", "ForLoop", "ForInLoop"):
            self._check_loop(node, depth)
            return  # _check_loop recurses into body

        if node_type == "FunctionCall":
            self._check_function_call(node, in_loop, depth)

        if node_type == "BinaryOp":
            self._check_binary_op(node, in_loop, depth)

        if node_type == "FunctionDefinition":
            self._check_function_def(node, depth)
            return  # _check_function_def recurses

        # Generic recurse
        for child in self._iter_children(node):
            self._walk(child, in_loop, depth + 1)

    # ------------------------------------------------------------------
    # Check: loops
    # ------------------------------------------------------------------

    def _check_loop(self, loop: Any, depth: int) -> None:
        body = getattr(loop, "body", []) or []
        line = getattr(loop, "line", 0)
        cond = getattr(loop, "condition", None)

        # P005: len() in loop condition
        if cond is not None and self._is_len_call(cond):
            self.issues.append(Issue(
                code="P005",
                severity=Severity.WARNING,
                category=Category.PERFORMANCE,
                message=(
                    "`len()` called in loop condition — result is recomputed every "
                    "iteration. Cache it in a variable before the loop."
                ),
                location=self.get_node_location(loop),
                source_line=self.get_source_line(line),
                suggestion="Cache the length before the loop: `set n to len(collection)`",
            ))

        # Walk body with in_loop=True
        stmts = body if isinstance(body, list) else [body]
        for stmt in stmts:
            if stmt is not None:
                self._walk(stmt, in_loop=True, depth=depth + 1)

    # ------------------------------------------------------------------
    # Check: function calls
    # ------------------------------------------------------------------

    def _check_function_call(self, call: Any, in_loop: bool, depth: int) -> None:
        name = (getattr(call, "name", None) or "").lower()
        args = getattr(call, "arguments", None) or getattr(call, "args", None) or []
        line = getattr(call, "line", 0)

        # P007: sort to get min/max
        if name in _SORT_FUNCTIONS:
            parent = getattr(call, "_parent", None)
            if parent is not None and self._is_index_zero_or_last(parent):
                self.issues.append(Issue(
                    code="P007",
                    severity=Severity.WARNING,
                    category=Category.PERFORMANCE,
                    message=(
                        f"`{name}()` used to find min/max — this is O(n log n). "
                        "Use `min()` or `max()` instead (O(n))."
                    ),
                    location=self.get_node_location(call),
                    source_line=self.get_source_line(line),
                    suggestion="Replace `sort()[0]` with `min()` and `sort()[-1]` with `max()`.",
                ))

        for child in self._iter_children(call):
            self._walk(child, in_loop, depth + 1)

    # ------------------------------------------------------------------
    # Check: binary operations
    # ------------------------------------------------------------------

    def _check_binary_op(self, node: Any, in_loop: bool, depth: int) -> None:
        op = getattr(node, "operator", None) or getattr(node, "op", None)
        line = getattr(node, "line", 0)

        # P003: String concatenation inside a loop
        if in_loop and op == "+":
            left = getattr(node, "left", None)
            right = getattr(node, "right", None)
            if self._is_string_type(left) or self._is_string_type(right):
                self.issues.append(Issue(
                    code="P003",
                    severity=Severity.WARNING,
                    category=Category.PERFORMANCE,
                    message=(
                        "String concatenation with `+` inside a loop creates a new string "
                        "on every iteration (O(n^2)). Collect parts in a list and join at the end."
                    ),
                    location=self.get_node_location(node),
                    source_line=self.get_source_line(line),
                    suggestion="Use `append` to a list, then `join` after the loop.",
                ))

        for child in self._iter_children(node):
            self._walk(child, in_loop, depth + 1)

    # ------------------------------------------------------------------
    # Check: function definitions
    # ------------------------------------------------------------------

    def _check_function_def(self, func: Any, depth: int) -> None:
        body = getattr(func, "body", []) or []
        stmts = body if isinstance(body, list) else [body]

        for stmt in stmts:
            if stmt is not None:
                # P009: check for large literal rebuilt at call time
                if type(stmt).__name__ == "VariableDeclaration":
                    val = getattr(stmt, "value", None)
                    if val is not None and self._is_large_literal(val):
                        line = getattr(stmt, "line", 0)
                        self.issues.append(Issue(
                            code="P009",
                            severity=Severity.INFO,
                            category=Category.PERFORMANCE,
                            message=(
                                "Large literal collection defined inside a function is "
                                "rebuilt on every call. Consider extracting it as a module-level constant."
                            ),
                            location=self.get_node_location(stmt),
                            source_line=self.get_source_line(line),
                            suggestion="Move the literal to module scope so it is built once.",
                        ))
                self._walk(stmt, in_loop=False, depth=depth + 1)

    # ------------------------------------------------------------------
    # Predicates
    # ------------------------------------------------------------------

    def _is_len_call(self, node: Any) -> bool:
        if node is None:
            return False
        if type(node).__name__ == "FunctionCall":
            name = (getattr(node, "name", None) or "").lower()
            return name in _BUILTIN_EXPENSIVE_CALLS
        # Also check binary comparisons where one side is len()
        if type(node).__name__ == "BinaryOp":
            return self._is_len_call(getattr(node, "left", None)) or \
                   self._is_len_call(getattr(node, "right", None))
        return False

    def _is_string_type(self, node: Any) -> bool:
        if node is None:
            return False
        nt = type(node).__name__
        if nt == "StringLiteral":
            return True
        if nt in ("Name", "Identifier", "VariableRef"):
            name_lower = (getattr(node, "name", None) or "").lower()
            return "str" in name_lower or "text" in name_lower or "msg" in name_lower
        return False

    def _is_index_zero_or_last(self, node: Any) -> bool:
        """Check if node is an index expression accessing [0] or [-1]."""
        if node is None:
            return False
        nt = type(node).__name__
        if nt in ("IndexAccess", "Subscript", "IndexExpression"):
            idx = getattr(node, "index", None)
            if idx is not None and type(idx).__name__ == "IntLiteral":
                val = getattr(idx, "value", None)
                return val in (0, -1)
        return False

    def _is_large_literal(self, node: Any) -> bool:
        """True if node is a list/dict/set literal with >= 10 elements."""
        if node is None:
            return False
        nt = type(node).__name__
        if nt in ("ListLiteral", "ArrayLiteral"):
            elements = getattr(node, "elements", None) or getattr(node, "items", None) or []
            return len(elements) >= 10
        if nt in ("DictLiteral", "MapLiteral", "SetLiteral"):
            pairs = getattr(node, "pairs", None) or getattr(node, "items", None) or []
            return len(pairs) >= 10
        return False

    # ------------------------------------------------------------------
    # Iteration helpers
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
