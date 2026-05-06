"""
Function Inlining Pass
======================

Inline small functions at call sites to:
- Eliminate function call overhead
- Enable further optimizations
- Improve code locality
"""

from typing import Dict, Set, Any, List
import copy
from nexuslang.optimizer import OptimizationPass


class FunctionInliningPass(OptimizationPass):
    """
    Function inlining optimization pass.
    
    Inlines small functions directly at their call sites.
    Uses a cost model to decide which functions to inline.
    """
    
    def __init__(self, max_size: int = 50, aggressive: bool = False):
        super().__init__("Function Inlining")
        self.max_size = max_size  # Max statements to inline
        self.aggressive = aggressive
        self.functions: Dict[str, Any] = {}  # function_name -> FunctionDefinition
        self.inlinable: Set[str] = set()  # Functions safe to inline
    
    def run(self, ast: Any) -> Any:
        """Run function inlining on the AST."""
        optimized_ast = copy.deepcopy(ast)
        
        # Phase 1: Collect function definitions
        self._collect_functions(optimized_ast)
        
        # Phase 2: Determine which functions can be inlined
        self._mark_inlinable()
        
        # Phase 3: Inline function calls
        self._inline_calls(optimized_ast)
        
        return optimized_ast
    
    def _collect_functions(self, ast: Any):
        """Collect all function definitions."""
        if not hasattr(ast, 'statements'):
            return
        
        for stmt in ast.statements:
            if type(stmt).__name__ == 'FunctionDefinition':
                self.functions[stmt.name] = stmt
    
    def _mark_inlinable(self):
        """Mark functions that are safe to inline."""
        for func_name, func_def in self.functions.items():
            # Don't inline main
            if func_name == 'main':
                continue
            
            # Don't inline recursive functions
            if self._is_recursive(func_def):
                continue
            
            # Check size
            size = self._estimate_size(func_def)
            if size > self.max_size:
                continue
            
            # Don't inline functions with complex control flow (unless aggressive)
            if not self.aggressive and self._has_complex_control_flow(func_def):
                continue
            
            # This function can be inlined
            self.inlinable.add(func_name)
    
    def _estimate_size(self, func_def: Any) -> int:
        """Estimate the size of a function in statements."""
        if not hasattr(func_def, 'body'):
            return 0
        
        return len(func_def.body)
    
    def _is_recursive(self, func_def: Any) -> bool:
        """Check if function calls itself (directly)."""
        func_name = func_def.name
        
        # Walk the function body looking for self-calls
        return self._contains_call(func_def.body, func_name)
    
    def _contains_call(self, body: List[Any], func_name: str) -> bool:
        """Check if body contains a call to func_name."""
        for stmt in body:
            if type(stmt).__name__ == 'FunctionCall':
                if hasattr(stmt, 'name') and stmt.name == func_name:
                    return True
            
            # Recursively check nested blocks
            if hasattr(stmt, 'body') and isinstance(stmt.body, list):
                if self._contains_call(stmt.body, func_name):
                    return True
            
            if hasattr(stmt, 'then_block') and isinstance(stmt.then_block, list):
                if self._contains_call(stmt.then_block, func_name):
                    return True
            
            if hasattr(stmt, 'else_block') and isinstance(stmt.else_block, list):
                if self._contains_call(stmt.else_block, func_name):
                    return True
        
        return False
    
    def _has_complex_control_flow(self, func_def: Any) -> bool:
        """Check if function has complex control flow."""
        if not hasattr(func_def, 'body'):
            return False
        
        # Count control flow statements
        control_flow_count = 0
        for stmt in func_def.body:
            stmt_type = type(stmt).__name__
            if stmt_type in ('IfStatement', 'WhileLoop', 'ForLoop', 'TryCatch'):
                control_flow_count += 1
        
        # More than 1 control flow statement = complex
        return control_flow_count > 1
    
    def _inline_calls(self, ast: Any):
        """Inline function calls where possible."""
        self._walk_and_inline(ast)
    
    def _walk_and_inline(self, node: Any):
        """Walk AST and inline function calls."""
        # Process statements list
        if hasattr(node, 'statements') and isinstance(node.statements, list):
            new_statements = []
            for stmt in node.statements:
                # Recursively process the statement
                self._walk_and_inline(stmt)
                
                # If this is an inlinable function call, inline it
                if type(stmt).__name__ == 'FunctionCall':
                    if hasattr(stmt, 'name') and stmt.name in self.inlinable:
                        # Inline the function
                        inlined = self._inline_function_call(stmt)
                        if inlined:
                            new_statements.extend(inlined)
                            self.stats.functions_inlined += 1
                            continue
                
                new_statements.append(stmt)
            
            node.statements = new_statements
        
        # Process function bodies
        if hasattr(node, 'body') and isinstance(node.body, list):
            new_body = []
            for stmt in node.body:
                self._walk_and_inline(stmt)
                
                if type(stmt).__name__ == 'FunctionCall':
                    if hasattr(stmt, 'name') and stmt.name in self.inlinable:
                        inlined = self._inline_function_call(stmt)
                        if inlined:
                            new_body.extend(inlined)
                            self.stats.functions_inlined += 1
                            continue
                
                new_body.append(stmt)
            
            node.body = new_body
        
        # Process if/else blocks
        if hasattr(node, 'then_block') and isinstance(node.then_block, list):
            for stmt in node.then_block:
                self._walk_and_inline(stmt)
        
        if hasattr(node, 'else_block') and isinstance(node.else_block, list):
            for stmt in node.else_block:
                self._walk_and_inline(stmt)
    
    def _inline_function_call(self, call: Any) -> List[Any]:
        """
        Inline a function call.
        Returns list of statements to replace the call.
        
        Full implementation:
        1. Create temporary variables for each parameter
        2. Initialize them with the argument values
        3. Substitute parameter references in the function body
        4. Handle return statements (convert to result assignment)
        """
        func_name = call.name
        if func_name not in self.functions:
            return None
        
        func_def = self.functions[func_name]
        
        # Get function parameters and call arguments
        parameters = getattr(func_def, 'parameters', [])
        arguments = getattr(call, 'arguments', [])
        
        if not parameters:
            parameters = []
        if not arguments:
            arguments = []
        
        # If mismatched parameter/argument count, don't inline
        if len(parameters) != len(arguments):
            return None
        
        # Copy the function body for modification
        inlined_body = copy.deepcopy(func_def.body)
        
        # Build parameter name to argument value mapping
        param_map = {}
        for param, arg in zip(parameters, arguments):
            param_name = param.name if hasattr(param, 'name') else str(param)
            param_map[param_name] = arg
        
        # Generate unique prefix for inlined variables to avoid name clashes
        inline_prefix = f"_inline_{func_name}_{id(call) % 10000}_"
        
        # Create initialization statements for parameters
        init_statements = []
        from nexuslang.parser.ast import VariableDeclaration
        
        for param_name, arg_value in param_map.items():
            # Create: set _inline_funcname_XXXX_paramname to arg_value
            inlined_param_name = f"{inline_prefix}{param_name}"
            init_stmt = VariableDeclaration(
                inlined_param_name,
                copy.deepcopy(arg_value),
                getattr(call, 'line_number', 0)
            )
            init_statements.append(init_stmt)
            # Update param_map to use the new name
            param_map[param_name] = inlined_param_name
        
        # Substitute parameter references in the inlined body
        self._substitute_params(inlined_body, param_map, inline_prefix)
        
        # Handle return statements (for now, simple return)
        # In a full implementation, we'd track the result variable
        
        return init_statements + inlined_body
    
    def _substitute_params(self, node: Any, param_map: Dict[str, str], prefix: str):
        """
        Recursively substitute parameter references with their inlined names.
        Also renames local variables to avoid conflicts.
        """
        if node is None:
            return
        
        # Handle lists (bodies, statements, etc.)
        if isinstance(node, list):
            for item in node:
                self._substitute_params(item, param_map, prefix)
            return
        
        # Skip non-objects
        if not hasattr(node, '__dict__'):
            return
        
        node_type = type(node).__name__
        
        # Substitute Identifier references
        if node_type == 'Identifier':
            if hasattr(node, 'name') and node.name in param_map:
                node.name = param_map[node.name]
        
        # Rename local variable declarations to avoid conflicts
        elif node_type == 'VariableDeclaration':
            if hasattr(node, 'name') and node.name not in param_map:
                old_name = node.name
                new_name = f"{prefix}{old_name}"
                node.name = new_name
                # Add to param_map for subsequent references
                param_map[old_name] = new_name
            
            # Also substitute in the value expression
            if hasattr(node, 'value'):
                self._substitute_params(node.value, param_map, prefix)
        
        # Handle variable assignments
        elif node_type == 'Assignment':
            if hasattr(node, 'target') and hasattr(node.target, 'name'):
                if node.target.name in param_map:
                    node.target.name = param_map[node.target.name]
            if hasattr(node, 'value'):
                self._substitute_params(node.value, param_map, prefix)
        
        # Handle binary operations
        elif node_type == 'BinaryOperation':
            if hasattr(node, 'left'):
                self._substitute_params(node.left, param_map, prefix)
            if hasattr(node, 'right'):
                self._substitute_params(node.right, param_map, prefix)
        
        # Handle function calls
        elif node_type == 'FunctionCall':
            if hasattr(node, 'arguments'):
                self._substitute_params(node.arguments, param_map, prefix)
        
        # Handle return statements
        elif node_type == 'ReturnStatement':
            if hasattr(node, 'value'):
                self._substitute_params(node.value, param_map, prefix)
        
        # Handle control flow structures
        elif node_type == 'IfStatement':
            if hasattr(node, 'condition'):
                self._substitute_params(node.condition, param_map, prefix)
            if hasattr(node, 'then_block'):
                self._substitute_params(node.then_block, param_map, prefix)
            if hasattr(node, 'else_block'):
                self._substitute_params(node.else_block, param_map, prefix)
        
        elif node_type == 'WhileLoop':
            if hasattr(node, 'condition'):
                self._substitute_params(node.condition, param_map, prefix)
            if hasattr(node, 'body'):
                self._substitute_params(node.body, param_map, prefix)
        
        elif node_type == 'ForLoop':
            if hasattr(node, 'iterator'):
                self._substitute_params(node.iterator, param_map, prefix)
            if hasattr(node, 'iterable'):
                self._substitute_params(node.iterable, param_map, prefix)
            if hasattr(node, 'body'):
                self._substitute_params(node.body, param_map, prefix)
        
        # Recursively process all other node attributes
        else:
            for attr_name, attr_value in node.__dict__.items():
                if attr_name.startswith('_') or attr_name in ('line_number', 'column'):
                    continue
                if isinstance(attr_value, list):
                    self._substitute_params(attr_value, param_map, prefix)
                elif hasattr(attr_value, '__dict__'):
                    self._substitute_params(attr_value, param_map, prefix)
