"""
C Header Parser for NexusLang FFI.
Parses C header files to automatically generate NexusLang extern declarations.

This module enables automatic binding generation for C libraries by parsing
their header files and extracting function signatures, struct definitions,
enums, typedefs, and macros.

Strategy: Use regex-based parsing for portability (works everywhere).
Future enhancement: Optional clang.cindex for complex headers.
"""

import re
import os
from typing import List, Dict, Optional, Tuple, Set, Any
from dataclasses import dataclass, field
from enum import Enum, auto


class CType(Enum):
    """C type categories for classification."""
    FUNDAMENTAL = auto()  # int, char, float, etc.
    POINTER = auto()       # T*
    ARRAY = auto()         # T[N]
    STRUCT = auto()        # struct name
    UNION = auto()         # union name
    ENUM = auto()          # enum name
    FUNCTION_POINTER = auto()  # RetType (*name)(Args)
    TYPEDEF = auto()       # typedef'd type
    VOID = auto()          # void type


@dataclass
class CParameter:
    """Represents a C function parameter."""
    name: str
    c_type: str
    nxl_type: str
    is_const: bool = False
    is_pointer: bool = False
    pointer_depth: int = 0  # Number of * levels
    array_size: Optional[int] = None
    
    def to_nxl_param(self) -> str:
        """Generate NexusLang parameter declaration."""
        if self.name:
            return f"{self.name} as {self.nxl_type}"
        else:
            return f"as {self.nxl_type}"


@dataclass
class CFunctionDeclaration:
    """Represents a C function declaration."""
    name: str
    return_type: str
    nxl_return_type: str
    parameters: List[CParameter]
    is_variadic: bool = False
    calling_convention: str = "cdecl"
    library: Optional[str] = None
    header_file: Optional[str] = None
    line_number: int = 0
    
    def to_nxl_declaration(self) -> str:
        """Generate NexusLang extern function declaration."""
        parts = [f"extern function {self.name}"]
        
        if self.parameters:
            param_strs = []
            for param in self.parameters:
                param_strs.append(param.to_nxl_param())
            parts.append(" with " + " and ".join(param_strs))
        
        parts.append(f" returns {self.nxl_return_type}")
        
        if self.library:
            parts.append(f' from library "{self.library}"')
        
        if self.calling_convention != "cdecl":
            parts.append(f' calling convention {self.calling_convention}')
        
        return "".join(parts)


@dataclass
class CStructField:
    """Represents a field in a C struct or union."""
    name: str
    c_type: str
    nxl_type: str
    is_pointer: bool = False
    array_size: Optional[int] = None
    bit_field_width: Optional[int] = None


@dataclass
class CStructDeclaration:
    """Represents a C struct or union declaration."""
    name: str
    is_union: bool = False
    fields: List[CStructField] = field(default_factory=list)
    is_packed: bool = False
    alignment: Optional[int] = None
    header_file: Optional[str] = None
    line_number: int = 0
    
    def to_nxl_declaration(self) -> str:
        """Generate NexusLang struct/union declaration."""
        keyword = "union" if self.is_union else "struct"
        lines = [f"{keyword} {self.name}"]
        
        for fld in self.fields:
            field_type = fld.nxl_type
            if fld.array_size:
                field_type = f"Array of {field_type}"
            lines.append(f"    {fld.name} as {field_type}")
        
        lines.append("end")
        return "\n".join(lines)


@dataclass
class CEnumDeclaration:
    """Represents a C enum declaration."""
    name: str
    values: Dict[str, int] = field(default_factory=dict)
    header_file: Optional[str] = None
    line_number: int = 0
    
    def to_nxl_constants(self) -> List[str]:
        """Generate NexusLang constant declarations for enum values."""
        constants = []
        for value_name, value_int in self.values.items():
            constants.append(f"set constant {value_name} to {value_int}")
        return constants


