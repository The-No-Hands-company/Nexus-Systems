"""
FFI (Foreign Function Interface) module for NexusLang.
Allows calling functions from C libraries using ctypes.
"""

import ctypes
import ctypes.util
from typing import Dict, List, Any, Optional, Callable
from ...runtime.runtime import Runtime

class FFILibrary:
    """Wrapper for a loaded C library."""
    
    def __init__(self, library_name: str, handle: ctypes.CDLL):
        self.name = library_name
        self.handle = handle
        self.functions: Dict[str, Callable] = {}
    
    def get_function(self, name: str, argtypes: List[Any], restype: Any) -> Callable:
        """Get a function from the library with specified signature."""
        if name in self.functions:
            return self.functions[name]
        
        func = getattr(self.handle, name)
        func.argtypes = argtypes
        func.restype = restype
        self.functions[name] = func
        return func

class FFIManager:
    """Manages FFI libraries and function calls."""
    
    def __init__(self):
        self.libraries: Dict[str, FFILibrary] = {}
        self.type_map = {
            'int': ctypes.c_int,
            'long': ctypes.c_long,
            'float': ctypes.c_float,
            'double': ctypes.c_double,
            'char': ctypes.c_char,
            'char*': ctypes.c_char_p,
            'void*': ctypes.c_void_p,
            'size_t': ctypes.c_size_t,
            'uint8': ctypes.c_uint8,
            'uint16': ctypes.c_uint16,
            'uint32': ctypes.c_uint32,
            'uint64': ctypes.c_uint64,
            'int8': ctypes.c_int8,
            'int16': ctypes.c_int16,
            'int32': ctypes.c_int32,
            'int64': ctypes.c_int64,
        }
    
    def load_library(self, name: str, path: Optional[str] = None) -> FFILibrary:
        """Load a C library by name or path."""
        if name in self.libraries:
            return self.libraries[name]
        
        try:
            if path:
                handle = ctypes.CDLL(path)
            else:
                # Try to find library in system paths
                lib_path = ctypes.util.find_library(name)
                if not lib_path:
                    raise RuntimeError(f"Library '{name}' not found in system paths")
                handle = ctypes.CDLL(lib_path)
            
            library = FFILibrary(name, handle)
            self.libraries[name] = library
            return library
        except OSError as e:
            raise RuntimeError(f"Failed to load library '{name}': {e}")
    
    def map_type(self, type_name: str) -> Any:
        """Map NexusLang/string type name to ctypes type."""
        if type_name in self.type_map:
            return self.type_map[type_name]
        elif type_name == 'void':
            return None
        elif type_name.startswith('struct_'):
            # Handle struct types - will be resolved at runtime
            return ctypes.c_void_p  # Placeholder for struct pointers
        else:
            raise ValueError(f"Unknown FFI type: {type_name}")
    
    def struct_to_ctypes(self, struct_instance, struct_name: str) -> ctypes.Structure:
        """
        Convert NexusLang StructureInstance to ctypes.Structure for FFI calls.
        Enables passing structs by value to C functions.
        """
        from ...runtime.structures import StructureInstance
        
        if not isinstance(struct_instance, StructureInstance):
            raise TypeError(f"Expected StructureInstance, got {type(struct_instance).__name__}")
        
        # Create dynamic ctypes.Structure class
        fields = []
        for field_name, field_info in struct_instance.definition.fields.items():
            # Map NexusLang types to ctypes types
            type_code = field_info.type_code
            if type_code in ('i', 'I'):  # int
                ctype = ctypes.c_int
            elif type_code in ('l', 'L'):  # long
                ctype = ctypes.c_long
            elif type_code in ('q', 'Q'):  # long long
                ctype = ctypes.c_longlong
            elif type_code == 'f':  # float
                ctype = ctypes.c_float
            elif type_code == 'd':  # double
                ctype = ctypes.c_double
            elif type_code == 'c':  # char
                ctype = ctypes.c_char
            elif type_code == 'b':  # signed char
                ctype = ctypes.c_byte
            elif type_code == 'B':  # unsigned char
                ctype = ctypes.c_ubyte
            elif type_code == 'h':  # short
                ctype = ctypes.c_short
            elif type_code == 'H':  # unsigned short
                ctype = ctypes.c_ushort
            else:
                ctype = ctypes.c_void_p  # Default to pointer
            
            fields.append((field_name, ctype))
        
        # Create ctypes Structure class
        class CStruct(ctypes.Structure):
            _fields_ = fields
        
        # Create instance and populate from NexusLang struct
        c_struct = CStruct()
        for field_name in struct_instance.definition.fields:
            value = struct_instance.get_field(field_name)
            setattr(c_struct, field_name, value)
        
        return c_struct
    
    def register_callback(self, callback_func: Callable, arg_types: List[str], return_type: str) -> Any:
        """
        Register NexusLang function as C callback (function pointer).
        Enables C→NLPL callbacks for event handlers, qsort comparators, etc.
        
        Example:
            def my_handler(value):
                print(f"Callback called with {value}")
                return 0
            
            callback_ptr = register_callback(my_handler, ['int'], 'int')
            # Pass callback_ptr to C function expecting function pointer
        """
        # Map types
        c_arg_types = [self.map_type(t) for t in arg_types]
        c_return_type = self.map_type(return_type) or None
        
        # Create ctypes function prototype
        CFUNCTYPE = ctypes.CFUNCTYPE(c_return_type, *c_arg_types)
        
        # Wrap NexusLang function to match C calling convention
        def c_wrapper(*args):
            try:
                result = callback_func(*args)
                return result if c_return_type else None
            except Exception as e:
                print(f"Error in callback: {e}")
                return 0 if c_return_type else None
        
        # Create and return function pointer
        callback_ptr = CFUNCTYPE(c_wrapper)
        return callback_ptr
    
    def call_variadic(self, lib_name: str, func_name: str,
                     fixed_arg_types: List[str], return_type: str,
                     fixed_args: List[Any], variadic_args: List[Any]) -> Any:
        """
        Call variadic C function (like printf, sprintf).
        Supports functions with variable number of arguments.
        
        Example:
            call_variadic('c', 'printf', ['char*'], 'int', 
                         ['Hello %s %d'], ['World', 42])
        """
        if lib_name not in self.libraries:
            raise RuntimeError(f"Library '{lib_name}' not loaded")
        
        library = self.libraries[lib_name]
        
        # Map fixed argument types
        c_fixed_types = [self.map_type(t) for t in fixed_arg_types]
        c_return_type = self.map_type(return_type)
        
        # Get the function without setting argtypes (variadic functions need this)
        func = getattr(library.handle, func_name)
        func.restype = c_return_type
        # Don't set argtypes for variadic functions!
        
        # Convert fixed arguments
        c_fixed_args = []
        for arg, arg_type in zip(fixed_args, c_fixed_types):
            if arg_type == ctypes.c_char_p and isinstance(arg, str):
                c_fixed_args.append(arg.encode('utf-8'))
            else:
                c_fixed_args.append(arg)
        
        # Convert variadic arguments (best-effort type inference)
        c_variadic_args = []
        for arg in variadic_args:
            if isinstance(arg, str):
                c_variadic_args.append(arg.encode('utf-8'))
            elif isinstance(arg, float):
                c_variadic_args.append(ctypes.c_double(arg))
            elif isinstance(arg, int):
                c_variadic_args.append(ctypes.c_int(arg))
            else:
                c_variadic_args.append(arg)
        
        # Call function with all arguments
        result = func(*c_fixed_args, *c_variadic_args)
        
        # Convert result
        if c_return_type == ctypes.c_char_p and result:
            return result.decode('utf-8')
        return result
    
    def call_function(self, lib_name: str, func_name: str, 
                     arg_types: List[str], return_type: str, 
                     args: List[Any]) -> Any:
        """Call a C function from a loaded library."""
        if lib_name not in self.libraries:
            raise RuntimeError(f"Library '{lib_name}' not loaded")
        
        library = self.libraries[lib_name]
        
        # Map type names to ctypes
        c_arg_types = [self.map_type(t) for t in arg_types]
        c_return_type = self.map_type(return_type)
        
        # Get the function
        func = library.get_function(func_name, c_arg_types, c_return_type)
        
        # Convert arguments to appropriate ctypes
        c_args = []
        for arg, arg_type in zip(args, c_arg_types):
            if arg_type == ctypes.c_char_p and isinstance(arg, str):
                c_args.append(arg.encode('utf-8'))
            else:
                c_args.append(arg)
        
        # Call the function
        result = func(*c_args)
        
        # Convert result back to Python type
        if c_return_type == ctypes.c_char_p and result:
            return result.decode('utf-8')
        return result

