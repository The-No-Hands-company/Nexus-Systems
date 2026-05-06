
from typing import List, Set, Dict, Any
from ...parser.ast import Program, FunctionDefinition, ExportStatement

class CHeaderGenerator:
    """Generates C header files from NexusLang AST."""
    
    def __init__(self):
        self.exported_names: Set[str] = set()
        self.functions: Dict[str, FunctionDefinition] = {}
        self.includes: Set[str] = set()
        
    def generate(self, ast: Program, output_name: str = "nxl_module") -> str:
        """Generate C header content."""
        self.exported_names.clear()
        self.functions.clear()
        self.includes.clear()
        
        # First pass: collect exported names and function definitions
        for stmt in ast.statements:
            if isinstance(stmt, ExportStatement):
                for name in stmt.names:
                    self.exported_names.add(name)
            elif isinstance(stmt, FunctionDefinition):
                self.functions[stmt.name] = stmt
                
        # Generate header content
        header_guard = f"{output_name.upper()}_H"
        
        lines = [
            f"#ifndef {header_guard}",
            f"#define {header_guard}",
            "",
            "#ifdef __cplusplus",
            'extern "C" {',
            "#endif",
            "",
            "#include <stdint.h>",
            "#include <stdbool.h>",
            ""
        ]
        
        # Generate prototypes for exported functions
        if self.exported_names:
            lines.append("/* Exported Functions */")
            
            for name in sorted(self.exported_names):
                if name in self.functions:
                    func_def = self.functions[name]
                    prototype = self._generate_function_prototype(func_def)
                    lines.append(f"{prototype};")
                else:
                    lines.append(f"/* Warning: Exported function '{name}' not found */")
            
            lines.append("")
            
        lines.extend([
            "#ifdef __cplusplus",
            "}",
            "#endif",
            "",
            f"#endif /* {header_guard} */"
        ])
        
        return "\n".join(lines)
        
    def _generate_function_prototype(self, node: FunctionDefinition) -> str:
        """Generate C function prototype."""
        # Return type
        ret_type = self._map_type(node.return_type) if node.return_type else "void"
        
        # Parameters
        params = []
        if node.parameters:
            for param in node.parameters:
                p_type = self._map_type(param.type_annotation) if hasattr(param, 'type_annotation') and param.type_annotation else "int64_t"
                params.append(f"{p_type} {param.name}")
        else:
            params.append("void")
            
        param_str = ", ".join(params)
        return f"{ret_type} {node.name}({param_str})"
        
    def _map_type(self, nxl_type: str) -> str:
        """Map NexusLang type to C type."""
        # Basic mapping - can be expanded
        mapping = {
            "Integer": "int64_t",
            "Float": "double",
            "Boolean": "bool",
            "String": "const char*",
            "Void": "void",
            "Byte": "uint8_t"
        }
        return mapping.get(nxl_type, "void*") # Default to void* for objects/unknowns
