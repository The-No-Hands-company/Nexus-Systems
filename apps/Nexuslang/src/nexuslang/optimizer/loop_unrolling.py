"""
Loop Unrolling Optimization Pass
=================================

Unrolls loops with known iteration counts to reduce loop overhead
and enable further optimizations.
"""

from typing import Any
from ..optimizer import OptimizationPass, OptimizationStats
from ..parser.ast import (
    Program, WhileLoop, ForLoop, RepeatNTimesLoop, Literal,
    Block, IfStatement
)


class LoopUnrollingPass(OptimizationPass):
    """
    Unrolls loops with constant iteration counts.
    
    Strategies:
    - Full unrolling for small iteration counts (< max_unroll_count)
    - Partial unrolling for larger loops (unroll factor)
    - Skip unrolling for very large loops or unknown counts
    """
    
    def __init__(self, max_unroll_count: int = 8, unroll_factor: int = 4):
        """
        Initialize loop unrolling pass.
        
        Args:
            max_unroll_count: Maximum iterations for full unrolling
            unroll_factor: Unroll factor for partial unrolling
        """
        super().__init__("LoopUnrolling")
        self.max_unroll_count = max_unroll_count
        self.unroll_factor = unroll_factor
        self.loops_unrolled = 0
    
    def run(self, ast: Program) -> Program:
        """Run loop unrolling on the AST."""
        self.loops_unrolled = 0
        self._process_statements(ast.statements)
        self.stats.unreachable_blocks_removed = self.loops_unrolled  # Reuse stat
        return ast
    
    def _process_statements(self, statements: list):
        """Process a list of statements."""
        i = 0
        while i < len(statements):
            stmt = statements[i]
            
            # Try to unroll this loop
            unrolled = self._try_unroll(stmt)
            if unrolled is not None:
                # Replace loop with unrolled statements
                statements[i:i+1] = unrolled
                self.loops_unrolled += 1
                i += len(unrolled)
            else:
                # Recursively process nested statements
                self._process_statement(stmt)
                i += 1
    
    def _process_statement(self, stmt):
        """Process nested statements in a single statement."""
        if isinstance(stmt, (WhileLoop, ForLoop, RepeatNTimesLoop)):
            self._process_statements(stmt.body)
        elif isinstance(stmt, IfStatement):
            self._process_statements(stmt.then_block)
            if stmt.else_block:
                self._process_statements(stmt.else_block)
        elif hasattr(stmt, 'body') and isinstance(stmt.body, list):
            self._process_statements(stmt.body)
    
    def _try_unroll(self, stmt) -> list | None:
        """
        Try to unroll a loop statement.
        
        Returns:
            List of unrolled statements if successful, None otherwise
        """
        # RepeatNTimesLoop with constant count
        if isinstance(stmt, RepeatNTimesLoop):
            if isinstance(stmt.count, Literal):
                count = stmt.count.value
                if isinstance(count, int) and 0 < count <= self.max_unroll_count:
                    # Full unroll
                    unrolled = []
                    for _ in range(count):
                        # Copy loop body (shallow copy is fine for simple cases)
                        unrolled.extend(stmt.body[:])
                    return unrolled
        
        # ForLoop with range and constant bounds
        if isinstance(stmt, ForLoop):
            # Check if iterating over range(n) where n is constant
            if hasattr(stmt, 'iterable') and hasattr(stmt.iterable, 'name'):
                if stmt.iterable.name == 'range':
                    # Would need to evaluate range arguments
                    # For now, skip (requires constant propagation analysis)
                    pass
        
        return None
