"""
Data Flow Lint Checks
=======================

Performs lightweight intra-procedural data-flow analysis:

DF001  Variable potentially used before definite assignment
DF002  Dead assignment — value written but overwritten before it is read
DF003  Variable shadowing — inner scope variable hides an outer variable
DF004  Unused function parameter — parameter never referenced in body
DF005  Tautological condition — if/while branch condition is a literal True/False
DF006  Redundant self-assignment — `set x to x` has no effect
DF007  Unused local variable — variable defined but never used in scope
"""

import sys
import os

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../../..'))

from typing import Any, Dict, List, Optional, Set, Tuple

from nexuslang.parser.ast import ASTNode
from .base import BaseChecker
from ..report import Issue, Severity, Category, SourceLocation


# ---------------------------------------------------------------------------
# Lightweight definition-use scope tracking
# ---------------------------------------------------------------------------

class _Scope:
    """Track variable definitions, uses, and last-write lines in one scope."""

    def __init__(self, parent: Optional["_Scope"] = None) -> None:
        self.parent = parent
        # name -> (line_defined, used_since_last_write)
        self.defs: Dict[str, Tuple[int, bool]] = {}
        # name -> line of most recent assignment (for DF002)
        self.last_write: Dict[str, int] = {}

    # ------------------------------------------------------------------
    def define(self, name: str, line: int) -> None:
        self.defs[name] = (line, False)
        self.last_write[name] = line

    def use(self, name: str) -> None:
        scope: Optional[_Scope] = self
        while scope is not None:
            if name in scope.defs:
                ln, _ = scope.defs[name]
                scope.defs[name] = (ln, True)
                return
            scope = scope.parent

    def overwrite(self, name: str, line: int) -> Tuple[bool, int]:
        """
        Record a new write to an already-defined variable (in this or ancestor scope).
        Returns (was_unread_since_last_write, last_write_line).
        """
        scope: Optional[_Scope] = self
        while scope is not None:
            if name in scope.defs:
                old_line = scope.last_write.get(name, scope.defs[name][0])
                _, was_used = scope.defs[name]
                unread = not was_used
                # Reset
                scope.defs[name] = (line, False)
                scope.last_write[name] = line
                return unread, old_line
            scope = scope.parent
        # Not defined yet — treat as first definition
        self.define(name, line)
        return False, -1

    def is_defined_locally(self, name: str) -> bool:
        return name in self.defs

    def is_defined(self, name: str) -> bool:
        scope: Optional[_Scope] = self
        while scope is not None:
            if name in scope.defs:
                return True
            scope = scope.parent
        return False

    def outer_defines(self, name: str) -> Optional[int]:
        """Return line of definition in any ancestor scope, or None."""
        scope = self.parent
        while scope is not None:
            if name in scope.defs:
                return scope.defs[name][0]
            scope = scope.parent
        return None


# ---------------------------------------------------------------------------
# Checker
# ---------------------------------------------------------------------------