@dataclass
class CTypedef:
    """Represents a C typedef declaration."""
    alias_name: str
    original_type: str
    nxl_type: str
    header_file: Optional[str] = None
    line_number: int = 0


class TypeMapper:
    """Maps C types to NexusLang types and vice versa."""
    
    def __init__(self):
        # C fundamental types -> NexusLang types
        self.c_to_nxl = {
            # Integer types
            'char': 'Integer',
            'signed char': 'Integer',
            'unsigned char': 'Integer',
            'short': 'Integer',
            'short int': 'Integer',
            'signed short': 'Integer',
            'signed short int': 'Integer',
            'unsigned short': 'Integer',
            'unsigned short int': 'Integer',
            'int': 'Integer',
            'signed': 'Integer',
            'signed int': 'Integer',
            'unsigned': 'Integer',
            'unsigned int': 'Integer',
            'long': 'Integer',
            'long int': 'Integer',
            'signed long': 'Integer',
            'signed long int': 'Integer',
            'unsigned long': 'Integer',
            'unsigned long int': 'Integer',
            'long long': 'Integer',
            'long long int': 'Integer',
            'signed long long': 'Integer',
            'signed long long int': 'Integer',
            'unsigned long long': 'Integer',
            'unsigned long long int': 'Integer',
            
            # Fixed-width types (stdint.h)
            'int8_t': 'Int8',
            'int16_t': 'Int16',
            'int32_t': 'Int32',
            'int64_t': 'Int64',
            'uint8_t': 'UInt8',
            'uint16_t': 'UInt16',
            'uint32_t': 'UInt32',
            'uint64_t': 'UInt64',
            'size_t': 'Integer',
            'ssize_t': 'Integer',
            'ptrdiff_t': 'Integer',
            'intptr_t': 'Integer',
            'uintptr_t': 'Integer',
            
            # Floating-point types
            'float': 'Float',
            'double': 'Float',
            'long double': 'Float',
            
            # Other types
            'void': 'Void',
            '_Bool': 'Boolean',
            'bool': 'Boolean',
        }
        
        # NexusLang types -> C types
        self.nxl_to_c = {
            'Integer': 'int',
            'Int8': 'int8_t',
            'Int16': 'int16_t',
            'Int32': 'int32_t',
            'Int64': 'int64_t',
            'UInt8': 'uint8_t',
            'UInt16': 'uint16_t',
            'UInt32': 'uint32_t',
            'UInt64': 'uint64_t',
            'Float': 'double',
            'Float32': 'float',
            'Float64': 'double',
            'Boolean': '_Bool',
            'Pointer': 'void*',
            'String': 'char*',
            'Void': 'void',
        }
        
        # Custom type mappings (user-defined)
        self.custom_mappings: Dict[str, str] = {}
        
        # Opaque types (FILE*, DIR*, etc.)
        self.opaque_types: Set[str] = {
            'FILE', 'DIR', 'pthread_t', 'pthread_mutex_t',
            'pthread_cond_t', 'pid_t', 'uid_t', 'gid_t'
        }
    
    def add_custom_mapping(self, c_type: str, nxl_type: str):
        """Add a custom C->NLPL type mapping."""
        self.custom_mappings[c_type] = nxl_type
    
    def c_to_nxl_type(self, c_type: str) -> str:
        """Convert C type to NexusLang type."""
        # Strip whitespace
        c_type = c_type.strip()
        
        # Handle const/volatile qualifiers
        c_type = re.sub(r'\bconst\b', '', c_type).strip()
        c_type = re.sub(r'\bvolatile\b', '', c_type).strip()
        c_type = re.sub(r'\brestrict\b', '', c_type).strip()
        
        # Check custom mappings first
        if c_type in self.custom_mappings:
            return self.custom_mappings[c_type]
        
        # Count pointer depth
        pointer_depth = c_type.count('*')
        base_type = c_type.replace('*', '').strip()
        
        # Special case: void* is Pointer
        if base_type == 'void' and pointer_depth > 0:
            return 'Pointer'
        
        # Special case: char* is String (null-terminated)
        if base_type == 'char' and pointer_depth == 1:
            return 'String'
        
        # All other pointer types become Pointer
        if pointer_depth > 0:
            return 'Pointer'
        
        # Handle struct/union/enum types
        if base_type.startswith('struct '):
            struct_name = base_type[7:].strip()
            return f"Struct_{struct_name}"
        
        if base_type.startswith('union '):
            union_name = base_type[6:].strip()
            return f"Union_{union_name}"
        
        if base_type.startswith('enum '):
            return 'Integer'  # Enums are integers
        
        # Check opaque types
        if base_type in self.opaque_types:
            return 'Pointer'
        
        # Look up in standard mappings
        if base_type in self.c_to_nxl:
            return self.c_to_nxl[base_type]
        
        # Default: treat as opaque pointer
        return 'Pointer'
    
    def nxl_to_c_type(self, nxl_type: str) -> str:
        """Convert NexusLang type to C type."""
        if nxl_type in self.nxl_to_c:
            return self.nxl_to_c[nxl_type]
        
        # Handle struct/union types
        if nxl_type.startswith("Struct_"):
            struct_name = nxl_type[7:]
            return f"struct {struct_name}"
        
        if nxl_type.startswith("Union_"):
            union_name = nxl_type[6:]
            return f"union {union_name}"
        
        # Default
        return "void*"


