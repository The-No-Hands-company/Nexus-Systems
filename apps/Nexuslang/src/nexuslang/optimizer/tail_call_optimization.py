"""
Tail Call Optimization Pass
============================

Optimizes tail-recursive functions to use iteration instead of recursion,
preventing stack overflow and improving performance.

The transformation converts a tail-recursive function::

    function factorial with n as Integer and acc as Integer returns Integer
        if n is less than or equal to 1
            return acc
        return factorial with n minus 1 and acc times n
    end

Into an iterative loop::

    function factorial with n as Integer and acc as Integer returns Integer
        while true
            if n is less than or equal to 1
                return acc
            end
            set n to n minus 1
            set acc to acc times n
        end
    end
"""

import copy
from typing import Any, List, Optional
from ..optimizer import OptimizationPass, OptimizationStats
from ..parser.ast import (
    Program, FunctionDefinition, ReturnStatement, FunctionCall,
    IfStatement, WhileLoop, Block, VariableDeclaration, Identifier, Literal,
    BinaryOperation, ContinueStatement, IndexAssignment,
)


class TailCallOptimizationPass(OptimizationPass):
    """Tail call optimization for recursive functions.

    Transforms tail-recursive functions into iterative loops.  Only
    *simple* tail recursion is handled: the recursive call must appear
    directly inside a ``return`` statement (not wrapped in another
    expression like ``return n times factorial(...)``).
    """

    def __init__(self):
        super().__init__("TailCallOptimization")
        self.functions_optimized = 0

    def run(self, ast: Program) -> Program:
        """Run tail call optimization on the AST."""
        self.functions_optimized = 0

        for stmt in ast.statements:
            if isinstance(stmt, FunctionDefinition):
                if self._optimize_function(stmt):
                    self.functions_optimized += 1

        self.stats.functions_inlined = self.functions_optimized
        return ast

    # ------------------------------------------------------------------
    # Core transformation
    # ------------------------------------------------------------------

    def _optimize_function(self, func: FunctionDefinition) -> bool:
        """Attempt to convert a tail-recursive function to a loop.

        Returns True when the transformation was applied.
        """
        if not func.body:
            return False
        if not func.parameters:
            return False
        if not self._is_tail_recursive(func):
            return False

        # Collect ALL tail-call sites in the body.
        tail_sites = self._collect_tail_return_stmts(func.body, func.name)
        if not tail_sites:
            return False

        # Build the transformed body:
        #   while true
        #       <original body with tail returns replaced by param reassignment + continue>
        #   end
        transformed_body = self._rewrite_body(func.body, func)
        if transformed_body is None:
            return False

        # Wrap in ``while true ... end``
        true_literal = Literal("Boolean", True)
        true_literal.line = getattr(func, "line", 0)
        true_literal.column = getattr(func, "column", 0)

        loop = WhileLoop(
            condition=true_literal,
            body=transformed_body,
        )
        loop.line = getattr(func, "line", 0)
        loop.column = getattr(func, "column", 0)

        func.body = [loop]
        return True

    # ------------------------------------------------------------------
    # Body rewriting
    # ------------------------------------------------------------------

    def _rewrite_body(
        self,
        stmts: List,
        func: FunctionDefinition,
    ) -> Optional[List]:
        """Return a copy of *stmts* where every tail ``return f(...)``
        is replaced with parameter re-assignments followed by ``continue``.

        Returns ``None`` if any tail call has an argument count mismatch.
        """
        result = []
        for stmt in stmts:
            rewritten = self._rewrite_stmt(stmt, func)
            if rewritten is None:
                return None
            result.append(rewritten)
        return result

    def _rewrite_stmt(self, stmt, func: FunctionDefinition):
        """Rewrite a single statement (recursively for if/else blocks)."""
        # --- tail return: ``return func_name with ...`` ---
        if isinstance(stmt, ReturnStatement) and self._is_tail_return(stmt, func.name):
            return self._build_reassignment_block(stmt.value, func)

        # --- if / else-if chains ---
        if isinstance(stmt, IfStatement):
            new_then = self._rewrite_body(
                stmt.then_block if isinstance(stmt.then_block, list) else [stmt.then_block],
                func,
            )
            if new_then is None:
                return None

            new_else = None
            if stmt.else_block:
                raw_else = stmt.else_block if isinstance(stmt.else_block, list) else [stmt.else_block]
                new_else = self._rewrite_body(raw_else, func)
                if new_else is None:
                    return None

            new_elif = []
            for cond, block in (stmt.elif_blocks if hasattr(stmt, "elif_blocks") and stmt.elif_blocks else []):
                raw_block = block if isinstance(block, list) else [block]
                rewritten_block = self._rewrite_body(raw_block, func)
                if rewritten_block is None:
                    return None
                new_elif.append((cond, rewritten_block))

            new_if = copy.copy(stmt)
            new_if.then_block = new_then
            new_if.else_block = new_else
            if hasattr(new_if, "elif_blocks"):
                new_if.elif_blocks = new_elif
            return new_if

        return stmt  # unchanged

    def _build_reassignment_block(self, call: FunctionCall, func: FunctionDefinition):
        """Build a Block that reassigns parameters from the call arguments
        and then continues the loop."""
        params = func.parameters
        args = call.arguments if hasattr(call, "arguments") and call.arguments else []

        if len(args) != len(params):
            return None

        assignments: list = []
        # Use temporary names to avoid order-dependent overwrites.
        temps = []
        for i, (param, arg) in enumerate(zip(params, args)):
            pname = param.name if hasattr(param, "name") else str(param)
            tmp_name = f"_tco_tmp_{pname}_{id(call)}"
            tmp_decl = VariableDeclaration(name=tmp_name, value=copy.deepcopy(arg))
            tmp_decl.line = getattr(call, "line", 0)
            tmp_decl.column = getattr(call, "column", 0)
            assignments.append(tmp_decl)
            temps.append((pname, tmp_name))

        for pname, tmp_name in temps:
            reassign = VariableDeclaration(name=pname, value=Identifier(tmp_name))
            reassign.line = getattr(call, "line", 0)
            reassign.column = getattr(call, "column", 0)
            assignments.append(reassign)

        cont = ContinueStatement()
        cont.line = getattr(call, "line", 0)
        cont.column = getattr(call, "column", 0)
        assignments.append(cont)

        block = Block(statements=assignments)
        block.line = getattr(call, "line", 0)
        block.column = getattr(call, "column", 0)
        return block

    # ------------------------------------------------------------------
    # Tail-call detection helpers
    # ------------------------------------------------------------------

    def _is_tail_recursive(self, func: FunctionDefinition) -> bool:
        """Check if function contains tail-recursive calls."""
        return self._contains_tail_call(func, func.name)

    def _is_tail_return(self, stmt: ReturnStatement, func_name: str) -> bool:
        """True if *stmt* is ``return func_name(...)``."""
        if not isinstance(stmt, ReturnStatement):
            return False
        if stmt.value is None:
            return False
        if not isinstance(stmt.value, FunctionCall):
            return False
        call_name = getattr(stmt.value, "name", None)
        return call_name == func_name

    def _collect_tail_return_stmts(self, stmts: list, func_name: str) -> list:
        """Recursively collect all ReturnStatements that are tail calls."""
        found = []
        for stmt in stmts:
            if isinstance(stmt, ReturnStatement) and self._is_tail_return(stmt, func_name):
                found.append(stmt)
            elif isinstance(stmt, IfStatement):
                then = stmt.then_block if isinstance(stmt.then_block, list) else [stmt.then_block]
                found.extend(self._collect_tail_return_stmts(then, func_name))
                if stmt.else_block:
                    eb = stmt.else_block if isinstance(stmt.else_block, list) else [stmt.else_block]
                    found.extend(self._collect_tail_return_stmts(eb, func_name))
                for _, block in (stmt.elif_blocks if hasattr(stmt, "elif_blocks") and stmt.elif_blocks else []):
                    blk = block if isinstance(block, list) else [block]
                    found.extend(self._collect_tail_return_stmts(blk, func_name))
        return found

    def _contains_tail_call(self, node, func_name: str) -> bool:
        """Check if node contains a tail call to func_name."""
        if isinstance(node, ReturnStatement):
            if node.value and isinstance(node.value, FunctionCall):
                if getattr(node.value, "name", None) == func_name:
                    return True

        if isinstance(node, IfStatement):
            then_has_tail = any(
                self._contains_tail_call(s, func_name)
                for s in (node.then_block if isinstance(node.then_block, list) else [node.then_block])
            )
            else_has_tail = False
            if node.else_block:
                else_has_tail = any(
                    self._contains_tail_call(s, func_name)
                    for s in (node.else_block if isinstance(node.else_block, list) else [node.else_block])
                )
            return then_has_tail or else_has_tail

        if hasattr(node, "body") and isinstance(node.body, list):
            if node.body:
                return self._contains_tail_call(node.body[-1], func_name)

        return False

    def _find_tail_call(self, func: FunctionDefinition) -> Optional[FunctionCall]:
        """Find the tail call in a function."""
        if not func.body:
            return None

        last_stmt = func.body[-1]

        if isinstance(last_stmt, ReturnStatement):
            if isinstance(last_stmt.value, FunctionCall):
                if getattr(last_stmt.value, "name", None) == func.name:
                    return last_stmt.value

        return None
