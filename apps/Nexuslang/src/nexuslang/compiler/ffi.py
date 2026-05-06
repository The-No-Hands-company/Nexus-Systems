"""
FFI (Foreign Function Interface) code generation for NexusLang.
Handles external function declarations and calls to C libraries.
Supports struct marshalling for passing NexusLang structs to C functions.
Advanced features: function pointers, opaque types, unions, nested types.
"""

from llvmlite import ir
from typing import Dict, List, Optional, Tuple, Any, Type
import ctypes.util


class FFICodegen:
    """Generates LLVM IR for FFI (Foreign Function Interface) operations."""
    
    def __init__(self, module: ir.Module, builder: ir.IRBuilder):
        self.module = module
        self.builder = builder
        
        # Track declared external functions
        self.extern_functions: Dict[str, ir.Function] = {}
        
        # Track loaded libraries
        self.loaded_libraries: Dict[str, str] = {}
        
        # Track struct type definitions for marshalling
        self.struct_types: Dict[str, ir.Type] = {}
        self.struct_field_info: Dict[str, List[Tuple[str, str]]] = {}
        
        # Track union type definitions
        self.union_types: Dict[str, ir.Type] = {}
        self.union_field_info: Dict[str, List[Tuple[str, str]]] = {}
        
        # Track opaque pointer types
        self.opaque_types: Dict[str, ir.Type] = {}
        
        # Track function pointer types
        self.function_pointer_types: Dict[str, ir.Type] = {}
        
        # Type mappings from NexusLang to LLVM types
        self.type_map = {
            'Integer': ir.IntType(64),
            'Int': ir.IntType(64),
            'Int8': ir.IntType(8),
            'Int16': ir.IntType(16),
            'Int32': ir.IntType(32),
            'Int64': ir.IntType(64),
            'UInt8': ir.IntType(8),
            'UInt16': ir.IntType(16),
            'UInt32': ir.IntType(32),
            'UInt64': ir.IntType(64),
            'Float': ir.DoubleType(),
            'Float32': ir.FloatType(),
            'Float64': ir.DoubleType(),
            'Double': ir.DoubleType(),
            'Boolean': ir.IntType(1),
            'Char': ir.IntType(8),
            'Pointer': ir.IntType(8).as_pointer(),
            'String': ir.IntType(8).as_pointer(),
            'Void': ir.VoidType(),
        }
        
        # Common C library paths
        self.library_paths = {
            'c': self._find_c_library(),
            'libc': self._find_c_library(),
            'm': ctypes.util.find_library('m'),
            'libm': ctypes.util.find_library('m'),
            'pthread': ctypes.util.find_library('pthread'),
            'libpthread': ctypes.util.find_library('pthread'),
            'dl': ctypes.util.find_library('dl'),
            'libdl': ctypes.util.find_library('dl'),
        }
    
    def _find_c_library(self) -> Optional[str]:
        """Find the C standard library."""
        # Try common names
        for name in ['c', 'msvcrt']:
            path = ctypes.util.find_library(name)
            if path:
                return path
        return None
    
    def register_opaque_type(self, name: str, base_type: str = "pointer"):
        """Register an opaque pointer type.
        
        Opaque types are used for types whose internal structure is not exposed
        (e.g., FILE*, DIR*, pthread_t).
        
        Args:
            name: Type name (e.g., "FILE", "DIR")
            base_type: Base type ("pointer", "struct", etc.)
        """
        # Opaque types are represented as i8* pointers
        opaque_type = ir.IntType(8).as_pointer()
        self.opaque_types[name] = opaque_type
        self.type_map[name] = opaque_type
    
    def register_function_pointer_type(self, name: str, param_types: List[str], return_type: str):
        """Register a function pointer type.
        
        Args:
            name: Type name
            param_types: List of parameter type names
            return_type: Return type name
        """
        # Map types to LLVM
        llvm_param_types = [self.map_type(t) for t in param_types]
        llvm_return_type = self.map_type(return_type)
        
        # Create function type
        func_type = ir.FunctionType(llvm_return_type, llvm_param_types)
        func_ptr_type = func_type.as_pointer()
        
        self.function_pointer_types[name] = func_ptr_type
        self.type_map[name] = func_ptr_type
    
    def register_union_type(self, name: str, fields: List[Tuple[str, str]]):
        """Register a union type for FFI marshalling.
        
        Unions in LLVM are represented as arrays of bytes matching the largest field.
        
        Args:
            name: Union name
            fields: List of (field_name, field_type) tuples
        """
        # Find the largest field size
        max_size = 0
        
        for fname, ftype in fields:
            field_llvm_type = self.map_type(ftype)
            # Approximate size calculation
            if isinstance(field_llvm_type, ir.IntType):
                size = field_llvm_type.width // 8
            elif isinstance(field_llvm_type, ir.FloatType):
                size = 4
            elif isinstance(field_llvm_type, ir.DoubleType):
                size = 8
            elif isinstance(field_llvm_type, ir.PointerType):
                size = 8  # Assume 64-bit pointers
            else:
                size = 8  # Default size
            
            max_size = max(max_size, size)
        
        # Create union as array of bytes
        union_type = ir.ArrayType(ir.IntType(8), max_size)
        
        self.union_types[name] = union_type
        self.union_field_info[name] = fields
        self.type_map[name] = union_type
    
    def register_struct_type(self, name: str, fields: List[Tuple[str, str]]):
        """Register a struct type for FFI marshalling.
        
        Args:
            name: Struct name
            fields: List of (field_name, field_type) tuples
        """
        # Build LLVM struct type
        field_types = [self.map_type(ftype) for fname, ftype in fields]
        struct_type = ir.LiteralStructType(field_types)
        
        self.struct_types[name] = struct_type
        self.struct_field_info[name] = fields
        self.type_map[name] = struct_type
    
    def map_type(self, type_name: str) -> ir.Type:
        """Map NexusLang type name to LLVM type."""
        if type_name in self.type_map:
            return self.type_map[type_name]
        
        # Check if it's a registered struct
        if type_name in self.struct_types:
            return self.struct_types[type_name]
        
        # Check if it's a registered union
        if type_name in self.union_types:
            return self.union_types[type_name]
        
        # Check if it's an opaque type
        if type_name in self.opaque_types:
            return self.opaque_types[type_name]
        
        # Check if it's a function pointer type
        if type_name in self.function_pointer_types:
            return self.function_pointer_types[type_name]
        
        # Handle pointer types
        if type_name.endswith('*') or 'Pointer' in type_name:
            base_type = type_name.replace('*', '').replace('Pointer', '').strip()
            if base_type in self.type_map:
                return self.type_map[base_type].as_pointer()
            if base_type in self.struct_types:
                return self.struct_types[base_type].as_pointer()
            if base_type in self.union_types:
                return self.union_types[base_type].as_pointer()
            return ir.IntType(8).as_pointer()
        
        # Default to i8* for unknown types
        return ir.IntType(8).as_pointer()

    
    def declare_extern_function(self, name: str, param_types: List[str], 
                               return_type: str, library: Optional[str] = None,
                               calling_convention: str = "cdecl") -> ir.Function:
        """Declare an external function from a C library.
        
        Args:
            name: Function name
            param_types: List of parameter type names
            return_type: Return type name
            library: Library name (e.g., 'c', 'm', 'pthread')
            calling_convention: Calling convention (cdecl, stdcall, etc.)
        
        Returns:
            LLVM Function object for the external function
        """
        # Check if already declared
        if name in self.extern_functions:
            return self.extern_functions[name]
        
        # Map types to LLVM
        llvm_param_types = [self.map_type(t) for t in param_types]
        llvm_return_type = self.map_type(return_type)
        
        # Create function type
        func_type = ir.FunctionType(llvm_return_type, llvm_param_types)
        
        # Declare the function in the module
        func = ir.Function(self.module, func_type, name=name)
        
        # Set calling convention
        if calling_convention.lower() == 'cdecl':
            func.calling_convention = 'ccc'  # C calling convention
        elif calling_convention.lower() == 'stdcall':
            func.calling_convention = 'x86_stdcallcc'
        
        # Mark as external linkage
        func.linkage = 'external'
        
        # Store library association
        if library:
            self.loaded_libraries[name] = library
        
        # Cache the function
        self.extern_functions[name] = func
        
        return func
    
    def call_extern_function(self, name: str, args: List[ir.Value]) -> ir.Value:
        """Call an external function.
        
        Args:
            name: Function name
            args: List of LLVM Value arguments
        
        Returns:
            Call instruction result
        """
        if name not in self.extern_functions:
            raise RuntimeError(f"External function '{name}' not declared")
        
        func = self.extern_functions[name]
        return self.builder.call(func, args, name=f"{name}_result")
    
    def declare_common_c_functions(self):
        """Declare commonly used C standard library functions."""
        
        # stdio.h functions
        self.declare_extern_function('printf', ['String'], 'Int', 'c')
        self.declare_extern_function('sprintf', ['String', 'String'], 'Int', 'c')
        self.declare_extern_function('fprintf', ['Pointer', 'String'], 'Int', 'c')
        self.declare_extern_function('scanf', ['String'], 'Int', 'c')
        self.declare_extern_function('puts', ['String'], 'Int', 'c')
        self.declare_extern_function('putchar', ['Int'], 'Int', 'c')
        self.declare_extern_function('getchar', [], 'Int', 'c')
        
        # stdlib.h functions
        self.declare_extern_function('malloc', ['Int64'], 'Pointer', 'c')
        self.declare_extern_function('calloc', ['Int64', 'Int64'], 'Pointer', 'c')
        self.declare_extern_function('realloc', ['Pointer', 'Int64'], 'Pointer', 'c')
        self.declare_extern_function('free', ['Pointer'], 'Void', 'c')
        self.declare_extern_function('exit', ['Int'], 'Void', 'c')
        self.declare_extern_function('abort', [], 'Void', 'c')
        self.declare_extern_function('atoi', ['String'], 'Int', 'c')
        self.declare_extern_function('atof', ['String'], 'Double', 'c')
        
        # string.h functions
        self.declare_extern_function('strlen', ['String'], 'Int64', 'c')
        self.declare_extern_function('strcmp', ['String', 'String'], 'Int', 'c')
        self.declare_extern_function('strncmp', ['String', 'String', 'Int64'], 'Int', 'c')
        self.declare_extern_function('strcpy', ['String', 'String'], 'String', 'c')
        self.declare_extern_function('strncpy', ['String', 'String', 'Int64'], 'String', 'c')
        self.declare_extern_function('strcat', ['String', 'String'], 'String', 'c')
        self.declare_extern_function('memcpy', ['Pointer', 'Pointer', 'Int64'], 'Pointer', 'c')
        self.declare_extern_function('memset', ['Pointer', 'Int', 'Int64'], 'Pointer', 'c')
        
        # math.h functions
        self.declare_extern_function('sin', ['Double'], 'Double', 'm')
        self.declare_extern_function('cos', ['Double'], 'Double', 'm')
        self.declare_extern_function('tan', ['Double'], 'Double', 'm')
        self.declare_extern_function('sqrt', ['Double'], 'Double', 'm')
        self.declare_extern_function('pow', ['Double', 'Double'], 'Double', 'm')
        self.declare_extern_function('exp', ['Double'], 'Double', 'm')
        self.declare_extern_function('log', ['Double'], 'Double', 'm')
        self.declare_extern_function('floor', ['Double'], 'Double', 'm')
        self.declare_extern_function('ceil', ['Double'], 'Double', 'm')
        
        # unistd.h functions
        self.declare_extern_function('read', ['Int', 'Pointer', 'Int64'], 'Int64', 'c')
        self.declare_extern_function('write', ['Int', 'Pointer', 'Int64'], 'Int64', 'c')
        self.declare_extern_function('close', ['Int'], 'Int', 'c')
        
    def generate_extern_declaration(self, node) -> ir.Function:
        """Generate LLVM IR for an extern function declaration.
        
        Args:
            node: ExternFunctionDeclaration AST node
        
        Returns:
            LLVM Function object
        """
        # Extract parameter types
        param_types = [param.type_annotation for param in node.parameters]
        
        # Declare the function
        return self.declare_extern_function(
            node.name,
            param_types,
            node.return_type,
            node.library,
            node.calling_convention
        )
    
    def link_libraries(self) -> List[str]:
        """Get list of libraries to link against.
        
        Returns:
            List of library names/paths to pass to linker
        """
        libraries = set()
        
        for func_name, lib_name in self.loaded_libraries.items():
            if lib_name in self.library_paths:
                libraries.add(lib_name)
        
        return list(libraries)
    
    def generate_library_link_flags(self) -> List[str]:
        """Generate linker flags for required libraries.
        
        Returns:
            List of linker flags (e.g., ['-lc', '-lm'])
        """
        flags = []
        libraries = self.link_libraries()
        
        for lib in libraries:
            if lib in ['c', 'libc']:
                flags.append('-lc')
            elif lib in ['m', 'libm']:
                flags.append('-lm')
            elif lib in ['pthread', 'libpthread']:
                flags.append('-lpthread')
            elif lib in ['dl', 'libdl']:
                flags.append('-ldl')
            else:
                flags.append(f'-l{lib}')
        
        return flags


