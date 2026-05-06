"""
C Code Generator for NexusLang
==========================

Generates C code from NexusLang AST for compilation to native executables.

Strategy:
- NexusLang variables → C variables
- NexusLang functions → C functions
- NexusLang classes → C structs + function pointers
- Memory management → malloc/free (with RAII wrappers)
- Standard library → Link against NexusLang C runtime library
"""

from typing import Any, Dict, List
from copy import deepcopy
from nexuslang.compiler import CodeGenerator
from nexuslang.parser.ast import *


class CCodeGenerator(CodeGenerator):
    """Generate C code from NexusLang AST."""
    
    def __init__(self, target: str, bounds_checking: bool = True):
        super().__init__(target)
        self.includes = set()
        self.forward_declarations = []
        self.global_variables = {}
        self.functions = {}
        self.symbol_table = {}  # Track variable types for type inference
        self.function_types = {}  # Track function return types
        self.current_class = None  # Track current class being processed
        self.class_properties = {}  # Map class_name -> set of property names
        self.property_types = {}  # Map (class_name, property_name) -> C type
        self.class_all_properties = {}  # Map class_name -> list of (prop_name, c_type) for inheritance
        self.class_parents = {}  # Map class_name -> parent_class_name for inheritance chain
        self.needed_runtime_functions = set()  # Track which runtime functions are needed
        self.bounds_checking = bounds_checking  # Enable/disable array bounds checking
        self.array_sizes = {}  # Track known array sizes: var_name -> size
        self.interface_types = {}  # Map interface_name -> {'methods': [...]}
        self.class_interfaces = {}  # Map class_name -> list of implemented interface names
        self.try_stack: List[str] = []  # Active jmp_buf names for nested try/catch blocks
        self.try_counter = 0
        self.exception_message_slot = "nxl_current_exception_message"
        self.uses_exceptions = False
        self.symbol_aliases: Dict[str, str] = {}
        self.top_level_statements: List[Any] = []        
        self.macro_definitions: Dict[str, MacroDefinition] = {}
        self.inside_top_level_init = False
        self.top_level_init_name = "__nxl_top_level_init"
        self.comptime_constant_values: Dict[str, Any] = {}
        self.macro_expansion_counter = 0
        self.loop_label_counter = 0
        self.loop_control_stack: List[tuple] = []  # (continue_label, break_label, loop_name)
        self.labeled_loop_controls: Dict[str, tuple] = {}  # label -> (continue_label, break_label)
        
    def generate(self, ast: Program) -> str:
        """Generate complete C program from AST."""
        self.reset()
        
        # Add default required includes
        self.includes.add("<stdio.h>")
        self.includes.add("<stdlib.h>")
        self.includes.add("<string.h>")
        self.includes.add("<stdbool.h>")
        self.includes.add("<stdint.h>")
        
        self.top_level_statements = [
            stmt for stmt in ast.statements
            if not isinstance(stmt, (FunctionDefinition, ClassDefinition, ExternFunctionDeclaration, ExternVariableDeclaration, MacroDefinition))
        ]

        has_user_main = any(isinstance(stmt, FunctionDefinition) and stmt.name == "main" for stmt in ast.statements)

        # Pre-collect compile-time metadata used while emitting function bodies.
        for stmt in ast.statements:
            if isinstance(stmt, MacroDefinition):
                self._collect_macro_definition(stmt)
            elif has_user_main and isinstance(stmt, VariableDeclaration):
                if self._can_materialize_global_variable(stmt):
                    self.global_variables[stmt.name] = self._infer_type(stmt.value)
            elif has_user_main and isinstance(stmt, ComptimeConst):
                const_type = self._infer_type(stmt.expr)
                self.global_variables[stmt.name] = const_type

        self.symbol_table.update(self.global_variables)

        # Generate program statements (this will add more includes as needed)
        # First pass: collect classes, globals and function definitions
        for stmt in ast.statements:
            if isinstance(stmt, ClassDefinition):
                self._generate_class_definition(stmt)
            elif isinstance(stmt, FunctionDefinition):
                self._generate_function_declaration(stmt)
            elif isinstance(stmt, ExternFunctionDeclaration):
                self._generate_extern_function(stmt)
            elif isinstance(stmt, ExternVariableDeclaration):
                # Generate extern variable declaration
                var_type = self._map_type(stmt.type_annotation) if hasattr(stmt, 'type_annotation') and stmt.type_annotation else 'void*'
                self.forward_declarations.append(f"extern {var_type} {stmt.name};")
                # Track in symbol table for type inference
                self.symbol_table[stmt.name] = var_type
            elif isinstance(stmt, MacroDefinition):
                self._collect_macro_definition(stmt)
            elif has_user_main and isinstance(stmt, VariableDeclaration):
                if self._can_materialize_global_variable(stmt):
                    self.global_variables[stmt.name] = self._infer_type(stmt.value)
            elif has_user_main and isinstance(stmt, ComptimeConst):
                const_type = self._infer_type(stmt.expr)
                self.global_variables[stmt.name] = const_type
        
        if has_user_main and self.top_level_statements:
            self.forward_declarations.append(f"static void {self.top_level_init_name}(void);")
        
        if not has_user_main:
            # Generate default main function for top-level statements
            self.emit_raw("int main(int argc, char** argv) {")
            self.indent()
        
            # Generate top-level statements
            for stmt in ast.statements:
                if not isinstance(stmt, (FunctionDefinition, ClassDefinition)):
                    self._generate_statement(stmt)
            
            self.emit("return 0;")
            self.dedent()
            self.emit_raw("}")
        else:
            # User defined main, just emit top-level statements (global init) if needed
            # For now, warn if there are regular statements outside functions
            # In a real compiler, we might put these in an _init function called by start
            if self.top_level_statements:
                self._generate_top_level_init_function()
        
        # Now prepend header and includes to the generated code
        header_lines = [
            "/*",
            " * Generated C code from NLPL",
            " * DO NOT EDIT - This file is auto-generated",
            " */",
            "",
        ]
        
        # Add includes
        for include in sorted(self.includes):
            header_lines.append(f"#include {include}")
        header_lines.append("")
        
        # Add forward declarations if any
        if self.forward_declarations:
            header_lines.extend(self.forward_declarations)
            header_lines.append("")

        if self.global_variables:
            for name, c_type in self.global_variables.items():
                header_lines.append(f"static {c_type} {name};")
            header_lines.append("")

        if self.uses_exceptions:
            header_lines.append(f"static const char* {self.exception_message_slot} = NULL;")
            header_lines.append("")
        
        # Add runtime helper functions if needed
        runtime_code = self._generate_runtime_functions()
        if runtime_code:
            header_lines.extend(runtime_code.split('\n'))
            header_lines.append("")

        # FFI safety macros (FORTIFY, Valgrind, nonnull attributes)
        ffi_macros = self._generate_ffi_safety_macros()
        if ffi_macros:
            header_lines.extend(ffi_macros.split('\n'))
            header_lines.append("")

        # Prepend header to output buffer
        self.output_buffer = header_lines + self.output_buffer
        
        return self.get_output()
    
    def _generate_function_declaration(self, node: FunctionDefinition) -> None:
        """Generate function declaration."""
        if self._has_top_level_yield(node):
            self._generate_yield_function_declaration(node)
            return

        # Determine return type
        if node.return_type:
            return_type = self._map_type(node.return_type)
        else:
            return_type = "void"
        
        # Track function return type for type inference
        self.function_types[node.name] = return_type
        
        # Generate parameter list
        if node.parameters:
            params = []
            for param in node.parameters:
                # Use explicit type annotation if provided
                if hasattr(param, 'type_annotation') and param.type_annotation:
                    param_type = self._map_type(param.type_annotation)
                else:
                    # Infer type from function body usage
                    param_type = self._infer_parameter_type(param.name, node.body, node.return_type)
                
                params.append(f"{param_type} {param.name}")
                # Add parameters to symbol table for type inference within function
                self.symbol_table[param.name] = param_type
            param_str = ", ".join(params)
        else:
            param_str = "void"
        
        # Function signature
        self.emit_raw(f"{return_type} {node.name}({param_str}) {{")
        self.indent()

        if node.name == "main" and self.top_level_statements:
            self.emit(f"{self.top_level_init_name}();")
        
        # Generate function body
        if node.body:
            for stmt in node.body:
                self._generate_statement(stmt)
        
        self.dedent()
        self.emit_raw("}")
        self.emit_raw("")
        
        # Clear function-local variables from symbol table
        # (Keep only global scope variables)
        if node.parameters:
            for param in node.parameters:
                if param.name in self.symbol_table:
                    del self.symbol_table[param.name]

    def _generate_top_level_init_function(self) -> None:
        """Emit initialization for top-level statements before user main runs."""
        self.emit_raw(f"static void {self.top_level_init_name}(void) {{")
        self.indent()
        self.inside_top_level_init = True
        try:
            for stmt in self.top_level_statements:
                self._generate_statement(stmt)
        finally:
            self.inside_top_level_init = False
        self.dedent()
        self.emit_raw("}")
        self.emit_raw("")

    def _can_materialize_global_variable(self, stmt: VariableDeclaration) -> bool:
        """Return True when a top-level variable can be represented as static storage."""
        if not isinstance(stmt.name, str) or getattr(stmt, 'value', None) is None:
            return False
        inferred_type = self._infer_type(stmt.value)
        return not inferred_type.endswith("[]")

    def _collect_macro_definition(self, node: MacroDefinition) -> None:
        """Collect macro definitions for compile-time expansion."""
        self.macro_definitions[node.name] = node

    def _has_top_level_yield(self, node: FunctionDefinition) -> bool:
        """Return True when a function body contains top-level yield statements."""
        if not getattr(node, 'body', None):
            return False
        return any(type(stmt).__name__ == 'YieldExpression' for stmt in node.body)

    def _resolve_symbol_name(self, name: str) -> str:
        """Resolve a logical symbol name to its emitted C identifier."""
        return self.symbol_aliases.get(name, name)

    def _collect_yield_locals(self, node: FunctionDefinition) -> Dict[str, str]:
        """Collect top-level local variables and their inferred C types."""
        local_types: Dict[str, str] = {}
        for stmt in getattr(node, 'body', []):
            if type(stmt).__name__ != 'VariableDeclaration':
                continue
            if not isinstance(stmt.name, str) or stmt.name in local_types:
                continue
            if getattr(stmt, 'var_type', None):
                local_types[stmt.name] = self._map_type(stmt.var_type)
            elif getattr(stmt, 'value', None) is not None:
                local_types[stmt.name] = self._infer_type(stmt.value)
            else:
                local_types[stmt.name] = 'int'
        return local_types

    def _emit_c_default_return(self, return_type: str) -> None:
        """Emit default return for exhausted yielded functions."""
        if return_type == 'void':
            self.emit("return;")
        elif return_type == 'double':
            self.emit("return 0.0;")
        elif return_type == 'const char*':
            self.emit('return "";')
        elif return_type == 'bool':
            self.emit("return false;")
        else:
            self.emit("return 0;")

    def _generate_yield_function_declaration(self, node: FunctionDefinition) -> None:
        """Generate function-body yield lowering as a resumable C state machine."""
        return_type = self._map_type(node.return_type) if node.return_type else 'int'
        self.function_types[node.name] = return_type

        params = []
        param_types: Dict[str, str] = {}
        for param in node.parameters or []:
            if hasattr(param, 'type_annotation') and param.type_annotation:
                param_type = self._map_type(param.type_annotation)
            else:
                param_type = self._infer_parameter_type(param.name, node.body, node.return_type)
            params.append(f"{param_type} {param.name}")
            param_types[param.name] = param_type
        param_str = ", ".join(params) if params else "void"

        state_name = f"__yield_state_{node.name}"
        param_aliases = {name: f"__yield_param_{node.name}_{name}" for name in param_types}
        local_types = self._collect_yield_locals(node)
        local_aliases = {name: f"__yield_local_{node.name}_{name}" for name in local_types}

        saved_symbol_table = dict(self.symbol_table)
        saved_aliases = dict(self.symbol_aliases)

        self.symbol_table.update(param_types)
        self.symbol_table.update(local_types)
        self.symbol_aliases.update(param_aliases)
        self.symbol_aliases.update(local_aliases)

        self.emit_raw(f"{return_type} {node.name}({param_str}) {{")
        self.indent()
        self.emit(f"static int {state_name} = 0;")
        for name, c_type in param_types.items():
            self.emit(f"static {c_type} {param_aliases[name]};")
        for name, c_type in local_types.items():
            self.emit(f"static {c_type} {local_aliases[name]};")

        yield_points = [(i, stmt) for i, stmt in enumerate(node.body or []) if type(stmt).__name__ == 'YieldExpression']

        self.emit(f"switch ({state_name}) {{")
        self.indent()
        for i in range(len(yield_points) + 1):
            self.emit(f"case {i}: goto yield_state_{i};")
        self.emit("default: goto yield_done;")
        self.dedent()
        self.emit("}")

        self.emit("yield_state_0:")
        self.indent()
        for name in param_types:
            self.emit(f"{param_aliases[name]} = {name};")
        self.dedent()

        cursor = 0
        for i, (yield_idx, yield_stmt) in enumerate(yield_points):
            if i > 0:
                self.emit(f"yield_state_{i}:")
            self.indent()
            for stmt in node.body[cursor:yield_idx]:
                self._generate_statement(stmt)
            yield_value = self._generate_expression(yield_stmt.value) if getattr(yield_stmt, 'value', None) is not None else '0'
            self.emit(f"{state_name} = {i + 1};")
            self.emit(f"return {yield_value};")
            self.dedent()
            cursor = yield_idx + 1

        self.emit(f"yield_state_{len(yield_points)}:")
        self.indent()
        tail_terminated = False
        for stmt in node.body[cursor:]:
            if isinstance(stmt, ReturnStatement):
                expr = self._generate_expression(stmt.value) if stmt.value is not None else None
                self.emit(f"{state_name} = -1;")
                if expr is not None:
                    self.emit(f"return {expr};")
                else:
                    self.emit("return;")
                tail_terminated = True
                break
            self._generate_statement(stmt)
        if not tail_terminated:
            self.emit(f"{state_name} = -1;")
            self._emit_c_default_return(return_type)
        self.dedent()

        self.emit("yield_done:")
        self.indent()
        self._emit_c_default_return(return_type)
        self.dedent()
        self.dedent()
        self.emit_raw("}")
        self.emit_raw("")

        self.symbol_table = saved_symbol_table
        self.symbol_aliases = saved_aliases
    
    def _generate_class_definition(self, node: ClassDefinition) -> None:
        """Generate C struct and methods for a class definition."""
        class_name = node.name
        
        # Track properties for this class (including inherited)
        property_names = set()
        
        # Store parent class name for inheritance tracking
        parent_name = node.parent_classes[0] if node.parent_classes else None
        self.class_parents[class_name] = parent_name
        
        # Generate struct definition
        self.emit_raw(f"// Class: {class_name}")
        if parent_name:
            self.emit_raw(f"// Inherits from: {parent_name}")
        
        self.emit_raw(f"typedef struct {class_name} {{")
        self.indent()
        
        # Flatten inheritance: include parent class properties first
        # This allows direct access like dog->name instead of dog->parent.name
        all_properties = []
        if parent_name and parent_name in self.class_all_properties:
            for prop_name, c_type in self.class_all_properties[parent_name]:
                all_properties.append((prop_name, c_type))
                property_names.add(prop_name)
                # Track inherited property type
                self.property_types[(class_name, prop_name)] = c_type
                self.emit(f"{c_type} {prop_name};  // inherited from {parent_name}")
        
        # Generate this class's own properties
        for prop in node.properties:
            if prop.type_annotation:
                c_type = self._map_type(prop.type_annotation)
            else:
                # Try to infer from default value
                if hasattr(prop, 'default_value') and prop.default_value:
                    c_type = self._infer_type(prop.default_value)
                else:
                    c_type = "void*"  # Default to generic pointer if no type specified
            
            # Track property type for type inference
            self.property_types[(class_name, prop.name)] = c_type
            property_names.add(prop.name)
            all_properties.append((prop.name, c_type))
            
            self.emit(f"{c_type} {prop.name};")
        
        # Store all properties (for child class inheritance)
        self.class_all_properties[class_name] = all_properties
        self.class_properties[class_name] = property_names
        
        # Generate function pointers for methods
        for method in node.methods:
            # Determine return type
            if method.return_type:
                return_type = self._map_type(method.return_type)
            else:
                return_type = "void"
            
            # Generate method function pointer type
            # Methods get implicit 'this' pointer as first parameter
            param_types = [f"struct {class_name}*"]  # 'this' pointer
            
            if method.parameters:
                for param in method.parameters:
                    if hasattr(param, 'type_annotation') and param.type_annotation:
                        param_type = self._map_type(param.type_annotation)
                    else:
                        param_type = "void*"
                    param_types.append(param_type)
            
            params_str = ", ".join(param_types)
            self.emit(f"{return_type} (*{method.name})({params_str});")
        
        self.dedent()
        self.emit_raw(f"}} {class_name};")
        self.emit_raw("")
        
        # Generate method implementations
        for method in node.methods:
            self._generate_method_implementation(class_name, method)
        
        # Generate constructor function
        self._generate_constructor(class_name, node)
    
    def _generate_method_implementation(self, class_name: str, method: MethodDefinition) -> None:
        """Generate implementation for a class method."""
        # Determine return type
        if method.return_type:
            return_type = self._map_type(method.return_type)
        else:
            return_type = "void"
        
        # Generate parameters (with 'this' as first parameter)
        params = [f"{class_name}* this"]
        
        if method.parameters:
            for param in method.parameters:
                if hasattr(param, 'type_annotation') and param.type_annotation:
                    param_type = self._map_type(param.type_annotation)
                else:
                    param_type = "void*"
                params.append(f"{param_type} {param.name}")
                # Add to symbol table for method body
                self.symbol_table[param.name] = param_type
        
        params_str = ", ".join(params)
        
        # Method naming: ClassName_methodName
        method_c_name = f"{class_name}_{method.name}"
        
        # Store method return type for type inference
        self.function_types[method_c_name] = return_type
        
        self.emit_raw(f"{return_type} {method_c_name}({params_str}) {{")
        self.indent()
        
        # Set current class context for property access resolution
        self.current_class = class_name
        
        # Generate method body
        if method.body:
            for stmt in method.body:
                self._generate_statement(stmt)
        
        # Clear class context
        self.current_class = None
        
        self.dedent()
        self.emit_raw("}")
        self.emit_raw("")
        
        # Clear method parameters from symbol table
        if method.parameters:
            for param in method.parameters:
                if param.name in self.symbol_table:
                    del self.symbol_table[param.name]
    
    def _generate_constructor(self, class_name: str, node: ClassDefinition) -> None:
        """Generate constructor function for a class."""
        # stdlib.h is already included by default in generate()
        
        constructor_name = f"{class_name}_new"
        
        self.emit_raw(f"{class_name}* {constructor_name}() {{")
        self.indent()
        
        # Allocate memory for the object
        self.emit(f"{class_name}* obj = ({class_name}*)malloc(sizeof({class_name}));")
        
        # Initialize properties to zero/null
        for prop in node.properties:
            if prop.type_annotation:
                c_type = self._map_type(prop.type_annotation)
                if c_type == "int" or c_type == "double":
                    self.emit(f"obj->{prop.name} = 0;")
                elif c_type == "bool":
                    self.emit(f"obj->{prop.name} = false;")
                elif c_type == "const char*":
                    self.emit(f'obj->{prop.name} = "";')
                else:
                    self.emit(f"obj->{prop.name} = NULL;")
        
        # Bind methods to function pointers
        for method in node.methods:
            method_c_name = f"{class_name}_{method.name}"
            self.emit(f"obj->{method.name} = {method_c_name};")
        
        self.emit("return obj;")
        
        self.dedent()
        self.emit_raw("}")
        self.emit_raw("")
    
    def _generate_statement(self, node: Any) -> None:
        """Generate C code for a statement."""
        if isinstance(node, VariableDeclaration):
            self._generate_variable_declaration(node)
        elif isinstance(node, FunctionCall):
            expr = self._generate_expression(node)
            self.emit(f"{expr};")
        elif isinstance(node, FunctionDefinition):
            # Functions are generated separately, not inline
            # They should be handled at the top level
            pass
        elif isinstance(node, PrintStatement):
            expr = self._generate_expression(node)
            self.emit(f"{expr};")
        elif isinstance(node, InterfaceDefinition):
            # Interfaces are compile-time metadata only, no runtime code needed
            # Interface compliance is checked at compile-time
            pass
        elif isinstance(node, ExternFunctionDeclaration):
            self._generate_extern_function(node)
        elif isinstance(node, ReturnStatement):
            if node.value:
                expr = self._generate_expression(node.value)
                self.emit(f"return {expr};")
            else:
                self.emit("return;")
        elif isinstance(node, IfStatement):
            self._generate_if_statement(node)
        elif isinstance(node, WhileLoop):
            self._generate_while_loop(node)
        elif isinstance(node, ForLoop):
            self._generate_for_loop(node)
        elif isinstance(node, ParallelForLoop):
            self._generate_parallel_for_loop(node)
        elif isinstance(node, SwitchStatement):
            self._generate_switch_statement(node)
        elif isinstance(node, (TryCatch, TryCatchBlock)):
            self._generate_try_catch(node)
        elif isinstance(node, RaiseStatement):
            self._generate_raise_statement(node)
        elif isinstance(node, BreakStatement):
            self._generate_break_statement(node)
        elif isinstance(node, ContinueStatement):
            self._generate_continue_statement(node)
        elif isinstance(node, FallthroughStatement):
            self.emit("/* fallthrough */")
        elif isinstance(node, SendStatement):
            channel_expr = self._generate_expression(node.channel)
            value_expr = self._generate_expression(node.value)
            self.includes.add("<pthread.h>")
            self.needed_runtime_functions.add("nxl_channel_create")
            self.needed_runtime_functions.add("nxl_channel_send")
            self.needed_runtime_functions.add("nxl_channel_receive")
            self.needed_runtime_functions.add("nxl_channel_close")
            self.emit(f"nxl_channel_send({channel_expr}, (intptr_t)({value_expr}));")
        elif isinstance(node, CloseStatement):
            channel_expr = self._generate_expression(node.channel)
            self.includes.add("<pthread.h>")
            self.needed_runtime_functions.add("nxl_channel_create")
            self.needed_runtime_functions.add("nxl_channel_send")
            self.needed_runtime_functions.add("nxl_channel_receive")
            self.needed_runtime_functions.add("nxl_channel_close")
            self.emit(f"nxl_channel_close({channel_expr});")
        elif isinstance(node, (RequireStatement, EnsureStatement, GuaranteeStatement, InvariantStatement)):
            self._generate_contract_statement(node)
        elif isinstance(node, ExpectStatement):
            self._generate_expect_statement(node)
        elif isinstance(node, MatchExpression):
            self._generate_match_expression(node)
        elif isinstance(node, ComptimeExpression):
            self._generate_comptime_expression(node)
        elif isinstance(node, ComptimeConst):
            self._generate_comptime_const(node)
        elif isinstance(node, ComptimeAssert):
            self._generate_comptime_assert(node)
        elif isinstance(node, MacroDefinition):
            self._collect_macro_definition(node)
        elif isinstance(node, MacroExpansion):
            self._generate_macro_expansion(node)
        elif isinstance(node, MemberAssignment):
            # Member assignment: object.field = value
            target_expr = self._generate_expression(node.target)
            value_expr = self._generate_expression(node.value)
            self.emit(f"{target_expr} = {value_expr};")
        elif isinstance(node, IndexAssignment):
            # Index assignment: array[index] = value
            target = node.target  # IndexExpression
            value_expr = self._generate_expression(node.value)
            
            # Generate bounds-checked index assignment
            if isinstance(target.array_expr, Identifier):
                arr_name = target.array_expr.name
                if arr_name in self.array_sizes:
                    size = self.array_sizes[arr_name]
                    index_expr = self._generate_expression(target.index_expr)
                    # Use bounds check with assignment
                    self.emit(f"(nxl_bounds_check({index_expr}, {size}, \"{arr_name}\", __LINE__), {arr_name}[{index_expr}] = {value_expr});")
                    self.needed_runtime_functions.add("nxl_bounds_check")
                else:
                    # No size info - generate without bounds check
                    target_expr = self._generate_expression(target)
                    self.emit(f"{target_expr} = {value_expr};")
            else:
                # Complex target (nested access, etc.) - no bounds check
                target_expr = self._generate_expression(target)
                self.emit(f"{target_expr} = {value_expr};")
        elif isinstance(node, UnsafeBlock):
            self._generate_unsafe_block(node)
        else:
            # Try to handle as expression
            try:
                expr = self._generate_expression(node)
                self.emit(f"{expr};")
            except:
                self.emit(f"/* Unhandled statement: {type(node).__name__} */")
    
    def _generate_variable_declaration(self, node: VariableDeclaration) -> None:
        """Generate variable declaration or assignment."""
        # Check if the "name" is actually a MemberAccess (object.property = value)
        if isinstance(node.name, MemberAccess):
            # This is an assignment to a member: object.property = value
            lhs = self._generate_expression(node.name)
            value_expr = self._generate_expression(node.value)
            self.emit(f"{lhs} = {value_expr};")
            return
        
        emitted_name = self._resolve_symbol_name(node.name)

        # Spilled yielded-function locals already have storage; emit assignment only.
        if isinstance(node.name, str) and node.name in self.symbol_aliases:
            value_expr = self._generate_expression(node.value)
            if node.name not in self.symbol_table:
                self.symbol_table[node.name] = self._infer_type(node.value)
            self.emit(f"{emitted_name} = {value_expr};")
            return

        if self.inside_top_level_init and isinstance(node.name, str) and node.name in self.global_variables:
            value_expr = self._generate_expression(node.value)
            self.emit(f"{node.name} = {value_expr};")
            return

        # Check if variable already exists in symbol table
        if node.name in self.symbol_table:
            # Variable exists - generate assignment only
            value_expr = self._generate_expression(node.value)
            self.emit(f"{emitted_name} = {value_expr};")
        else:
            # New variable - generate declaration with type
            value_expr = self._generate_expression(node.value)
            var_type = self._infer_type(node.value)
            
            # Track array size for bounds checking
            if isinstance(node.value, ListExpression) and node.value.elements:
                self.array_sizes[node.name] = len(node.value.elements)
            
            # Store in symbol table
            self.symbol_table[node.name] = var_type
            
            # Handle array type declarations specially
            if "[]" in var_type:
                # Array type: extract base type and array dimensions
                # e.g., "int[][]" → base="int", dims="[][]"
                base_type = var_type
                dims = ""
                while base_type.endswith("[]"):
                    dims += "[]"
                    base_type = base_type[:-2]
                
                # For 2D arrays, we need to specify inner dimension sizes in C
                # e.g., int matrix[3][3] for a 3x3 matrix
                if isinstance(node.value, ListExpression) and node.value.elements:
                    if isinstance(node.value.elements[0], ListExpression):
                        # 2D array - determine inner dimension size
                        inner_size = len(node.value.elements[0].elements)
                        outer_size = len(node.value.elements)
                        self.emit(f"{base_type} {emitted_name}[{outer_size}][{inner_size}] = {value_expr};")
                    else:
                        # 1D array
                        self.emit(f"{base_type} {emitted_name}{dims} = {value_expr};")
                else:
                    self.emit(f"{base_type} {emitted_name}{dims} = {value_expr};")
            else:
                # Regular type
                self.emit(f"{var_type} {emitted_name} = {value_expr};")
    
    def _infer_parameter_type(self, param_name: str, body: List[Any], return_type: Any = None) -> str:
        """
        Infer parameter type from how it's used in the function body.
        
        Strategy:
        1. Check arithmetic operations (plus, minus, etc.) → int or double
        2. Check string operations (concatenation, print) → const char*
        3. Check comparison operations → infer from context
        4. If used in return statement, match return type
        5. Default to int for numeric contexts
        """
        if not body:
            return "int"  # Default
        
        # Analyze all usages of the parameter
        usages = self._find_parameter_usages(param_name, body)
        
        for usage_type in usages:
            if usage_type == "arithmetic":
                # Used in arithmetic → numeric type
                # Default to int unless we have evidence of floating point
                return "int"
            elif usage_type == "string":
                # Used as string (print, concatenation)
                return "const char*"
            elif usage_type == "return":
                # Returned directly → match return type
                if return_type:
                    return self._map_type(return_type)
        
        # Default to int (most common for general parameters)
        return "int"
    
    def _find_parameter_usages(self, param_name: str, body: List[Any]) -> List[str]:
        """
        Find how a parameter is used in the function body.
        Returns list of usage types: 'arithmetic', 'string', 'return', etc.
        """
        usages = []
        
        for stmt in body:
            if isinstance(stmt, VariableDeclaration):
                # Check if parameter is in the value expression
                usages.extend(self._analyze_expression_for_param(param_name, stmt.value))
            elif isinstance(stmt, ReturnStatement):
                if isinstance(stmt.value, Identifier) and stmt.value.name == param_name:
                    usages.append("return")
                else:
                    usages.extend(self._analyze_expression_for_param(param_name, stmt.value))
            elif isinstance(stmt, FunctionCall):
                # Check standalone function calls (like print statements)
                usages.extend(self._analyze_expression_for_param(param_name, stmt))
        
        return usages
    
    def _analyze_expression_for_param(self, param_name: str, expr: Any) -> List[str]:
        """Analyze an expression to see how a parameter is used."""
        usages = []
        
        if expr is None:
            return usages
        
        if isinstance(expr, Identifier) and expr.name == param_name:
            # Found the parameter, but need context to determine type
            return ["unknown"]
        
        elif isinstance(expr, BinaryOperation):
            from ...parser.lexer import TokenType
            
            # Check operator type
            op_type = expr.operator.type if hasattr(expr.operator, 'type') else expr.operator
            
            # Arithmetic operators
            arithmetic_ops = {
                TokenType.PLUS, TokenType.MINUS, 
                TokenType.TIMES, TokenType.DIVIDED_BY
            }
            
            if op_type in arithmetic_ops:
                # Check if parameter is in this operation
                if self._expression_contains_param(param_name, expr.left) or \
                   self._expression_contains_param(param_name, expr.right):
                    usages.append("arithmetic")
            
            # Recursively check sub-expressions
            usages.extend(self._analyze_expression_for_param(param_name, expr.left))
            usages.extend(self._analyze_expression_for_param(param_name, expr.right))
        
        elif isinstance(expr, FunctionCall):
            # Check if parameter is passed to print → string
            if expr.name == "print" or expr.name == "printf":
                for arg in expr.arguments:
                    if isinstance(arg, Identifier) and arg.name == param_name:
                        usages.append("string")
        
        return usages
    
    def _expression_contains_param(self, param_name: str, expr: Any) -> bool:
        """Check if an expression contains a parameter."""
        if isinstance(expr, Identifier):
            return expr.name == param_name
        elif isinstance(expr, BinaryOperation):
            return (self._expression_contains_param(param_name, expr.left) or
                    self._expression_contains_param(param_name, expr.right))
        return False
    
    def _infer_type_from_literal(self, node: Any) -> str:
        """Infer C type from a Literal AST node."""
        if node.type == "string":
            return "const char*"
        elif node.type == "integer":
            return "int"
        elif node.type == "float":
            return "double"
        elif node.type == "boolean":
            return "bool"
        else:
            return "void*"

    def _infer_type_from_binary_op(self, node: Any) -> str:
        """Infer C type from a BinaryOperation AST node."""
        left_type = self._infer_type(node.left)
        right_type = self._infer_type(node.right)

        if left_type == right_type and left_type in ["int", "double"]:
            return left_type

        if set([left_type, right_type]) == {"int", "double"}:
            return "double"

        if left_type == "int" and right_type == "int":
            return "int"

        from ...parser.lexer import TokenType
        comparison_ops = {
            TokenType.EQUAL_TO, TokenType.NOT_EQUAL_TO,
            TokenType.LESS_THAN, TokenType.GREATER_THAN,
            TokenType.LESS_THAN_OR_EQUAL_TO, TokenType.GREATER_THAN_OR_EQUAL_TO
        }

        op_type = node.operator.type if hasattr(node.operator, 'type') else node.operator
        if op_type in comparison_ops:
            return "bool"

        return left_type

    def _infer_member_access_type(self, expr: Any) -> str:
        """Infer C type for a MemberAccess expression."""
        obj_type = self._infer_type(expr.object_expr)

        # Handle case where object is wrapped in FunctionCall due to "call" keyword
        if isinstance(expr.object_expr, FunctionCall) and not expr.object_expr.arguments:
            obj_name = expr.object_expr.name
            if obj_name in self.symbol_table:
                obj_type = self.symbol_table[obj_name]

        # Strip pointer suffix to get class name
        class_name = obj_type[:-1] if obj_type.endswith("*") else obj_type

        # Method call: look up method return type
        is_method_call = expr.is_method_call or len(expr.arguments) > 0 or isinstance(expr.object_expr, FunctionCall)
        if is_method_call:
            method_key = f"{class_name}_{expr.member_name}"
            if method_key in self.function_types:
                return self.function_types[method_key]

        # Property: look up in property_types table
        if (class_name, expr.member_name) in self.property_types:
            return self.property_types[(class_name, expr.member_name)]

        return "void*"

    # Stdlib function return-type table shared by _infer_function_call_type
    _STDLIB_RETURN_TYPES: dict = {
        # String functions
        "length": "int",
        "uppercase": "char*",
        "lowercase": "char*",
        "concatenate": "char*",
        "contains": "bool",
        "substring": "char*",
        # Math functions
        "sqrt": "double",
        "abs": "int",
        "power": "double",
        "floor": "double",
        "ceil": "double",
        "round": "double",
        "sin": "double",
        "cos": "double",
        "tan": "double",
        # File I/O functions
        "read_file": "char*",
        "read_text": "char*",
        "write_file": "bool",
        "write_text": "bool",
        "append_file": "bool",
        "file_exists": "bool",
        "file_size": "long",
        "delete_file": "bool",
        "copy_file": "bool",
        "create_directory": "bool",
        "is_directory": "bool",
        "is_file": "bool",
        # Console I/O
        "read_line": "char*",
        "read_int": "int",
        "read_float": "double",
        # Array functions
        "array_length": "int",
        "array_push": "void",
        "array_pop": "void*",
        "array_get": "void*",
        "array_set": "void",
        "array_slice": "void*",
        "array_reverse": "void",
        "array_sort": "void",
        "array_find": "int",
        # Short array function names
        "arrlen": "int",
        "arrpush": "void*",
        "arrpop": "void*",
        "arrget": "void*",
        "arrset": "void",
        "arrslice": "void*",
        "arrreverse": "void",
        "arrsort": "void",
        "arrfind": "int",
        "arrclear": "void",
        "arrinsert": "void",
        "arrremove": "void*",
        # Additional string functions
        "index_of": "int",
        "replace": "char*",
        "trim": "char*",
        "split": "char**",
        "join": "char*",
        "starts_with": "bool",
        "ends_with": "bool",
        # Additional math functions
        "min": "int",
        "max": "int",
        "random": "double",
        "random_int": "int",
    }

    def _infer_function_call_type(self, expr: Any) -> str:
        """Infer C type for a FunctionCall expression."""
        if expr.name in self.function_types:
            return self.function_types[expr.name]
        if expr.name in self._STDLIB_RETURN_TYPES:
            return self._STDLIB_RETURN_TYPES[expr.name]
        return "int"

    def _infer_identifier_type(self, expr: Identifier) -> str:
        """Infer C type for an identifier expression."""
        # Check if it's a property in the current class
        if self.current_class and (self.current_class, expr.name) in self.property_types:
            return self.property_types[(self.current_class, expr.name)]
        # Look up variable type from symbol table
        if expr.name in self.symbol_table:
            return self.symbol_table[expr.name]
        if expr.name in self.global_variables:
            return self.global_variables[expr.name]
        return "int"

    def _infer_list_expression_type(self, expr: ListExpression) -> str:
        """Infer C type for a list expression."""
        # Infer array type from elements
        if not expr.elements:
            # Empty array defaults to int[]
            return "int[]"

        # Check all elements to find the most general type
        element_types = [self._infer_type(elem) for elem in expr.elements]

        # If any element is double, the array should be double
        if "double" in element_types:
            return "double[]"
        # If all are int, use int
        if all(t == "int" for t in element_types):
            return "int[]"
        # If all are const char*, use const char*
        if all(t == "const char*" for t in element_types):
            return "const char*[]"
        # If all are bool, use bool
        if all(t == "bool" for t in element_types):
            return "bool[]"
        # Otherwise use the first element's type
        return f"{element_types[0]}[]"

    def _infer_index_expression_type(self, expr: IndexExpression) -> str:
        """Infer C type for an index expression."""
        # Array indexing returns the element type
        array_type = self._infer_type(expr.array_expr)
        # Strip the [] suffix to get element type
        if array_type.endswith("[]"):
            return array_type[:-2]
        # If it's a pointer type (e.g., "int*"), strip the *
        if array_type.endswith("*"):
            return array_type[:-1]
        # Default to the same type (might be void*)
        return array_type

    def _infer_type(self, expr: Any) -> str:
        """Infer C type from NexusLang expression."""
        if isinstance(expr, Literal):
            return self._infer_type_from_literal(expr)

        if isinstance(expr, BinaryOperation):
            return self._infer_type_from_binary_op(expr)

        if isinstance(expr, Identifier):
            return self._infer_identifier_type(expr)

        if isinstance(expr, UnaryOperation):
            return self._infer_type(expr.operand)

        if isinstance(expr, ObjectInstantiation):
            return f"{expr.class_name}*"

        if isinstance(expr, MemberAccess):
            return self._infer_member_access_type(expr)

        if isinstance(expr, FunctionCall):
            return self._infer_function_call_type(expr)

        if isinstance(expr, ListExpression):
            return self._infer_list_expression_type(expr)

        if isinstance(expr, IndexExpression):
            return self._infer_index_expression_type(expr)

        if isinstance(expr, ChannelCreation):
            return "void*"

        if isinstance(expr, ReceiveExpression):
            return "intptr_t"

        if isinstance(expr, YieldExpression):
            return self._infer_type(expr.value) if getattr(expr, "value", None) is not None else "intptr_t"

        return "void*"
    
    def _generate_extern_function(self, node: ExternFunctionDeclaration) -> None:
        """Generate extern function declaration for FFI."""
        # For C, we need to add the function declaration to forward declarations
        
        # Map NexusLang type to C type
        return_type = self._map_type(node.return_type) if node.return_type else "void"
        
        # Build parameter list
        params = []
        for param in node.parameters:
            param_type = self._map_type(param.type_annotation) if param.type_annotation else "void*"
            params.append(f"{param_type} {param.name}")
        
        # Handle variadic functions
        if node.variadic:
            params.append("...")
        
        # Generate extern declaration
        params_str = ", ".join(params) if params else "void"
        extern_decl = f"extern {return_type} {node.name}({params_str});"
        
        if extern_decl not in self.forward_declarations:
            self.forward_declarations.append(extern_decl)
            
        # Add library to required libraries for linking
        if hasattr(node, 'library') and node.library:
            self.required_libraries.add(node.library)
    
    def _generate_if_statement(self, node: IfStatement) -> None:
        """Generate if statement."""
        condition = self._generate_expression(node.condition)
        
        self.emit(f"if ({condition}) {{")
        self.indent()
        
        if hasattr(node, 'then_block') and node.then_block:
            for stmt in node.then_block:
                self._generate_statement(stmt)
        
        self.dedent()
        self.emit("}")
        
        if hasattr(node, 'else_block') and node.else_block:
            self.emit("else {")
            self.indent()
            
            for stmt in node.else_block:
                self._generate_statement(stmt)
            
            self.dedent()
            self.emit("}")
    
    def _generate_while_loop(self, node: WhileLoop) -> None:
        """Generate while loop."""
        condition = self._generate_expression(node.condition)

        loop_id = self.loop_label_counter
        self.loop_label_counter += 1
        continue_label = f"__nxl_loop_continue_{loop_id}"
        break_label = f"__nxl_loop_break_{loop_id}"
        self._push_loop_control_context(getattr(node, 'label', None), continue_label, break_label)
        
        self.emit(f"while ({condition}) {{")
        self.indent()
        
        if node.body:
            for stmt in node.body:
                self._generate_statement(stmt)

        # Target for labeled continue statements in this loop.
        self.emit(f"{continue_label}: ;")
        
        self.dedent()
        self.emit("}")
        self._pop_loop_control_context()
        self.emit(f"{break_label}: ;")
    
    def _generate_for_loop(self, node: ForLoop) -> None:
        """Generate for loop."""
        # For NLPL's "for each x in collection" style loops
        # Generate as C for loop with index
        
        iterator = node.iterator
        loop_id = self.loop_label_counter
        self.loop_label_counter += 1
        continue_label = f"__nxl_loop_continue_{loop_id}"
        break_label = f"__nxl_loop_break_{loop_id}"
        self._push_loop_control_context(getattr(node, 'label', None), continue_label, break_label)
        
        # Determine the collection type and how to get its length
        collection_type = self._infer_type(node.iterable)
        
        # Check if we can determine the array size
        if isinstance(node.iterable, ListExpression):
            # Direct array literal - we need to declare a temporary array
            array_size = len(node.iterable.elements)
            
            # Infer element type
            element_type = self._infer_type(node.iterable.elements[0]) if node.iterable.elements else "int"
            
            # Generate the element expressions
            element_exprs = [self._generate_expression(elem) for elem in node.iterable.elements]
            elements_str = ", ".join(element_exprs)
            
            # Create a temporary array and then loop over it
            temp_array = f"_temp_arr_{id(node)}"
            
            self.emit(f"/* For each loop over array literal */")
            self.emit(f"{element_type} {temp_array}[] = {{{elements_str}}};")
            self.emit(f"for (int _i = 0; _i < {array_size}; _i++) {{")
            self.indent()
            self.emit(f"{element_type} {iterator} = {temp_array}[_i];")
            
        elif isinstance(node.iterable, Identifier):
            # Variable reference - look up its type
            var_name = node.iterable.name
            if var_name in self.symbol_table:
                var_type = self.symbol_table[var_name]
                
                if var_type.endswith("[]"):
                    # It's an array - get element type
                    element_type = var_type[:-2]
                    
                    # For arrays, we need sizeof to get length
                    # Note: This only works for stack-allocated arrays, not pointers
                    self.emit(f"/* For each loop over array */")
                    self.emit(f"for (int _i = 0; _i < sizeof({collection_expr})/sizeof({element_type}); _i++) {{")
                    self.indent()
                    self.emit(f"{element_type} {iterator} = {collection_expr}[_i];")
                else:
                    # Unknown collection type - use placeholder
                    self.emit(f"/* For each loop - collection type unknown */")
                    self.emit(f"for (int _i = 0; _i < /* length */; _i++) {{")
                    self.indent()
                    self.emit(f"/* Unknown type */ {iterator} = {collection_expr}[_i];")
            else:
                # Variable not in symbol table
                self.emit(f"/* For each loop - variable not found */")
                self.emit(f"for (int _i = 0; _i < /* length */; _i++) {{")
                self.indent()
                self.emit(f"int {iterator} = {collection_expr}[_i];")
        else:
            # Some other expression
            self.emit(f"/* For each loop - generic */")
            self.emit(f"for (int _i = 0; _i < /* length */; _i++) {{")
            self.indent()
            self.emit(f"int {iterator} = {collection_expr}[_i];")
        
        # Generate loop body
        if node.body:
            for stmt in node.body:
                self._generate_statement(stmt)

        # Target for labeled continue statements in this loop.
        self.emit(f"{continue_label}: ;")
        
        self.dedent()
        self.emit("}")
        self._pop_loop_control_context()
        self.emit(f"{break_label}: ;")

    def _push_loop_control_context(self, loop_name: str, continue_label: str, break_label: str) -> None:
        """Track loop control labels for labeled break/continue lowering."""
        self.loop_control_stack.append((continue_label, break_label, loop_name))
        if loop_name:
            self.labeled_loop_controls[loop_name] = (continue_label, break_label)

    def _pop_loop_control_context(self) -> None:
        """Pop loop control labels when loop emission is complete."""
        if not self.loop_control_stack:
            return
        _, _, loop_name = self.loop_control_stack.pop()
        if loop_name and loop_name in self.labeled_loop_controls:
            del self.labeled_loop_controls[loop_name]

    def _generate_break_statement(self, node: BreakStatement) -> None:
        """Generate break statement with optional loop label support."""
        if getattr(node, 'label', None):
            target = self.labeled_loop_controls.get(node.label)
            if target is None:
                self.emit(f'/* ERROR: Label "{node.label}" not found */')
                return
            _, break_label = target
            self.emit(f"goto {break_label};")
            return

        self.emit("break;")

    def _generate_continue_statement(self, node: ContinueStatement) -> None:
        """Generate continue statement with optional loop label support."""
        if getattr(node, 'label', None):
            target = self.labeled_loop_controls.get(node.label)
            if target is None:
                self.emit(f'/* ERROR: Label "{node.label}" not found */')
                return
            continue_label, _ = target
            self.emit(f"goto {continue_label};")
            return

        self.emit("continue;")

    def _generate_parallel_for_loop(self, node: ParallelForLoop) -> None:
        """Generate parallel-for loop (sequential fallback in C backend)."""
        self.emit("/* parallel-for lowered to sequential loop */")

        iterator = node.var_name
        iterable = node.iterable

        if isinstance(iterable, ListExpression):
            array_size = len(iterable.elements)
            element_type = self._infer_type(iterable.elements[0]) if iterable.elements else "int"
            element_exprs = [self._generate_expression(elem) for elem in iterable.elements]
            elements_str = ", ".join(element_exprs)
            temp_array = f"_temp_arr_{id(node)}"
            self.emit(f"{element_type} {temp_array}[] = {{{elements_str}}};")
            self.emit(f"for (int _i = 0; _i < {array_size}; _i++) {{")
            self.indent()
            self.emit(f"{element_type} {iterator} = {temp_array}[_i];")
        elif isinstance(iterable, Identifier):
            collection_expr = iterable.name
            var_type = self.symbol_table.get(collection_expr, "intptr_t[]")
            element_type = var_type[:-2] if var_type.endswith("[]") else "intptr_t"
            size = self.array_sizes.get(collection_expr, 0)
            self.emit(f"for (int _i = 0; _i < {size}; _i++) {{")
            self.indent()
            self.emit(f"{element_type} {iterator} = {collection_expr}[_i];")
        else:
            collection_expr = self._generate_expression(iterable)
            self.emit("for (int _i = 0; _i < 0; _i++) {")
            self.indent()
            self.emit(f"int {iterator} = {collection_expr}[_i];")

        if node.body:
            for stmt in node.body:
                self._generate_statement(stmt)

        self.dedent()
        self.emit("}")

    def _generate_contract_statement(self, node: Any) -> None:
        """Generate runtime checks for contract statements."""
        condition = self._generate_expression(node.condition)
        kind = type(node).__name__.replace("Statement", "").lower()
        self._emit_contract_guard(
            condition,
            getattr(node, "message_expr", None),
            f"{kind} contract failed",
        )

    def _generate_expect_statement(self, node: ExpectStatement) -> None:
        """Generate runtime checks for expect assertions."""
        actual = self._generate_expression(node.actual_expr)
        expected = self._generate_expression(node.expected_expr) if getattr(node, "expected_expr", None) else None
        matcher = getattr(node, "matcher", "equal")

        condition = "(0)"
        if matcher == "equal" and expected is not None:
            condition = f"(({actual}) == ({expected}))"
        elif matcher == "greater_than" and expected is not None:
            condition = f"(({actual}) > ({expected}))"
        elif matcher == "less_than" and expected is not None:
            condition = f"(({actual}) < ({expected}))"
        elif matcher == "greater_than_or_equal_to" and expected is not None:
            condition = f"(({actual}) >= ({expected}))"
        elif matcher == "less_than_or_equal_to" and expected is not None:
            condition = f"(({actual}) <= ({expected}))"
        elif matcher == "be_true":
            condition = f"({actual})"
        elif matcher == "be_false":
            condition = f"(!({actual}))"
        elif matcher == "be_null":
            condition = f"(({actual}) == NULL)"
        elif matcher == "contain" and expected is not None:
            condition = f"(strstr({actual}, {expected}) != NULL)"
        elif matcher == "start_with" and expected is not None:
            condition = f"(strncmp({actual}, {expected}, strlen({expected})) == 0)"
        elif matcher == "end_with" and expected is not None:
            condition = f"(strlen({actual}) >= strlen({expected}) && strcmp({actual} + strlen({actual}) - strlen({expected}), {expected}) == 0)"
        elif matcher == "approximate_equal" and expected is not None:
            self.includes.add("<math.h>")
            tolerance = self._generate_expression(node.tolerance_expr) if getattr(node, "tolerance_expr", None) else "1e-6"
            condition = f"(fabs(({actual}) - ({expected})) <= ({tolerance}))"

        if getattr(node, "negated", False):
            condition = f"(!({condition}))"

        self._emit_contract_guard(
            condition,
            getattr(node, "message_expr", None),
            f"expect assertion failed ({matcher})",
        )

    def _generate_match_expression(self, node: MatchExpression) -> None:
        """Generate statement-level pattern matching as a guarded case chain."""
        match_value_type = self._infer_type(node.expression)
        unique_suffix = f"{len(self.symbol_aliases)}_{len(self.output_buffer)}"
        done_var = f"__nxl_match_done_{unique_suffix}"
        source_name = node.expression.name if isinstance(node.expression, Identifier) else None

        if isinstance(node.expression, Identifier):
            match_var = self._resolve_symbol_name(node.expression.name)
            emit_match_decl = False
        else:
            match_var = f"__nxl_match_value_{unique_suffix}"
            emit_match_decl = "[]" not in match_value_type

        self.emit("{")
        self.indent()
        if emit_match_decl:
            match_value_expr = self._generate_expression(node.expression)
            self.emit(f"{match_value_type} {match_var} = {match_value_expr};")
        elif not isinstance(node.expression, Identifier):
            raise RuntimeError("C backend cannot match directly on non-identifier array/list expressions")

        self.emit(f"bool {done_var} = false;")

        for case_index, case in enumerate(node.cases):
            condition_expr = self._generate_match_condition(case.pattern, match_var, match_value_type, source_name)
            if condition_expr is None:
                raise RuntimeError(
                    f"C backend does not yet support pattern type '{type(case.pattern).__name__}'"
                )

            self.emit(f"if (!{done_var}) {{")
            self.indent()
            self.emit(f"bool __nxl_case_match_{case_index} = {condition_expr};")

            saved_symbol_table = dict(self.symbol_table)
            saved_aliases = dict(self.symbol_aliases)
            try:
                self.emit(f"if (__nxl_case_match_{case_index}) {{")
                self.indent()
                self._generate_pattern_bindings(case.pattern, match_var, match_value_type, source_name)
                if getattr(case, 'guard', None) is not None:
                    guard_expr = self._generate_expression(case.guard)
                    self.emit(f"__nxl_case_match_{case_index} = ({guard_expr});")
                self.emit(f"if (__nxl_case_match_{case_index}) {{")
                self.indent()
                for stmt in case.body:
                    self._generate_statement(stmt)
                self.emit(f"{done_var} = true;")
                self.dedent()
                self.emit("}")
                self.dedent()
                self.emit("}")
            finally:
                self.symbol_table = saved_symbol_table
                self.symbol_aliases = saved_aliases

            self.dedent()
            self.emit("}")

        self.emit(f"if (!{done_var}) {{")
        self.indent()
        self.emit('fprintf(stderr, "%s\\n", "Non-exhaustive pattern match");')
        self.emit("exit(1);")
        self.dedent()
        self.emit("}")
        self.dedent()
        self.emit("}")

    def _variant_suffix(self, variant_name: str) -> str:
        """Return normalized suffix for dotted variant names."""
        if not variant_name:
            return ""
        return variant_name.split('.')[-1]

    def _ensure_forward_declaration(self, declaration: str) -> None:
        """Add a forward declaration once."""
        if declaration not in self.forward_declarations:
            self.forward_declarations.append(declaration)

    def _list_element_type(self, list_type: str) -> str:
        """Infer element type from list type notation like int[] or int[][]."""
        if isinstance(list_type, str) and list_type.endswith("[]"):
            return list_type[:-2] or "intptr_t"
        return "intptr_t"

    def _is_pointer_like_c_type(self, c_type: str) -> bool:
        """Return True for pointer/array C types."""
        if not isinstance(c_type, str):
            return False
        return "*" in c_type or c_type.endswith("[]")

    def _supports_scalar_variant_match(self, c_type: str) -> bool:
        """Return True for scalar integer-like C types used in fallback Option/Result matching."""
        if not isinstance(c_type, str):
            return False
        if self._is_pointer_like_c_type(c_type):
            return False
        normalized = c_type.replace("const", "").replace("volatile", "").strip()
        integer_like = {
            "bool", "char", "signed char", "unsigned char",
            "short", "unsigned short", "int", "unsigned int",
            "long", "unsigned long", "long long", "unsigned long long",
            "intptr_t", "uintptr_t", "size_t"
        }
        return normalized in integer_like

    def _generate_match_condition(self, pattern: Any, match_var: str, match_type: str, source_name: str | None) -> str | None:
        """Return a C condition expression for supported pattern kinds."""
        if isinstance(pattern, WildcardPattern):
            return "true"
        if isinstance(pattern, IdentifierPattern):
            return "true"
        if isinstance(pattern, LiteralPattern):
            literal_expr = self._generate_expression(pattern.value)
            literal_type = self._infer_type(pattern.value)
            if literal_type == "const char*":
                return f"strcmp({match_var}, {literal_expr}) == 0"
            return f"{match_var} == {literal_expr}"

        if isinstance(pattern, OptionPattern):
            if self._supports_scalar_variant_match(match_type):
                has_value = f"(((intptr_t)({match_var})) != 0)"
            else:
                self._ensure_forward_declaration("extern bool NLPL_Optional_has_value(void* opt);")
                has_value = f"NLPL_Optional_has_value((void*)({match_var}))"
            return has_value if pattern.variant == "Some" else f"(!{has_value})"

        if isinstance(pattern, ResultPattern):
            if self._supports_scalar_variant_match(match_type):
                is_ok = f"(((intptr_t)({match_var})) >= 0)"
            else:
                self._ensure_forward_declaration("extern bool NLPL_Result_is_ok(void* res);")
                is_ok = f"NLPL_Result_is_ok((void*)({match_var}))"
            return is_ok if pattern.variant == "Ok" else f"(!{is_ok})"

        if isinstance(pattern, VariantPattern):
            variant = self._variant_suffix(pattern.variant_name)
            if variant in ("Some", "None"):
                if self._supports_scalar_variant_match(match_type):
                    has_value = f"(((intptr_t)({match_var})) != 0)"
                else:
                    self._ensure_forward_declaration("extern bool NLPL_Optional_has_value(void* opt);")
                    has_value = f"NLPL_Optional_has_value((void*)({match_var}))"
                return has_value if variant == "Some" else f"(!{has_value})"
            if variant in ("Ok", "Err"):
                if self._supports_scalar_variant_match(match_type):
                    is_ok = f"(((intptr_t)({match_var})) >= 0)"
                else:
                    self._ensure_forward_declaration("extern bool NLPL_Result_is_ok(void* res);")
                    is_ok = f"NLPL_Result_is_ok((void*)({match_var}))"
                return is_ok if variant == "Ok" else f"(!{is_ok})"
            return None

        if isinstance(pattern, TuplePattern):
            if source_name is None or source_name not in self.array_sizes:
                return None
            tuple_len = self.array_sizes[source_name]
            if tuple_len != len(pattern.patterns):
                return "false"
            conditions = []
            elem_type = self._list_element_type(match_type)
            for index, elem_pattern in enumerate(pattern.patterns):
                element_expr = f"{match_var}[{index}]"
                element_cond = self._generate_match_condition(elem_pattern, element_expr, elem_type, None)
                if element_cond is None:
                    return None
                conditions.append(f"({element_cond})")
            return " && ".join(conditions) if conditions else "true"

        if isinstance(pattern, ListPattern):
            if source_name is None or source_name not in self.array_sizes:
                return None
            list_len = self.array_sizes[source_name]
            min_len = len(pattern.patterns)
            if pattern.rest_binding:
                length_cond = f"({list_len} >= {min_len})"
            else:
                length_cond = f"({list_len} == {min_len})"
            conditions = [length_cond]
            elem_type = self._list_element_type(match_type)
            for index, elem_pattern in enumerate(pattern.patterns):
                element_expr = f"{match_var}[{index}]"
                element_cond = self._generate_match_condition(elem_pattern, element_expr, elem_type, None)
                if element_cond is None:
                    return None
                conditions.append(f"({element_cond})")
            return " && ".join(conditions)

        return None

    def _generate_pattern_bindings(self, pattern: Any, match_var: str, match_type: str, source_name: str | None) -> None:
        """Emit local bindings for supported pattern variables."""
        if isinstance(pattern, IdentifierPattern):
            bound_name = pattern.name
            emitted_name = f"__match_bind_{bound_name}_{len(self.symbol_aliases)}_{len(self.output_buffer)}"
            self.symbol_table[bound_name] = match_type
            self.symbol_aliases[bound_name] = emitted_name
            self.emit(f"{match_type} {emitted_name} = {match_var};")
            return

        if isinstance(pattern, OptionPattern) and pattern.binding:
            bound_name = pattern.binding
            emitted_name = f"__match_bind_{bound_name}_{len(self.symbol_aliases)}_{len(self.output_buffer)}"
            self.symbol_table[bound_name] = "intptr_t"
            self.symbol_aliases[bound_name] = emitted_name
            if self._supports_scalar_variant_match(match_type):
                self.emit(f"intptr_t {emitted_name} = (intptr_t)({match_var});")
            else:
                self._ensure_forward_declaration("extern intptr_t NLPL_Optional_get_value(void* opt);")
                self.emit(f"intptr_t {emitted_name} = NLPL_Optional_get_value((void*)({match_var}));")
            return

        if isinstance(pattern, ResultPattern) and pattern.binding:
            variant = pattern.variant
            bound_name = pattern.binding
            emitted_name = f"__match_bind_{bound_name}_{len(self.symbol_aliases)}_{len(self.output_buffer)}"
            if variant == "Ok":
                self.symbol_table[bound_name] = "intptr_t"
                self.symbol_aliases[bound_name] = emitted_name
                if self._supports_scalar_variant_match(match_type):
                    self.emit(f"intptr_t {emitted_name} = (intptr_t)({match_var});")
                else:
                    self._ensure_forward_declaration("extern intptr_t NLPL_Result_get_value(void* res);")
                    self.emit(f"intptr_t {emitted_name} = NLPL_Result_get_value((void*)({match_var}));")
            else:
                self.symbol_table[bound_name] = "const char*"
                self.symbol_aliases[bound_name] = emitted_name
                if self._supports_scalar_variant_match(match_type):
                    self.emit(f"const char* {emitted_name} = \"error\";")
                else:
                    self._ensure_forward_declaration("extern const char* NLPL_Result_get_error(void* res);")
                    self.emit(f"const char* {emitted_name} = NLPL_Result_get_error((void*)({match_var}));")
            return

        if isinstance(pattern, VariantPattern) and pattern.bindings:
            variant = self._variant_suffix(pattern.variant_name)
            bound_name = pattern.bindings[0]
            emitted_name = f"__match_bind_{bound_name}_{len(self.symbol_aliases)}_{len(self.output_buffer)}"
            if variant == "Some":
                self.symbol_table[bound_name] = "intptr_t"
                self.symbol_aliases[bound_name] = emitted_name
                if self._supports_scalar_variant_match(match_type):
                    self.emit(f"intptr_t {emitted_name} = (intptr_t)({match_var});")
                else:
                    self._ensure_forward_declaration("extern intptr_t NLPL_Optional_get_value(void* opt);")
                    self.emit(f"intptr_t {emitted_name} = NLPL_Optional_get_value((void*)({match_var}));")
            elif variant == "Ok":
                self.symbol_table[bound_name] = "intptr_t"
                self.symbol_aliases[bound_name] = emitted_name
                if self._supports_scalar_variant_match(match_type):
                    self.emit(f"intptr_t {emitted_name} = (intptr_t)({match_var});")
                else:
                    self._ensure_forward_declaration("extern intptr_t NLPL_Result_get_value(void* res);")
                    self.emit(f"intptr_t {emitted_name} = NLPL_Result_get_value((void*)({match_var}));")
            elif variant == "Err":
                self.symbol_table[bound_name] = "const char*"
                self.symbol_aliases[bound_name] = emitted_name
                if self._supports_scalar_variant_match(match_type):
                    self.emit(f"const char* {emitted_name} = \"error\";")
                else:
                    self._ensure_forward_declaration("extern const char* NLPL_Result_get_error(void* res);")
                    self.emit(f"const char* {emitted_name} = NLPL_Result_get_error((void*)({match_var}));")
            return

        if isinstance(pattern, TuplePattern):
            elem_type = self._list_element_type(match_type)
            for index, elem_pattern in enumerate(pattern.patterns):
                self._generate_pattern_bindings(elem_pattern, f"{match_var}[{index}]", elem_type, None)
            return

        if isinstance(pattern, ListPattern):
            elem_type = self._list_element_type(match_type)
            for index, elem_pattern in enumerate(pattern.patterns):
                self._generate_pattern_bindings(elem_pattern, f"{match_var}[{index}]", elem_type, None)
            if pattern.rest_binding and source_name in self.array_sizes:
                bound_name = pattern.rest_binding
                emitted_name = f"__match_bind_{bound_name}_{len(self.symbol_aliases)}_{len(self.output_buffer)}"
                offset = len(pattern.patterns)
                self.symbol_table[bound_name] = f"{elem_type}*"
                self.symbol_aliases[bound_name] = emitted_name
                self.emit(f"{elem_type}* {emitted_name} = &{match_var}[{offset}];")
            return

    def _generate_comptime_expression(self, node: ComptimeExpression) -> None:
        """Lower comptime eval as normal expression evaluation."""
        expr = self._generate_expression(node.expr)
        if expr and not expr.startswith("/*"):
            self.emit(f"(void)({expr});")

    def _generate_comptime_const(self, node: ComptimeConst) -> None:
        """Lower comptime const to either global init or local declaration."""
        value_expr = self._generate_expression(node.expr)
        const_value = self._evaluate_constant_expr(node.expr)
        if const_value is not None:
            self.comptime_constant_values[node.name] = const_value

        if node.name in self.global_variables:
            self.emit(f"{node.name} = {value_expr};")
            return

        emitted_name = self._resolve_symbol_name(node.name)
        var_type = self._infer_type(node.expr)
        if node.name in self.symbol_table:
            self.emit(f"{emitted_name} = {value_expr};")
        else:
            self.symbol_table[node.name] = var_type
            self.emit(f"{var_type} {emitted_name} = {value_expr};")

    def _generate_comptime_assert(self, node: ComptimeAssert) -> None:
        """Lower comptime assert with compile-time folding when possible."""
        constant_value = self._evaluate_constant_expr(node.condition)
        if constant_value is not None:
            if bool(constant_value):
                return
            message = "Compile-time assertion failed"
            if getattr(node, 'message_expr', None) is not None:
                message_value = self._evaluate_constant_expr(node.message_expr)
                if message_value is not None:
                    message = f"Compile-time assertion failed: {message_value}"
            raise RuntimeError(message)

        condition = self._generate_expression(node.condition)
        self._emit_contract_guard(
            condition,
            getattr(node, "message_expr", None),
            "Compile-time assertion failed",
        )

    def _collect_macro_declared_names(self, node: Any, names: set) -> None:
        """Collect variable names declared within a macro body."""
        if node is None:
            return
        if isinstance(node, list):
            for item in node:
                self._collect_macro_declared_names(item, names)
            return

        if isinstance(node, VariableDeclaration) and isinstance(getattr(node, 'name', None), str):
            names.add(node.name)

        if isinstance(node, (FunctionDefinition, AsyncFunctionDefinition, LambdaExpression, ClassDefinition, MethodDefinition)):
            return

        if not hasattr(node, '__dict__'):
            return

        for value in vars(node).values():
            self._collect_macro_declared_names(value, names)

    def _substitute_macro_node(self, node: Any, argument_map: Dict[str, Any], local_renames: Dict[str, str]) -> Any:
        """Recursively substitute macro argument identifiers in an AST node."""
        if node is None:
            return None
        if isinstance(node, dict):
            return {
                key: self._substitute_macro_node(value, argument_map, local_renames)
                for key, value in node.items()
            }
        if isinstance(node, list):
            return [self._substitute_macro_node(item, argument_map, local_renames) for item in node]
        if isinstance(node, Identifier) and node.name in argument_map:
            return deepcopy(argument_map[node.name])
        if isinstance(node, Identifier) and node.name in local_renames:
            cloned_identifier = deepcopy(node)
            cloned_identifier.name = local_renames[node.name]
            return cloned_identifier
        if not hasattr(node, '__dict__'):
            return node

        cloned = deepcopy(node)
        if isinstance(cloned, VariableDeclaration) and isinstance(getattr(cloned, 'name', None), str):
            if cloned.name in local_renames:
                cloned.name = local_renames[cloned.name]
        for attr_name, attr_value in vars(cloned).items():
            setattr(cloned, attr_name, self._substitute_macro_node(attr_value, argument_map, local_renames))
        return cloned

    def _generate_macro_expansion(self, node: MacroExpansion) -> None:
        """Generate code for macro expansion via compile-time AST substitution."""
        if node.name not in self.macro_definitions:
            raise RuntimeError(f"Undefined macro: {node.name}")

        macro_def = self.macro_definitions[node.name]
        argument_map = node.arguments or {}
        for param in macro_def.parameters:
            if param not in argument_map:
                raise RuntimeError(f"Macro '{node.name}' requires argument '{param}'")

        declared_names = set()
        self._collect_macro_declared_names(macro_def.body, declared_names)
        self.macro_expansion_counter += 1
        local_renames = {
            name: f"__macro_{node.name}_{self.macro_expansion_counter}_{name}"
            for name in declared_names
        }

        saved_symbol_table = dict(self.symbol_table)
        saved_aliases = dict(self.symbol_aliases)

        self.emit("{")
        self.indent()
        try:
            for macro_stmt in macro_def.body:
                expanded_stmt = self._substitute_macro_node(macro_stmt, argument_map, local_renames)
                self._generate_statement(expanded_stmt)
        finally:
            self.symbol_table = saved_symbol_table
            self.symbol_aliases = saved_aliases
        self.dedent()
        self.emit("}")

    def _emit_contract_guard(self, condition_expr: str, message_expr: Any, default_message: str) -> None:
        """Emit C runtime guard that aborts execution on failed contract/assertion."""
        if message_expr is not None:
            message = self._generate_expression(message_expr)
        else:
            message = f'"{default_message}"'

        self.emit(f"if (!({condition_expr})) {{")
        self.indent()
        self.emit(f"fprintf(stderr, \"%s\\n\", (const char*)({message}));")
        self.emit("exit(1);")
        self.dedent()
        self.emit("}")
    
    def _generate_try_catch(self, node: Any) -> None:
        """Generate C code for try-catch error handling using setjmp/longjmp."""
        # Include setjmp.h for error handling
        self.includes.add("<setjmp.h>")
        self.uses_exceptions = True
        
        # Generate unique label for this try-catch block
        self.try_counter += 1
        jmp_buf_name = f"nxl_try_jmp_{self.try_counter}"
        
        # Declare jump buffer for this try-catch block
        self.emit(f"jmp_buf {jmp_buf_name};")
        self.emit(f"if (setjmp({jmp_buf_name}) == 0) {{")
        self.indent()

        self.try_stack.append(jmp_buf_name)
        
        # Generate try block code
        for stmt in self._iter_block_statements(node.try_block):
            self._generate_statement(stmt)

        self.try_stack.pop()
        
        self.dedent()
        self.emit("} else {")
        self.indent()
        
        # Generate catch block code
        # If there's an exception variable, declare it
        if hasattr(node, 'exception_var') and node.exception_var:
            self.emit(
                f"const char* {node.exception_var} = "
                f"{self.exception_message_slot} ? {self.exception_message_slot} : \"Error occurred\";"
            )
        
        for stmt in self._iter_block_statements(node.catch_block):
            self._generate_statement(stmt)
        
        self.dedent()
        self.emit("}")

    def _iter_block_statements(self, block_like: Any) -> List[Any]:
        """Return a statement list from list-based or Block-based AST nodes."""
        if block_like is None:
            return []
        if isinstance(block_like, list):
            return block_like
        if hasattr(block_like, "statements") and isinstance(block_like.statements, list):
            return block_like.statements
        return [block_like]

    def _generate_raise_statement(self, node: RaiseStatement) -> None:
        """Generate C raise/throw lowering with setjmp/longjmp parity."""
        message = self._generate_expression(node.message) if getattr(node, "message", None) is not None else '"Error raised"'

        if self.try_stack:
            self.includes.add("<setjmp.h>")
            self.uses_exceptions = True
            active_jmp = self.try_stack[-1]
            self.emit(f"{self.exception_message_slot} = (const char*)({message});")
            self.emit(f"longjmp({active_jmp}, 1);")
            return

        # Uncaught raise falls back to process termination with message.
        self.emit(f"fprintf(stderr, \"%s\\n\", (const char*)({message}));")
        self.emit("exit(1);")

    def _generate_switch_statement(self, node: SwitchStatement) -> None:
        """Generate C switch/case lowering including explicit fallthrough semantics."""
        switch_expr = self._generate_expression(node.expression)
        self.emit(f"switch ({switch_expr}) {{")
        self.indent()

        for case in node.cases:
            case_value = self._generate_expression(case.value)
            self.emit(f"case {case_value}:")
            self.indent()

            case_terminated = False
            has_fallthrough = False
            for stmt in case.body:
                self._generate_statement(stmt)
                stmt_name = type(stmt).__name__
                if stmt_name in ("BreakStatement", "ContinueStatement", "ReturnStatement"):
                    case_terminated = True
                elif stmt_name == "FallthroughStatement":
                    has_fallthrough = True
                    case_terminated = True

            if not case_terminated and not has_fallthrough:
                self.emit("break;")

            self.dedent()

        self.emit("default:")
        self.indent()
        if node.default_case:
            default_terminated = False
            for stmt in node.default_case:
                self._generate_statement(stmt)
                if type(stmt).__name__ in ("BreakStatement", "ContinueStatement", "ReturnStatement"):
                    default_terminated = True
            if not default_terminated:
                self.emit("break;")
        else:
            self.emit("break;")
        self.dedent()

        self.dedent()
        self.emit("}")
    
    def _generate_expression(self, node: Any) -> str:
        """Generate C expression code."""
        if isinstance(node, Literal):
            return self._generate_literal(node)
        
        elif isinstance(node, Identifier):
            # Check if we're in a method and this is a property reference
            if self.current_class and self.current_class in self.class_properties:
                if node.name in self.class_properties[self.current_class]:
                    return f"this->{node.name}"
            return self._resolve_symbol_name(node.name)
        
        elif isinstance(node, ObjectInstantiation):
            return self._generate_object_instantiation(node)
        
        elif isinstance(node, MemberAccess):
            return self._generate_member_access(node)
        
        elif isinstance(node, IndexExpression):
            return self._generate_index_expression(node)
        
        elif isinstance(node, ListExpression):
            return self._generate_list_expression(node)
        
        elif isinstance(node, DictExpression):
            return self._generate_dict_expression(node)
        
        elif isinstance(node, BinaryOperation):
            return self._generate_binary_operation(node)
        
        elif isinstance(node, UnaryOperation):
            return self._generate_unary_operation(node)
        
        elif isinstance(node, FunctionCall):
            return self._generate_function_call(node)

        elif isinstance(node, PrintStatement):
            expr_node = node.expression
            if isinstance(expr_node, list):
                if not expr_node:
                    return 'printf("\\n")'
                expr_node = expr_node[0]

            if expr_node is None:
                return 'printf("\\n")'

            arg_expr = self._generate_expression(expr_node)
            arg_type = self._infer_type(expr_node)

            if arg_type == "const char*":
                return f'printf("%s\\n", {arg_expr})'
            if arg_type == "double":
                return f'printf("%f\\n", {arg_expr})'
            if arg_type == "bool":
                return f'printf("%s\\n", ({arg_expr}) ? "true" : "false")'
            if arg_type.endswith('*'):
                return f'printf("%p\\n", {arg_expr})'
            return f'printf("%d\\n", {arg_expr})'

        elif isinstance(node, ChannelCreation):
            self.includes.add("<pthread.h>")
            self.needed_runtime_functions.add("nxl_channel_create")
            self.needed_runtime_functions.add("nxl_channel_send")
            self.needed_runtime_functions.add("nxl_channel_receive")
            self.needed_runtime_functions.add("nxl_channel_close")
            return "nxl_channel_create()"

        elif isinstance(node, ReceiveExpression):
            self.includes.add("<pthread.h>")
            self.needed_runtime_functions.add("nxl_channel_create")
            self.needed_runtime_functions.add("nxl_channel_send")
            self.needed_runtime_functions.add("nxl_channel_receive")
            self.needed_runtime_functions.add("nxl_channel_close")
            channel_expr = self._generate_expression(node.channel)
            return f"nxl_channel_receive({channel_expr})"

        elif isinstance(node, YieldExpression):
            if node.value is None:
                return "0"
            return self._generate_expression(node.value)
        
        else:
            return f"/* Unhandled expression: {type(node).__name__} */"
    
    def _generate_literal(self, node: Literal) -> str:
        """Generate literal value."""
        if node.type == "string":
            # Escape special characters in string
            escaped = str(node.value).replace('\\', '\\\\')  # Escape backslashes first
            escaped = escaped.replace('"', '\\"')  # Escape quotes
            escaped = escaped.replace('\n', '\\n')  # Escape newlines
            escaped = escaped.replace('\t', '\\t')  # Escape tabs
            escaped = escaped.replace('\r', '\\r')  # Escape carriage returns
            return f'"{escaped}"'
        elif node.type == "integer":
            return str(node.value)
        elif node.type == "float":
            return str(node.value)
        elif node.type == "boolean":
            return "true" if node.value else "false"
        else:
            return f"{node.value}"

    def _evaluate_constant_expr(self, expr: Any):
        """Try to evaluate an expression to a Python constant."""
        from ...parser.lexer import TokenType

        if expr is None:
            return None
        if isinstance(expr, Literal):
            return expr.value
        if isinstance(expr, Identifier):
            return self.comptime_constant_values.get(expr.name)
        if isinstance(expr, UnaryOperation):
            operand_val = self._evaluate_constant_expr(expr.operand)
            if operand_val is None:
                return None
            op = getattr(expr.operator, 'lexeme', str(expr.operator))
            if op in ('-', 'minus', 'negate'):
                return -operand_val
            if op in ('+', 'plus'):
                return operand_val
            return None
        if isinstance(expr, BinaryOperation):
            left_val = self._evaluate_constant_expr(expr.left)
            right_val = self._evaluate_constant_expr(expr.right)
            if left_val is None or right_val is None:
                return None
            op = getattr(expr.operator, 'lexeme', str(expr.operator))
            op_type = getattr(expr.operator, 'type', None)
            try:
                if op_type == TokenType.PLUS or op in ('+', 'plus'):
                    if isinstance(left_val, str) or isinstance(right_val, str):
                        return f"{left_val}{right_val}"
                    return left_val + right_val
                if op_type == TokenType.MINUS or op in ('-', 'minus'):
                    return left_val - right_val
                if op_type == TokenType.TIMES or op in ('*', 'times'):
                    return left_val * right_val
                if op_type == TokenType.DIVIDED_BY or op in ('/', 'divided_by'):
                    return left_val / right_val
                if op_type == TokenType.EQUAL_TO or op in ('==', 'equal', 'is equal to'):
                    return left_val == right_val
                if op_type == TokenType.NOT_EQUAL_TO or op in ('!=', 'not equal', 'is not equal to'):
                    return left_val != right_val
                if op_type == TokenType.LESS_THAN or op in ('<', 'less_than', 'is less than'):
                    return left_val < right_val
                if op_type == TokenType.LESS_THAN_OR_EQUAL_TO or op in ('<=', 'less_than_or_equal_to', 'is less than or equal to'):
                    return left_val <= right_val
                if op_type == TokenType.GREATER_THAN or op in ('>', 'greater_than', 'is greater than'):
                    return left_val > right_val
                if op_type == TokenType.GREATER_THAN_OR_EQUAL_TO or op in ('>=', 'greater_than_or_equal_to', 'is greater than or equal to'):
                    return left_val >= right_val
                if op_type == TokenType.AND or op in ('and', '&&'):
                    return bool(left_val) and bool(right_val)
                if op_type == TokenType.OR or op in ('or', '||'):
                    return bool(left_val) or bool(right_val)
            except Exception:
                return None
        return None
    
    def _generate_binary_operation(self, node: BinaryOperation) -> str:
        """Generate binary operation."""
        left = self._generate_expression(node.left)
        right = self._generate_expression(node.right)
        
        # Map NexusLang operators to C operators
        from ...parser.lexer import TokenType
        
        # Check for string concatenation (PLUS operator with string operands)
        if hasattr(node.operator, 'type'):
            op_type = node.operator.type
        else:
            op_type = node.operator
            
        # Handle string concatenation specially
        if op_type == TokenType.PLUS:
            # Infer types of operands
            left_type = self._infer_type(node.left)
            right_type = self._infer_type(node.right)
            
            # If both are strings, use string concatenation
            if left_type == "const char*" or right_type == "const char*":
                # Include string.h for string operations
                self.includes.add("<string.h>")
                
                # Generate code for string concatenation using malloc and strcpy/strcat
                # This allocates a new string with enough space for both strings
                return f"({{ char* _tmp = malloc(strlen({left}) + strlen({right}) + 1); strcpy(_tmp, {left}); strcat(_tmp, {right}); _tmp; }})"
        
        op_map = {
            TokenType.PLUS: "+",
            TokenType.MINUS: "-",
            TokenType.TIMES: "*",
            TokenType.DIVIDED_BY: "/",
            TokenType.MODULO: "%",
            TokenType.POWER: "pow",  # Will need to include math.h
            TokenType.FLOOR_DIVIDE: "/",  # C integer division is floor by default
            TokenType.EQUAL_TO: "==",
            TokenType.NOT_EQUAL_TO: "!=",
            TokenType.LESS_THAN: "<",
            TokenType.GREATER_THAN: ">",
            TokenType.LESS_THAN_OR_EQUAL_TO: "<=",
            TokenType.GREATER_THAN_OR_EQUAL_TO: ">=",
            TokenType.AND: "&&",
            TokenType.OR: "||",
            TokenType.BITWISE_AND: "&",
            TokenType.BITWISE_OR: "|",
            TokenType.BITWISE_XOR: "^",
            TokenType.LEFT_SHIFT: "<<",
            TokenType.RIGHT_SHIFT: ">>",
        }
        
        # Special handling for power operator
        if op_type == TokenType.POWER:
            self.includes.add("<math.h>")
            return f"pow({left}, {right})"
        
        c_op = op_map.get(op_type, "/* unknown operator */")
        
        return f"({left} {c_op} {right})"
    
    def _generate_unary_operation(self, node: UnaryOperation) -> str:
        """Generate unary operation."""
        operand = self._generate_expression(node.operand)
        
        from ...parser.lexer import TokenType
        
        op_map = {
            TokenType.NOT: "!",
            TokenType.MINUS: "-",
            TokenType.BITWISE_NOT: "~",
        }
        
        if hasattr(node.operator, 'type'):
            op_type = node.operator.type
        else:
            op_type = node.operator
        
        c_op = op_map.get(op_type, "/* unknown operator */")
        
        return f"({c_op}{operand})"
    
    def _generate_object_instantiation(self, node: ObjectInstantiation) -> str:
        """Generate C code for object instantiation (new ClassName)."""
        constructor_name = f"{node.class_name}_new"
        
        # For now, we don't pass constructor arguments (constructors with params not yet implemented)
        # Just call the constructor function
        return f"{constructor_name}()"
    
    def _generate_member_access(self, node: MemberAccess) -> str:
        """Generate C code for member access (object.property or object.method())."""
        # Get the object expression
        # Handle case where parser incorrectly creates FunctionCall for object name with "call" keyword
        if isinstance(node.object_expr, FunctionCall) and not node.object_expr.arguments:
            # This is actually just an identifier (e.g., "p" not "p()")
            object_expr = node.object_expr.name
            object_node = node.object_expr  # Keep the node for type checking
        else:
            object_expr = self._generate_expression(node.object_expr)
            object_node = node.object_expr
        
        # Determine if this is a method call
        # Check 1: Explicit method call flag
        # Check 2: Has arguments (definitely a method call)
        # Check 3: The member is a method (check if it's a function pointer in class)
        is_method = node.is_method_call or len(node.arguments) > 0
        
        # Check if the member is a known method by looking at the object type
        if not is_method and isinstance(object_node, (Identifier, FunctionCall)):
            obj_name = object_node.name if isinstance(object_node, (Identifier, FunctionCall)) else None
            if obj_name and obj_name in self.symbol_table:
                obj_type = self.symbol_table[obj_name]
                # If type is a class name (starts with uppercase or ends with *)
                if obj_type.endswith('*') or (obj_type and obj_type[0].isupper()):
                    class_name = obj_type.rstrip('*')
                    # Check if this member is a method in the class definition
                    # For now, assume members accessed with "call" keyword are methods
                    # This is indicated by the FunctionCall wrapper around the identifier
                    if isinstance(node.object_expr, FunctionCall):
                        is_method = True
        
        if is_method:
            # Method call: object->method(object, args...)
            # The first argument is 'this' pointer
            args = [object_expr]  # 'this' pointer
            for arg in node.arguments:
                args.append(self._generate_expression(arg))
            args_str = ", ".join(args)
            
            # Call through function pointer: object->method(object, ...)
            return f"{object_expr}->{node.member_name}({args_str})"
        else:
            # Property access: object->property
            return f"{object_expr}->{node.member_name}"
    
    def _generate_function_call(self, node: FunctionCall) -> str:
        """Generate function call."""
        # Handle built-in functions specially
        if node.name == "print":
            # Map to printf with appropriate format
            if node.arguments:
                # Infer type of argument to determine format specifier
                arg = node.arguments[0]
                arg_expr = self._generate_expression(arg)
                arg_type = self._infer_type(arg)
                
                # Choose format specifier based on type
                if arg_type == "const char*":
                    return f'printf("%s\\n", {arg_expr})'
                elif arg_type == "int":
                    return f'printf("%d\\n", {arg_expr})'
                elif arg_type == "double":
                    return f'printf("%f\\n", {arg_expr})'
                elif arg_type == "bool":
                    return f'printf("%s\\n", {arg_expr} ? "true" : "false")'
                else:
                    # Default to pointer
                    return f'printf("%p\\n", {arg_expr})'
            return 'printf("\\n")'
        
        # Map NexusLang stdlib functions to C equivalents
        elif node.name in self._get_stdlib_mappings():
            return self._generate_stdlib_call(node)
        
        else:
            # Regular function call
            if node.arguments:
                arg_exprs = [self._generate_expression(arg) for arg in node.arguments]
                args_str = ", ".join(arg_exprs)
            else:
                args_str = ""
            
            return f"{node.name}({args_str})"
    
    def _generate_index_expression(self, node: IndexExpression) -> str:
        """Generate C code for array indexing: array[index]."""
        array_expr = self._generate_expression(node.array_expr)
        index_expr = self._generate_expression(node.index_expr)
        
        # If bounds checking is enabled, use the bounds-checked accessor
        if self.bounds_checking:
            # Get array name for size lookup
            array_name = None
            if isinstance(node.array_expr, Identifier):
                array_name = node.array_expr.name
            
            # Try to get known array size
            if array_name and array_name in self.array_sizes:
                size = self.array_sizes[array_name]
                self.needed_runtime_functions.add("nxl_bounds_check")
                return f"(nxl_bounds_check({index_expr}, {size}, \"{array_name}\", __LINE__), {array_expr}[{index_expr}])"
        
        # In C, array indexing is straightforward: array[index]
        return f"{array_expr}[{index_expr}]"
    
    def _generate_list_expression(self, node: ListExpression) -> str:
        """Generate C code for list literal: [1, 2, 3]."""
        # For now, generate as C array initializer: {1, 2, 3}
        # In the future, this could create dynamic arrays or use a list struct
        if node.elements:
            element_exprs = [self._generate_expression(elem) for elem in node.elements]
            elements_str = ", ".join(element_exprs)
            return f"{{{elements_str}}}"
        else:
            # Empty list
            return "{}"
    
    def _generate_dict_expression(self, node: Any) -> str:
        """Generate C code for dictionary literal.
        
        Note: C doesn't have native dictionaries. This generates a comment for now.
        A real implementation would need a hash table library or struct-based approach.
        """
        # For now, generate a comment indicating this needs manual implementation
        # In the future, could use a hash table library like uthash
        return "/* Dictionary literals not yet supported in C generation - use a hash table library */"
    
    def _get_stdlib_mappings(self):
        """Get mapping of NexusLang stdlib functions to C functions."""
        return {
            # String functions
            "length": "nxl_string_length",
            "uppercase": "nxl_uppercase",
            "lowercase": "nxl_lowercase",
            "concatenate": "nxl_concat",
            "contains": "nxl_string_contains",
            "substring": "nxl_substring",
            "index_of": "nxl_index_of",
            "replace": "nxl_replace",
            "trim": "nxl_trim",
            "split": "nxl_split",
            "join": "nxl_join",
            "starts_with": "nxl_starts_with",
            "ends_with": "nxl_ends_with",
            
            # Array functions (full and short names)
            "array_length": "nxl_array_length",
            "array_push": "nxl_array_push",
            "array_pop": "nxl_array_pop",
            "array_get": "nxl_array_get",
            "array_set": "nxl_array_set",
            "array_slice": "nxl_array_slice",
            "array_reverse": "nxl_array_reverse",
            "array_sort": "nxl_array_sort",
            "array_find": "nxl_array_find",
            # Short array function names
            "arrlen": "nxl_array_length",
            "arrpush": "nxl_array_push",
            "arrpop": "nxl_array_pop",
            "arrget": "nxl_array_get",
            "arrset": "nxl_array_set",
            "arrslice": "nxl_array_slice",
            "arrreverse": "nxl_array_reverse",
            "arrsort": "nxl_array_sort",
            "arrfind": "nxl_array_find",
            "arrclear": "nxl_array_clear",
            "arrinsert": "nxl_array_insert",
            "arrremove": "nxl_array_remove",
            
            # Math functions
            "sqrt": "sqrt",
            "abs": "abs",
            "power": "pow",
            "floor": "floor",
            "ceil": "ceil",
            "round": "round",
            "sin": "sin",
            "cos": "cos",
            "tan": "tan",
            "min": "nxl_min",
            "max": "nxl_max",
            "random": "nxl_random",
            "random_int": "nxl_random_int",
            
            # File I/O functions
            "read_file": "nxl_read_file",
            "read_text": "nxl_read_file",
            "write_file": "nxl_write_file",
            "write_text": "nxl_write_file",
            "append_file": "nxl_append_file",
            "file_exists": "nxl_file_exists",
            "file_size": "nxl_file_size",
            "delete_file": "nxl_delete_file",
            "copy_file": "nxl_copy_file",
            "create_directory": "nxl_create_directory",
            "is_directory": "nxl_is_directory",
            "is_file": "nxl_is_file",
            
            # Console I/O
            "read_line": "nxl_read_line",
            "read_int": "nxl_read_int",
            "read_float": "nxl_read_float",
            "print_int": "nxl_print_int",
            "print_float": "nxl_print_float",
        }
    
    def _generate_stdlib_call(self, node: FunctionCall) -> str:
        """Generate stdlib function call with appropriate C mapping."""
        mapping = self._get_stdlib_mappings()[node.name]
        
        # Generate arguments
        if node.arguments:
            arg_exprs = [self._generate_expression(arg) for arg in node.arguments]
            args_str = ", ".join(arg_exprs)
        else:
            args_str = ""
        
        # Add required includes based on function
        if node.name in ["sqrt", "power", "abs", "floor", "ceil", "round", "sin", "cos", "tan"]:
            self.includes.add("<math.h>")
        elif node.name in ["length", "uppercase", "lowercase", "concatenate", "contains", "substring",
                           "index_of", "replace", "trim", "split", "join", "starts_with", "ends_with"]:
            self.includes.add("<string.h>")
            self.includes.add("<ctype.h>")
            self.needed_runtime_functions.add(mapping)
        elif node.name in ["array_length", "array_push", "array_pop", "array_get", "array_set",
                           "array_slice", "array_reverse", "array_sort", "array_find",
                           "arrlen", "arrpush", "arrpop", "arrget", "arrset",
                           "arrslice", "arrreverse", "arrsort", "arrfind", "arrclear",
                           "arrinsert", "arrremove"]:
            self.needed_runtime_functions.add(mapping)
        elif node.name in ["min", "max", "random", "random_int"]:
            self.needed_runtime_functions.add(mapping)
            if node.name in ["random", "random_int"]:
                self.includes.add("<time.h>")
        elif node.name in ["read_file", "read_text", "write_file", "write_text", "append_file", 
                          "file_exists", "file_size", "delete_file", "copy_file", 
                          "create_directory", "is_directory", "is_file"]:
            self.includes.add("<sys/stat.h>")
            self.needed_runtime_functions.add(mapping)
        elif node.name in ["read_line", "read_int", "read_float", "print_int", "print_float"]:
            self.needed_runtime_functions.add(mapping)
        
        return f"{mapping}({args_str})"
    
    def _map_type(self, nxl_type: str) -> str:
        """Map NexusLang type to C type."""
        if isinstance(nxl_type, str) and nxl_type.lower().startswith("channel"):
            return "void*"

        type_map = {
            "Integer": "int",
            "Float": "double",
            "String": "const char*",
            "Boolean": "bool",
            "Void": "void",
            "Nothing": "void",  # NexusLang uses "Nothing" for void return type
        }
        
        return type_map.get(nxl_type, "void*")
    
    def _generate_ffi_safety_macros(self) -> str:
        """Return a block of C preprocessor macros for FFI safety.

        Emits:
        - _FORTIFY_SOURCE=2 (buffer overflow detection in glibc)
        - GCC/Clang nonnull / returns_nonnull attribute helpers
        - Conditional Valgrind memcheck macro (no-op when Valgrind not present)

        These are emitted unconditionally: they are purely additive and safe
        even on MSVC (where the GNU attributes expand to nothing).
        """
        return (
            "/* NexusLang FFI Safety Macros -- auto-generated, do not edit */\n"
            "#ifndef _FORTIFY_SOURCE\n"
            "#  define _FORTIFY_SOURCE 2\n"
            "#endif\n"
            "#if defined(__GNUC__) || defined(__clang__)\n"
            "#  define NLPL_NONNULL(...) __attribute__((nonnull(__VA_ARGS__)))\n"
            "#  define NLPL_RETURNS_NONNULL __attribute__((returns_nonnull))\n"
            "#else\n"
            "#  define NLPL_NONNULL(...)\n"
            "#  define NLPL_RETURNS_NONNULL\n"
            "#endif\n"
            "#if defined(__has_include) && __has_include(<valgrind/memcheck.h>)\n"
            "#  include <valgrind/memcheck.h>\n"
            "#  define NLPL_VALGRIND_CHECK(p) \\\n"
            "       VALGRIND_CHECK_MEM_IS_ADDRESSABLE((p), sizeof(*(p)))\n"
            "#else\n"
            "#  define NLPL_VALGRIND_CHECK(p) ((void)0)\n"
            "#endif\n"
        )

    def _generate_unsafe_block(self, node: Any) -> None:
        """Generate C code for an 'unsafe' FFI block.

        The unsafe keyword is a compile-time annotation; in generated C there
        is no runtime overhead.  We emit a comment pair so that the block is
        clearly visible in the generated source for auditing purposes.
        """
        self.emit("/* unsafe block begin */")
        for stmt in node.body:
            self._generate_statement(stmt)
        self.emit("/* unsafe block end */")

    def _collect_bounds_and_ffi_runtime(self, code_parts: list) -> None:
        """Append bounds check and FFI pointer validation C implementations to code_parts."""
        if "nxl_bounds_check" in self.needed_runtime_functions:
            code_parts.append('''
void nxl_bounds_check(int index, int size, const char* array_name, int line) {
    if (index < 0 || index >= size) {
        fprintf(stderr, "NLPL Runtime Error: Array index out of bounds\\n");
        fprintf(stderr, "  Array '%s' has size %d, but index %d was accessed\\n", array_name, size, index);
        fprintf(stderr, "  at line %d\\n", line);
        exit(1);
    }
}''')

        if "nxl_ffi_check_ptr" in self.needed_runtime_functions:
            code_parts.append('''
// FFI pointer validation: checks for NULL and (when compiled with ASan) poisoned memory.
static inline void* nxl_ffi_check_ptr(void* ptr, const char* name, int line) {
    if (!ptr) {
        fprintf(stderr, "NLPL FFI Error: NULL pointer passed for parameter '%s' at line %d\\n", name, line);
        exit(1);
    }
#if defined(__has_feature)
#  if __has_feature(address_sanitizer)
    if (__asan_address_is_poisoned(ptr)) {
        fprintf(stderr, "NLPL FFI Error: poisoned pointer for parameter '%s' at line %d\\n", name, line);
        exit(1);
    }
#  endif
#endif
    NLPL_VALGRIND_CHECK(ptr);
    return ptr;
}''')

    def _collect_file_and_dir_runtime(self, code_parts: list) -> None:
        """Append file I/O and directory C implementations to code_parts."""
        if "nxl_read_file" in self.needed_runtime_functions:
            code_parts.append('''
char* nxl_read_file(const char* filepath) {
    FILE* file = fopen(filepath, "r");
    if (!file) return NULL;
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    char* buffer = (char*)malloc(size + 1);
    if (!buffer) { fclose(file); return NULL; }
    size_t read_size = fread(buffer, 1, size, file);
    buffer[read_size] = '\\0';
    fclose(file);
    return buffer;
}''')

        if "nxl_write_file" in self.needed_runtime_functions:
            code_parts.append('''
bool nxl_write_file(const char* filepath, const char* content) {
    FILE* file = fopen(filepath, "w");
    if (!file) return false;
    size_t len = strlen(content);
    size_t written = fwrite(content, 1, len, file);
    fclose(file);
    return written == len;
}''')

        if "nxl_append_file" in self.needed_runtime_functions:
            code_parts.append('''
bool nxl_append_file(const char* filepath, const char* content) {
    FILE* file = fopen(filepath, "a");
    if (!file) return false;
    size_t len = strlen(content);
    size_t written = fwrite(content, 1, len, file);
    fclose(file);
    return written == len;
}''')

        if "nxl_file_exists" in self.needed_runtime_functions:
            code_parts.append('''
bool nxl_file_exists(const char* filepath) {
    struct stat st;
    return stat(filepath, &st) == 0;
}''')

        if "nxl_file_size" in self.needed_runtime_functions:
            code_parts.append('''
long nxl_file_size(const char* filepath) {
    struct stat st;
    if (stat(filepath, &st) != 0) return -1;
    return st.st_size;
}''')

        if "nxl_delete_file" in self.needed_runtime_functions:
            code_parts.append('''
bool nxl_delete_file(const char* filepath) {
    return remove(filepath) == 0;
}''')

        if "nxl_copy_file" in self.needed_runtime_functions:
            code_parts.append('''
bool nxl_copy_file(const char* src, const char* dst) {
    FILE* source = fopen(src, "rb");
    if (!source) return false;
    FILE* dest = fopen(dst, "wb");
    if (!dest) { fclose(source); return false; }
    char buffer[8192];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), source)) > 0) {
        if (fwrite(buffer, 1, bytes_read, dest) != bytes_read) {
            fclose(source); fclose(dest); return false;
        }
    }
    fclose(source); fclose(dest);
    return true;
}''')

        if "nxl_create_directory" in self.needed_runtime_functions:
            code_parts.append('''
#ifdef _WIN32
#include <direct.h>
bool nxl_create_directory(const char* path) { return _mkdir(path) == 0; }
#else
bool nxl_create_directory(const char* path) { return mkdir(path, 0755) == 0; }
#endif''')

        if "nxl_is_directory" in self.needed_runtime_functions:
            code_parts.append('''
bool nxl_is_directory(const char* path) {
    struct stat st;
    if (stat(path, &st) != 0) return false;
    return S_ISDIR(st.st_mode);
}''')

        if "nxl_is_file" in self.needed_runtime_functions:
            code_parts.append('''
bool nxl_is_file(const char* path) {
    struct stat st;
    if (stat(path, &st) != 0) return false;
    return S_ISREG(st.st_mode);
}''')

    def _collect_string_runtime(self, code_parts: list) -> None:
        """Append string utility C implementations to code_parts."""
        if "nxl_string_length" in self.needed_runtime_functions:
            code_parts.append('''
int nxl_string_length(const char* str) {
    return str ? (int)strlen(str) : 0;
}''')

        if "nxl_uppercase" in self.needed_runtime_functions:
            code_parts.append('''
char* nxl_uppercase(const char* str) {
    if (!str) return NULL;
    size_t len = strlen(str);
    char* result = (char*)malloc(len + 1);
    if (!result) return NULL;
    for (size_t i = 0; i < len; i++) result[i] = toupper((unsigned char)str[i]);
    result[len] = '\\0';
    return result;
}''')

        if "nxl_lowercase" in self.needed_runtime_functions:
            code_parts.append('''
char* nxl_lowercase(const char* str) {
    if (!str) return NULL;
    size_t len = strlen(str);
    char* result = (char*)malloc(len + 1);
    if (!result) return NULL;
    for (size_t i = 0; i < len; i++) result[i] = tolower((unsigned char)str[i]);
    result[len] = '\\0';
    return result;
}''')

        if "nxl_concat" in self.needed_runtime_functions:
            code_parts.append('''
char* nxl_concat(const char* str1, const char* str2) {
    if (!str1) str1 = "";
    if (!str2) str2 = "";
    size_t len1 = strlen(str1), len2 = strlen(str2);
    char* result = (char*)malloc(len1 + len2 + 1);
    if (!result) return NULL;
    strcpy(result, str1);
    strcat(result, str2);
    return result;
}''')

        if "nxl_string_contains" in self.needed_runtime_functions:
            code_parts.append('''
bool nxl_string_contains(const char* str, const char* substr) {
    if (!str || !substr) return false;
    return strstr(str, substr) != NULL;
}''')

        if "nxl_substring" in self.needed_runtime_functions:
            code_parts.append('''
char* nxl_substring(const char* str, int start, int length) {
    if (!str || start < 0 || length <= 0) {
        char* empty = (char*)malloc(1);
        if (empty) empty[0] = '\\0';
        return empty;
    }
    int str_len = (int)strlen(str);
    if (start >= str_len) {
        char* empty = (char*)malloc(1);
        if (empty) empty[0] = '\\0';
        return empty;
    }
    if (start + length > str_len) length = str_len - start;
    char* result = (char*)malloc(length + 1);
    if (!result) return NULL;
    strncpy(result, str + start, length);
    result[length] = '\\0';
    return result;
}''')

        if "nxl_index_of" in self.needed_runtime_functions:
            code_parts.append('''
int nxl_index_of(const char* str, const char* substr) {
    if (!str || !substr) return -1;
    const char* found = strstr(str, substr);
    return found ? (int)(found - str) : -1;
}''')

        if "nxl_replace" in self.needed_runtime_functions:
            code_parts.append('''
char* nxl_replace(const char* str, const char* old_sub, const char* new_sub) {
    if (!str || !old_sub || !new_sub) return NULL;
    size_t str_len = strlen(str), old_len = strlen(old_sub), new_len = strlen(new_sub);
    if (old_len == 0) return strdup(str);
    
    // Count occurrences
    int count = 0;
    const char* p = str;
    while ((p = strstr(p, old_sub)) != NULL) { count++; p += old_len; }
    
    // Allocate result
    size_t result_len = str_len + count * (new_len - old_len);
    char* result = (char*)malloc(result_len + 1);
    if (!result) return NULL;
    
    // Build result
    char* r = result;
    p = str;
    while (*p) {
        if (strstr(p, old_sub) == p) {
            memcpy(r, new_sub, new_len);
            r += new_len;
            p += old_len;
        } else {
            *r++ = *p++;
        }
    }
    *r = '\\0';
    return result;
}''')

        if "nxl_trim" in self.needed_runtime_functions:
            code_parts.append('''
char* nxl_trim(const char* str) {
    if (!str) return NULL;
    while (isspace((unsigned char)*str)) str++;
    if (*str == '\\0') return strdup("");
    const char* end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    size_t len = end - str + 1;
    char* result = (char*)malloc(len + 1);
    if (!result) return NULL;
    memcpy(result, str, len);
    result[len] = '\\0';
    return result;
}''')

        if "nxl_starts_with" in self.needed_runtime_functions:
            code_parts.append('''
bool nxl_starts_with(const char* str, const char* prefix) {
    if (!str || !prefix) return false;
    return strncmp(str, prefix, strlen(prefix)) == 0;
}''')

        if "nxl_ends_with" in self.needed_runtime_functions:
            code_parts.append('''
bool nxl_ends_with(const char* str, const char* suffix) {
    if (!str || !suffix) return false;
    size_t str_len = strlen(str), suf_len = strlen(suffix);
    if (suf_len > str_len) return false;
    return strcmp(str + str_len - suf_len, suffix) == 0;
}''')

    def _collect_console_runtime(self, code_parts: list) -> None:
        """Append console I/O C implementations (read_line, read_int, read_float) to code_parts."""
        if "nxl_read_line" in self.needed_runtime_functions:
            code_parts.append('''
char* nxl_read_line(void) {
    char buffer[4096];
    if (!fgets(buffer, sizeof(buffer), stdin)) return NULL;
    size_t len = strlen(buffer);
    if (len > 0 && buffer[len - 1] == '\\n') buffer[len - 1] = '\\0';
    return strdup(buffer);
}''')

        if "nxl_read_int" in self.needed_runtime_functions:
            code_parts.append('''
int nxl_read_int(void) {
    int value; int c;
    if (scanf("%d", &value) != 1) return 0;
    while ((c = getchar()) != '\\n' && c != EOF);
    return value;
}''')

        if "nxl_read_float" in self.needed_runtime_functions:
            code_parts.append('''
double nxl_read_float(void) {
    double value; int c;
    if (scanf("%lf", &value) != 1) return 0.0;
    while ((c = getchar()) != '\\n' && c != EOF);
    return value;
}''')

    def _collect_array_runtime(self, code_parts: list) -> None:
        """Append dynamic array struct and all nxl_array_* C implementations to code_parts."""
        if any(fn.startswith("nxl_array_") for fn in self.needed_runtime_functions):
            self._append_array_struct_and_core(code_parts)
        
        self._append_array_basic_ops(code_parts)
        self._append_array_mutation_ops(code_parts)
        self._append_array_search_and_transform(code_parts)

    def _append_array_struct_and_core(self, code_parts: list) -> None:
        """Append array struct definition and core create/free functions."""
        code_parts.append('''
// Static array length macro (for compile-time known arrays)
#define NLPL_STATIC_ARRLEN(arr) (sizeof(arr) / sizeof((arr)[0]))

// Dynamic array structure for NexusLang arrays
typedef struct NLPLArray {
    void** data;
    int size;
    int capacity;
    size_t elem_size;
} NLPLArray;

NLPLArray* nxl_array_create(int initial_capacity, size_t elem_size) {
    NLPLArray* arr = (NLPLArray*)malloc(sizeof(NLPLArray));
    if (!arr) return NULL;
    arr->capacity = initial_capacity > 0 ? initial_capacity : 8;
    arr->size = 0;
    arr->elem_size = elem_size;
    arr->data = (void**)malloc(sizeof(void*) * arr->capacity);
    if (!arr->data) { free(arr); return NULL; }
    return arr;
}

NLPLArray* nxl_array_from_static_int(int* static_arr, int count) {
    NLPLArray* arr = nxl_array_create(count, sizeof(int));
    if (!arr) return NULL;
    for (int i = 0; i < count; i++) {
        arr->data[i] = (void*)(intptr_t)static_arr[i];
    }
    arr->size = count;
    return arr;
}

int* nxl_array_to_static_int(NLPLArray* arr) {
    if (!arr || arr->size == 0) return NULL;
    int* result = (int*)malloc(sizeof(int) * arr->size);
    if (!result) return NULL;
    for (int i = 0; i < arr->size; i++) {
        result[i] = (int)(intptr_t)arr->data[i];
    }
    return result;
}

void nxl_array_free(NLPLArray* arr) {
    if (arr) {
        if (arr->data) free(arr->data);
        free(arr);
    }
}''')

    def _append_array_basic_ops(self, code_parts: list) -> None:
        """Append basic array operations: length, get, set, push, pop."""
        if "nxl_array_length" in self.needed_runtime_functions:
            code_parts.append('''
int nxl_array_length(NLPLArray* arr) {
    return arr ? arr->size : 0;
}''')

        if "nxl_array_push" in self.needed_runtime_functions:
            code_parts.append('''
void nxl_array_push(NLPLArray* arr, void* elem) {
    if (!arr) return;
    if (arr->size >= arr->capacity) {
        arr->capacity *= 2;
        arr->data = (void**)realloc(arr->data, sizeof(void*) * arr->capacity);
        if (!arr->data) return;
    }
    arr->data[arr->size++] = elem;
}''')

        if "nxl_array_pop" in self.needed_runtime_functions:
            code_parts.append('''
void* nxl_array_pop(NLPLArray* arr) {
    if (!arr || arr->size == 0) return NULL;
    return arr->data[--arr->size];
}''')

        if "nxl_array_get" in self.needed_runtime_functions:
            code_parts.append('''
void* nxl_array_get(NLPLArray* arr, int index) {
    if (!arr || index < 0 || index >= arr->size) return NULL;
    return arr->data[index];
}''')

        if "nxl_array_set" in self.needed_runtime_functions:
            code_parts.append('''
void nxl_array_set(NLPLArray* arr, int index, void* elem) {
    if (!arr || index < 0 || index >= arr->size) return;
    arr->data[index] = elem;
}''')

    def _append_array_mutation_ops(self, code_parts: list) -> None:
        """Append array mutation operations: insert, remove, clear."""
        if "nxl_array_insert" in self.needed_runtime_functions:
            code_parts.append('''
void nxl_array_insert(NLPLArray* arr, int index, void* elem) {
    if (!arr || index < 0 || index > arr->size) return;
    if (arr->size >= arr->capacity) {
        arr->capacity *= 2;
        arr->data = (void**)realloc(arr->data, sizeof(void*) * arr->capacity);
        if (!arr->data) return;
    }
    for (int i = arr->size; i > index; i--) {
        arr->data[i] = arr->data[i - 1];
    }
    arr->data[index] = elem;
    arr->size++;
}''')

        if "nxl_array_remove" in self.needed_runtime_functions:
            code_parts.append('''
void* nxl_array_remove(NLPLArray* arr, int index) {
    if (!arr || index < 0 || index >= arr->size) return NULL;
    void* elem = arr->data[index];
    for (int i = index; i < arr->size - 1; i++) {
        arr->data[i] = arr->data[i + 1];
    }
    arr->size--;
    return elem;
}''')

        if "nxl_array_clear" in self.needed_runtime_functions:
            code_parts.append('''
void nxl_array_clear(NLPLArray* arr) {
    if (arr) arr->size = 0;
}''')

    def _append_array_search_and_transform(self, code_parts: list) -> None:
        """Append array search and transformation operations: find, reverse, slice, sort."""
        if "nxl_array_find" in self.needed_runtime_functions:
            code_parts.append('''
int nxl_array_find(NLPLArray* arr, void* elem) {
    if (!arr) return -1;
    for (int i = 0; i < arr->size; i++) {
        if (arr->data[i] == elem) return i;
    }
    return -1;
}''')

        if "nxl_array_reverse" in self.needed_runtime_functions:
            code_parts.append('''
void nxl_array_reverse(NLPLArray* arr) {
    if (!arr || arr->size < 2) return;
    for (int i = 0; i < arr->size / 2; i++) {
        void* temp = arr->data[i];
        arr->data[i] = arr->data[arr->size - 1 - i];
        arr->data[arr->size - 1 - i] = temp;
    }
}''')

        if "nxl_array_slice" in self.needed_runtime_functions:
            code_parts.append('''
NLPLArray* nxl_array_slice(NLPLArray* arr, int start, int end) {
    if (!arr || start < 0 || end > arr->size || start >= end) {
        return nxl_array_create(0, sizeof(void*));
    }
    int new_size = end - start;
    NLPLArray* result = nxl_array_create(new_size, arr->elem_size);
    if (!result) return NULL;
    for (int i = start; i < end; i++) {
        nxl_array_push(result, arr->data[i]);
    }
    return result;
}''')

        if "nxl_array_sort" in self.needed_runtime_functions:
            code_parts.append('''
static int nxl_int_compare(const void* a, const void* b) {
    intptr_t ia = (intptr_t)*(void**)a;
    intptr_t ib = (intptr_t)*(void**)b;
    return (ia > ib) - (ia < ib);
}

void nxl_array_sort(NLPLArray* arr) {
    if (!arr || arr->size < 2) return;
    qsort(arr->data, arr->size, sizeof(void*), nxl_int_compare);
}''')

    def _collect_math_runtime(self, code_parts: list) -> None:
        """Append math utility C implementations to code_parts."""
        if "nxl_min" in self.needed_runtime_functions:
            code_parts.append('''
int nxl_min(int a, int b) {
    return a < b ? a : b;
}''')

        if "nxl_max" in self.needed_runtime_functions:
            code_parts.append('''
int nxl_max(int a, int b) {
    return a > b ? a : b;
}''')

        if "nxl_random" in self.needed_runtime_functions:
            code_parts.append('''
static int nxl_random_seeded = 0;
double nxl_random(void) {
    if (!nxl_random_seeded) { srand((unsigned)time(NULL)); nxl_random_seeded = 1; }
    return (double)rand() / RAND_MAX;
}''')

        if "nxl_random_int" in self.needed_runtime_functions:
            code_parts.append('''
int nxl_random_int(int min_val, int max_val) {
    if (!nxl_random_seeded) { srand((unsigned)time(NULL)); nxl_random_seeded = 1; }
    return min_val + rand() % (max_val - min_val + 1);
}''')

    def _generate_runtime_functions(self) -> str:
        """Generate inline C implementations of NexusLang runtime functions."""
        if not self.needed_runtime_functions:
            return ""

        code_parts = ["// NexusLang Runtime Functions"]
        self._collect_bounds_and_ffi_runtime(code_parts)
        self._collect_file_and_dir_runtime(code_parts)
        self._collect_string_runtime(code_parts)
        self._collect_console_runtime(code_parts)
        self._collect_array_runtime(code_parts)
        self._collect_channel_runtime(code_parts)
        self._collect_math_runtime(code_parts)
        return "\n".join(code_parts)

    def _collect_channel_runtime(self, code_parts: list) -> None:
        """Append channel runtime helpers to code_parts."""
        required = {"nxl_channel_create", "nxl_channel_send", "nxl_channel_receive", "nxl_channel_close"}
        if not required.intersection(self.needed_runtime_functions):
            return

        code_parts.append('''
typedef struct NLPLChannelNode {
    intptr_t value;
    struct NLPLChannelNode* next;
} NLPLChannelNode;

typedef struct NLPLChannel {
    NLPLChannelNode* head;
    NLPLChannelNode* tail;
    int closed;
    pthread_mutex_t lock;
    pthread_cond_t has_data;
} NLPLChannel;

void* nxl_channel_create(void) {
    NLPLChannel* ch = (NLPLChannel*)malloc(sizeof(NLPLChannel));
    if (!ch) return NULL;
    ch->head = NULL;
    ch->tail = NULL;
    ch->closed = 0;
    if (pthread_mutex_init(&ch->lock, NULL) != 0) {
        free(ch);
        return NULL;
    }
    if (pthread_cond_init(&ch->has_data, NULL) != 0) {
        pthread_mutex_destroy(&ch->lock);
        free(ch);
        return NULL;
    }
    return (void*)ch;
}

void nxl_channel_send(void* channel, intptr_t value) {
    if (!channel) return;
    NLPLChannel* ch = (NLPLChannel*)channel;
    pthread_mutex_lock(&ch->lock);
    if (ch->closed) {
        pthread_mutex_unlock(&ch->lock);
        return;
    }
    NLPLChannelNode* node = (NLPLChannelNode*)malloc(sizeof(NLPLChannelNode));
    if (!node) {
        pthread_mutex_unlock(&ch->lock);
        return;
    }
    node->value = value;
    node->next = NULL;
    if (!ch->tail) {
        ch->head = node;
        ch->tail = node;
        pthread_cond_signal(&ch->has_data);
        pthread_mutex_unlock(&ch->lock);
        return;
    }
    ch->tail->next = node;
    ch->tail = node;
    pthread_cond_signal(&ch->has_data);
    pthread_mutex_unlock(&ch->lock);
}

intptr_t nxl_channel_receive(void* channel) {
    if (!channel) return 0;
    NLPLChannel* ch = (NLPLChannel*)channel;
    pthread_mutex_lock(&ch->lock);
    while (!ch->head && !ch->closed) {
        pthread_cond_wait(&ch->has_data, &ch->lock);
    }
    if (!ch->head && ch->closed) {
        pthread_mutex_unlock(&ch->lock);
        return 0;
    }
    NLPLChannelNode* node = ch->head;
    intptr_t value = node->value;
    ch->head = node->next;
    if (!ch->head) ch->tail = NULL;
    free(node);
    pthread_mutex_unlock(&ch->lock);
    return value;
}

void nxl_channel_close(void* channel) {
    if (!channel) return;
    NLPLChannel* ch = (NLPLChannel*)channel;
    pthread_mutex_lock(&ch->lock);
    ch->closed = 1;
    pthread_cond_broadcast(&ch->has_data);
    pthread_mutex_unlock(&ch->lock);
}''')