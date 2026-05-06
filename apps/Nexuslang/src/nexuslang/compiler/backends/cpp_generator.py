"""
C++ Code Generator for NexusLang
============================

Generates C++ code from NexusLang AST for compilation to native executables.
Extends C generator with OOP features, RAII, templates, etc.

Strategy:
- NexusLang classes → C++ classes
- NexusLang generics → C++ templates
- Memory management → RAII + smart pointers
- Standard library → C++ STL + NexusLang runtime
"""

from typing import Any, Dict, List
from nexuslang.compiler.backends.c_generator import CCodeGenerator
from nexuslang.parser.ast import *


class CppCodeGenerator(CCodeGenerator):
    """Generate C++ code from NexusLang AST."""
    
    def __init__(self, target: str):
        super().__init__(target)
        self.namespaces = []
        self.classes = {}
        self.ffi_functions = set()  # Track FFI function names for special handling
        
    def generate(self, ast: Program) -> str:
        """Generate complete C++ program from AST."""
        self.reset()
        
        # Add required includes
        self.includes.add("<iostream>")
        self.includes.add("<string>")
        self.includes.add("<vector>")
        self.includes.add("<memory>")
        self.includes.add("<cstdlib>")
        
        # Generate header
        self.emit_raw("/*")
        self.emit_raw(" * Generated C++ code from NLPL")
        self.emit_raw(" * DO NOT EDIT - This file is auto-generated")
        self.emit_raw(" */")
        self.emit_raw("")
        
        # Emit includes
        for include in sorted(self.includes):
            self.emit_raw(f"#include {include}")
        self.emit_raw("")
        
        # Use std namespace for convenience
        self.emit_raw("using namespace std;")
        self.emit_raw("")
        
        # First pass: collect extern declarations, classes, and functions
        for stmt in ast.statements:
            if isinstance(stmt, ExternFunctionDeclaration):
                self._generate_extern_function(stmt)
        
        # Forward declarations
        if self.forward_declarations:
            for decl in self.forward_declarations:
                self.emit_raw(decl)
            self.emit_raw("")
        
        # Generate class definitions
        for stmt in ast.statements:
            if isinstance(stmt, ClassDefinition):
                self._generate_class_definition(stmt)
        
        # Generate function definitions
        for stmt in ast.statements:
            if isinstance(stmt, FunctionDefinition):
                self._generate_cpp_function_declaration(stmt)
        
        # Generate main function
        self.emit_raw("int main(int argc, char** argv) {")
        self.indent()
        
        # Generate top-level statements
        for stmt in ast.statements:
            if not isinstance(stmt, (FunctionDefinition, ClassDefinition, ExternFunctionDeclaration)):
                self._generate_statement(stmt)
        
        self.emit("return 0;")
        self.dedent()
        self.emit_raw("}")
        
        return self.get_output()
    
    def _generate_class_definition(self, node: ClassDefinition) -> None:
        """Generate C++ class definition."""
        # Class declaration
        if node.parent_classes:
            # Use first parent class for single inheritance
            parent = node.parent_classes[0]
            self.emit_raw(f"class {node.name} : public {parent} {{")
        else:
            self.emit_raw(f"class {node.name} {{")
        
        self.emit_raw("public:")
        self.indent()
        
        # Generate constructor (look for special __init__ method)
        constructor = None
        for method in node.methods:
            if hasattr(method, 'name') and method.name == "__init__":
                constructor = method
                break
        
        if constructor:
            self._generate_constructor(node.name, constructor)
        
        # Generate methods (skip __init__)
        for method in node.methods:
            if not (hasattr(method, 'name') and method.name == "__init__"):
                self._generate_method(method)
        
        # Generate member variables
        self.dedent()
        self.emit_raw("private:")
        self.indent()
        
        for prop in node.properties:
            prop_type = self._map_type(prop.type_annotation) if hasattr(prop, 'type_annotation') else "auto"
            self.emit(f"{prop_type} {prop.name};")
        
        self.dedent()
        self.emit_raw("};")
        self.emit_raw("")
    
    def _generate_constructor(self, class_name: str, constructor: Any) -> None:
        """Generate C++ constructor."""
        if hasattr(constructor, 'parameters') and constructor.parameters:
            params = []
            for param in constructor.parameters:
                param_type = self._map_type(param.type_annotation) if hasattr(param, 'type_annotation') else "auto"
                params.append(f"{param_type} {param.name}")
            param_str = ", ".join(params)
        else:
            param_str = ""
        
        self.emit(f"{class_name}({param_str}) {{")
        self.indent()
        
        if hasattr(constructor, 'body') and constructor.body:
            for stmt in constructor.body:
                self._generate_statement(stmt)
        
        self.dedent()
        self.emit("}")
        self.emit_raw("")
    
    def _generate_method(self, method: FunctionDefinition) -> None:
        """Generate C++ method."""
        # Determine return type
        if hasattr(method, 'return_type') and method.return_type:
            return_type = self._map_type(method.return_type)
        else:
            return_type = "void"
        
        # Generate parameter list
        if method.parameters:
            params = []
            for param in method.parameters:
                param_type = self._map_type(param.type_annotation) if hasattr(param, 'type_annotation') else "auto"
                params.append(f"{param_type} {param.name}")
            param_str = ", ".join(params)
        else:
            param_str = ""
        
        # Method signature
        self.emit(f"{return_type} {method.name}({param_str}) {{")
        self.indent()
        
        # Generate method body
        if method.body:
            for stmt in method.body:
                self._generate_statement(stmt)
        
        self.dedent()
        self.emit("}")
        self.emit_raw("")
    
    def _generate_cpp_function_declaration(self, node: FunctionDefinition) -> None:
        """Generate C++ function (uses auto return type when possible)."""
        # Determine return type
        if node.return_type:
            return_type = self._map_type(node.return_type)
        else:
            return_type = "auto"
        
        # Generate parameter list
        if node.parameters:
            params = []
            for param in node.parameters:
                param_type = self._map_type(param.type_annotation) if hasattr(param, 'type_annotation') and param.type_annotation else "auto"
                params.append(f"{param_type} {param.name}")
            param_str = ", ".join(params)
        else:
            param_str = ""
        
        # Function signature
        self.emit_raw(f"{return_type} {node.name}({param_str}) {{")
        self.indent()
        
        # Generate function body
        if node.body:
            for stmt in node.body:
                self._generate_statement(stmt)
        
        self.dedent()
        self.emit_raw("}")
        self.emit_raw("")
    
    def _generate_variable_declaration(self, node: VariableDeclaration) -> None:
        """Generate C++ variable declaration or assignment."""
        value_expr = self._generate_expression(node.value)
        
        # Check if variable already exists in symbol table
        if node.name in self.symbol_table:
            # Variable exists - generate assignment only
            self.emit(f"{node.name} = {value_expr};")
        else:
            # New variable - use auto for type deduction in C++
            self.emit(f"auto {node.name} = {value_expr};")
            # Add to symbol table (type inference from value)
            self.symbol_table[node.name] = self._infer_type(node.value)
    
    def _infer_type(self, expr: Any) -> str:
        """Infer C++ type from expression (overrides C version)."""
        if isinstance(expr, Literal):
            if expr.type == "string":
                return "string"  # C++ uses std::string, not const char*
            elif expr.type == "integer":
                return "int"
            elif expr.type == "float":
                return "double"
            elif expr.type == "boolean":
                return "bool"
        
        # For other cases, use parent class logic
        return super()._infer_type(expr)
    
    def _generate_function_call(self, node: FunctionCall) -> str:
        """Generate C++ function call with FFI and built-in support."""
        # Handle built-in functions specially
        if node.name == "print":
            # Map to cout
            if node.arguments:
                arg_exprs = [self._generate_expression(arg) for arg in node.arguments]
                # Chain cout operations
                cout_chain = " << ".join(arg_exprs)
                return f'(cout << {cout_chain} << endl)'
            return '(cout << endl)'
        
        # Check if this is an FFI function call
        is_ffi = node.name in self.ffi_functions
        
        # Generate arguments
        args = []
        for arg in node.arguments:
            arg_expr = self._generate_expression(arg)
            
            # If calling FFI function and argument looks like a string, convert appropriately
            if is_ffi:
                # If it's a string literal wrapped in string(), extract the raw literal
                if arg_expr.startswith("string(\"") and arg_expr.endswith("\")"):
                    # string("hello") -> "hello"
                    raw_literal = arg_expr[7:-1]  # Remove "string(" prefix and ")" suffix
                    args.append(raw_literal)
                elif self._is_string_variable(arg):
                    # It's a variable holding a C++ string - add .c_str()
                    args.append(f"{arg_expr}.c_str()")
                else:
                    args.append(arg_expr)
            else:
                args.append(arg_expr)
        
        args_str = ", ".join(args)
        return f"{node.name}({args_str})"
    
    def _generate_literal(self, node: Literal) -> str:
        """Generate C++ literal value."""
        if node.type == "string":
            # Escape special characters in string
            escaped = str(node.value).replace('\\', '\\\\')  # Escape backslashes first
            escaped = escaped.replace('"', '\\"')  # Escape quotes
            escaped = escaped.replace('\n', '\\n')  # Escape newlines
            escaped = escaped.replace('\t', '\\t')  # Escape tabs
            escaped = escaped.replace('\r', '\\r')  # Escape carriage returns
            return f'string("{escaped}")'
        elif node.type == "integer":
            return str(node.value)
        elif node.type == "float":
            return str(node.value)
        elif node.type == "boolean":
            return "true" if node.value else "false"
        else:
            return f"{node.value}"
    
    def _map_type(self, nxl_type: str) -> str:
        """Map NexusLang type to C++ type."""
        type_map = {
            "Integer": "int",
            "Float": "double",
            "String": "string",
            "Boolean": "bool",
            "Void": "void",
            "List": "vector",
            "Dict": "map",
        }
        
        # Handle generic types like List<T>
        if "<" in nxl_type and ">" in nxl_type:
            base_type = nxl_type[:nxl_type.index("<")]
            type_params = nxl_type[nxl_type.index("<")+1:nxl_type.index(">")]
            
            cpp_base = type_map.get(base_type, base_type)
            cpp_params = self._map_type(type_params)
            
            return f"{cpp_base}<{cpp_params}>"
        
        return type_map.get(nxl_type, nxl_type)
    
    def _generate_extern_function(self, node: ExternFunctionDeclaration) -> None:
        """Generate extern function declaration for FFI (C++ version)."""
        # Track this as an FFI function
        self.ffi_functions.add(node.name)
        
        # For common C library functions (printf, malloc, etc.), don't redeclare
        # They're already available from standard headers
        common_c_functions = {
            'printf', 'fprintf', 'sprintf', 'snprintf',
            'scanf', 'fscanf', 'sscanf',
            'malloc', 'calloc', 'realloc', 'free',
            'memcpy', 'memset', 'memmove', 'memcmp',
            'strlen', 'strcpy', 'strncpy', 'strcmp', 'strncmp',
            'strcat', 'strncat', 'strchr', 'strrchr',
            'sqrt', 'pow', 'sin', 'cos', 'tan', 'log', 'exp',
            'abs', 'fabs', 'ceil', 'floor',
            'fopen', 'fclose', 'fread', 'fwrite', 'fseek', 'ftell',
            'getchar', 'putchar', 'gets', 'puts',
        }
        
        # Skip declaration for common C functions - they're in standard headers
        if node.name in common_c_functions:
            return
        
        # For custom FFI functions, generate extern "C" declaration
        return_type = self._map_type(node.return_type) if node.return_type else "void"
        
        params = []
        for param in node.parameters:
            if param.type_annotation == "Pointer":
                param_type = "void*"
            elif param.type_annotation == "String":
                param_type = "const char*"
            else:
                param_type = self._map_type(param.type_annotation) if param.type_annotation else "void*"
            params.append(f"{param_type} {param.name}")
        
        # Handle variadic functions
        if node.variadic:
            params.append("...")
        
        # Generate extern "C" declaration
        params_str = ", ".join(params) if params else "void"
        extern_decl = f'extern "C" {return_type} {node.name}({params_str});'
        
        # Add to forward declarations
        if extern_decl not in self.forward_declarations:
            self.forward_declarations.append(extern_decl)
    
    def _is_string_variable(self, expr: Any) -> bool:
        """Check if an expression is a C++ string variable."""
        if isinstance(expr, Identifier):
            # Check symbol table for string type
            var_type = self.symbol_table.get(expr.name, "")
            return var_type == "string"
        return False

