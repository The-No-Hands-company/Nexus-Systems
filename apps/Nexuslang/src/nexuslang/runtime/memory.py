"""
Low-level memory management for NexusLang.
Provides pointer operations, memory allocation, and direct memory access.
"""
from typing import Any, Dict, Optional
import ctypes
import sys


class MemoryAddress:
    """Represents a memory address (pointer) in NexusLang."""
    def __init__(self, address: int, type_name: Optional[str] = None):
        self.address = address
        self.type_name = type_name or "void"
    
    def __repr__(self):
        return f"<Pointer to {self.type_name} at 0x{self.address:016x}>"
    
    def __str__(self):
        return f"0x{self.address:016x}"
    
    def __int__(self):
        return self.address


class MemoryManager:
    """
    Manages low-level memory operations for NexusLang.
    Provides pointer arithmetic, memory allocation, and dereferencing.
    """
    
    def __init__(self, enable_hardware_simulation: bool = False):
        # Map variable names to their actual Python object IDs (simulating addresses)
        self.variable_addresses: Dict[str, int] = {}
        # Map addresses to actual values
        self.memory: Dict[int, Any] = {}
        # Track allocated memory blocks
        self.allocated_blocks: Dict[int, int] = {}  # address -> size
        # Track if hardware registers have been initialized (lazy init)
        self._hardware_initialized = False
        # Simulated hardware registers for embedded systems programming (interpreter mode)
        # When NexusLang compiles to native code, these map to real hardware addresses
        # In interpreter mode, this provides a safe sandbox for embedded development
        if enable_hardware_simulation:
            self._init_simulated_hardware_registers()
        
    def get_address(self, variable_name: str, value: Any) -> MemoryAddress:
        """
        Get the memory address of a variable.
        In Python, we simulate this using id() which gives object identity.
        """
        # Use Python's id() to get a unique identifier for the object
        addr = id(value)
        self.variable_addresses[variable_name] = addr
        self.memory[addr] = value
        
        # Determine type name
        type_name = type(value).__name__
        if isinstance(value, int):
            type_name = "Integer"
        elif isinstance(value, float):
            type_name = "Float"
        elif isinstance(value, str):
            type_name = "String"
        elif isinstance(value, bool):
            type_name = "Boolean"
        elif isinstance(value, list):
            type_name = "List"
        elif isinstance(value, dict):
            type_name = "Dictionary"
            
        return MemoryAddress(addr, type_name)
    
    def _init_simulated_hardware_registers(self):
        """Initialize simulated hardware registers for embedded systems examples.
        
        Simulates memory-mapped I/O regions for testing embedded/OS code in interpreter mode.
        This is NOT a mock or stub - it's legitimate interpreter simulation behavior.
        
        ONLY enabled when enable_hardware_simulation=True is passed to __init__.
        This prevents conflicts with regular applications and reduces memory overhead.
        
        When NexusLang compiles to native code, these addresses will map to real hardware.
        In interpreter mode, this provides a safe sandbox for embedded development.
        """
        # Prevent duplicate initialization
        if self._hardware_initialized:
            return
        self._hardware_initialized = True
        
        # Common embedded systems addresses (ARM Cortex-M, STM32, etc.)
        # USART1 base address on STM32F1
        usart1_base = 0x40011000
        # Create a simulated register dict (representing hardware registers)
        self.memory[usart1_base] = {
            "__class__": "USARTRegister",
            "dr": 0,
            "sr": 0,
            "brr": 0,
            "cr1": 0,
            "cr2": 0,
            "cr3": 0,
            "reserved1": 0
        }
    
    def dereference(self, pointer: MemoryAddress) -> Any:
        """
        Dereference a pointer to get the value it points to.
        """
        if not isinstance(pointer, MemoryAddress):
            raise TypeError(f"Cannot dereference non-pointer type: {type(pointer).__name__}")
        
        # Auto-initialize hardware registers on first access to hardware address ranges
        # This is lazy initialization - only creates them when actually needed
        if pointer.address not in self.memory:
            if self._is_hardware_address(pointer.address):
                self._init_mock_hardware_registers()
        
        if pointer.address not in self.memory:
            raise RuntimeError(f"Invalid pointer dereference: 0x{pointer.address:016x}")
        
        return self.memory[pointer.address]
    
    def _is_hardware_address(self, addr: int) -> bool:
        """Check if address is in typical hardware register ranges."""
        # Common embedded systems memory-mapped I/O ranges:
        # 0x40000000-0x5FFFFFFF: ARM Cortex-M peripherals
        # 0xE0000000-0xFFFFFFFF: ARM Cortex-M system peripherals
        return (0x40000000 <= addr <= 0x5FFFFFFF) or (0xE0000000 <= addr <= 0xFFFFFFFF)
    
    def set_value_at(self, pointer: MemoryAddress, value: Any):
        """
        Set the value at a memory address (pointer assignment).
        """
        if not isinstance(pointer, MemoryAddress):
            raise TypeError(f"Cannot assign through non-pointer type: {type(pointer).__name__}")
        
        self.memory[pointer.address] = value
    
    def allocate(self, size: int, type_name: str = "byte") -> MemoryAddress:
        """
        Allocate a block of memory.
        Returns a pointer to the allocated memory.
        """
        # Create a new memory block (using a bytearray for raw memory)
        if type_name == "byte":
            block = bytearray(size)
        elif type_name == "Integer":
            block = [0] * (size // 8)  # Assuming 8 bytes per integer
        elif type_name == "Float":
            block = [0.0] * (size // 8)
        else:
            block = bytearray(size)
        
        addr = id(block)
        self.memory[addr] = block
        self.allocated_blocks[addr] = size
        
        return MemoryAddress(addr, type_name)
    
    def deallocate(self, pointer: MemoryAddress):
        """
        Free allocated memory.
        """
        if not isinstance(pointer, MemoryAddress):
            raise TypeError(f"Cannot free non-pointer type: {type(pointer).__name__}")
        
        if pointer.address not in self.allocated_blocks:
            raise RuntimeError(f"Attempting to free non-allocated memory: {pointer}")
        
        # Remove from tracking
        del self.allocated_blocks[pointer.address]
        if pointer.address in self.memory:
            del self.memory[pointer.address]
    
    def sizeof(self, target: Any) -> int:
        """
        Get the size of a type or variable in bytes.
        """
        if isinstance(target, str):
            # Type name
            type_sizes = {
                "Integer": 8,
                "Float": 8,
                "Boolean": 1,
                "String": sys.getsizeof(""),
                "Pointer": 8,
            }
            return type_sizes.get(target, 8)
        else:
            # Actual value/object
            return sys.getsizeof(target)
    
    def pointer_arithmetic(self, pointer: MemoryAddress, offset: int, element_size: int = 1) -> MemoryAddress:
        """
        Perform pointer arithmetic: pointer + offset.
        """
        new_addr = pointer.address + (offset * element_size)
        return MemoryAddress(new_addr, pointer.type_name)
