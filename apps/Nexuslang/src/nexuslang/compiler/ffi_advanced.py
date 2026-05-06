"""
Advanced FFI features for NexusLang:
- Function pointer support and callbacks
- Automatic string conversion (NLPL String <-> C char*)
- Memory ownership tracking
- Callback trampoline generation

This module extends the basic FFI to support complex interactions
between NexusLang and C code.
"""

from llvmlite import ir
from typing import Dict, List, Optional, Callable, Set, Tuple, Type
from dataclasses import dataclass, field
from enum import Enum, auto


class MemoryOwnership(Enum):
    """Memory ownership semantics for FFI pointers."""
    OWNED = auto()      # NexusLang owns the memory (must free)
    BORROWED = auto()   # C code owns it (don't free)
    TRANSFER = auto()   # Ownership transferred from C to NexusLang
    SHARED = auto()     # Shared ownership (reference counted)


@dataclass
class CallbackSignature:
    """Represents a callback function signature for C->NLPL calls."""
    name: str
    param_types: List[str]
    return_type: str
    nxl_function: Optional[str] = None  # Name of NexusLang function
    trampoline_func: Optional[ir.Function] = None  # Generated trampoline
    c_function_ptr_type: Optional[ir.Type] = None


class StringConverter:
    """Handles automatic string conversion between NexusLang and C."""
    
    def __init__(self, module: ir.Module, builder: ir.IRBuilder):
        self.module = module
        self.builder = builder
        
        # Runtime helpers for string conversion
        self.helpers = self._create_string_helpers()
    
    def _create_string_helpers(self) -> Dict[str, ir.Function]:
        """Create runtime helper functions for string operations."""
        helpers = {}
        
        i8 = ir.IntType(8)
        i64 = ir.IntType(64)
        i8_ptr = i8.as_pointer()
        i8_ptr_ptr = i8_ptr.as_pointer()
        
        # nxl_string_to_c(nxl_str) -> char*
        # Converts NexusLang string to null-terminated C string
        # Allocates memory with malloc - caller must free
        string_to_c_type = ir.FunctionType(i8_ptr, [i8_ptr, i64])
        string_to_c = ir.Function(self.module, string_to_c_type, name='nxl_string_to_c')
        string_to_c.linkage = 'internal'
        helpers['string_to_c'] = string_to_c
        
        # nxl_string_from_c(c_str) -> nxl_str
        # Converts C null-terminated string to NexusLang string
        # Makes a copy - original C string unchanged
        string_from_c_type = ir.FunctionType(i8_ptr, [i8_ptr])
        string_from_c = ir.Function(self.module, string_from_c_type, name='nxl_string_from_c')
        string_from_c.linkage = 'internal'
        helpers['string_from_c'] = string_from_c
        
        # nxl_strlen(str) -> size_t
        # UTF-8 aware string length
        strlen_type = ir.FunctionType(i64, [i8_ptr])
        strlen_func = ir.Function(self.module, strlen_type, name='nxl_strlen')
        strlen_func.linkage = 'internal'
        helpers['strlen'] = strlen_func
        
        # nxl_string_dup(str) -> str
        # Duplicate a string (allocates new memory)
        strdup_type = ir.FunctionType(i8_ptr, [i8_ptr])
        strdup_func = ir.Function(self.module, strdup_type, name='nxl_string_dup')
        strdup_func.linkage = 'internal'
        helpers['strdup'] = strdup_func
        
        # Generate implementations
        self._implement_string_to_c(string_to_c)
        self._implement_string_from_c(string_from_c)
        self._implement_strlen(strlen_func)
        self._implement_strdup(strdup_func)
        
        return helpers
    
    def _implement_string_to_c(self, func: ir.Function):
        """Implement nxl_string_to_c helper."""
        entry = func.append_basic_block(name="entry")
        builder = ir.IRBuilder(entry)
        
        nxl_str = func.args[0]
        length = func.args[1]
        
        i8 = ir.IntType(8)
        i64 = ir.IntType(64)
        i8_ptr = i8.as_pointer()
        
        # Declare malloc
        malloc_type = ir.FunctionType(i8_ptr, [i64])
        malloc_func = ir.Function(self.module, malloc_type, name='malloc')
        malloc_func.linkage = 'external'
        
        # Declare memcpy
        memcpy_type = ir.FunctionType(i8_ptr, [i8_ptr, i8_ptr, i64])
        memcpy_func = ir.Function(self.module, memcpy_type, name='memcpy')
        memcpy_func.linkage = 'external'
        
        # Allocate length + 1 bytes (for null terminator)
        one = ir.Constant(i64, 1)
        alloc_size = builder.add(length, one, name='alloc_size')
        c_str = builder.call(malloc_func, [alloc_size], name='c_str')
        
        # Copy NexusLang string data
        builder.call(memcpy_func, [c_str, nxl_str, length])
        
        # Add null terminator
        null_ptr = builder.gep(c_str, [length], name='null_ptr')
        null_byte = ir.Constant(i8, 0)
        builder.store(null_byte, null_ptr)
        
        builder.ret(c_str)
    
    def _implement_string_from_c(self, func: ir.Function):
        """Implement nxl_string_from_c helper."""
        entry = func.append_basic_block(name="entry")
        builder = ir.IRBuilder(entry)
        
        c_str = func.args[0]
        
        i8 = ir.IntType(8)
        i64 = ir.IntType(64)
        i8_ptr = i8.as_pointer()
        
        # Declare strlen from libc
        strlen_type = ir.FunctionType(i64, [i8_ptr])
        strlen_func = ir.Function(self.module, strlen_type, name='strlen')
        strlen_func.linkage = 'external'
        
        # Declare malloc
        malloc_type = ir.FunctionType(i8_ptr, [i64])
        malloc_func = ir.Function(self.module, malloc_type, name='malloc')
        malloc_func.linkage = 'external'
        
        # Declare memcpy
        memcpy_type = ir.FunctionType(i8_ptr, [i8_ptr, i8_ptr, i64])
        memcpy_func = ir.Function(self.module, memcpy_type, name='memcpy')
        memcpy_func.linkage = 'external'
        
        # Get string length
        length = builder.call(strlen_func, [c_str], name='length')
        
        # Allocate NexusLang string (no null terminator needed)
        nxl_str = builder.call(malloc_func, [length], name='nxl_str')
        
        # Copy data
        builder.call(memcpy_func, [nxl_str, c_str, length])
        
        builder.ret(nxl_str)
    
    def _implement_strlen(self, func: ir.Function):
        """Implement nxl_strlen helper."""
        entry = func.append_basic_block(name="entry")
        loop = func.append_basic_block(name="loop")
        count_block = func.append_basic_block(name="count")
        advance = func.append_basic_block(name="advance")
        end = func.append_basic_block(name="end")

        builder = ir.IRBuilder(entry)

        i8 = ir.IntType(8)
        i64 = ir.IntType(64)
        c_str = func.args[0]

        # idx tracks byte offset, chars tracks Unicode code point count.
        idx_ptr = builder.alloca(i64, name='idx_ptr')
        chars_ptr = builder.alloca(i64, name='chars_ptr')
        builder.store(ir.Constant(i64, 0), idx_ptr)
        builder.store(ir.Constant(i64, 0), chars_ptr)
        builder.branch(loop)

        builder.position_at_start(loop)
        idx = builder.load(idx_ptr, name='idx')
        byte_ptr = builder.gep(c_str, [idx], name='byte_ptr')
        byte = builder.load(byte_ptr, name='byte')

        is_null = builder.icmp_unsigned('==', byte, ir.Constant(i8, 0), name='is_null')
        builder.cbranch(is_null, end, count_block)

        builder.position_at_start(count_block)
        # UTF-8 continuation bytes have prefix 10xxxxxx (0x80..0xBF).
        masked = builder.and_(byte, ir.Constant(i8, 0xC0), name='masked')
        is_continuation = builder.icmp_unsigned('==', masked, ir.Constant(i8, 0x80), name='is_continuation')
        chars = builder.load(chars_ptr, name='chars')
        next_chars = builder.select(
            is_continuation,
            chars,
            builder.add(chars, ir.Constant(i64, 1), name='chars_inc'),
            name='next_chars'
        )
        builder.store(next_chars, chars_ptr)
        builder.branch(advance)

        builder.position_at_start(advance)
        next_idx = builder.add(idx, ir.Constant(i64, 1), name='idx_inc')
        builder.store(next_idx, idx_ptr)
        builder.branch(loop)

        builder.position_at_start(end)
        result = builder.load(chars_ptr, name='result')
        builder.ret(result)
    
    def _implement_strdup(self, func: ir.Function):
        """Implement nxl_string_dup helper."""
        entry = func.append_basic_block(name="entry")
        builder = ir.IRBuilder(entry)
        
        str_arg = func.args[0]
        
        i8 = ir.IntType(8)
        i64 = ir.IntType(64)
        i8_ptr = i8.as_pointer()
        
        # Get string length
        strlen_func = self.helpers['strlen']
        length = builder.call(strlen_func, [str_arg], name='length')
        
        # Allocate new memory
        malloc_type = ir.FunctionType(i8_ptr, [i64])
        malloc_func = ir.Function(self.module, malloc_type, name='malloc')
        malloc_func.linkage = 'external'
        
        new_str = builder.call(malloc_func, [length], name='new_str')
        
        # Copy data
        memcpy_type = ir.FunctionType(i8_ptr, [i8_ptr, i8_ptr, i64])
        memcpy_func = ir.Function(self.module, memcpy_type, name='memcpy')
        memcpy_func.linkage = 'external'
        
        builder.call(memcpy_func, [new_str, str_arg, length])
        
        builder.ret(new_str)
    
    def convert_to_c_string(self, nxl_string: ir.Value, length: ir.Value) -> ir.Value:
        """Convert NexusLang string to C null-terminated string.
        
        Args:
            nxl_string: NexusLang string pointer
            length: String length in bytes
        
        Returns:
            Pointer to null-terminated C string (caller must free)
        """
        return self.builder.call(
            self.helpers['string_to_c'],
            [nxl_string, length],
            name='c_str'
        )
    
    def convert_from_c_string(self, c_string: ir.Value) -> ir.Value:
        """Convert C null-terminated string to NexusLang string.
        
        Args:
            c_string: C string pointer
        
        Returns:
            NexusLang string pointer (caller owns memory)
        """
        return self.builder.call(
            self.helpers['string_from_c'],
            [c_string],
            name='nxl_str'
        )
    
    def get_string_length(self, string: ir.Value) -> ir.Value:
        """Get length of a string.
        
        Args:
            string: String pointer (NLPL or C)
        
        Returns:
            String length as i64
        """
        return self.builder.call(
            self.helpers['strlen'],
            [string],
            name='str_len'
        )
    
    def duplicate_string(self, string: ir.Value) -> ir.Value:
        """Create a copy of a string.
        
        Args:
            string: String pointer to duplicate
        
        Returns:
            Pointer to new string (caller owns memory)
        """
        return self.builder.call(
            self.helpers['strdup'],
            [string],
            name='str_copy'
        )


