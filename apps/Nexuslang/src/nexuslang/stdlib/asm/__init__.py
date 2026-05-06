"""
Inline Assembly module for NexusLang.
Provides support for inline assembly code execution with platform detection.
"""

import platform
import struct
import ctypes
import mmap
from typing import Dict, List, Any, Optional
from ...runtime.runtime import Runtime

class AssemblyExecutor:
    """Executes inline assembly code on different platforms."""
    
    def __init__(self):
        self.platform = platform.machine().lower()
        self.system = platform.system().lower()
        self.is_64bit = struct.calcsize("P") == 8
        
    def get_architecture(self) -> str:
        """Detect current CPU architecture."""
        if self.platform in ['x86_64', 'amd64']:
            return 'x86_64'
        elif self.platform in ['i386', 'i686', 'x86']:
            return 'x86'
        elif self.platform in ['aarch64', 'arm64']:
            return 'arm64'
        elif self.platform.startswith('arm'):
            return 'arm'
        elif self.platform in ['riscv64']:
            return 'riscv64'
        else:
            return 'unknown'
    
    def assemble_code(self, asm_code: str, arch: Optional[str] = None) -> bytes:
        """
        Assemble assembly code to machine code.
        This is a simplified implementation. Production would use NASM, GAS, or LLVM.
        """
        if arch is None:
            arch = self.get_architecture()
        
        # For now, we'll support simple hardcoded instructions
        # In production, this would integrate with an assembler
        instructions = {
            'x86_64': {
                'nop': b'\x90',
                'ret': b'\xc3',
                'int3': b'\xcc',  # Breakpoint
                'mov rax, 0': b'\x48\xc7\xc0\x00\x00\x00\x00',
                'mov rax, 1': b'\x48\xc7\xc0\x01\x00\x00\x00',
                'mov rax, rbx': b'\x48\x89\xd8',
                'add rax, rbx': b'\x48\x01\xd8',
                'sub rax, rbx': b'\x48\x29\xd8',
                'inc rax': b'\x48\xff\xc0',
                'dec rax': b'\x48\xff\xc8',
            },
            'x86': {
                'nop': b'\x90',
                'ret': b'\xc3',
                'int3': b'\xcc',
            },
            'arm64': {
                'nop': b'\x1f\x20\x03\xd5',
                'ret': b'\xc0\x03\x5f\xd6',
            },
        }
        
        # Parse and assemble line by line
        machine_code = bytearray()
        has_ret = False
        
        for line in asm_code.strip().split('\n'):
            line = line.strip().lower()
            if not line or line.startswith(';') or line.startswith('#'):
                continue  # Skip empty lines and comments
            
            if line == 'ret':
                has_ret = True
            
            if arch in instructions and line in instructions[arch]:
                machine_code.extend(instructions[arch][line])
            else:
                raise ValueError(f"Unsupported instruction '{line}' for architecture {arch}")
        
        # Safety: Always append RET if not present to prevent runaway execution
        if not has_ret and arch in instructions:
            machine_code.extend(instructions[arch]['ret'])
        
        return bytes(machine_code)
    
    def execute_assembly(self, machine_code: bytes) -> int:
        """
        Execute machine code in memory.
        Returns the value in RAX/EAX (return value).
        """
        if not machine_code:
            raise ValueError("Empty machine code")
        
        # Allocate executable memory
        code_size = len(machine_code)
        page_size = mmap.PAGESIZE
        # Round up to page size
        alloc_size = ((code_size + page_size - 1) // page_size) * page_size
        
        if self.system == 'linux' or self.system == 'darwin':
            # Use mmap to allocate executable memory
            mem = mmap.mmap(-1, alloc_size,
                          prot=mmap.PROT_READ | mmap.PROT_WRITE | mmap.PROT_EXEC,
                          flags=mmap.MAP_PRIVATE | mmap.MAP_ANONYMOUS)
            mem.write(machine_code)
            mem.seek(0)  # Reset position
            
            # Get memory address
            mem_addr = ctypes.addressof(ctypes.c_char.from_buffer(mem))
            
            # Create a ctypes function pointer
            func_type = ctypes.CFUNCTYPE(ctypes.c_int64 if self.is_64bit else ctypes.c_int32)
            func = func_type(mem_addr)
            
            # Execute
            try:
                result = func()
            except Exception as e:
                mem.close()
                raise RuntimeError(f"Assembly execution failed: {e}")
            finally:
                mem.close()
            
            return result if result is not None else 0
        elif self.system == 'windows':
            # Windows: Use VirtualAlloc for executable memory
            kernel32 = ctypes.windll.kernel32
            
            # Allocate executable memory
            MEM_COMMIT = 0x1000
            MEM_RESERVE = 0x2000
            PAGE_EXECUTE_READWRITE = 0x40
            
            addr = kernel32.VirtualAlloc(None, code_size,
                                        MEM_COMMIT | MEM_RESERVE,
                                        PAGE_EXECUTE_READWRITE)
            if not addr:
                raise RuntimeError("Failed to allocate executable memory")
            
            # Copy machine code to allocated memory
            ctypes.memmove(addr, machine_code, code_size)
            
            # Create function pointer and execute
            func_type = ctypes.CFUNCTYPE(ctypes.c_int64 if self.is_64bit else ctypes.c_int32)
            func = func_type(addr)
            
            try:
                result = func()
            finally:
                # Free memory
                kernel32.VirtualFree(addr, 0, 0x8000)  # MEM_RELEASE
            
            return result
        else:
            raise RuntimeError(f"Unsupported operating system: {self.system}")

def register_asm_functions(runtime: Runtime) -> None:
    """Register inline assembly functions with the runtime."""
    executor = AssemblyExecutor()
    
    # Store executor in runtime for access
    runtime.asm_executor = executor
    
    # Platform detection functions
    runtime.register_function("asm_get_architecture", lambda: executor.get_architecture())
    runtime.register_function("asm_get_platform", lambda: executor.system)
    runtime.register_function("asm_is_64bit", lambda: executor.is_64bit)
    
    # Assembly execution
    def execute_asm(code: str) -> int:
        """Execute inline assembly code and return result."""
        machine_code = executor.assemble_code(code)
        return executor.execute_assembly(machine_code)
    
    def execute_asm_with_arch(code: str, arch: str) -> int:
        """Execute inline assembly for specific architecture."""
        machine_code = executor.assemble_code(code, arch)
        return executor.execute_assembly(machine_code)
    
    runtime.register_function("execute_asm", execute_asm)
    runtime.register_function("execute_asm_with_arch", execute_asm_with_arch)
    
    # Helper functions for common assembly operations
    def asm_nop() -> None:
        """Execute a NOP (no operation) instruction."""
        executor.execute_assembly(executor.assemble_code("nop\nret"))
    
    def asm_breakpoint() -> None:
        """Trigger a breakpoint (INT3 on x86)."""
        if executor.get_architecture().startswith('x86'):
            executor.execute_assembly(executor.assemble_code("int3"))
    
    runtime.register_function("asm_nop", asm_nop)
    runtime.register_function("asm_breakpoint", asm_breakpoint)
    
    # CPU feature detection (x86/x64)
    def has_cpuid() -> bool:
        """Check if CPUID instruction is available (x86/x64 only)."""
        return executor.get_architecture() in ['x86', 'x86_64']
    
    runtime.register_function("has_cpuid", has_cpuid)