def register_ffi_functions(runtime: Runtime) -> None:
    """Register FFI functions with the runtime."""
    ffi_manager = FFIManager()
    runtime.ffi_manager = ffi_manager

    _register_ffi_core_functions(runtime, ffi_manager)
    _register_ffi_c_helpers(runtime, ffi_manager)
    _register_ffi_string_helpers(runtime)
    _register_ffi_struct_callback_variadic_functions(runtime, ffi_manager)


def _register_ffi_core_functions(runtime: Runtime, ffi_manager: FFIManager) -> None:
    runtime.register_function(
        "ffi_load_library",
        lambda name, path=None: ffi_manager.load_library(name, path),
    )
    runtime.register_function(
        "ffi_call",
        lambda lib, func, arg_types, return_type, *args:
            ffi_manager.call_function(lib, func, arg_types, return_type, list(args)),
    )


def _register_ffi_c_helpers(runtime: Runtime, ffi_manager: FFIManager) -> None:
    def c_strlen(s: str) -> int:
        """Call C strlen function."""
        try:
            ffi_manager.load_library("c")
            return ffi_manager.call_function("c", "strlen", ["char*"], "size_t", [s])
        except Exception as e:
            raise RuntimeError(f"Failed to call strlen: {e}")

    def c_malloc(size: int) -> int:
        """Call C malloc function."""
        try:
            ffi_manager.load_library("c")
            return ffi_manager.call_function("c", "malloc", ["size_t"], "void*", [size])
        except Exception as e:
            raise RuntimeError(f"Failed to call malloc: {e}")

    def c_free(ptr: int) -> None:
        """Call C free function."""
        try:
            ffi_manager.load_library("c")
            ffi_manager.call_function("c", "free", ["void*"], "void", [ptr])
        except Exception as e:
            raise RuntimeError(f"Failed to call free: {e}")

    runtime.register_function("c_strlen", c_strlen)
    runtime.register_function("c_malloc", c_malloc)
    runtime.register_function("c_free", c_free)