class CallbackManager:
    """Manages callbacks from C code to NexusLang functions."""
    
    def __init__(self, module: ir.Module, builder: ir.IRBuilder):
        self.module = module
        self.builder = builder
        
        # Registered callbacks
        self.callbacks: Dict[str, CallbackSignature] = {}
        
        # Generated trampolines
        self.trampolines: Dict[str, ir.Function] = {}
    
    def register_callback(self, name: str, param_types: List[str], 
                         return_type: str, nxl_function: str) -> ir.Value:
        """Register an NexusLang function as a C callback.
        
        Creates a trampoline function that C code can call, which then
        invokes the NexusLang function with proper type conversions.
        
        Args:
            name: Callback name
            param_types: List of parameter type names
            return_type: Return type name
            nxl_function: Name of NexusLang function to call
        
        Returns:
            Function pointer that C code can call
        """
        signature = CallbackSignature(
            name=name,
            param_types=param_types,
            return_type=return_type,
            nxl_function=nxl_function
        )

        trampoline = self._generate_trampoline(signature)
        signature.trampoline_func = trampoline
        self.callbacks[name] = signature
        self.trampolines[name] = trampoline
        return trampoline
    
    def _generate_trampoline(self, sig: CallbackSignature) -> ir.Function:
        """Generate trampoline function for callback.
        
        The trampoline:
        1. Receives parameters from C code
        2. Converts types if needed (e.g., C strings to NexusLang strings)
        3. Calls the NexusLang function
        4. Converts return value if needed
        5. Returns to C code
        
        Args:
            sig: Callback signature
        
        Returns:
            Generated trampoline function
        """
        from .ffi import FFICodegen
        
        # Map types to LLVM
        ffi = FFICodegen(self.module, self.builder)
        param_llvm_types = [ffi.map_type(t) for t in sig.param_types]
        return_llvm_type = ffi.map_type(sig.return_type)
        
        # Create trampoline function type
        func_type = ir.FunctionType(return_llvm_type, param_llvm_types)
        trampoline = ir.Function(
            self.module, 
            func_type, 
            name=f"callback_trampoline_{sig.name}"
        )
        trampoline.calling_convention = 'ccc'  # C calling convention
        
        # Create entry block
        entry = trampoline.append_basic_block(name="entry")
        builder = ir.IRBuilder(entry)
        
        # Resolve/create callback target and emit direct call-through.
        target = self.module.globals.get(sig.nxl_function)
        if target is None:
            target = ir.Function(self.module, func_type, name=sig.nxl_function)
            target.linkage = 'external'
        elif not isinstance(target, ir.Function):
            raise TypeError(
                f"Callback target '{sig.nxl_function}' exists but is not callable"
            )
        elif target.function_type != func_type:
            raise TypeError(
                f"Callback target '{sig.nxl_function}' signature mismatch: "
                f"expected {func_type}, got {target.function_type}"
            )

        call_args = list(trampoline.args)
        if sig.return_type == 'Void':
            builder.call(target, call_args)
            builder.ret_void()
        else:
            result = builder.call(target, call_args, name='callback_result')
            builder.ret(result)
        
        return trampoline
    
    def get_callback_pointer(self, name: str) -> Optional[ir.Value]:
        """Get function pointer for a registered callback.
        
        Args:
            name: Callback name
        
        Returns:
            Function pointer or None if not registered
        """
        if name in self.trampolines:
            return self.trampolines[name]
        return None


