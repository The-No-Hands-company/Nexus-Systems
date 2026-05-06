"""
Control Flow Checker
====================

Analyses control flow within functions and loops:
- Missing return statements in typed functions (CF001)
- Infinite loops without reachable break or return (CF002)
- Unreachable code after unconditional jumps (CF003, complements dead_code.py)
"""

from typing import List, Optional
import sys
import os

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../../..'))

from nexuslang.parser.ast import ASTNode
from .base import BaseChecker
from ..report import Issue, Severity, Category


class ControlFlowChecker(BaseChecker):
    """
    Static analysis checker for control-flow issues.

    Error codes
    -----------
    CF001 : Missing return in typed function
        A function that declares a non-void return type contains a code path
        that may reach the end without a return statement.
    CF002 : Potential infinite loop
        A ``loop forever`` construct or ``while true`` loop that contains no
        reachable ``break`` or ``return`` statement.
    CF003 : Unreachable code after unconditional jump
        Code that appears after a ``return``, ``break``, or ``continue``
        inside a block (complementary to dead_code.D001/D002).
    """

    CHECKER_NAME = "control_flow"

    def check(self, ast: ASTNode, source: str = "", lines: List[str] = None) -> List[Issue]:
        """Run all control-flow checks on *ast*."""
        self.issues = []
        self.current_source = source
        self.current_lines = lines or []
        self._check_node(ast)
        return self.issues

    # ------------------------------------------------------------------
    # Recursive AST walker
    # ------------------------------------------------------------------

    def _check_node(self, node: ASTNode, depth: int = 0) -> None:
        """Recursively check *node* and its subtree."""
        if node is None or depth > 100:
            return

        node_type = node.__class__.__name__

        if node_type == "FunctionDefinition":
            self._check_function(node)
            # CF003 inside the function body (list stored in .body)
            self._check_block_jumps(node)
            # Also recurse into nested functions
            self._recurse_children(node, depth + 1)
            return  # _check_function already recurses the body

        if node_type in ("WhileLoop", "InfiniteLoop", "LoopForever"):
            self._check_infinite_loop(node)
            # CF003 inside the loop body
            self._check_block_jumps(node)

        if node_type in ("ForLoop", "ForEachLoop", "ForEach", "RepeatNTimesLoop",
                         "RepeatWhileLoop"):
            # CF003 inside for/for-each/repeat loop bodies
            self._check_block_jumps(node)

        if node_type in ("Block", "FunctionBody", "Program"):
            self._check_block_jumps(node)

        self._recurse_children(node, depth + 1)

    def _recurse_children(self, node: ASTNode, depth: int) -> None:
        """Recurse into child nodes without re-triggering top-level logic."""
        for attr in ("body", "statements", "then_block", "else_block",
                     "then_branch", "else_branch", "else_body", "cases", "finally_body"):
            child = getattr(node, attr, None)
            if child is None:
                continue
            if isinstance(child, list):
                for item in child:
                    if isinstance(item, ASTNode):
                        self._check_node(item, depth)
            elif isinstance(child, ASTNode):
                self._check_node(child, depth)

    # ------------------------------------------------------------------
    # CF001 – Missing return in typed function
    # ------------------------------------------------------------------

    def _check_function(self, node: ASTNode) -> None:
        """Check that typed functions always return a value."""
        return_type = getattr(node, "return_type", None) or getattr(node, "returns", None)
        if return_type is None:
            return

        # Normalise to string for comparison
        if hasattr(return_type, "name"):
            return_type_str = return_type.name
        elif hasattr(return_type, "type_name"):
            return_type_str = return_type.type_name
        else:
            return_type_str = str(return_type)

        # void / nothing / None mean no return required
        if return_type_str.lower() in ("void", "nothing", "none", ""):
            return

        body = getattr(node, "body", None) or getattr(node, "statements", [])
        if not body:
            # Empty body → always missing a return for typed functions
            location = self.get_node_location(node)
            func_name = getattr(node, "name", "<anonymous>")
            self.issues.append(Issue(
                code="CF001",
                severity=Severity.WARNING,
                category=Category.CONTROL_FLOW,
                message=(
                    f"Function '{func_name}' declares return type "
                    f"'{return_type_str}' but has an empty body"
                ),
                location=location,
                source_line=self.get_source_line(location.line),
                suggestion="Add a return statement or change the return type to 'nothing'",
            ))
            return

        if not self._block_always_returns(body):
            location = self.get_node_location(node)
            func_name = getattr(node, "name", "<anonymous>")
            self.issues.append(Issue(
                code="CF001",
                severity=Severity.WARNING,
                category=Category.CONTROL_FLOW,
                message=(
                    f"Function '{func_name}' may not return a value on all "
                    f"code paths (declared return type: '{return_type_str}')"
                ),
                location=location,
                source_line=self.get_source_line(location.line),
                suggestion=(
                    "Ensure every code path ends with a 'return' statement, "
                    "or change the return type to 'nothing'"
                ),
            ))

    def _block_always_returns(self, stmts: list) -> bool:
        """Return True when *stmts* unconditionally ends in a return.

        Checks the last statement(s) conservatively:
        - A ``return``/``break``/``continue`` statement → always returns.
        - An ``if`` with both branches always returning → always returns.
        - Anything else → may not return.
        """
        if not stmts:
            return False

        # Walk to last non-trivial statement (ignore comments/newlines)
        for stmt in reversed(stmts):
            stype = stmt.__class__.__name__
            if stype in ("NewlineStatement", "Comment", "Pass"):
                continue

            if stype == "ReturnStatement":
                return True

            if stype == "IfStatement":
                return self._if_always_returns(stmt)

            # Any other terminal statement → does not guarantee a return
            return False

        return False

    def _if_always_returns(self, node: ASTNode) -> bool:
        """Return True when an if/else block guarantees a return on every path."""
        # Must have an else branch to be exhaustive.
        # IfStatement stores branches in then_block / else_block (plain lists).
        else_stmts = getattr(node, "else_block", None)
        if else_stmts is None:
            # Fall back to other possible attribute names
            else_stmts = getattr(node, "else_branch", None) or getattr(node, "else_body", None)
        if not else_stmts:
            return False

        then_stmts = getattr(node, "then_block", None)
        if then_stmts is None:
            then_stmts = getattr(node, "then_branch", None) or getattr(node, "body", [])
        if isinstance(then_stmts, ASTNode):
            then_stmts = getattr(then_stmts, "statements", [then_stmts])
        if isinstance(else_stmts, ASTNode):
            else_stmts = getattr(else_stmts, "statements", [else_stmts])

        return (self._block_always_returns(then_stmts) and
                self._block_always_returns(else_stmts))

    # ------------------------------------------------------------------
    # CF002 – Potential infinite loop
    # ------------------------------------------------------------------

    def _check_infinite_loop(self, node: ASTNode) -> None:
        """Emit CF002 if the loop has no reachable break or return."""
        node_type = node.__class__.__name__

        is_infinite = False

        if node_type == "InfiniteLoop" or node_type == "LoopForever":
            is_infinite = True
        elif node_type == "WhileLoop":
            condition = getattr(node, "condition", None)
            if condition is not None:
                ctype = condition.__class__.__name__
                # Parser produces Literal(type='boolean', value=True) for "while true"
                if ctype == "Literal" and getattr(condition, "value", None) is True:
                    is_infinite = True
                # Also catch dedicated BooleanLiteral node (future-proof)
                elif ctype == "BooleanLiteral" and getattr(condition, "value", None) is True:
                    is_infinite = True
                # Also catch identifier "true" (legacy parser variants)
                elif ctype == "Identifier" and getattr(condition, "name", "") == "true":
                    is_infinite = True

        if not is_infinite:
            return

        body = getattr(node, "body", None) or getattr(node, "statements", [])
        if self._block_has_escape(body):
            return  # loop can exit

        location = self.get_node_location(node)
        self.issues.append(Issue(
            code="CF002",
            severity=Severity.WARNING,
            category=Category.CONTROL_FLOW,
            message="Potential infinite loop: no reachable 'break' or 'return' found",
            location=location,
            source_line=self.get_source_line(location.line),
            suggestion="Add a 'break' or 'return' statement to allow the loop to exit",
        ))

    def _block_has_escape(self, stmts) -> bool:
        """Return True when *stmts* contains a break or return (shallow scan)."""
        if not stmts:
            return False
        if isinstance(stmts, ASTNode):
            stmts = getattr(stmts, "statements", [stmts])
        for stmt in stmts:
            if stmt is None:
                continue
            stype = stmt.__class__.__name__
            if stype in ("BreakStatement", "Break", "ReturnStatement", "Return"):
                return True
            # Recurse one level into conditional branches (not into nested loops
            # to avoid false negatives from enclosed loop breaks)
            if stype in ("IfStatement", "If"):
                then_b = (getattr(stmt, "then_block", None)
                          or getattr(stmt, "then_branch", None)
                          or getattr(stmt, "body", []))
                else_b = (getattr(stmt, "else_block", None)
                          or getattr(stmt, "else_branch", None)
                          or getattr(stmt, "else_body", []))
                for branch in (then_b, else_b):
                    if self._block_has_escape(branch):
                        return True
        return False

    # ------------------------------------------------------------------
    # CF003 – Unreachable code after jump (duplicate / extension of D001)
    # ------------------------------------------------------------------

    def _check_block_jumps(self, node: ASTNode) -> None:
        """Emit CF003 for any statement following an unconditional jump.

        This mirrors DeadCodeChecker.D001/D002 but emits a CF-prefixed code to
        distinguish control-flow analysis results from dead-code analysis.
        """
        stmts = getattr(node, "statements", None) or getattr(node, "body", [])
        if not stmts:
            return

        found_jump = False
        for stmt in stmts:
            if stmt is None:
                continue
            stype = stmt.__class__.__name__
            if found_jump:
                location = self.get_node_location(stmt)
                self.issues.append(Issue(
                    code="CF003",
                    severity=Severity.WARNING,
                    category=Category.CONTROL_FLOW,
                    message="Unreachable code: statement follows an unconditional jump",
                    location=location,
                    source_line=self.get_source_line(location.line),
                    suggestion="Remove the unreachable code or restructure the control flow",
                ))
                break  # report only the first unreachable statement per block
            if stype in ("ReturnStatement", "Return",
                         "BreakStatement", "Break",
                         "ContinueStatement", "Continue"):
                found_jump = True
