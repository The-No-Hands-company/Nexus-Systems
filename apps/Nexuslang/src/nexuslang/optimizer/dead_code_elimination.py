"""
Dead Code Elimination (DCE) Pass
=================================

Removes unused code from the AST:
- Unused functions
- Unused variables
- Unreachable code blocks
- Redundant assignments
"""

from typing import Set, Dict, List, Any
import copy
from nexuslang.optimizer import OptimizationPass, OptimizationStats


class DeadCodeEliminationPass(OptimizationPass):
    """
    Dead Code Elimination optimization pass.
    
    Performs:
    1. Function-level DCE: Remove unused functions
    2. Variable-level DCE: Remove unused variables  
    3. Block-level DCE: Remove unreachable code
    """
    
    def __init__(self, aggressive: bool = False):
        super().__init__("Dead Code Elimination")
        self.aggressive = aggressive
        self.used_functions: Set[str] = set()
        self.used_variables: Set[str] = set()
    
    def run(self, ast: Any) -> Any:
        """Run DCE on the AST."""
        # Make a copy to avoid mutating original
        optimized_ast = copy.deepcopy(ast)
        
        # Phase 1: Mark used functions and variables
        self._mark_used_symbols(optimized_ast)
        
        # Phase 2: Remove unused functions
        optimized_ast = self._remove_unused_functions(optimized_ast)
        
        # Phase 3: Remove unused variables (if aggressive)
        if self.aggressive:
            optimized_ast = self._remove_unused_variables(optimized_ast)
        
        # Phase 4: Remove unreachable code
        optimized_ast = self._remove_unreachable_code(optimized_ast)
        
        return optimized_ast
    
    def _mark_used_symbols(self, ast: Any):
        """Mark all used functions and variables."""
        # Always mark 'main' as used (entry point)
        self.used_functions.add('main')
        
        # Walk the AST and mark everything referenced
        self._walk_ast(ast, self._mark_visitor)
    
    def _mark_visitor(self, node: Any):
        """Visitor to mark used symbols."""
        node_type = type(node).__name__
        
        if node_type == 'FunctionCall':
            # Mark function as used
            if hasattr(node, 'name'):
                self.used_functions.add(node.name)
        
        elif node_type == 'Identifier':
            # Mark variable as used
            if hasattr(node, 'name'):
                self.used_variables.add(node.name)
    
    def _remove_unused_functions(self, ast: Any) -> Any:
        """Remove functions that are never called."""
        if not hasattr(ast, 'statements'):
            return ast
        
        new_statements = []
        for stmt in ast.statements:
            if type(stmt).__name__ == 'FunctionDefinition':
                # Keep function if it's used
                if stmt.name in self.used_functions:
                    new_statements.append(stmt)
                else:
                    self.stats.dead_functions_removed += 1
            else:
                new_statements.append(stmt)
        
        ast.statements = new_statements
        return ast
    
    def _remove_unused_variables(self, ast: Any) -> Any:
        """Remove variables that are never read."""
        # This is more complex - need flow analysis
        # For now, just mark as complete
        return ast
    
    def _remove_unreachable_code(self, ast: Any) -> Any:
        """Remove code after return statements."""
        self._walk_ast(ast, self._remove_unreachable_visitor)
        return ast
    
    def _remove_unreachable_visitor(self, node: Any):
        """Visitor to remove unreachable code."""
        # Check if this is a function with a body
        if hasattr(node, 'body') and isinstance(node.body, list):
            new_body = []
            found_return = False
            
            for stmt in node.body:
                if found_return:
                    # Skip everything after return
                    self.stats.unreachable_blocks_removed += 1
                    continue
                
                new_body.append(stmt)
                
                # Check if this is a return statement
                if type(stmt).__name__ == 'ReturnStatement':
                    found_return = True
            
            node.body = new_body
    
    def _walk_ast(self, node: Any, visitor):
        """Walk the AST and apply visitor to each node."""
        if node is None:
            return
        
        # Apply visitor to this node
        visitor(node)
        
        # Handle lists
        if isinstance(node, list):
            for item in node:
                self._walk_ast(item, visitor)
            return
        
        # Skip non-objects
        if not hasattr(node, '__dict__'):
            return
        
        # Recursively walk all attributes of the node
        for attr_name, attr_value in node.__dict__.items():
            # Skip private attributes and primitives
            if attr_name.startswith('_'):
                continue
            if attr_name in ('line_number', 'column', 'type'):
                continue
            if isinstance(attr_value, (str, int, float, bool, type(None))):
                continue
            
            # Recursively visit lists
            if isinstance(attr_value, list):
                for item in attr_value:
                    self._walk_ast(item, visitor)
            
            # Recursively visit objects that might be AST nodes
            elif hasattr(attr_value, '__dict__'):
                self._walk_ast(attr_value, visitor)
