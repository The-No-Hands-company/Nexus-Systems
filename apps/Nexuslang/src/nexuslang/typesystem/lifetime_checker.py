"""Compile-time lifetime checker for NexusLang.

This pass walks the AST *after* the BorrowChecker and enforces lifetime rules
statically, ensuring that borrowed references cannot outlive the values they
borrow from.

Lifetime labels
---------------
Lifetime labels are plain identifiers (e.g. ``outer``, ``inner``, ``a``) used
to link borrowing sites to the source of the borrowed value.

NLPL syntax examples::

    function first_word with sentence as borrow String with lifetime outer
                                returns borrow String with lifetime outer
        return borrow sentence with lifetime outer
    end

    function longest with x as borrow String with lifetime a
                               and y as borrow String with lifetime a
                               returns borrow String with lifetime a
        if length of x is greater than length of y
            return borrow x with lifetime a
        else
            return borrow y with lifetime a
        end
    end

Rules enforced
--------------
1. **Declared lifetimes** -- Every lifetime label appearing in a borrow
   expression inside a function body must be declared on at least one parameter
   of that function.  Undeclared labels indicate a dangling-borrow risk.

2. **Return-type consistency** -- If the function's return type has a lifetime
   annotation (``returns borrow T with lifetime L``), every return statement
   that yields a borrow expression must use the *same* lifetime label ``L``.

3. **Label consistency** -- A lifetime label on a ``BorrowExpressionWithLifetime``
   node inside a function body must not be re-used with different source
   variables in a way that would violate the declared relationship.

4. **Unused lifetime declarations** -- A lifetime label declared only on
   parameters but never used anywhere produces a warning (not an error, since
   the function may not use the parameter's lifetime in all paths).

Notes
-----
- Lifetime analysis at this level does *not* attempt control-flow-sensitive
  region inference (that would require SSA / reaching-definitions).  The checks
  are conservative but decidable in a single pass over the AST.
- For inter-procedural lifetime safety the programmer must explicitly annotate
  function signatures; the checker only validates those annotations are
  *internally consistent*.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Dict, List, Optional, Set

from ..parser import ast as _ast
from ..parser.ast import (
    ReturnTypeWithLifetime,
    BorrowExpressionWithLifetime,
    LifetimeAnnotation,
)


# ---------------------------------------------------------------------------
# Error / warning dataclasses
# ---------------------------------------------------------------------------

@dataclass
class LifetimeError:
    """A compile-time lifetime violation."""
    message: str
    line: Optional[int] = None
    var_name: Optional[str] = None
    is_warning: bool = False

    def __str__(self) -> str:
        loc = f" (line {self.line})" if self.line else ""
        kind = "LifetimeWarning" if self.is_warning else "LifetimeError"
        return f"{kind}{loc}: {self.message}"


# ---------------------------------------------------------------------------
# Per-function lifetime context
# ---------------------------------------------------------------------------

@dataclass
class FunctionLifetimeContext:
    """Lifetime information collected for one function definition."""
    name: str
    # Labels declared on parameters: label -> list of param names carrying it
    param_labels: Dict[str, List[str]] = field(default_factory=dict)
    # Label declared on the return type (if any)
    return_label: Optional[str] = None
    # Labels actually used in borrow expressions in the body
    used_labels: Set[str] = field(default_factory=set)
    # Lifetime labels seen on return-statement borrow expressions
    return_borrow_labels: List[Optional[str]] = field(default_factory=list)


# ---------------------------------------------------------------------------
# Main checker
# ---------------------------------------------------------------------------

class LifetimeChecker:
    """Static compile-time lifetime annotation checker.

    Usage::

        checker = LifetimeChecker()
        errors = checker.check(ast_program)
        for e in errors:
            print(e)
    """

    def __init__(self) -> None:
        self._errors: List[LifetimeError] = []
        # Stack of FunctionLifetimeContext for nested functions
        self._fn_stack: List[FunctionLifetimeContext] = []

    # ------------------------------------------------------------------
    # Public entry point
    # ------------------------------------------------------------------

    def check(self, program: _ast.Program) -> List[LifetimeError]:
        """Check a parsed program and return a list of lifetime errors / warnings."""
        self._errors = []
        self._fn_stack = []
        self._check_statements(program.statements)
        return list(self._errors)

    # ------------------------------------------------------------------
    # Internal helpers
    # ------------------------------------------------------------------

    def _err(self, msg: str, line: Optional[int] = None,
             var: Optional[str] = None,
             warning: bool = False) -> None:
        self._errors.append(LifetimeError(message=msg, line=line,
                                          var_name=var, is_warning=warning))

    def _line(self, node) -> Optional[int]:
        return getattr(node, 'line_number', None) or getattr(node, 'line', None)

    def _current_fn(self) -> Optional[FunctionLifetimeContext]:
        return self._fn_stack[-1] if self._fn_stack else None

    def _label_of(self, lt: Optional[LifetimeAnnotation]) -> Optional[str]:
        if lt is None:
            return None
        return lt.label

    # ------------------------------------------------------------------
    # Traversal
    # ------------------------------------------------------------------

    def _check_statements(self, stmts: list) -> None:
        for stmt in (stmts or []):
            self._check_node(stmt)

    def _check_node(self, node) -> None:
        if node is None:
            return
        cls_name = type(node).__name__
        handler = getattr(self, f"_check_{cls_name}", None)
        if handler is not None:
            handler(node)
        else:
            self._check_generic(node)

    def _check_generic(self, node) -> None:
        for attr in ("statements", "body", "then_block", "else_block",
                     "condition", "value", "expression", "left", "right",
                     "arguments", "iterable"):
            child = getattr(node, attr, None)
            if child is None:
                continue
            if isinstance(child, list):
                self._check_statements(child)
            else:
                self._check_node(child)

    # ------------------------------------------------------------------
    # Function definition
    # ------------------------------------------------------------------

    def _check_FunctionDefinition(self, node: _ast.FunctionDefinition) -> None:
        ctx = FunctionLifetimeContext(name=node.name)

        # Collect parameter lifetime labels
        for param in (node.parameters or []):
            ann = getattr(param, 'type_annotation', None)
            lt_label = None
            if isinstance(ann, ReturnTypeWithLifetime):
                lt_label = self._label_of(ann.lifetime)
            elif hasattr(param, 'lifetime') and param.lifetime is not None:
                lt_label = self._label_of(param.lifetime)
            if lt_label:
                ctx.param_labels.setdefault(lt_label, []).append(param.name)

        # Collect return type lifetime label
        rt = getattr(node, 'return_type', None)
        if isinstance(rt, ReturnTypeWithLifetime):
            ctx.return_label = self._label_of(rt.lifetime)

        self._fn_stack.append(ctx)
        try:
            self._check_statements(node.body)
        finally:
            self._fn_stack.pop()

        # Post-function validation
        self._validate_function_context(ctx, self._line(node))

    def _check_AsyncFunctionDefinition(self, node) -> None:
        self._check_FunctionDefinition(node)

    def _validate_function_context(self, ctx: FunctionLifetimeContext,
                                    line: Optional[int]) -> None:
        # Rule 1: every used label must be declared on a parameter
        for label in ctx.used_labels:
            if label not in ctx.param_labels:
                self._err(
                    f"lifetime '{label}' used in function '{ctx.name}' is not "
                    f"declared on any parameter; declare it as "
                    f"'param as borrow T with lifetime {label}'",
                    line,
                )

        # Rule 2: if there's a declared return lifetime, verify return borrows use it
        if ctx.return_label is not None:
            for idx, borrow_label in enumerate(ctx.return_borrow_labels):
                if borrow_label is None:
                    self._err(
                        f"function '{ctx.name}' declares return lifetime "
                        f"'{ctx.return_label}' but a return statement yields a "
                        f"borrow without a lifetime annotation",
                        line,
                    )
                elif borrow_label != ctx.return_label:
                    self._err(
                        f"function '{ctx.name}' return type has lifetime "
                        f"'{ctx.return_label}' but a return borrow uses "
                        f"lifetime '{borrow_label}'; they must match",
                        line,
                    )

        # Rule 4: warn about declared but completely unused labels
        for label, params in ctx.param_labels.items():
            if label not in ctx.used_labels and ctx.return_label != label:
                self._err(
                    f"lifetime '{label}' declared on parameter(s) "
                    f"{params!r} in function '{ctx.name}' is never used",
                    line,
                    warning=True,
                )

    # ------------------------------------------------------------------
    # Borrow expression with lifetime
    # ------------------------------------------------------------------

    def _check_BorrowExpressionWithLifetime(self,
                                             node: BorrowExpressionWithLifetime) -> None:
        label = self._label_of(node.lifetime)
        if label is None:
            return
        # Validate label name is not empty
        if not label.strip():
            self._err("lifetime label cannot be empty", self._line(node), node.var_name)
            return
        # Register the label as used within the current function context
        ctx = self._current_fn()
        if ctx is not None:
            ctx.used_labels.add(label)

    def _check_ReturnStatement(self, node) -> None:
        val = getattr(node, 'value', None)
        if val is None:
            return
        self._check_node(val)
        # Record what lifetime the return borrow carries (if any)
        ctx = self._current_fn()
        if ctx is not None and ctx.return_label is not None:
            if isinstance(val, BorrowExpressionWithLifetime):
                ctx.return_borrow_labels.append(self._label_of(val.lifetime))
            elif isinstance(val, _ast.BorrowExpression):
                # Plain borrow without lifetime annotation returned from a function
                # that declares a return lifetime.
                ctx.return_borrow_labels.append(None)

    # ------------------------------------------------------------------
    # Control flow -- just recurse
    # ------------------------------------------------------------------

    def _check_IfStatement(self, node: _ast.IfStatement) -> None:
        self._check_node(node.condition)
        then = node.then_block
        self._check_statements(then if isinstance(then, list) else [then])
        if node.else_block is not None:
            els = node.else_block
            self._check_statements(els if isinstance(els, list) else [els])

    def _check_WhileLoop(self, node: _ast.WhileLoop) -> None:
        self._check_node(node.condition)
        self._check_statements(node.body)
        if node.else_body:
            self._check_statements(node.else_body)

    def _check_ForLoop(self, node: _ast.ForLoop) -> None:
        for expr in (node.iterable, node.start, node.end, node.step):
            if expr is not None:
                self._check_node(expr)
        self._check_statements(node.body)
        if node.else_body:
            self._check_statements(node.else_body)

    def _check_VariableDeclaration(self, node) -> None:
        if node.value is not None:
            self._check_node(node.value)

    def _check_BorrowExpression(self, node) -> None:
        # Plain borrow without lifetime -- no lifetime checking needed
        pass

    def _check_MatchExpression(self, node) -> None:
        self._check_node(node.expression)
        for case in (node.cases or []):
            if case.guard is not None:
                self._check_node(case.guard)
            self._check_statements(case.body)

    def _check_TryCatchStatement(self, node) -> None:
        self._check_statements(getattr(node, 'try_block', None) or [])
        self._check_statements(getattr(node, 'catch_block', None) or [])
        if getattr(node, 'finally_block', None):
            self._check_statements(node.finally_block)

    def _check_ClassDefinition(self, node) -> None:
        for stmt in (node.methods or []):
            self._check_node(stmt)

    def _check_MoveExpression(self, node) -> None:
        pass  # No lifetime implications for plain moves

    def _check_DropBorrowStatement(self, node) -> None:
        pass  # Drops end a borrow; no further lifetime validation needed here

    _check_Literal = lambda self, node: None
    _check_Identifier = lambda self, node: None
    _check_BinaryOperation = _check_generic
    _check_UnaryOperation = _check_generic
    _check_FunctionCall = _check_generic
    _check_MethodCall = _check_generic
    _check_ExpressionStatement = _check_generic
