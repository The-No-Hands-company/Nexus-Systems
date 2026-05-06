"""Compile-time borrow checker for NexusLang.

This pass walks the AST *before* interpretation and enforces ownership rules
statically, so errors are reported at analysis time rather than at runtime.

Rules enforced
--------------
1. Use-after-move       -- accessing a variable after it has been moved is an error.
2. Move-while-borrowed  -- moving a variable that has active borrows is an error.
3. Double-mutable-borrow -- taking a second mutable borrow while one is active.
4. Immutable-borrow-while-mutably-borrowed -- taking any borrow while a mutable
   borrow is active.
5. Mutate-while-borrowed -- assigning a variable (VariableDeclaration / set) while
   it is borrowed (immutably or mutably).

Control-flow merging
--------------------
For branches (if/else) the checker runs both branches from the same pre-branch
state and then conservatively merges: a variable is considered *moved* after the
branch if it was moved in *either* sub-branch.  This prevents use-after-maybe-move
surprises while keeping the checker decidable.

For loops (while/for) the body is checked starting from the pre-loop state.  The
checker does not attempt fixed-point analysis; any move inside a loop body is
flagged as if it executes once (which may produce false positives for loops that
only execute once, but that is the safe conservative choice).

Lifetime information (if present on BorrowExpression / Parameter nodes via the
LifetimeAnnotation attribute) is validated by the separate LifetimeChecker pass.
"""

from __future__ import annotations

import copy
from dataclasses import dataclass, field
from typing import Dict, List, Optional, Set, Tuple

from ..parser import ast as _ast


# ---------------------------------------------------------------------------
# Error dataclass
# ---------------------------------------------------------------------------

@dataclass
class BorrowError:
    """A compile-time borrow / ownership violation."""
    message: str
    line: Optional[int] = None
    var_name: Optional[str] = None

    def __str__(self) -> str:
        loc = f" (line {self.line})" if self.line else ""
        return f"BorrowError{loc}: {self.message}"


# ---------------------------------------------------------------------------
# Per-variable borrow state
# ---------------------------------------------------------------------------

@dataclass
class VarBorrowState:
    """Borrow state for a single variable."""
    immutable_count: int = 0
    is_mutable: bool = False
    is_moved: bool = False

    def copy(self) -> "VarBorrowState":
        return VarBorrowState(
            immutable_count=self.immutable_count,
            is_mutable=self.is_mutable,
            is_moved=self.is_moved,
        )

    def is_borrowed(self) -> bool:
        return self.immutable_count > 0 or self.is_mutable


# ---------------------------------------------------------------------------
# Scope stack helper
# ---------------------------------------------------------------------------

class BorrowScope:
    """A stack of variable-state dictionaries (innermost last)."""

    def __init__(self) -> None:
        self._stack: List[Dict[str, VarBorrowState]] = [{}]  # global scope

    # -- scope management --

    def push(self) -> None:
        self._stack.append({})

    def pop(self) -> None:
        if len(self._stack) > 1:
            self._stack.pop()

    # -- state access --

    def get(self, name: str) -> Optional[VarBorrowState]:
        for scope in reversed(self._stack):
            if name in scope:
                return scope[name]
        return None

    def set(self, name: str, state: VarBorrowState) -> None:
        # Update the innermost scope that already has this name; else add to current.
        for scope in reversed(self._stack):
            if name in scope:
                scope[name] = state
                return
        self._stack[-1][name] = state

    def define(self, name: str, state: Optional[VarBorrowState] = None) -> None:
        """Introduce a new variable in the current (innermost) scope."""
        self._stack[-1][name] = state or VarBorrowState()

    def all_names(self) -> Set[str]:
        names: Set[str] = set()
        for scope in self._stack:
            names.update(scope.keys())
        return names

    # -- snapshot / restore for branch analysis --

    def snapshot(self) -> List[Dict[str, VarBorrowState]]:
        return [dict(scope) for scope in self._stack]

    def restore(self, snap: List[Dict[str, VarBorrowState]]) -> None:
        self._stack = [dict(scope) for scope in snap]

    def merge_moved_from(self, other_snap: List[Dict[str, VarBorrowState]]) -> None:
        """After a branch encoded in other_snap, mark any var moved in other_snap
        as moved in self (conservative: union of moved sets)."""
        # Build flat view from other_snap
        other_flat: Dict[str, VarBorrowState] = {}
        for scope in other_snap:
            other_flat.update(scope)
        for name, other_state in other_flat.items():
            if other_state.is_moved:
                self_state = self.get(name)
                if self_state is not None:
                    self_state.is_moved = True
                    self.set(name, self_state)