def _register_ffi_string_helpers(runtime: Runtime) -> None:
    def to_c_string(s: str) -> bytes:
        if not isinstance(s, str):
            raise TypeError(f"to_c_string expects string, got {type(s).__name__}")
        return s.encode('utf-8') + b'\x00'

    def from_c_string(c_str: bytes) -> str:
        if isinstance(c_str, bytes):
            if c_str.endswith(b'\x00'):
                c_str = c_str[:-1]
            return c_str.decode('utf-8')
        if isinstance(c_str, str):
            return c_str
        if isinstance(c_str, int):
            char_p = ctypes.cast(c_str, ctypes.c_char_p)
            if char_p.value:
                return char_p.value.decode('utf-8')
            return ""
        raise TypeError(f"from_c_string expects bytes, string, or pointer, got {type(c_str).__name__}")

    def string_to_pointer(s: str) -> int:
        if not isinstance(s, str):
            raise TypeError(f"string_to_pointer expects string, got {type(s).__name__}")
        c_str = s.encode('utf-8')
        ptr = ctypes.cast(ctypes.c_char_p(c_str), ctypes.c_void_p).value
        return ptr if ptr is not None else 0

    def pointer_to_string(ptr: int, length: Optional[int] = None) -> str:
        if not isinstance(ptr, int):
            raise TypeError(f"pointer_to_string expects integer pointer, got {type(ptr).__name__}")

        if ptr == 0:
            raise ValueError("Cannot dereference null pointer")

        if length is not None:
            if ptr is None or (hasattr(ptr, 'value') and ptr.value == 0):
                raise FFIError("from_c_string: null pointer dereference")
            c_array = ctypes.cast(ptr, ctypes.POINTER(ctypes.c_char * length))
            if not c_array:
                raise FFIError("from_c_string: cast produced null pointer")
            return c_array.contents.value.decode('utf-8')

        c_str = ctypes.cast(ptr, ctypes.c_char_p)
        return c_str.value.decode('utf-8') if c_str.value else ""

    runtime.register_function("to_c_string", to_c_string)
    runtime.register_function("from_c_string", from_c_string)
    runtime.register_function("string_to_pointer", string_to_pointer)
    runtime.register_function("pointer_to_string", pointer_to_string)


def _register_ffi_struct_callback_variadic_functions(runtime: Runtime, ffi_manager: FFIManager) -> None:
    def ffi_struct_to_ctypes(struct_instance, struct_name: str = ""):
        return ffi_manager.struct_to_ctypes(struct_instance, struct_name)

    def ffi_register_callback(callback_func: Callable, arg_types: List[str], return_type: str):
        return ffi_manager.register_callback(callback_func, arg_types, return_type)

    def ffi_call_variadic(
        lib_name: str,
        func_name: str,
        fixed_arg_types: List[str],
        return_type: str,
        fixed_args: List[Any],
        variadic_args: List[Any],
    ):
        return ffi_manager.call_variadic(
            lib_name,
            func_name,
            fixed_arg_types,
            return_type,
            fixed_args,
            variadic_args,
        )

    runtime.register_function("ffi_struct_to_ctypes", ffi_struct_to_ctypes)
    runtime.register_function("ffi_register_callback", ffi_register_callback)
    runtime.register_function("ffi_call_variadic", ffi_call_variadic)