def create_ffi_runtime_helpers(module: ir.Module, builder: ir.IRBuilder) -> Dict[str, ir.Function]:
    """Create runtime helper functions for FFI operations.
    
    Returns:
        Dictionary of helper function names to LLVM Function objects
    """
    helpers = {}
    
    # Helper: Safe string copy
    i8 = ir.IntType(8)
    i64 = ir.IntType(64)
    i8_ptr = i8.as_pointer()
    
    # string_copy(dest, src, max_len) -> dest
    string_copy_type = ir.FunctionType(i8_ptr, [i8_ptr, i8_ptr, i64])
    string_copy = ir.Function(module, string_copy_type, name='nxl_string_copy')
    
    # Helper: Check null pointer
    # is_null(ptr) -> bool
    is_null_type = ir.FunctionType(ir.IntType(1), [i8_ptr])
    is_null = ir.Function(module, is_null_type, name='nxl_is_null')
    
    helpers['string_copy'] = string_copy
    helpers['is_null'] = is_null
    
    return helpers


class StructMarshaller:
    """Handles marshalling of structs between NexusLang and C representations."""
    
    def __init__(self, ffi_codegen: FFICodegen):
        self.ffi = ffi_codegen
        self.module = ffi_codegen.module
        self.builder = ffi_codegen.builder
        
        # Cache for marshalling functions
        self.marshal_functions: Dict[str, ir.Function] = {}
        self.unmarshal_functions: Dict[str, ir.Function] = {}
    
    def marshal_struct_to_c(self, struct_name: str, nxl_value: ir.Value) -> ir.Value:
        """Marshal an NexusLang struct to C-compatible representation.
        
        For most cases, NexusLang structs are already C-compatible, so this
        just ensures proper memory layout and alignment.
        
        Args:
            struct_name: Name of the struct type
            nxl_value: LLVM value containing the NexusLang struct
        
        Returns:
            LLVM value containing C-compatible struct
        """
        if struct_name not in self.ffi.struct_types:
            raise ValueError(f"Unknown struct type: {struct_name}")
        
        # If the struct is already by-value, return as-is
        # If it's a pointer, we may need to dereference or copy
        struct_type = self.ffi.struct_types[struct_name]
        
        # Check if we need to create a copy for C
        # For now, assume direct compatibility
        return nxl_value
    
    def unmarshal_struct_from_c(self, struct_name: str, c_value: ir.Value) -> ir.Value:
        """Unmarshal a C struct to NexusLang representation.
        
        Args:
            struct_name: Name of the struct type
            c_value: LLVM value containing the C struct
        
        Returns:
            LLVM value containing NexusLang struct
        """
        if struct_name not in self.ffi.struct_types:
            raise ValueError(f"Unknown struct type: {struct_name}")
        
        # For now, assume direct compatibility
        return c_value
    
    def pass_struct_by_value(self, struct_name: str, struct_ptr: ir.Value) -> ir.Value:
        """Prepare struct to be passed by value to C function.
        
        Args:
            struct_name: Name of the struct type
            struct_ptr: Pointer to the struct
        
        Returns:
            Loaded struct value ready to pass
        """
        if struct_name not in self.ffi.struct_types:
            raise ValueError(f"Unknown struct type: {struct_name}")
        
        struct_type = self.ffi.struct_types[struct_name]
        
        # Load the struct from memory
        return self.builder.load(struct_ptr, name=f"{struct_name}_value")
    
    def pass_struct_by_reference(self, struct_name: str, struct_value: ir.Value) -> ir.Value:
        """Prepare struct to be passed by reference to C function.
        
        Args:
            struct_name: Name of the struct type
            struct_value: The struct value or pointer
        
        Returns:
            Pointer to the struct
        """
        if struct_name not in self.ffi.struct_types:
            raise ValueError(f"Unknown struct type: {struct_name}")
        
        struct_type = self.ffi.struct_types[struct_name]
        
        # If already a pointer, return as-is
        if isinstance(struct_value.type, ir.PointerType):
            return struct_value
        
        # Otherwise, allocate on stack and store value
        struct_ptr = self.builder.alloca(struct_type, name=f"{struct_name}_ptr")
        self.builder.store(struct_value, struct_ptr)
        return struct_ptr
    
    def copy_struct(self, struct_name: str, src_ptr: ir.Value, dst_ptr: ir.Value):
        """Deep copy a struct from source to destination.
        
        Args:
            struct_name: Name of the struct type
            src_ptr: Source struct pointer
            dst_ptr: Destination struct pointer
        """
        if struct_name not in self.ffi.struct_types:
            raise ValueError(f"Unknown struct type: {struct_name}")
        
        struct_type = self.ffi.struct_types[struct_name]
        fields = self.ffi.struct_field_info[struct_name]
        
        # Copy each field
        for i, (field_name, field_type) in enumerate(fields):
            # Get pointers to source and destination fields
            src_field_ptr = self.builder.gep(
                src_ptr, 
                [ir.Constant(ir.IntType(32), 0), ir.Constant(ir.IntType(32), i)],
                name=f"src_{field_name}_ptr"
            )
            dst_field_ptr = self.builder.gep(
                dst_ptr,
                [ir.Constant(ir.IntType(32), 0), ir.Constant(ir.IntType(32), i)],
                name=f"dst_{field_name}_ptr"
            )
            
            # Load and store the field
            field_value = self.builder.load(src_field_ptr, name=f"{field_name}_value")
            self.builder.store(field_value, dst_field_ptr)
    
    def generate_struct_constructor(self, struct_name: str, field_values: List[ir.Value]) -> ir.Value:
        """Generate code to construct a struct from field values.
        
        Args:
            struct_name: Name of the struct type
            field_values: List of LLVM values for each field
        
        Returns:
            Constructed struct value
        """
        if struct_name not in self.ffi.struct_types:
            raise ValueError(f"Unknown struct type: {struct_name}")
        
        struct_type = self.ffi.struct_types[struct_name]
        fields = self.ffi.struct_field_info[struct_name]
        
        if len(field_values) != len(fields):
            raise ValueError(
                f"Field count mismatch: expected {len(fields)}, got {len(field_values)}"
            )
        
        # Allocate struct on stack
        struct_ptr = self.builder.alloca(struct_type, name=f"{struct_name}_tmp")
        
        # Initialize each field
        for i, value in enumerate(field_values):
            field_ptr = self.builder.gep(
                struct_ptr,
                [ir.Constant(ir.IntType(32), 0), ir.Constant(ir.IntType(32), i)],
                name=f"field_{i}_ptr"
            )
            self.builder.store(value, field_ptr)
        
        # Load the complete struct
        return self.builder.load(struct_ptr, name=f"{struct_name}_value")
    
    def extract_field(self, struct_name: str, struct_value: ir.Value, 
                     field_index: int) -> ir.Value:
        """Extract a field from a struct.
        
        Args:
            struct_name: Name of the struct type
            struct_value: Struct value (not pointer)
            field_index: Index of the field to extract
        
        Returns:
            Field value
        """
        if struct_name not in self.ffi.struct_types:
            raise ValueError(f"Unknown struct type: {struct_name}")
        
        return self.builder.extract_value(struct_value, field_index, name="field_value")
    
    def insert_field(self, struct_name: str, struct_value: ir.Value,
                    field_index: int, field_value: ir.Value) -> ir.Value:
        """Insert a field value into a struct.
        
        Args:
            struct_name: Name of the struct type
            struct_value: Original struct value
            field_index: Index of the field to update
            field_value: New value for the field
        
        Returns:
            Updated struct value
        """
        if struct_name not in self.ffi.struct_types:
            raise ValueError(f"Unknown struct type: {struct_name}")
        
        return self.builder.insert_value(
            struct_value, field_value, field_index, name="updated_struct"
        )