# ---------------------------------------------------------------------------
# Main checker
# ---------------------------------------------------------------------------

class BorrowChecker:
    """Static compile-time borrow and ownership checker.

    Usage::

        checker = BorrowChecker()
        errors = checker.check(ast_program)
        for e in errors:
            print(e)
    """

    def __init__(self) -> None:
        self._errors: List[BorrowError] = []
        self._scope = BorrowScope()
        # Track which function we are currently inside (for return analysis).
        self._current_function: Optional[str] = None

    # ------------------------------------------------------------------
    # Public entry point
    # ------------------------------------------------------------------

    def check(self, program: _ast.Program) -> List[BorrowError]:
        """Check a parsed program and return a list of borrow errors."""
        self._errors = []
        self._scope = BorrowScope()
        self._current_function = None
        self._check_statements(program.statements)
        return list(self._errors)

    # ------------------------------------------------------------------
    # Internal helpers
    # ------------------------------------------------------------------

    def _err(self, msg: str, line: Optional[int], var: Optional[str] = None) -> None:
        self._errors.append(BorrowError(message=msg, line=line, var_name=var))

    def _line(self, node) -> Optional[int]:
        return getattr(node, 'line_number', None) or getattr(node, 'line', None)

    # ------------------------------------------------------------------
    # Statement dispatch
    # ------------------------------------------------------------------

    def _check_statements(self, stmts: list) -> None:
        for stmt in (stmts or []):
            self._check_node(stmt)

    def _check_node(self, node) -> None:  # noqa: C901  (complex by necessity)
        if node is None:
            return
        cls_name = type(node).__name__

        handler = getattr(self, f"_check_{cls_name}", None)
        if handler is not None:
            handler(node)
        else:
            # Generic fallback: recurse into known child attributes.
            self._check_generic(node)

    def _check_generic(self, node) -> None:
        """Recursively check any child nodes we don't have a specific handler for."""
        for attr in ("statements", "body", "then_block", "else_block",
                     "condition", "value", "expression", "left", "right",
                     "arguments", "iterable", "iterator"):
            child = getattr(node, attr, None)
            if child is None:
                continue
            if isinstance(child, list):
                self._check_statements(child)
            else:
                self._check_node(child)

    # ------------------------------------------------------------------
    # Statement handlers
    # ------------------------------------------------------------------

    def _check_Program(self, node: _ast.Program) -> None:
        self._check_statements(node.statements)

    def _check_VariableDeclaration(self, node: _ast.VariableDeclaration) -> None:
        # Check the RHS expression first (might contain a move/borrow).
        self._check_node(node.value)
        # If an existing variable with this name is currently borrowed, assigning
        # to it (shadowing / reassigning) is an error -- the borrow contract would
        # be violated just like in the runtime set_variable() check.
        name = node.name
        line = self._line(node)
        existing = self._scope.get(name)
        if existing is not None and existing.is_borrowed():
            kind = "mutably" if existing.is_mutable else "immutably"
            self._err(
                f"cannot assign to '{name}': it is currently borrowed {kind}; "
                f"drop the borrow first",
                line, name,
            )
        # If RHS is a MoveExpression, the source is already invalidated by
        # _check_MoveExpression which ran above.  We now define the LHS as owned.
        self._scope.define(name, VarBorrowState())

    def _check_FunctionDefinition(self, node: _ast.FunctionDefinition) -> None:
        prev_fn = self._current_function
        self._current_function = node.name
        self._scope.push()
        # Register parameters as owned variables.
        for param in (node.parameters or []):
            self._scope.define(param.name, VarBorrowState())
        self._check_statements(node.body)
        self._scope.pop()
        self._current_function = prev_fn

    def _check_AsyncFunctionDefinition(self, node) -> None:
        self._check_FunctionDefinition(node)

    def _check_IfStatement(self, node: _ast.IfStatement) -> None:
        # Check condition expression.
        self._check_node(node.condition)

        # Save state before branching.
        pre_snap = self._scope.snapshot()

        # Check then-branch.
        self._check_statements(node.then_block if isinstance(node.then_block, list)
                                else [node.then_block])
        then_snap = self._scope.snapshot()

        # Restore to pre-branch state and check else-branch.
        self._scope.restore(pre_snap)
        else_block = node.else_block
        if else_block is not None:
            stmts = else_block if isinstance(else_block, list) else [else_block]
            self._check_statements(stmts)
        else_snap = self._scope.snapshot()

        # Merge: variables moved in either branch are considered moved.
        # Start from the else_snap (which already contains else state) and
        # union-in moves from the then_snap.
        self._scope.restore(else_snap)
        self._scope.merge_moved_from(then_snap)

    def _check_WhileLoop(self, node: _ast.WhileLoop) -> None:
        self._check_node(node.condition)
        # Check the body once (conservative: assume it may execute).
        self._scope.push()
        self._check_statements(node.body)
        self._scope.pop()
        # else_body executes when loop finishes normally.
        if node.else_body:
            self._scope.push()
            self._check_statements(node.else_body)
            self._scope.pop()

    def _check_ForLoop(self, node: _ast.ForLoop) -> None:
        if node.iterable is not None:
            self._check_node(node.iterable)
        for expr in (node.start, node.end, node.step):
            if expr is not None:
                self._check_node(expr)
        self._scope.push()
        # The loop variable is owned inside the body.
        if node.iterator:
            self._scope.define(node.iterator, VarBorrowState())
        self._check_statements(node.body)
        self._scope.pop()
        if node.else_body:
            self._scope.push()
            self._check_statements(node.else_body)
            self._scope.pop()

    def _check_ReturnStatement(self, node) -> None:
        if hasattr(node, 'value') and node.value is not None:
            self._check_node(node.value)

    # ------------------------------------------------------------------
    # Ownership / borrow expression handlers
    # ------------------------------------------------------------------

    def _check_MoveExpression(self, node: _ast.MoveExpression) -> None:
        name = node.var_name
        line = self._line(node)
        state = self._scope.get(name)
        if state is None:
            # Unknown variable - name error handled elsewhere.
            return

        if state.is_moved:
            self._err(
                f"use of moved value: '{name}' was already moved and cannot be moved again",
                line, name,
            )
            return

        if state.is_borrowed():
            kind = "mutably" if state.is_mutable else "immutably"
            self._err(
                f"cannot move '{name}': it is currently borrowed {kind}; "
                f"drop the borrow before moving",
                line, name,
            )
            return

        # Mark source as moved.
        new_state = state.copy()
        new_state.is_moved = True
        self._scope.set(name, new_state)

    def _check_BorrowExpression(self, node: _ast.BorrowExpression) -> None:
        name = node.var_name
        mutable = node.mutable
        line = self._line(node)
        state = self._scope.get(name)
        if state is None:
            return

        if state.is_moved:
            self._err(
                f"cannot borrow '{name}': it has been moved",
                line, name,
            )
            return

        if mutable:
            if state.immutable_count > 0:
                self._err(
                    f"cannot borrow '{name}' as mutable: it is already borrowed "
                    f"immutably ({state.immutable_count} active borrow(s))",
                    line, name,
                )
                return
            if state.is_mutable:
                self._err(
                    f"cannot borrow '{name}' as mutable: already mutably borrowed",
                    line, name,
                )
                return
            new_state = state.copy()
            new_state.is_mutable = True
            self._scope.set(name, new_state)
        else:
            if state.is_mutable:
                self._err(
                    f"cannot borrow '{name}' immutably: it is already mutably borrowed",
                    line, name,
                )
                return
            new_state = state.copy()
            new_state.immutable_count += 1
            self._scope.set(name, new_state)

    def _check_DropBorrowStatement(self, node: _ast.DropBorrowStatement) -> None:
        name = node.var_name
        mutable = node.mutable
        line = self._line(node)
        state = self._scope.get(name)

        if mutable:
            if state is None or not state.is_mutable:
                self._err(
                    f"no active mutable borrow of '{name}' to drop",
                    line, name,
                )
                return
            new_state = state.copy()
            new_state.is_mutable = False
            self._scope.set(name, new_state)
        else:
            if state is None or state.immutable_count <= 0:
                self._err(
                    f"no active immutable borrow of '{name}' to drop",
                    line, name,
                )
                return
            new_state = state.copy()
            new_state.immutable_count = max(0, state.immutable_count - 1)
            self._scope.set(name, new_state)

    # ------------------------------------------------------------------
    # Identifier / assignment -- detect use-after-move and write-while-borrowed
    # ------------------------------------------------------------------

    def _check_Identifier(self, node) -> None:
        name = node.name
        line = self._line(node)
        state = self._scope.get(name)
        if state is not None and state.is_moved:
            self._err(
                f"use of moved value: '{name}' was moved and cannot be used",
                line, name,
            )

    def _check_VariableAssignment(self, node) -> None:
        # Generic assignment: set x to <expr>
        name = getattr(node, 'name', None)
        if name:
            line = self._line(node)
            state = self._scope.get(name)
            if state is not None and state.is_borrowed():
                kind = "mutably" if state.is_mutable else "immutably"
                self._err(
                    f"cannot assign to '{name}': it is currently borrowed {kind}; "
                    f"drop the borrow first",
                    line, name,
                )
        # Check the RHS value as well.
        if hasattr(node, 'value'):
            self._check_node(node.value)

    # Some AST designs use AssignmentStatement or Assignment
    _check_AssignmentStatement = _check_VariableAssignment
    _check_Assignment = _check_VariableAssignment

    # ------------------------------------------------------------------
    # Expression handlers that recurse into sub-expressions
    # ------------------------------------------------------------------

    def _check_BinaryOperation(self, node) -> None:
        self._check_node(node.left)
        self._check_node(node.right)

    def _check_UnaryOperation(self, node) -> None:
        self._check_node(getattr(node, 'operand', None) or getattr(node, 'expression', None))

    def _check_FunctionCall(self, node) -> None:
        for arg in (node.arguments or []):
            self._check_node(arg)

    def _check_MethodCall(self, node) -> None:
        self._check_node(getattr(node, 'object', None))
        for arg in getattr(node, 'arguments', []) or []:
            self._check_node(arg)

    def _check_IndexExpression(self, node) -> None:
        self._check_node(getattr(node, 'object', None))
        self._check_node(getattr(node, 'index', None))

    def _check_MemberAccess(self, node) -> None:
        self._check_node(getattr(node, 'object', None))

    def _check_ListLiteral(self, node) -> None:
        for elem in (node.elements or []):
            self._check_node(elem)

    def _check_DictLiteral(self, node) -> None:
        for k, v in (node.pairs or []):
            self._check_node(k)
            self._check_node(v)

    def _check_ClassDefinition(self, node) -> None:
        self._scope.push()
        for stmt in (node.methods or []):
            self._check_node(stmt)
        self._scope.pop()

    def _check_MatchExpression(self, node) -> None:
        self._check_node(node.expression)
        for case in (node.cases or []):
            pre = self._scope.snapshot()
            # Bind pattern variables in the case body scope.
            self._scope.push()
            if case.guard is not None:
                self._check_node(case.guard)
            self._check_statements(case.body)
            self._scope.pop()
            case_snap = self._scope.snapshot()
            self._scope.restore(pre)
            self._scope.merge_moved_from(case_snap)

    def _check_TryCatchStatement(self, node) -> None:
        pre = self._scope.snapshot()
        self._scope.push()
        self._check_statements(getattr(node, 'try_block', None) or [])
        self._scope.pop()
        try_snap = self._scope.snapshot()

        self._scope.restore(pre)
        self._scope.push()
        self._check_statements(getattr(node, 'catch_block', None) or [])
        self._scope.pop()
        catch_snap = self._scope.snapshot()

        self._scope.restore(catch_snap)
        self._scope.merge_moved_from(try_snap)

        if getattr(node, 'finally_block', None):
            self._check_statements(node.finally_block)

    # Aliases for common AST names
    _check_ExpressionStatement = _check_generic
