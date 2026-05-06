"""
DWARF Debug Information Generator for NexusLang
===========================================

Generates DWARF debug information for NexusLang programs compiled to LLVM IR.
This enables source-level debugging with GDB/LLDB.

Features:
- Source location tracking (file, line, column)
- Variable debug info (name, type, location)
- Function debug info (parameters, locals, return type)
- Type debug info (structs, classes, arrays)
- Call stack unwinding support
"""

from typing import Any, Dict, List, Optional, Set, Tuple
from .symbols import SymbolTable, DebugSymbol, SymbolType


class DebugInfoGenerator:
    """
    Generates DWARF debug information metadata for LLVM IR.
    
    DWARF metadata format in LLVM IR:
    - !DICompileUnit - Compilation unit metadata
    - !DIFile - Source file information
    - !DISubprogram - Function debug info
    - !DILocalVariable - Local variable debug info
    - !DIBasicType - Basic type debug info
    - !DICompositeType - Struct/class debug info
    - !DILocation - Source location (line, column)
    """
    
    def __init__(self, source_file: str, source_code: str):
        self.source_file = source_file
        self.source_code = source_code
        self.symbol_table = SymbolTable()
        self.symbol_table.source_file = source_file
        
        # Debug metadata nodes
        self.di_nodes: List[str] = []
        self.di_counter = 0
        
        # Debug info references
        self.di_file: Optional[str] = None
        self.di_compile_unit: Optional[str] = None
        self.di_types: Dict[str, str] = {}  # type_name -> !DIType reference
        self.di_functions: Dict[str, str] = {}  # function_name -> !DISubprogram reference
        self.di_variables: Dict[str, str] = {}  # variable_name -> !DIVariable reference
        
        # Scope stack (for nested scopes)
        self.scope_stack: List[str] = ["global"]
        
    def current_scope(self) -> str:
        """Get current scope name."""
        return self.scope_stack[-1]
    
    def enter_scope(self, name: str):
        """Enter a new scope."""
        self.scope_stack.append(name)
    
    def exit_scope(self):
        """Exit current scope."""
        if len(self.scope_stack) > 1:
            self.scope_stack.pop()
    
    def new_di_node(self) -> str:
        """Allocate a new debug info node reference."""
        ref = f"!{self.di_counter}"
        self.di_counter += 1
        return ref
    
    def generate_compile_unit(self) -> str:
        """Generate compilation unit debug info."""
        # !DIFile
        file_ref = self.new_di_node()
        self.di_file = file_ref
        file_name = self.source_file.split('/')[-1]
        directory = '/'.join(self.source_file.split('/')[:-1]) or '.'
        self.di_nodes.append(
            f'{file_ref} = !DIFile(filename: "{file_name}", directory: "{directory}")'
        )
        
        # !DICompileUnit
        cu_ref = self.new_di_node()
        self.di_compile_unit = cu_ref
        self.di_nodes.append(
            f'{cu_ref} = distinct !DICompileUnit('
            f'language: DW_LANG_C99, '  # Use C99 as base language
            f'file: {file_ref}, '
            f'producer: "NexusLang Compiler", '
            f'isOptimized: false, '
            f'runtimeVersion: 0, '
            f'emissionKind: FullDebug, '
            f'enums: !{{}}, '
            f'retainedTypes: !{{}}'
            f')'
        )
        
        return cu_ref
    
    def generate_basic_type(self, type_name: str, size: int, encoding: str) -> str:
        """
        Generate basic type debug info.
        
        Args:
            type_name: NexusLang type name (Integer, Float, String, etc.)
            size: Size in bits
            encoding: DWARF encoding (DW_ATE_signed, DW_ATE_float, etc.)
        """
        if type_name in self.di_types:
            return self.di_types[type_name]
        
        ref = self.new_di_node()
        self.di_types[type_name] = ref
        self.di_nodes.append(
            f'{ref} = !DIBasicType('
            f'name: "{type_name}", '
            f'size: {size}, '
            f'encoding: {encoding}'
            f')'
        )
        return ref
    
    def generate_composite_type(self, type_name: str, fields: List[Tuple[str, str]]) -> str:
        """
        Generate composite type (struct/class) debug info.
        
        Args:
            type_name: Struct/class name
            fields: List of (field_name, field_type) tuples
        """
        if type_name in self.di_types:
            return self.di_types[type_name]
        
        # Generate field debug info
        field_refs = []
        for field_name, field_type in fields:
            field_ref = self.new_di_node()
            type_ref = self._get_or_create_type(field_type)
            self.di_nodes.append(
                f'{field_ref} = !DIDerivedType('
                f'tag: DW_TAG_member, '
                f'name: "{field_name}", '
                f'scope: {self.di_file}, '
                f'file: {self.di_file}, '
                f'line: 0, '
                f'baseType: {type_ref}, '
                f'size: {self._get_type_size(field_type)}, '
                f'offset: 0'
                f')'
            )
            field_refs.append(field_ref)
        
        # Generate composite type
        ref = self.new_di_node()
        self.di_types[type_name] = ref
        elements_str = ', '.join(field_refs) if field_refs else ''
        self.di_nodes.append(
            f'{ref} = !DICompositeType('
            f'tag: DW_TAG_structure_type, '
            f'name: "{type_name}", '
            f'file: {self.di_file}, '
            f'line: 0, '
            f'size: {len(fields) * 64}, '  # Simplified size calculation
            f'elements: !{{{elements_str}}}'
            f')'
        )
        return ref
    
    def generate_function(self, name: str, return_type: str, 
                         parameters: List[Tuple[str, str]], line: int) -> str:
        """
        Generate function debug info.
        
        Args:
            name: Function name
            return_type: Return type
            parameters: List of (param_name, param_type) tuples
            line: Source line number
        """
        # Create subprogram node
        ref = self.new_di_node()
        self.di_functions[name] = ref
        
        # Return type
        ret_type_ref = self._get_or_create_type(return_type)
        
        # Parameter types
        param_type_refs = [self._get_or_create_type(ptype) for _, ptype in parameters]
        subroutine_type_ref = self._create_subroutine_type(ret_type_ref, param_type_refs)
        
        # Generate subprogram
        self.di_nodes.append(
            f'{ref} = distinct !DISubprogram('
            f'name: "{name}", '
            f'linkageName: "{name}", '
            f'scope: {self.di_file}, '
            f'file: {self.di_file}, '
            f'line: {line}, '
            f'type: {subroutine_type_ref}, '
            f'scopeLine: {line}, '
            f'spFlags: DISPFlagDefinition, '
            f'unit: {self.di_compile_unit}'
            f')'
        )
        
        # Add to symbol table
        symbol = DebugSymbol(
            name=name,
            type=SymbolType.FUNCTION,
            data_type=return_type,
            file=self.source_file,
            line=line,
            column=0,
            scope="global",
            llvm_name=f"@{name}"
        )
        self.symbol_table.add_symbol(symbol)
        
        return ref
    
    def generate_variable(self, name: str, var_type: str, line: int, 
                         column: int, is_parameter: bool = False) -> str:
        """
        Generate variable debug info.
        
        Args:
            name: Variable name
            var_type: Variable type
            line: Source line number
            column: Source column number
            is_parameter: True if this is a function parameter
        """
        ref = self.new_di_node()
        self.di_variables[name] = ref
        
        type_ref = self._get_or_create_type(var_type)
        scope_ref = self.di_functions.get(self.current_scope(), self.di_file)
        
        tag = "DW_TAG_arg_variable" if is_parameter else "DW_TAG_auto_variable"
        
        self.di_nodes.append(
            f'{ref} = !DILocalVariable('
            f'name: "{name}", '
            f'scope: {scope_ref}, '
            f'file: {self.di_file}, '
            f'line: {line}, '
            f'type: {type_ref}'
            f')'
        )
        
        # Add to symbol table
        symbol_type = SymbolType.PARAMETER if is_parameter else SymbolType.LOCAL
        symbol = DebugSymbol(
            name=name,
            type=symbol_type,
            data_type=var_type,
            file=self.source_file,
            line=line,
            column=column,
            scope=self.current_scope(),
            llvm_name=f"%{name}"
        )
        self.symbol_table.add_symbol(symbol)
        
        return ref
    
    def generate_location(self, line: int, column: int, scope: Optional[str] = None) -> str:
        """Generate source location debug info."""
        ref = self.new_di_node()
        scope_ref = scope or self.di_file
        self.di_nodes.append(
            f'{ref} = !DILocation(line: {line}, column: {column}, scope: {scope_ref})'
        )
        return ref
    
    def _get_or_create_type(self, type_name: str) -> str:
        """Get or create debug info for a type."""
        if type_name in self.di_types:
            return self.di_types[type_name]
        
        # Basic types
        type_map = {
            'Integer': (64, 'DW_ATE_signed'),
            'Float': (64, 'DW_ATE_float'),
            'Boolean': (1, 'DW_ATE_boolean'),
            'String': (64, 'DW_ATE_address'),  # Pointer to char array
            'void': (0, 'DW_ATE_signed'),
        }
        
        if type_name in type_map:
            size, encoding = type_map[type_name]
            return self.generate_basic_type(type_name, size, encoding)
        
        # Default: treat as opaque type
        return self.generate_basic_type(type_name, 64, 'DW_ATE_signed')
    
    def _get_type_size(self, type_name: str) -> int:
        """Get size in bits for a type."""
        sizes = {
            'Integer': 64,
            'Float': 64,
            'Boolean': 1,
            'String': 64,
        }
        return sizes.get(type_name, 64)
    
    def _create_subroutine_type(self, return_type: str, param_types: List[str]) -> str:
        """Create subroutine type (function signature)."""
        ref = self.new_di_node()
        types = [return_type] + param_types
        types_str = ', '.join(types)
        self.di_nodes.append(
            f'{ref} = !DISubroutineType(types: !{{{types_str}}})'
        )
        return ref
    
    def get_debug_metadata(self) -> str:
        """Get all debug metadata as LLVM IR."""
        if not self.di_nodes:
            return ""
        
        lines = []
        lines.append('')
        lines.append('; Debug information')
        lines.extend(self.di_nodes)
        lines.append('')
        lines.append(f'!llvm.dbg.cu = !{{{self.di_compile_unit}}}')
        lines.append('!llvm.module.flags = !{!100, !101}')
        lines.append('!100 = !{i32 2, !"Dwarf Version", i32 4}')
        lines.append('!101 = !{i32 2, !"Debug Info Version", i32 3}')
        lines.append('')
        
        return '\n'.join(lines)
    
    def get_location_annotation(self, line: int, column: int, scope: Optional[str] = None) -> str:
        """Get location annotation for an instruction."""
        loc_ref = self.generate_location(line, column, scope)
        return f", !dbg {loc_ref}"