class CallbackManager:
    """Manages callback functions for FFI.
    
    Callbacks allow C code to call back into NexusLang functions. This is essential
    for APIs like qsort, signal handlers, event handlers, etc.
    """
    
    def __init__(self, ffi_codegen: FFICodegen):
        self.ffi = ffi_codegen
        self.module = ffi_codegen.module
        self.builder = ffi_codegen.builder
        
        # Track callback wrappers
        self.callback_wrappers: Dict[str, ir.Function] = {}
        
        # Track NexusLang function -> callback wrapper mapping
        self.nxl_to_callback: Dict[str, str] = {}
    
    def create_callback_wrapper(self, nxl_func_name: str, 
                               param_types: List[str],
                               return_type: str,
                               calling_convention: str = "cdecl") -> ir.Function:
        """Create a C-callable wrapper for an NexusLang function.
        
        This generates a wrapper function that:
        1. Has C calling convention
        2. Converts C parameters to NexusLang format
        3. Calls the NexusLang function
        4. Converts NexusLang return value to C format
        
        Args:
            nxl_func_name: Name of the NexusLang function to wrap
            param_types: List of parameter types (NLPL type names)
            return_type: Return type (NLPL type name)
            calling_convention: C calling convention (default: cdecl)
        
        Returns:
            LLVM Function that can be passed as function pointer to C
        """
        # Generate unique wrapper name
        wrapper_name = f"__callback_{nxl_func_name}"
        
        # Check if already created
        if wrapper_name in self.callback_wrappers:
            return self.callback_wrappers[wrapper_name]
        
        # Map types to LLVM
        llvm_param_types = [self.ffi.map_type(t) for t in param_types]
        llvm_return_type = self.ffi.map_type(return_type)
        
        # Create wrapper function type with C calling convention
        wrapper_type = ir.FunctionType(llvm_return_type, llvm_param_types)
        wrapper_func = ir.Function(self.module, wrapper_type, name=wrapper_name)
        
        # Set calling convention
        if calling_convention.lower() == 'cdecl':
            wrapper_func.calling_convention = 'ccc'
        elif calling_convention.lower() == 'stdcall':
            wrapper_func.calling_convention = 'x86_stdcallcc'
        
        # Make it externally visible so C can call it
        wrapper_func.linkage = 'external'
        
        # Create entry block
        entry_block = wrapper_func.append_basic_block(name="entry")
        
        # Save current builder position
        old_builder = self.builder
        
        # Create new builder for wrapper
        wrapper_builder = ir.IRBuilder(entry_block)
        
        # Get the NexusLang function to call
        nxl_func = self.module.get_global(nxl_func_name)
        if nxl_func is None:
            # Function might be defined later, create a declaration
            nxl_func_type = ir.FunctionType(llvm_return_type, llvm_param_types)
            nxl_func = ir.Function(self.module, nxl_func_type, name=nxl_func_name)
        
        # Convert C parameters to NexusLang format
        converted_args = []
        for i, (arg, param_type) in enumerate(zip(wrapper_func.args, param_types)):
            # String marshalling: C char* -> NexusLang string
            if param_type in ['String', 'str']:
                # C strings are already i8*, so direct pass-through
                # NexusLang will handle null-termination
                converted_args.append(arg)
            
            # Struct marshalling: C struct -> NexusLang struct
            elif param_type in self.ffi.struct_types:
                # Structs are passed by value or pointer
                # If by pointer, dereference; if by value, use directly
                if isinstance(arg.type, ir.PointerType):
                    # Load the struct from pointer
                    struct_value = wrapper_builder.load(arg, name=f\"struct_{i}_value\")\n                    converted_args.append(struct_value)
                else:
                    # Already by value
                    converted_args.append(arg)
            
            # Pointer types: Direct pass-through
            elif param_type.endswith('*') or param_type == 'Pointer':
                converted_args.append(arg)
            
            # Primitive types: Direct pass-through
            else:
                converted_args.append(arg)
        
        # Call the NexusLang function
        result = wrapper_builder.call(nxl_func, converted_args, name="nxl_result")
        
        # Convert NexusLang return value to C format (if needed)
        c_result = result  # Direct pass-through for now
        
        # Return the result
        if return_type == 'Void':
            wrapper_builder.ret_void()
        else:
            wrapper_builder.ret(c_result)
        
        # Restore builder
        self.builder = old_builder
        
        # Cache the wrapper
        self.callback_wrappers[wrapper_name] = wrapper_func
        self.nxl_to_callback[nxl_func_name] = wrapper_name
        
        return wrapper_func
    
    def get_callback_pointer(self, nxl_func_name: str,
                           param_types: List[str],
                           return_type: str) -> ir.Value:
        """Get a function pointer to a callback wrapper.
        
        This can be passed to C functions expecting function pointers.
        
        Args:
            nxl_func_name: Name of the NexusLang function
            param_types: Parameter types
            return_type: Return type
        
        Returns:
            Function pointer as LLVM value
        """
        # Create or get the wrapper
        wrapper = self.create_callback_wrapper(
            nxl_func_name, param_types, return_type
        )
        
        # Cast to appropriate function pointer type if needed
        # For most cases, wrapper itself is the function pointer
        return wrapper
    
    def create_comparison_callback(self, compare_func_name: str) -> ir.Function:
        """Create a qsort/bsearch-compatible comparison callback.
        
        The comparison function must have signature:
        int compare(const void* a, const void* b)
        
        Args:
            compare_func_name: Name of NexusLang comparison function
        
        Returns:
            Wrapper function compatible with qsort/bsearch
        """
        return self.create_callback_wrapper(
            compare_func_name,
            ['Pointer', 'Pointer'],  # void* pointers
            'Int'  # Returns int
        )
    
    def create_signal_handler_callback(self, handler_func_name: str) -> ir.Function:
        """Create a signal handler callback.
        
        Signal handlers have signature:
        void handler(int signum)
        
        Args:
            handler_func_name: Name of NexusLang signal handler function
        
        Returns:
            Wrapper function compatible with signal()
        """
        return self.create_callback_wrapper(
            handler_func_name,
            ['Int'],  # Signal number
            'Void'  # No return
        )
    
    def create_foreach_callback(self, action_func_name: str,
                               element_type: str) -> ir.Function:
        """Create a callback for iteration/foreach operations.
        
        Common in APIs that iterate over collections with user-defined actions.
        
        Args:
            action_func_name: Name of NexusLang action function
            element_type: Type of elements being iterated
        
        Returns:
            Wrapper function for iteration callback
        """
        return self.create_callback_wrapper(
            action_func_name,
            [element_type],  # Element being processed
            'Void'  # Typically no return
        )
    
    def declare_callback_extern(self, c_func_name: str,
                               param_types: List[str],
                               callback_param_index: int,
                               callback_param_types: List[str],
                               callback_return_type: str,
                               return_type: str,
                               library: Optional[str] = None) -> ir.Function:
        """Declare an external C function that takes a callback.
        
        Args:
            c_func_name: Name of the C function
            param_types: Types of all parameters (including callback)
            callback_param_index: Index of the callback parameter
            callback_param_types: Parameter types of the callback function
            callback_return_type: Return type of the callback
            return_type: Return type of the C function
            library: Library name
        
        Returns:
            LLVM Function declaration
        """
        # Map parameter types
        llvm_params = []
        for i, ptype in enumerate(param_types):
            if i == callback_param_index:
                # This is the callback parameter - use function pointer type
                cb_param_types = [self.ffi.map_type(t) for t in callback_param_types]
                cb_return = self.ffi.map_type(callback_return_type)
                callback_type = ir.FunctionType(cb_return, cb_param_types)
                llvm_params.append(callback_type.as_pointer())
            else:
                llvm_params.append(self.ffi.map_type(ptype))
        
        # Create function type
        llvm_return = self.ffi.map_type(return_type)
        func_type = ir.FunctionType(llvm_return, llvm_params)
        
        # Declare the function
        func = ir.Function(self.module, func_type, name=c_func_name)
        func.calling_convention = 'ccc'
        func.linkage = 'external'
        
        # Cache it
        self.ffi.extern_functions[c_func_name] = func
        
        if library:
            self.ffi.loaded_libraries[c_func_name] = library
        
        return func