class MemoryOwnershipTracker:
    """Tracks memory ownership for FFI pointers."""
    
    def __init__(self):
        # Maps pointer values to ownership semantics
        self.ownership: Dict[str, MemoryOwnership] = {}
        
        # Tracks which pointers need cleanup
        self.cleanup_required: Set[str] = set()
    
    def mark_owned(self, pointer_name: str):
        """Mark pointer as owned by NexusLang (must free)."""
        self.ownership[pointer_name] = MemoryOwnership.OWNED
        self.cleanup_required.add(pointer_name)
    
    def mark_borrowed(self, pointer_name: str):
        """Mark pointer as borrowed from C (don't free)."""
        self.ownership[pointer_name] = MemoryOwnership.BORROWED
    
    def mark_transferred(self, pointer_name: str):
        """Mark pointer as transferred from C to NexusLang."""
        self.ownership[pointer_name] = MemoryOwnership.TRANSFER
        self.cleanup_required.add(pointer_name)
    
    def mark_shared(self, pointer_name: str):
        """Mark pointer as having shared ownership."""
        self.ownership[pointer_name] = MemoryOwnership.SHARED
    
    def requires_cleanup(self, pointer_name: str) -> bool:
        """Check if pointer requires cleanup by NexusLang."""
        return pointer_name in self.cleanup_required
    
    def get_ownership(self, pointer_name: str) -> Optional[MemoryOwnership]:
        """Get ownership semantics for a pointer."""
        return self.ownership.get(pointer_name)
    
    def transfer_ownership(self, from_ptr: str, to_ptr: str):
        """Transfer ownership from one pointer to another."""
        if from_ptr in self.ownership:
            ownership = self.ownership[from_ptr]
            self.ownership[to_ptr] = ownership
            
            if from_ptr in self.cleanup_required:
                self.cleanup_required.remove(from_ptr)
                if ownership in (MemoryOwnership.OWNED, MemoryOwnership.TRANSFER):
                    self.cleanup_required.add(to_ptr)
    
    def release(self, pointer_name: str):
        """Release tracking for a pointer (after cleanup)."""
        if pointer_name in self.ownership:
            del self.ownership[pointer_name]
        if pointer_name in self.cleanup_required:
            self.cleanup_required.remove(pointer_name)