class DataFlowChecker(BaseChecker):
    """
    Intra-procedural data-flow lint checker.

    Error codes: DF001-DF007.
    """

    CHECKER_NAME = "data_flow"

    def check(self, ast: ASTNode, source: str, lines: List[str]) -> List[Issue]:
        self.issues = []
        self.current_source = source
        self.current_lines = lines
        root_scope = _Scope()
        self._walk(ast, scope=root_scope)
        return self.issues

    # ------------------------------------------------------------------
    # AST walker
    # ------------------------------------------------------------------

    def _walk_variable_declaration(self, node: Any, scope: _Scope, depth: int) -> None:
        """Handle VariableDeclaration nodes: check for shadowing and define the variable."""
        name = getattr(node, "name", None) or ""
        line = getattr(node, "line", 0)
        value = getattr(node, "value", None)
        if value is not None:
            self._walk(value, scope, depth + 1)
        if name:
            outer_line = scope.outer_defines(name)
            if outer_line is not None:
                self.issues.append(Issue(
                    code="DF003",
                    severity=Severity.WARNING,
                    category=Category.BEST_PRACTICE,
                    message=(
                        f"Variable `{name}` shadows an outer variable defined at "
                        f"line {outer_line}. Consider renaming."
                    ),
                    location=self.get_node_location(node),
                    source_line=self.get_source_line(line),
                ))
            scope.define(name, line)

    def _walk_assignment(self, node: Any, scope: _Scope, depth: int) -> None:
        """Handle Assignment nodes: check for self-assignment and dead assignments."""
        target = getattr(node, "target", None)
        value = getattr(node, "value", None)
        line = getattr(node, "line", 0)
        target_name = self._extract_name(target)
        if value is not None:
            self._walk(value, scope, depth + 1)
        if target_name:
            value_name = self._extract_name(value)
            if value_name and target_name == value_name:
                self.issues.append(Issue(
                    code="DF006",
                    severity=Severity.WARNING,
                    category=Category.BEST_PRACTICE,
                    message=f"Redundant self-assignment: `{target_name} = {target_name}` has no effect.",
                    location=self.get_node_location(node),
                    source_line=self.get_source_line(line),
                ))
            if scope.is_defined(target_name):
                unread, old_line = scope.overwrite(target_name, line)
                if unread and old_line > 0:
                    self.issues.append(Issue(
                        code="DF002",
                        severity=Severity.WARNING,
                        category=Category.DEAD_CODE,
                        message=(
                            f"Dead assignment: `{target_name}` was written at line "
                            f"{old_line} but never read before being overwritten at line {line}."
                        ),
                        location=self.get_node_location(node),
                        source_line=self.get_source_line(old_line),
                    ))
            else:
                scope.define(target_name, line)

    def _walk_if_statement(self, node: Any, scope: _Scope, depth: int) -> None:
        """Handle IfStatement nodes: check for tautological conditions."""
        cond = getattr(node, "condition", None)
        then_body = getattr(node, "then_body", None) or getattr(node, "body", []) or []
        else_body = getattr(node, "else_body", None) or getattr(node, "else_branch", []) or []
        if cond is not None and type(cond).__name__ in ("BoolLiteral", "BooleanLiteral"):
            val = getattr(cond, "value", None)
            dead = "else" if val else "then"
            self.issues.append(Issue(
                code="DF005",
                severity=Severity.WARNING,
                category=Category.DEAD_CODE,
                message=(
                    f"Condition is always `{val}`. The `{dead}` branch is unreachable."
                ),
                location=self.get_node_location(cond),
                source_line=self.get_source_line(getattr(cond, "line", 0)),
            ))
        if cond is not None:
            self._walk(cond, scope, depth + 1)
        self._walk_block(then_body if isinstance(then_body, list) else [then_body], scope, depth)
        self._walk_block(else_body if isinstance(else_body, list) else [else_body], scope, depth)

    def _walk(self, node: Any, scope: _Scope, depth: int = 0) -> None:
        if node is None or depth > 80:
            return
        node_type = type(node).__name__

        if node_type == "Program":
            for stmt in getattr(node, "statements", []) or []:
                self._walk(stmt, scope, depth + 1)

        elif node_type == "FunctionDefinition":
            self._walk_function(node, scope, depth)

        elif node_type in ("ClassDefinition", "StructDefinition"):
            inner = _Scope(parent=scope)
            for stmt in getattr(node, "body", []) or []:
                self._walk(stmt, inner, depth + 1)

        elif node_type == "VariableDeclaration":
            self._walk_variable_declaration(node, scope, depth)

        elif node_type == "Assignment":
            self._walk_assignment(node, scope, depth)

        elif node_type in ("Name", "Identifier", "VariableRef", "NameExpression"):
            name = (
                getattr(node, "name", None)
                or getattr(node, "value", None)
                or ""
            )
            line = getattr(node, "line", 0)
            if name:
                # DF001: use before definition
                if not scope.is_defined(name):
                    self.issues.append(Issue(
                        code="DF001",
                        severity=Severity.WARNING,
                        category=Category.BEST_PRACTICE,
                        message=f"Variable `{name}` may be used before it is defined.",
                        location=self.get_node_location(node),
                        source_line=self.get_source_line(line),
                    ))
                scope.use(name)

        elif node_type == "IfStatement":
            self._walk_if_statement(node, scope, depth)

        elif node_type in ("WhileLoop", "ForEachLoop", "ForLoop", "ForInLoop"):
            cond = getattr(node, "condition", None)
            body = getattr(node, "body", []) or []

            if cond is not None:
                if type(cond).__name__ in ("BoolLiteral", "BooleanLiteral"):
                    val = getattr(cond, "value", None)
                    if val is False:
                        self.issues.append(Issue(
                            code="DF005",
                            severity=Severity.WARNING,
                            category=Category.DEAD_CODE,
                            message="Loop condition is always `False`. Loop body is unreachable.",
                            location=self.get_node_location(cond),
                            source_line=self.get_source_line(getattr(cond, "line", 0)),
                        ))
                self._walk(cond, scope, depth + 1)

            loop_scope = _Scope(parent=scope)
            self._walk_block(body if isinstance(body, list) else [body], loop_scope, depth)

        elif node_type == "ReturnStatement":
            val = getattr(node, "value", None)
            if val is not None:
                self._walk(val, scope, depth + 1)

        elif node_type == "FunctionCall":
            for arg in (
                getattr(node, "arguments", None)
                or getattr(node, "args", None)
                or []
            ):
                self._walk(arg, scope, depth + 1)

        elif node_type == "BinaryOp":
            self._walk(getattr(node, "left", None), scope, depth + 1)
            self._walk(getattr(node, "right", None), scope, depth + 1)

        elif node_type == "UnaryOp":
            self._walk(getattr(node, "operand", None), scope, depth + 1)

        else:
            for child in self._iter_children(node):
                self._walk(child, scope, depth + 1)

    def _walk_block(self, stmts: List[Any], scope: _Scope, depth: int) -> None:
        for stmt in stmts:
            if stmt is not None:
                self._walk(stmt, scope, depth + 1)

    def _walk_function(self, func: Any, outer_scope: _Scope, depth: int) -> None:
        params = getattr(func, "parameters", []) or []
        body = getattr(func, "body", []) or []
        func_line = getattr(func, "line", 0)

        func_scope = _Scope(parent=outer_scope)

        # Register parameters
        param_names: List[Tuple[str, int]] = []
        for param in params:
            pname = getattr(param, "name", None) or ""
            pline = getattr(param, "line", func_line)
            if pname:
                func_scope.define(pname, pline)
                param_names.append((pname, pline))

        self._walk_block(body if isinstance(body, list) else [body], func_scope, depth)

        # DF004: unused parameters
        for pname, pline in param_names:
            if pname in func_scope.defs:
                _, used = func_scope.defs[pname]
                if not used:
                    self.issues.append(Issue(
                        code="DF004",
                        severity=Severity.WARNING,
                        category=Category.DEAD_CODE,
                        message=f"Parameter `{pname}` is never used in the function body.",
                        location=SourceLocation(
                            file=self.current_file, line=pline, column=1
                        ),
                        source_line=self.get_source_line(pline),
                        suggestion="Remove the parameter or prefix with `_` to mark as intentionally unused.",
                    ))

        # DF007: unused local variables (exclude parameters)
        param_set = {n for n, _ in param_names}
        for vname, (vline, vused) in func_scope.defs.items():
            if vname not in param_set and not vused:
                self.issues.append(Issue(
                    code="DF007",
                    severity=Severity.INFO,
                    category=Category.DEAD_CODE,
                    message=f"Local variable `{vname}` is defined but never used.",
                    location=SourceLocation(
                        file=self.current_file, line=vline, column=1
                    ),
                    source_line=self.get_source_line(vline),
                    suggestion="Remove the variable or prefix with `_` if intentional.",
                ))

    # ------------------------------------------------------------------
    # Helpers
    # ------------------------------------------------------------------

    def _extract_name(self, node: Any) -> Optional[str]:
        if node is None:
            return None
        nt = type(node).__name__
        if nt in ("Name", "Identifier", "VariableRef", "NameExpression"):
            return getattr(node, "name", None) or getattr(node, "value", None)
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