class CHeaderParser:
    """
    Parses C header files to extract function declarations, structs, enums, and typedefs.
    Generates NexusLang extern declarations automatically.
    """
    
    def __init__(self, library_name: Optional[str] = None):
        self.type_mapper = TypeMapper()
        self.library_name = library_name
        
        # Parsed declarations
        self.functions: List[CFunctionDeclaration] = []
        self.structs: List[CStructDeclaration] = []
        self.enums: List[CEnumDeclaration] = []
        self.typedefs: List[CTypedef] = []
        
        # Track included headers to avoid duplicates
        self.parsed_headers: Set[str] = set()
        
        # Preprocessor definitions (simple macros)
        self.macros: Dict[str, str] = {}
    
    def parse_header(self, header_path: str) -> bool:
        """
        Parse a C header file.
        
        Args:
            header_path: Path to the .h file
        
        Returns:
            True if successfully parsed, False otherwise
        """
        if header_path in self.parsed_headers:
            return True  # Already parsed
        
        if not os.path.exists(header_path):
            print(f"Warning: Header file not found: {header_path}")
            return False
        
        try:
            with open(header_path, 'r', encoding='utf-8', errors='ignore') as f:
                content = f.read()
            
            # Mark as parsed
            self.parsed_headers.add(header_path)
            
            # Preprocess: remove comments
            content = self._remove_comments(content)
            
            # Extract declarations
            self._extract_functions(content, header_path)
            self._extract_structs(content, header_path)
            self._extract_enums(content, header_path)
            self._extract_typedefs(content, header_path)
            self._extract_macros(content)
            
            return True
            
        except Exception as e:
            print(f"Error parsing header {header_path}: {e}")
            return False
    
    def _remove_comments(self, content: str) -> str:
        """Remove C-style comments (// and /* */)."""
        # Remove single-line comments
        content = re.sub(r'//.*?$', '', content, flags=re.MULTILINE)
        # Remove multi-line comments
        content = re.sub(r'/\*.*?\*/', '', content, flags=re.DOTALL)
        return content
    
    def _extract_functions(self, content: str, header_file: str):
        """Extract function declarations from header content."""
        # Regex pattern for function declarations
        # Matches: return_type function_name(parameters);
        # Handles: const, *, arrays, etc.
        
        # Pattern explanation:
        # - Return type: word + optional *, const, etc.
        # - Function name: identifier
        # - Parameters: anything between ()
        # - Ends with ;
        pattern = r'''
            (?P<return_type>
                (?:const\s+)?
                (?:unsigned\s+|signed\s+)?
                (?:struct\s+|union\s+|enum\s+)?
                [a-zA-Z_][a-zA-Z0-9_]*
                (?:\s*\*+)?
                (?:\s+const)?
            )
            \s+
            (?P<function_name>[a-zA-Z_][a-zA-Z0-9_]*)
            \s*
            \(
                (?P<parameters>[^)]*)
            \)
            \s*;
        '''
        
        for match in re.finditer(pattern, content, re.VERBOSE | re.MULTILINE):
            return_type = match.group('return_type').strip()
            function_name = match.group('function_name')
            params_str = match.group('parameters').strip()
            
            # Skip if this looks like a macro or typedef
            if return_type.startswith('#') or 'typedef' in return_type:
                continue
            
            # Parse parameters
            parameters = self._parse_parameters(params_str)
            
            # Check if variadic
            is_variadic = '...' in params_str
            
            # Convert return type
            nxl_return = self.type_mapper.c_to_nxl_type(return_type)
            if nxl_return == 'Void':
                nxl_return = 'nothing'
            
            func = CFunctionDeclaration(
                name=function_name,
                return_type=return_type,
                nxl_return_type=nxl_return,
                parameters=parameters,
                is_variadic=is_variadic,
                library=self.library_name,
                header_file=header_file,
            )
            
            self.functions.append(func)
    
    def _parse_parameters(self, params_str: str) -> List[CParameter]:
        """Parse function parameter list."""
        parameters = []
        
        if not params_str or params_str == 'void':
            return parameters
        
        # Split by commas (but not inside parentheses or angle brackets)
        param_list = self._split_params(params_str)
        
        for param in param_list:
            param = param.strip()
            
            if param == '...':
                continue  # Variadic marker
            
            if not param or param == 'void':
                continue
            
            # Parse parameter: type name
            # Handle: "int x", "const char* str", "int arr[10]", "int (*func)(int)"
            
            # Count pointers
            pointer_depth = param.count('*')
            is_pointer = pointer_depth > 0
            
            # Remove const/volatile
            param = re.sub(r'\bconst\b', '', param)
            param = re.sub(r'\bvolatile\b', '', param)
            param = re.sub(r'\brestrict\b', '', param)
            param = param.strip()
            
            # Extract array size if present
            array_match = re.search(r'\[(\d+)\]', param)
            array_size = None
            if array_match:
                array_size = int(array_match.group(1))
                param = re.sub(r'\[\d+\]', '', param)
            
            # Split type and name (name is last token)
            tokens = param.replace('*', '').split()
            if tokens:
                param_name = tokens[-1] if len(tokens) > 1 else ""
                c_type = ' '.join(tokens[:-1]) if len(tokens) > 1 else tokens[0]
                
                if pointer_depth > 0:
                    c_type += '*' * pointer_depth
                
                nxl_type = self.type_mapper.c_to_nxl_type(c_type)
                
                parameters.append(CParameter(
                    name=param_name,
                    c_type=c_type,
                    nxl_type=nxl_type,
                    is_pointer=is_pointer,
                    pointer_depth=pointer_depth,
                    array_size=array_size
                ))
        
        return parameters
    
    def _split_params(self, params_str: str) -> List[str]:
        """Split parameter string by commas, respecting nested parentheses."""
        params = []
        current = []
        depth = 0
        
        for char in params_str:
            if char == '(':
                depth += 1
            elif char == ')':
                depth -= 1
            elif char == ',' and depth == 0:
                params.append(''.join(current))
                current = []
                continue
            current.append(char)
        
        if current:
            params.append(''.join(current))
        
        return params
    
    def _extract_structs(self, content: str, header_file: str):
        """Extract struct and union declarations."""
        # Pattern for struct/union definitions
        pattern = r'''
            (?P<keyword>struct|union)
            \s+
            (?P<name>[a-zA-Z_][a-zA-Z0-9_]*)
            \s*
            \{
                (?P<body>.*?)
            \}
            \s*;
        '''
        
        for match in re.finditer(pattern, content, re.VERBOSE | re.DOTALL):
            keyword = match.group('keyword')
            name = match.group('name')
            body = match.group('body')
            
            is_union = (keyword == 'union')
            
            # Parse fields
            fields = self._parse_struct_fields(body)
            
            struct = CStructDeclaration(
                name=name,
                is_union=is_union,
                fields=fields,
                header_file=header_file
            )
            
            self.structs.append(struct)
    
    def _parse_struct_fields(self, body: str) -> List[CStructField]:
        """Parse struct/union field declarations."""
        fields = []
        
        # Split by semicolons
        field_lines = [line.strip() for line in body.split(';') if line.strip()]
        
        for line in field_lines:
            # Skip empty lines
            if not line:
                continue
            
            # Parse: type name or type name[size]
            # Handle bit fields: int flags : 3;
            
            # Check for bit field
            bit_field_width = None
            if ':' in line:
                parts = line.split(':')
                line = parts[0].strip()
                try:
                    bit_field_width = int(parts[1].strip())
                except ValueError:
                    pass
            
            # Extract array size
            array_match = re.search(r'\[(\d+)\]', line)
            array_size = None
            if array_match:
                array_size = int(array_match.group(1))
                line = re.sub(r'\[\d+\]', '', line)
            
            # Parse type and name
            tokens = line.replace('*', ' * ').split()
            if tokens:
                field_name = tokens[-1]
                c_type = ' '.join(tokens[:-1])
                
                is_pointer = '*' in c_type
                nxl_type = self.type_mapper.c_to_nxl_type(c_type)
                
                fields.append(CStructField(
                    name=field_name,
                    c_type=c_type,
                    nxl_type=nxl_type,
                    is_pointer=is_pointer,
                    array_size=array_size,
                    bit_field_width=bit_field_width
                ))
        
        return fields
    
    def _extract_enums(self, content: str, header_file: str):
        """Extract enum declarations."""
        pattern = r'''
            enum
            \s+
            (?P<name>[a-zA-Z_][a-zA-Z0-9_]*)
            \s*
            \{
                (?P<body>.*?)
            \}
            \s*;
        '''
        
        for match in re.finditer(pattern, content, re.VERBOSE | re.DOTALL):
            name = match.group('name')
            body = match.group('body')
            
            # Parse enum values
            values = self._parse_enum_values(body)
            
            enum = CEnumDeclaration(
                name=name,
                values=values,
                header_file=header_file
            )
            
            self.enums.append(enum)
    
    def _parse_enum_values(self, body: str) -> Dict[str, int]:
        """Parse enum value assignments."""
        values = {}
        current_value = 0
        
        # Split by commas
        items = [item.strip() for item in body.split(',') if item.strip()]
        
        for item in items:
            # Parse: NAME or NAME = VALUE
            if '=' in item:
                parts = item.split('=')
                name = parts[0].strip()
                try:
                    value_str = parts[1].strip()
                    # Handle hex, octal, binary literals
                    if value_str.startswith('0x') or value_str.startswith('0X'):
                        current_value = int(value_str, 16)
                    elif value_str.startswith('0b') or value_str.startswith('0B'):
                        current_value = int(value_str, 2)
                    elif value_str.startswith('0') and len(value_str) > 1:
                        current_value = int(value_str, 8)
                    else:
                        current_value = int(value_str)
                except ValueError:
                    # Complex expression, use current_value
                    pass
            else:
                name = item
            
            values[name] = current_value
            current_value += 1
        
        return values
    
    def _extract_typedefs(self, content: str, header_file: str):
        """Extract typedef declarations."""
        # Simple typedef pattern
        pattern = r'typedef\s+(.+?)\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*;'
        
        for match in re.finditer(pattern, content):
            original_type = match.group(1).strip()
            alias_name = match.group(2)
            
            nxl_type = self.type_mapper.c_to_nxl_type(original_type)
            
            typedef = CTypedef(
                alias_name=alias_name,
                original_type=original_type,
                nxl_type=nxl_type,
                header_file=header_file
            )
            
            self.typedefs.append(typedef)
            
            # Register in type mapper
            self.type_mapper.add_custom_mapping(alias_name, nxl_type)
    
    def _extract_macros(self, content: str):
        """Extract simple #define macros (constants only)."""
        # Pattern for simple constant defines
        pattern = r'#define\s+([A-Z_][A-Z0-9_]*)\s+(.+?)(?:\n|$)'
        
        for match in re.finditer(pattern, content):
            macro_name = match.group(1)
            macro_value = match.group(2).strip()
            
            # Only handle numeric and string constants
            if macro_value.isdigit() or macro_value.startswith('"') or macro_value.startswith("'"):
                self.macros[macro_name] = macro_value
    
    def generate_nxl_module(self, output_path: Optional[str] = None) -> str:
        """
        Generate complete NexusLang module with all extern declarations.
        
        Args:
            output_path: Optional file path to write output
        
        Returns:
            Generated NexusLang module source code
        """
        lines = []
        
        # Header comment
        lines.append("# Auto-generated NexusLang FFI bindings")
        if self.library_name:
            lines.append(f"# Library: {self.library_name}")
        lines.append("# Generated by NexusLang C Header Parser")
        lines.append("")
        
        # Macro constants
        if self.macros:
            lines.append("# Constants from macros")
            for name, value in self.macros.items():
                lines.append(f"set constant {name} to {value}")
            lines.append("")
        
        # Enum values
        if self.enums:
            lines.append("# Enum constants")
            for enum in self.enums:
                lines.append(f"# enum {enum.name}")
                lines.extend(enum.to_nxl_constants())
                lines.append("")
        
        # Struct declarations
        if self.structs:
            lines.append("# Struct and Union declarations")
            for struct in self.structs:
                lines.append(struct.to_nxl_declaration())
                lines.append("")
        
        # Function declarations
        if self.functions:
            lines.append("# External function declarations")
            for func in self.functions:
                lines.append(func.to_nxl_declaration())
                lines.append("")
        
        output = "\n".join(lines)
        
        # Write to file if path provided
        if output_path:
            with open(output_path, 'w', encoding='utf-8') as f:
                f.write(output)
            print(f"Generated NexusLang bindings: {output_path}")
        
        return output
    
    def get_summary(self) -> Dict[str, int]:
        """Get summary of parsed declarations."""
        return {
            'functions': len(self.functions),
            'structs': len(self.structs),
            'enums': len(self.enums),
            'typedefs': len(self.typedefs),
            'macros': len(self.macros),
        }


def parse_c_header(header_path: str, library_name: Optional[str] = None, 
                   output_path: Optional[str] = None) -> CHeaderParser:
    """
    Convenience function to parse a C header and generate NexusLang bindings.
    
    Args:
        header_path: Path to C header file
        library_name: Library name for extern declarations
        output_path: Optional output file for generated bindings
    
    Returns:
        CHeaderParser instance with parsed declarations
    
    Example:
        >>> parser = parse_c_header('/usr/include/math.h', 'm', 'math_bindings.nxl')
        >>> print(f"Parsed {parser.get_summary()['functions']} functions")
    """
    parser = CHeaderParser(library_name=library_name)
    
    if parser.parse_header(header_path):
        parser.generate_nxl_module(output_path)
        summary = parser.get_summary()
        print(f"Successfully parsed {header_path}:")
        print(f"  Functions: {summary['functions']}")
        print(f"  Structs: {summary['structs']}")
        print(f"  Enums: {summary['enums']}")
        print(f"  Typedefs: {summary['typedefs']}")
        print(f"  Macros: {summary['macros']}")
    else:
        print(f"Failed to parse {header_path}")
    
    return parser