class FunctionPointerManager:
    """Manages function pointers for FFI."""
    
    def __init__(self, module: ir.Module, builder: ir.IRBuilder):
        self.module = module
        self.builder = builder
        
        # Function pointer types
        self.function_types: Dict[str, ir.Type] = {}
    
    def create_function_pointer_type(self, name: str, param_types: List[str],
                                    return_type: str) -> ir.Type:
        """Create a function pointer type.
        
        Args:
            name: Type name for the function pointer
            param_types: List of parameter type names
            return_type: Return type name
        
        Returns:
            LLVM function pointer type
        """
        from .ffi import FFICodegen
        
        ffi = FFICodegen(self.module, self.builder)
        param_llvm_types = [ffi.map_type(t) for t in param_types]
        return_llvm_type = ffi.map_type(return_type)
        
        func_type = ir.FunctionType(return_llvm_type, param_llvm_types)
        func_ptr_type = func_type.as_pointer()
        
        self.function_types[name] = func_ptr_type
        return func_ptr_type
    
    def call_function_pointer(self, func_ptr: ir.Value, args: List[ir.Value]) -> ir.Value:
        """Call a function through a function pointer.
        
        Args:
            func_ptr: Function pointer value
            args: Arguments to pass
        
        Returns:
            Call result
        """
        return self.builder.call(func_ptr, args, name='indirect_call_result')
    
    def cast_function_pointer(self, func_ptr: ir.Value, 
                            target_type: ir.Type) -> ir.Value:
        """Cast a function pointer to a different type.
        
        Args:
            func_ptr: Original function pointer
            target_type: Target function pointer type
        
        Returns:
            Casted function pointer
        """
        return self.builder.bitcast(func_ptr, target_type, name='casted_func_ptr')


def create_ffi_cleanup_function(module: ir.Module, 
                                ownership_tracker: MemoryOwnershipTracker) -> ir.Function:
    """Create cleanup function that frees all owned FFI pointers.
    
    This function should be called at program exit or when leaving scope.
    
    Args:
        module: LLVM module
        ownership_tracker: Ownership tracker with pointers to clean up
    
    Returns:
        Cleanup function
    """
    i8_ptr = ir.IntType(8).as_pointer()
    void_type = ir.VoidType()
    
    cleanup_type = ir.FunctionType(void_type, [])
    cleanup_func = ir.Function(module, cleanup_type, name='nxl_ffi_cleanup')
    
    entry = cleanup_func.append_basic_block(name="entry")
    builder = ir.IRBuilder(entry)
    
    # Declare free function
    free_type = ir.FunctionType(void_type, [i8_ptr])
    free_func = ir.Function(module, free_type, name='free')
    free_func.linkage = 'external'
    
    # Free all owned pointers
    # Note: This is a simplified version - actual implementation would
    # need to track pointers at runtime
    
    builder.ret_void()
    
    return cleanup_func
