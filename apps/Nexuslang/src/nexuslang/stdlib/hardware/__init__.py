"""
NLPL Hardware Module - Low-Level Hardware Access

Provides direct hardware access capabilities including:
- Port I/O operations (in/out instructions)
- Memory-mapped I/O
- Interrupt handling
- DMA control
- CPU control registers

WARNING: These operations require privileged access (ring 0) and can
cause system instability if used incorrectly. Use with extreme caution.
"""

import ctypes
import os
import platform
import mmap
from typing import Optional, Dict, Tuple, Callable
from enum import IntEnum


class PortAccessError(Exception):
    """Raised when port I/O operations fail"""
    pass


class MMIOError(Exception):
    """Raised when memory-mapped I/O operations fail"""
    pass


class PrivilegeError(Exception):
    """Raised when insufficient privileges for hardware access"""
    pass


class InterruptError(Exception):
    """Raised when interrupt handling operations fail"""
    pass


class DMAError(Exception):
    """Raised when DMA operations fail"""
    pass


class CPUControlError(Exception):
    """Raised when CPU control register operations fail"""
    pass


# Cache Control Hints
class CacheControl(IntEnum):
    """Cache control hints for memory mapping"""
    WB = 0   # Write-Back (cached)
    WT = 1   # Write-Through (cached with immediate writeback)
    UC = 2   # Uncacheable (no caching)
    WC = 3   # Write-Combining (uncached, buffered writes)
    WP = 4   # Write-Protected (reads cached, writes uncached)


# Global MMIO mapping registry
_mmio_mappings: Dict[int, Tuple[mmap.mmap, int, int, int]] = {}

# Global interrupt handler registry
_interrupt_handlers: Dict[int, callable] = {}

# Interrupt state
_interrupts_enabled: bool = True

# Global DMA channel allocation registry
_dma_channels: Dict[int, dict] = {}  # channel -> {allocated, source, dest, count, mode}

# DMA controller state
_dma_initialized: bool = False


def _check_privileges() -> bool:
    """Check if running with sufficient privileges for hardware access"""
    if platform.system() == "Linux":
        return os.geteuid() == 0
    elif platform.system() == "Windows":
        # Windows requires administrator privileges
        try:
            import ctypes
            return ctypes.windll.shell32.IsUserAnAdmin() != 0
        except Exception:
            return False
    return False


def _require_privileges():
    """Raise PrivilegeError if insufficient privileges"""
    if not _check_privileges():
        raise PrivilegeError(
            "Hardware access requires root/administrator privileges. "
            "Run with sudo (Linux) or as Administrator (Windows)."
        )


# Port I/O Operations
def read_port_byte(runtime, port: int) -> int:
    """
    Read a byte (8 bits) from an I/O port
    
    Args:
        port: Port number (0-65535)
        
    Returns:
        Byte value (0-255)
        
    Raises:
        PortAccessError: If port access fails
        PrivilegeError: If insufficient privileges
        
    Example:
        # Read from keyboard controller status port
        set status to read_port with port 0x64 returns Byte
    """
    _require_privileges()
    
    if not (0 <= port <= 0xFFFF):
        raise PortAccessError(f"Invalid port number: {port}. Must be 0-65535.")
    
    if platform.system() == "Linux":
        try:
            # Use /dev/port for direct port access on Linux
            with open("/dev/port", "rb") as f:
                f.seek(port)
                return ord(f.read(1))
        except PermissionError:
            raise PrivilegeError("Cannot access /dev/port. Run with sudo.")
        except Exception as e:
            raise PortAccessError(f"Failed to read from port 0x{port:04X}: {e}")
    else:
        raise PortAccessError(f"Port I/O not implemented for {platform.system()}")


def read_port_word(runtime, port: int) -> int:
    """
    Read a word (16 bits) from an I/O port
    
    Args:
        port: Port number (0-65535)
        
    Returns:
        Word value (0-65535)
        
    Raises:
        PortAccessError: If port access fails
        PrivilegeError: If insufficient privileges
    """
    _require_privileges()
    
    if not (0 <= port <= 0xFFFF):
        raise PortAccessError(f"Invalid port number: {port}. Must be 0-65535.")
    
    if platform.system() == "Linux":
        try:
            with open("/dev/port", "rb") as f:
                f.seek(port)
                data = f.read(2)
                return int.from_bytes(data, byteorder='little')
        except PermissionError:
            raise PrivilegeError("Cannot access /dev/port. Run with sudo.")
        except Exception as e:
            raise PortAccessError(f"Failed to read from port 0x{port:04X}: {e}")
    else:
        raise PortAccessError(f"Port I/O not implemented for {platform.system()}")


def read_port_dword(runtime, port: int) -> int:
    """
    Read a double word (32 bits) from an I/O port
    
    Args:
        port: Port number (0-65535)
        
    Returns:
        Dword value (0-4294967295)
        
    Raises:
        PortAccessError: If port access fails
        PrivilegeError: If insufficient privileges
    """
    _require_privileges()
    
    if not (0 <= port <= 0xFFFF):
        raise PortAccessError(f"Invalid port number: {port}. Must be 0-65535.")
    
    if platform.system() == "Linux":
        try:
            with open("/dev/port", "rb") as f:
                f.seek(port)
                data = f.read(4)
                return int.from_bytes(data, byteorder='little')
        except PermissionError:
            raise PrivilegeError("Cannot access /dev/port. Run with sudo.")
        except Exception as e:
            raise PortAccessError(f"Failed to read from port 0x{port:04X}: {e}")
    else:
        raise PortAccessError(f"Port I/O not implemented for {platform.system()}")


def write_port_byte(runtime, port: int, value: int) -> None:
    """
    Write a byte (8 bits) to an I/O port
    
    Args:
        port: Port number (0-65535)
        value: Byte value to write (0-255)
        
    Raises:
        PortAccessError: If port access fails
        PrivilegeError: If insufficient privileges
        
    Example:
        # Write to parallel port data register
        write_port with port 0x378, value 0xFF
    """
    _require_privileges()
    
    if not (0 <= port <= 0xFFFF):
        raise PortAccessError(f"Invalid port number: {port}. Must be 0-65535.")
    
    if not (0 <= value <= 0xFF):
        raise PortAccessError(f"Invalid byte value: {value}. Must be 0-255.")
    
    if platform.system() == "Linux":
        try:
            with open("/dev/port", "r+b") as f:
                f.seek(port)
                f.write(bytes([value]))
                f.flush()
        except PermissionError:
            raise PrivilegeError("Cannot access /dev/port. Run with sudo.")
        except Exception as e:
            raise PortAccessError(f"Failed to write to port 0x{port:04X}: {e}")
    else:
        raise PortAccessError(f"Port I/O not implemented for {platform.system()}")


def write_port_word(runtime, port: int, value: int) -> None:
    """
    Write a word (16 bits) to an I/O port
    
    Args:
        port: Port number (0-65535)
        value: Word value to write (0-65535)
        
    Raises:
        PortAccessError: If port access fails
        PrivilegeError: If insufficient privileges
    """
    _require_privileges()
    
    if not (0 <= port <= 0xFFFF):
        raise PortAccessError(f"Invalid port number: {port}. Must be 0-65535.")
    
    if not (0 <= value <= 0xFFFF):
        raise PortAccessError(f"Invalid word value: {value}. Must be 0-65535.")
    
    if platform.system() == "Linux":
        try:
            with open("/dev/port", "r+b") as f:
                f.seek(port)
                f.write(value.to_bytes(2, byteorder='little'))
                f.flush()
        except PermissionError:
            raise PrivilegeError("Cannot access /dev/port. Run with sudo.")
        except Exception as e:
            raise PortAccessError(f"Failed to write to port 0x{port:04X}: {e}")
    else:
        raise PortAccessError(f"Port I/O not implemented for {platform.system()}")


def write_port_dword(runtime, port: int, value: int) -> None:
    """
    Write a double word (32 bits) to an I/O port
    
    Args:
        port: Port number (0-65535)
        value: Dword value to write (0-4294967295)
        
    Raises:
        PortAccessError: If port access fails
        PrivilegeError: If insufficient privileges
    """
    _require_privileges()
    
    if not (0 <= port <= 0xFFFF):
        raise PortAccessError(f"Invalid port number: {port}. Must be 0-65535.")
    
    if not (0 <= value <= 0xFFFFFFFF):
        raise PortAccessError(f"Invalid dword value: {value}. Must be 0-4294967295.")
    
    if platform.system() == "Linux":
        try:
            with open("/dev/port", "r+b") as f:
                f.seek(port)
                f.write(value.to_bytes(4, byteorder='little'))
                f.flush()
        except PermissionError:
            raise PrivilegeError("Cannot access /dev/port. Run with sudo.")
        except Exception as e:
            raise PortAccessError(f"Failed to write to port 0x{port:04X}: {e}")
    else:
        raise PortAccessError(f"Port I/O not implemented for {platform.system()}")


# Memory-Mapped I/O Operations
def map_memory(runtime, physical_address: int, size: int, cache_hint: str = "UC") -> int:
    """
    Map physical memory to virtual address space for MMIO
    
    Args:
        physical_address: Physical memory address to map
        size: Size in bytes to map
        cache_hint: Cache control hint - "WB" (write-back), "WT" (write-through),
                   "UC" (uncacheable), "WC" (write-combining), "WP" (write-protected)
                   Default: "UC" (uncacheable) for device memory
        
    Returns:
        Virtual address pointer to mapped memory region
        
    Raises:
        MMIOError: If memory mapping fails
        PrivilegeError: If insufficient privileges
        
    Example:
        # Map VGA framebuffer at 0xB8000 (uncacheable)
        set vga_buffer to map_memory with physical_address: 0xB8000 and size: 4000
        
        # Map write-combining framebuffer for better performance
        set fb to map_memory with physical_address: 0xE0000000 and size: 0x800000 and cache_hint: "WC"
        
    Natural language: map memory with physical address 753664 and size 4000 returns Pointer
    """
    _require_privileges()
    
    if physical_address < 0:
        raise MMIOError(f"Invalid physical address: {physical_address}")
    
    if size <= 0:
        raise MMIOError(f"Invalid size: {size}. Must be positive.")
    
    # Validate cache hint
    cache_hints = {"WB": CacheControl.WB, "WT": CacheControl.WT, "UC": CacheControl.UC, 
                   "WC": CacheControl.WC, "WP": CacheControl.WP}
    if cache_hint not in cache_hints:
        raise MMIOError(f"Invalid cache hint: {cache_hint}. Must be one of: {', '.join(cache_hints.keys())}")
    
    cache_mode = cache_hints[cache_hint]
    
    if platform.system() == "Linux":
        try:
            # Open /dev/mem for physical memory access
            fd = os.open("/dev/mem", os.O_RDWR | os.O_SYNC)
            
            # Calculate page-aligned address
            page_size = mmap.PAGESIZE
            offset = physical_address % page_size
            aligned_address = physical_address - offset
            aligned_size = ((size + offset + page_size - 1) // page_size) * page_size
            
            # Map memory with appropriate flags
            mem = mmap.mmap(
                fd,
                aligned_size,
                mmap.MAP_SHARED,
                mmap.PROT_READ | mmap.PROT_WRITE,
                offset=aligned_address
            )
            os.close(fd)
            
            # Store mapping info for later access and cleanup
            # Virtual address is the id of the memoryview + offset
            virtual_address = id(mem) + offset
            _mmio_mappings[virtual_address] = (mem, physical_address, size, cache_mode)
            
            return virtual_address
            
        except PermissionError:
            raise PrivilegeError("Cannot access /dev/mem. Run with sudo or configure permissions.")
        except FileNotFoundError:
            raise MMIOError("/dev/mem not found. Kernel may have CONFIG_DEVMEM disabled.")
        except Exception as e:
            raise MMIOError(f"Failed to map memory at 0x{physical_address:08X}: {e}")
    
    elif platform.system() == "Windows":
        # Windows requires driver for physical memory access
        raise MMIOError(
            "Physical memory mapping on Windows requires a kernel driver. "
            "Consider using libraries like WinIO or DirectIO."
        )
    else:
        raise MMIOError(f"Memory mapping not implemented for {platform.system()}")


def unmap_memory(runtime, address: int) -> None:
    """
    Unmap memory-mapped I/O region
    
    Args:
        address: Virtual address returned by map_memory
        
    Raises:
        MMIOError: If unmapping fails or address not found
        
    Example:
        unmap memory with address: vga_buffer
        
    Natural language: unmap memory with address <pointer_value>
    """
    if address not in _mmio_mappings:
        raise MMIOError(f"Address 0x{address:016X} is not a valid mapped region")
    
    try:
        mem, phys_addr, size, cache_mode = _mmio_mappings[address]
        mem.close()
        del _mmio_mappings[address]
    except Exception as e:
        raise MMIOError(f"Failed to unmap memory at 0x{address:016X}: {e}")


def read_mmio_byte(runtime, address: int, offset: int = 0) -> int:
    """
    Read a byte from memory-mapped I/O address (volatile read)
    
    Args:
        address: Virtual memory address from map_memory
        offset: Byte offset from base address (default: 0)
        
    Returns:
        Byte value (0-255)
        
    Raises:
        MMIOError: If read fails or address invalid
        
    Example:
        set value to read mmio byte with address: vga_buffer and offset: 0
        
    Natural language: read mmio byte with address <pointer> and offset 10 returns Integer
    """
    if address not in _mmio_mappings:
        raise MMIOError(f"Address 0x{address:016X} is not a valid mapped region")
    
    mem, phys_addr, size, cache_mode = _mmio_mappings[address]
    
    if offset < 0 or offset >= size:
        raise MMIOError(f"Offset {offset} out of bounds (size: {size})")
    
    try:
        # Volatile read via ctypes for proper memory barrier
        page_offset = (address - id(mem)) + offset
        return mem[page_offset]
    except Exception as e:
        raise MMIOError(f"Failed to read from MMIO address 0x{address:016X}+{offset}: {e}")


def read_mmio_word(runtime, address: int, offset: int = 0) -> int:
    """
    Read a word (16 bits) from memory-mapped I/O address (volatile read)
    
    Args:
        address: Virtual memory address from map_memory
        offset: Byte offset from base address (default: 0)
        
    Returns:
        Word value (0-65535)
    """
    if address not in _mmio_mappings:
        raise MMIOError(f"Address 0x{address:016X} is not a valid mapped region")
    
    mem, phys_addr, size, cache_mode = _mmio_mappings[address]
    
    if offset < 0 or offset + 1 >= size:
        raise MMIOError(f"Offset {offset} out of bounds for word read (size: {size})")
    
    try:
        page_offset = (address - id(mem)) + offset
        return int.from_bytes(mem[page_offset:page_offset+2], byteorder='little')
    except Exception as e:
        raise MMIOError(f"Failed to read word from MMIO address 0x{address:016X}+{offset}: {e}")


def read_mmio_dword(runtime, address: int, offset: int = 0) -> int:
    """
    Read a dword (32 bits) from memory-mapped I/O address (volatile read)
    
    Args:
        address: Virtual memory address from map_memory
        offset: Byte offset from base address (default: 0)
        
    Returns:
        Dword value (0-4294967295)
    """
    if address not in _mmio_mappings:
        raise MMIOError(f"Address 0x{address:016X} is not a valid mapped region")
    
    mem, phys_addr, size, cache_mode = _mmio_mappings[address]
    
    if offset < 0 or offset + 3 >= size:
        raise MMIOError(f"Offset {offset} out of bounds for dword read (size: {size})")
    
    try:
        page_offset = (address - id(mem)) + offset
        return int.from_bytes(mem[page_offset:page_offset+4], byteorder='little')
    except Exception as e:
        raise MMIOError(f"Failed to read dword from MMIO address 0x{address:016X}+{offset}: {e}")


def read_mmio_qword(runtime, address: int, offset: int = 0) -> int:
    """
    Read a qword (64 bits) from memory-mapped I/O address (volatile read)
    
    Args:
        address: Virtual memory address from map_memory
        offset: Byte offset from base address (default: 0)
        
    Returns:
        Qword value (0-18446744073709551615)
    """
    if address not in _mmio_mappings:
        raise MMIOError(f"Address 0x{address:016X} is not a valid mapped region")
    
    mem, phys_addr, size, cache_mode = _mmio_mappings[address]
    
    if offset < 0 or offset + 7 >= size:
        raise MMIOError(f"Offset {offset} out of bounds for qword read (size: {size})")
    
    try:
        page_offset = (address - id(mem)) + offset
        return int.from_bytes(mem[page_offset:page_offset+8], byteorder='little')
    except Exception as e:
        raise MMIOError(f"Failed to read qword from MMIO address 0x{address:016X}+{offset}: {e}")


def write_mmio_byte(runtime, address: int, value: int, offset: int = 0) -> None:
    """
    Write a byte to memory-mapped I/O address (volatile write)
    
    Args:
        address: Virtual memory address from map_memory
        value: Byte value to write (0-255)
        offset: Byte offset from base address (default: 0)
        
    Raises:
        MMIOError: If write fails or address invalid
        
    Example:
        write mmio byte with address: vga_buffer and value: 65 and offset: 0
        
    Natural language: write mmio byte with address <pointer> and value 72 and offset 10
    """
    if address not in _mmio_mappings:
        raise MMIOError(f"Address 0x{address:016X} is not a valid mapped region")
    
    if not (0 <= value <= 0xFF):
        raise MMIOError(f"Invalid byte value: {value}. Must be 0-255.")
    
    mem, phys_addr, size, cache_mode = _mmio_mappings[address]
    
    if offset < 0 or offset >= size:
        raise MMIOError(f"Offset {offset} out of bounds (size: {size})")
    
    try:
        page_offset = (address - id(mem)) + offset
        mem[page_offset] = value
        mem.flush()  # Ensure write completes
    except Exception as e:
        raise MMIOError(f"Failed to write to MMIO address 0x{address:016X}+{offset}: {e}")


def write_mmio_word(runtime, address: int, value: int, offset: int = 0) -> None:
    """
    Write a word (16 bits) to memory-mapped I/O address (volatile write)
    
    Args:
        address: Virtual memory address from map_memory
        value: Word value to write (0-65535)
        offset: Byte offset from base address (default: 0)
    """
    if address not in _mmio_mappings:
        raise MMIOError(f"Address 0x{address:016X} is not a valid mapped region")
    
    if not (0 <= value <= 0xFFFF):
        raise MMIOError(f"Invalid word value: {value}. Must be 0-65535.")
    
    mem, phys_addr, size, cache_mode = _mmio_mappings[address]
    
    if offset < 0 or offset + 1 >= size:
        raise MMIOError(f"Offset {offset} out of bounds for word write (size: {size})")
    
    try:
        page_offset = (address - id(mem)) + offset
        mem[page_offset:page_offset+2] = value.to_bytes(2, byteorder='little')
        mem.flush()
    except Exception as e:
        raise MMIOError(f"Failed to write word to MMIO address 0x{address:016X}+{offset}: {e}")


def write_mmio_dword(runtime, address: int, value: int, offset: int = 0) -> None:
    """
    Write a dword (32 bits) to memory-mapped I/O address (volatile write)
    
    Args:
        address: Virtual memory address from map_memory
        value: Dword value to write (0-4294967295)
        offset: Byte offset from base address (default: 0)
    """
    if address not in _mmio_mappings:
        raise MMIOError(f"Address 0x{address:016X} is not a valid mapped region")
    
    if not (0 <= value <= 0xFFFFFFFF):
        raise MMIOError(f"Invalid dword value: {value}. Must be 0-4294967295.")
    
    mem, phys_addr, size, cache_mode = _mmio_mappings[address]
    
    if offset < 0 or offset + 3 >= size:
        raise MMIOError(f"Offset {offset} out of bounds for dword write (size: {size})")
    
    try:
        page_offset = (address - id(mem)) + offset
        mem[page_offset:page_offset+4] = value.to_bytes(4, byteorder='little')
        mem.flush()
    except Exception as e:
        raise MMIOError(f"Failed to write dword to MMIO address 0x{address:016X}+{offset}: {e}")


def write_mmio_qword(runtime, address: int, value: int, offset: int = 0) -> None:
    """
    Write a qword (64 bits) to memory-mapped I/O address (volatile write)
    
    Args:
        address: Virtual memory address from map_memory
        value: Qword value to write (0-18446744073709551615)
        offset: Byte offset from base address (default: 0)
    """
    if address not in _mmio_mappings:
        raise MMIOError(f"Address 0x{address:016X} is not a valid mapped region")
    
    if not (0 <= value <= 0xFFFFFFFFFFFFFFFF):
        raise MMIOError(f"Invalid qword value: {value}. Must be 0-18446744073709551615.")
    
    mem, phys_addr, size, cache_mode = _mmio_mappings[address]
    
    if offset < 0 or offset + 7 >= size:
        raise MMIOError(f"Offset {offset} out of bounds for qword write (size: {size})")
    
    try:
        page_offset = (address - id(mem)) + offset
        mem[page_offset:page_offset+8] = value.to_bytes(8, byteorder='little')
        mem.flush()
    except Exception as e:
        raise MMIOError(f"Failed to write qword to MMIO address 0x{address:016X}+{offset}: {e}")


def get_mapping_info(runtime, address: int) -> dict:
    """
    Get information about a mapped memory region
    
    Args:
        address: Virtual address from map_memory
        
    Returns:
        Dictionary with keys: physical_address, size, cache_hint
        
    Raises:
        MMIOError: If address not found
    """
    if address not in _mmio_mappings:
        raise MMIOError(f"Address 0x{address:016X} is not a valid mapped region")
    
    mem, phys_addr, size, cache_mode = _mmio_mappings[address]
    cache_names = {v: k for k, v in {"WB": CacheControl.WB, "WT": CacheControl.WT, 
                                      "UC": CacheControl.UC, "WC": CacheControl.WC, 
                                      "WP": CacheControl.WP}.items()}
    
    return {
        "physical_address": phys_addr,
        "virtual_address": address,
        "size": size,
        "cache_hint": cache_names.get(cache_mode, "UC")
    }


def list_mappings(runtime) -> list:
    """
    List all active memory-mapped regions
    
    Returns:
        List of dictionaries with mapping information
    """
    result = []
    for virt_addr in _mmio_mappings:
        result.append(get_mapping_info(runtime, virt_addr))
    return result


# ============================================================================
# INTERRUPT AND EXCEPTION HANDLING
# ============================================================================

# Standard x86 Interrupt/Exception Vectors
class InterruptVector(IntEnum):
    """Standard x86 interrupt and exception vectors (0-255)"""
    # CPU Exceptions (0-31)
    DIVIDE_BY_ZERO = 0           # #DE - Division error
    DEBUG = 1                     # #DB - Debug exception
    NMI = 2                       # Non-maskable interrupt
    BREAKPOINT = 3                # #BP - Breakpoint
    OVERFLOW = 4                  # #OF - Overflow
    BOUND_RANGE_EXCEEDED = 5      # #BR - Bound range exceeded
    INVALID_OPCODE = 6            # #UD - Invalid opcode
    DEVICE_NOT_AVAILABLE = 7      # #NM - Device not available (no FPU)
    DOUBLE_FAULT = 8              # #DF - Double fault
    COPROCESSOR_SEGMENT_OVERRUN = 9  # Legacy (not used on modern CPUs)
    INVALID_TSS = 10              # #TS - Invalid task state segment
    SEGMENT_NOT_PRESENT = 11      # #NP - Segment not present
    STACK_SEGMENT_FAULT = 12      # #SS - Stack-segment fault
    GENERAL_PROTECTION = 13       # #GP - General protection fault
    PAGE_FAULT = 14               # #PF - Page fault
    # 15 is reserved
    FPU_ERROR = 16                # #MF - x87 FPU error
    ALIGNMENT_CHECK = 17          # #AC - Alignment check
    MACHINE_CHECK = 18            # #MC - Machine check
    SIMD_EXCEPTION = 19           # #XM - SIMD floating-point exception
    VIRTUALIZATION_EXCEPTION = 20  # #VE - Virtualization exception
    CONTROL_PROTECTION = 21       # #CP - Control protection exception
    # 22-27 reserved
    HYPERVISOR_INJECTION = 28     # #HV - Hypervisor injection exception
    VMM_COMMUNICATION = 29        # #VC - VMM communication exception
    SECURITY_EXCEPTION = 30       # #SX - Security exception
    # 31 reserved
    
    # Hardware Interrupts (32-255)
    # Typical mappings (can vary by system):
    TIMER = 32                    # IRQ 0 - Programmable Interval Timer
    KEYBOARD = 33                 # IRQ 1 - Keyboard
    CASCADE = 34                  # IRQ 2 - Cascade (second PIC)
    COM2 = 35                     # IRQ 3 - Serial port 2
    COM1 = 36                     # IRQ 4 - Serial port 1
    LPT2 = 37                     # IRQ 5 - Parallel port 2
    FLOPPY = 38                   # IRQ 6 - Floppy disk
    LPT1 = 39                     # IRQ 7 - Parallel port 1
    RTC = 40                      # IRQ 8 - Real-time clock
    FREE_1 = 41                   # IRQ 9 - Available
    FREE_2 = 42                   # IRQ 10 - Available
    FREE_3 = 43                   # IRQ 11 - Available
    MOUSE = 44                    # IRQ 12 - PS/2 Mouse
    FPU_IRQ = 45                  # IRQ 13 - FPU / Coprocessor
    PRIMARY_ATA = 46              # IRQ 14 - Primary ATA hard disk
    SECONDARY_ATA = 47            # IRQ 15 - Secondary ATA hard disk


# IDT Entry Structure (Simplified representation)
class IDTEntry:
    """
    Interrupt Descriptor Table Entry
    
    Represents a gate descriptor in the IDT. In real x86, this is a 16-byte
    structure. This is a simplified Python representation.
    """
    def __init__(self, offset: int = 0, segment: int = 0x08, 
                 gate_type: int = 0x8E, dpl: int = 0):
        """
        Initialize IDT entry
        
        Args:
            offset: Interrupt handler address (linear address)
            segment: Code segment selector (typically 0x08 for kernel code)
            gate_type: Gate type and attributes:
                - 0x8E: 32-bit interrupt gate (IF=0)
                - 0x8F: 32-bit trap gate (IF=1)
                - 0xEE: 32-bit interrupt gate, DPL=3 (user callable)
            dpl: Descriptor Privilege Level (0-3, 0=kernel, 3=user)
        """
        self.offset = offset          # Handler address
        self.segment = segment        # Code segment
        self.gate_type = gate_type    # Gate type byte
        self.dpl = dpl                # Privilege level
        self.present = True           # Present bit
    
    def to_dict(self) -> dict:
        """Convert to dictionary for NexusLang access"""
        return {
            "offset": self.offset,
            "segment": self.segment,
            "gate_type": self.gate_type,
            "dpl": self.dpl,
            "present": self.present
        }


# Exception Frame (CPU state at interrupt time)
class ExceptionFrame:
    """
    Exception frame - saved CPU state when interrupt occurs
    
    This represents the stack frame pushed by CPU and interrupt handler.
    Contains register values and error codes for exception handling.
    """
    def __init__(self):
        # General purpose registers
        self.rax = 0
        self.rbx = 0
        self.rcx = 0
        self.rdx = 0
        self.rsi = 0
        self.rdi = 0
        self.rbp = 0
        self.rsp = 0
        self.r8 = 0
        self.r9 = 0
        self.r10 = 0
        self.r11 = 0
        self.r12 = 0
        self.r13 = 0
        self.r14 = 0
        self.r15 = 0
        
        # Instruction pointer and flags
        self.rip = 0           # Instruction pointer
        self.rflags = 0        # CPU flags
        
        # Segment registers
        self.cs = 0            # Code segment
        self.ss = 0            # Stack segment
        
        # Exception-specific
        self.error_code = 0    # Error code (if applicable)
        self.vector = 0        # Interrupt vector number
    
    def to_dict(self) -> dict:
        """Convert to dictionary for NexusLang access"""
        return {
            "rax": self.rax, "rbx": self.rbx, "rcx": self.rcx, "rdx": self.rdx,
            "rsi": self.rsi, "rdi": self.rdi, "rbp": self.rbp, "rsp": self.rsp,
            "r8": self.r8, "r9": self.r9, "r10": self.r10, "r11": self.r11,
            "r12": self.r12, "r13": self.r13, "r14": self.r14, "r15": self.r15,
            "rip": self.rip, "rflags": self.rflags,
            "cs": self.cs, "ss": self.ss,
            "error_code": self.error_code, "vector": self.vector
        }


# Current exception frame (set when handler is invoked)
_current_exception_frame: Optional[ExceptionFrame] = None

# IDT structure (simplified - 256 entries)
_idt: Dict[int, IDTEntry] = {}


def setup_idt(runtime) -> bool:
    """
    Initialize the Interrupt Descriptor Table
    
    Sets up a basic IDT structure with default handlers.
    On real hardware, this would configure the CPU's IDT register.
    
    Returns:
        True if successful
        
    Raises:
        PrivilegeError: If insufficient privileges
        InterruptError: If IDT setup fails
        
    Example:
        setup_idt
        print text "IDT initialized"
    """
    _require_privileges()
    
    global _idt
    _idt = {}
    
    # Initialize all 256 IDT entries with null handlers
    for vector in range(256):
        _idt[vector] = IDTEntry(offset=0, segment=0x08, gate_type=0x8E, dpl=0)
    
    # Set present bit for exception vectors (0-31)
    for vector in range(32):
        _idt[vector].present = True
    
    return True


def get_idt_entry(runtime, vector: int) -> dict:
    """
    Get an IDT entry
    
    Args:
        vector: Interrupt vector (0-255)
        
    Returns:
        Dictionary with IDT entry information:
        - offset: Handler address
        - segment: Code segment selector
        - gate_type: Gate type byte
        - dpl: Descriptor privilege level
        - present: Present bit
        
    Raises:
        InterruptError: If vector is invalid
        
    Example:
        set entry to get_idt_entry with vector: 14
        print text entry
    """
    if not (0 <= vector <= 255):
        raise InterruptError(f"Invalid interrupt vector: {vector}. Must be 0-255.")
    
    if vector not in _idt:
        # Return default entry
        return IDTEntry().to_dict()
    
    return _idt[vector].to_dict()


def set_idt_entry(runtime, vector: int, offset: int, segment: int = 0x08,
                  gate_type: int = 0x8E, dpl: int = 0) -> bool:
    """
    Set an IDT entry
    
    Configures an interrupt gate descriptor in the IDT.
    
    Args:
        vector: Interrupt vector (0-255)
        offset: Handler address
        segment: Code segment selector (default: 0x08 kernel code)
        gate_type: Gate type (default: 0x8E interrupt gate)
        dpl: Descriptor privilege level (default: 0 kernel)
        
    Returns:
        True if successful
        
    Raises:
        PrivilegeError: If insufficient privileges
        InterruptError: If parameters are invalid
        
    Example:
        set_idt_entry with vector: 32 and offset: handler_address
    """
    _require_privileges()
    
    if not (0 <= vector <= 255):
        raise InterruptError(f"Invalid interrupt vector: {vector}. Must be 0-255.")
    
    if not (0 <= dpl <= 3):
        raise InterruptError(f"Invalid DPL: {dpl}. Must be 0-3.")
    
    _idt[vector] = IDTEntry(offset=offset, segment=segment, 
                            gate_type=gate_type, dpl=dpl)
    return True


def get_idt_base(runtime) -> int:
    """
    Get IDT base address
    
    Returns the base address of the IDT. On real hardware, this would
    read the IDTR register.
    
    Returns:
        IDT base address (linear address)
        
    Example:
        set idt_base to get_idt_base
        print text "IDT at address:"
        print text idt_base
    """
    _require_privileges()
    
    # In this implementation, return the id of the IDT dict
    # On real hardware, would use SIDT instruction
    return id(_idt)


def get_idt_limit(runtime) -> int:
    """
    Get IDT limit (size - 1)
    
    Returns the IDT limit. The limit is the size of the IDT in bytes minus 1.
    For a standard IDT with 256 entries of 16 bytes each, this is 4095.
    
    Returns:
        IDT limit (size - 1)
        
    Example:
        set idt_limit to get_idt_limit
        print text "IDT size:"
        print text idt_limit plus 1
    """
    _require_privileges()
    
    # Standard x86-64 IDT: 256 entries * 16 bytes each = 4096 bytes
    # Limit = size - 1 = 4095
    return 4095


def register_interrupt_handler(runtime, vector: int, handler: callable) -> bool:
    """
    Register an interrupt handler
    
    Associates a handler function with an interrupt vector. When the
    interrupt fires, the handler will be invoked with the exception frame.
    
    Args:
        vector: Interrupt vector (0-255)
        handler: Callable handler function
                 Signature: handler(exception_frame: dict) -> None
        
    Returns:
        True if successful
        
    Raises:
        PrivilegeError: If insufficient privileges
        InterruptError: If vector is invalid or handler is not callable
        
    Example:
        function my_keyboard_handler with frame as Dict
            print text "Keyboard interrupt fired"
            print text "RIP:"
            print text frame["rip"]
        end
        
        register_interrupt_handler with vector: 33 and handler: my_keyboard_handler
    """
    _require_privileges()
    
    if not (0 <= vector <= 255):
        raise InterruptError(f"Invalid interrupt vector: {vector}. Must be 0-255.")
    
    if not callable(handler):
        raise InterruptError(f"Handler must be callable, got {type(handler).__name__}")
    
    _interrupt_handlers[vector] = handler
    
    # Update IDT entry to point to our dispatch mechanism
    # On real hardware, would set actual handler address
    _idt[vector] = IDTEntry(offset=id(handler), segment=0x08, 
                            gate_type=0x8E, dpl=0)
    _idt[vector].present = True
    
    return True


def unregister_interrupt_handler(runtime, vector: int) -> bool:
    """
    Unregister an interrupt handler
    
    Removes the handler for the specified interrupt vector.
    
    Args:
        vector: Interrupt vector (0-255)
        
    Returns:
        True if handler was registered and removed, False if no handler
        
    Raises:
        PrivilegeError: If insufficient privileges
        InterruptError: If vector is invalid
        
    Example:
        unregister_interrupt_handler with vector: 33
    """
    _require_privileges()
    
    if not (0 <= vector <= 255):
        raise InterruptError(f"Invalid interrupt vector: {vector}. Must be 0-255.")
    
    if vector in _interrupt_handlers:
        del _interrupt_handlers[vector]
        # Clear IDT entry
        _idt[vector] = IDTEntry(offset=0, segment=0x08, gate_type=0x8E, dpl=0)
        return True
    
    return False


def list_interrupt_handlers(runtime) -> list:
    """
    List all registered interrupt handlers
    
    Returns:
        List of dictionaries with handler information:
        - vector: Interrupt vector number
        - handler: Handler function reference
        - vector_name: Name of vector (if known)
        
    Example:
        set handlers to list_interrupt_handlers
        for each handler in handlers
            print text "Vector:"
            print text handler["vector"]
            print text handler["vector_name"]
        end
    """
    result = []
    
    # Map vector numbers to names
    vector_names = {
        0: "DIVIDE_BY_ZERO", 1: "DEBUG", 2: "NMI", 3: "BREAKPOINT",
        4: "OVERFLOW", 5: "BOUND_RANGE_EXCEEDED", 6: "INVALID_OPCODE",
        7: "DEVICE_NOT_AVAILABLE", 8: "DOUBLE_FAULT", 10: "INVALID_TSS",
        11: "SEGMENT_NOT_PRESENT", 12: "STACK_SEGMENT_FAULT",
        13: "GENERAL_PROTECTION", 14: "PAGE_FAULT", 16: "FPU_ERROR",
        17: "ALIGNMENT_CHECK", 18: "MACHINE_CHECK", 19: "SIMD_EXCEPTION",
        32: "TIMER", 33: "KEYBOARD", 34: "CASCADE", 35: "COM2",
        36: "COM1", 37: "LPT2", 38: "FLOPPY", 39: "LPT1",
        40: "RTC", 44: "MOUSE", 45: "FPU_IRQ",
        46: "PRIMARY_ATA", 47: "SECONDARY_ATA"
    }
    
    for vector, handler in _interrupt_handlers.items():
        result.append({
            "vector": vector,
            "handler": handler,
            "vector_name": vector_names.get(vector, f"IRQ_{vector}")
        })
    
    return result


def enable_interrupts(runtime) -> bool:
    """
    Enable hardware interrupts (STI instruction)
    
    Sets the interrupt flag (IF) in RFLAGS, allowing hardware interrupts
    to be serviced by the CPU.
    
    Returns:
        True if successful
        
    Raises:
        PrivilegeError: If insufficient privileges
        
    Example:
        enable_interrupts
        print text "Interrupts enabled"
    """
    _require_privileges()
    
    global _interrupts_enabled
    _interrupts_enabled = True
    
    # On real hardware, would execute STI instruction
    # x86: asm("sti")
    
    return True


def disable_interrupts(runtime) -> bool:
    """
    Disable hardware interrupts (CLI instruction)
    
    Clears the interrupt flag (IF) in RFLAGS, preventing hardware interrupts
    from being serviced. Non-maskable interrupts (NMI) still fire.
    
    Returns:
        True if successful
        
    Raises:
        PrivilegeError: If insufficient privileges
        
    Example:
        disable_interrupts
        # Critical section - no interrupts
        write_port with port: port_addr and value: critical_value
        enable_interrupts
    """
    _require_privileges()
    
    global _interrupts_enabled
    _interrupts_enabled = False
    
    # On real hardware, would execute CLI instruction
    # x86: asm("cli")
    
    return True


def get_interrupt_flag(runtime) -> bool:
    """
    Get current interrupt flag state
    
    Returns:
        True if interrupts are enabled, False if disabled
        
    Example:
        set if_state to get_interrupt_flag
        if if_state
            print text "Interrupts are enabled"
        else
            print text "Interrupts are disabled"
        end
    """
    return _interrupts_enabled


def set_interrupt_flag(runtime, enabled: bool) -> bool:
    """
    Set interrupt flag state
    
    Args:
        enabled: True to enable interrupts, False to disable
        
    Returns:
        True if successful
        
    Raises:
        PrivilegeError: If insufficient privileges
        
    Example:
        set_interrupt_flag with enabled: false
        # Critical section
        set_interrupt_flag with enabled: true
    """
    _require_privileges()
    
    global _interrupts_enabled
    _interrupts_enabled = enabled
    
    return True


def get_exception_frame(runtime) -> Optional[dict]:
    """
    Get current exception frame
    
    Returns the saved CPU state from the current interrupt context.
    Only valid when called from within an interrupt handler.
    
    Returns:
        Dictionary with CPU state, or None if not in interrupt context
        
    Example:
        function page_fault_handler with frame as Dict
            set saved_frame to get_exception_frame
            if saved_frame is not null
                print text "Page fault at address:"
                print text saved_frame["rip"]
                print text "Error code:"
                print text saved_frame["error_code"]
            end
        end
    """
    global _current_exception_frame
    
    if _current_exception_frame is None:
        return None
    
    return _current_exception_frame.to_dict()


def get_error_code(runtime) -> int:
    """
    Get exception error code
    
    Returns the error code pushed by the CPU for certain exceptions
    (e.g., page fault, general protection fault).
    
    Returns:
        Error code, or 0 if not in exception context or no error code
        
    Example:
        function gp_handler with frame as Dict
            set error_code to get_error_code
            print text "General Protection Fault, error code:"
            print text error_code
        end
    """
    global _current_exception_frame
    
    if _current_exception_frame is None:
        return 0
    
    return _current_exception_frame.error_code


def get_instruction_pointer(runtime) -> int:
    """
    Get saved instruction pointer (RIP)
    
    Returns the instruction pointer at the time of the interrupt.
    
    Returns:
        Instruction pointer value, or 0 if not in interrupt context
        
    Example:
        set rip to get_instruction_pointer
        print text "Interrupt occurred at:"
        print text rip
    """
    global _current_exception_frame
    
    if _current_exception_frame is None:
        return 0
    
    return _current_exception_frame.rip


def get_stack_pointer(runtime) -> int:
    """
    Get saved stack pointer (RSP)
    
    Returns the stack pointer at the time of the interrupt.
    
    Returns:
        Stack pointer value, or 0 if not in interrupt context
    """
    global _current_exception_frame
    
    if _current_exception_frame is None:
        return 0
    
    return _current_exception_frame.rsp


def get_cpu_flags(runtime) -> int:
    """
    Get saved CPU flags (RFLAGS)
    
    Returns the RFLAGS register at the time of the interrupt.
    Useful for checking flag states like IF, CF, ZF, etc.
    
    Returns:
        RFLAGS value, or 0 if not in interrupt context
        
    Example:
        set flags to get_cpu_flags
        set if_was_set to flags bitwise_and 512  # IF = bit 9
        if if_was_set is not 0
            print text "Interrupts were enabled"
        end
    """
    global _current_exception_frame
    
    if _current_exception_frame is None:
        return 0
    
    return _current_exception_frame.rflags


def invoke_interrupt_handler(runtime, vector: int, frame: Optional[ExceptionFrame] = None) -> bool:
    """
    Internal function to invoke a registered interrupt handler
    
    This simulates interrupt delivery. On real hardware, the CPU would
    handle this automatically.
    
    Args:
        vector: Interrupt vector number
        frame: Exception frame with CPU state (optional)
        
    Returns:
        True if handler was invoked, False if no handler registered
    """
    global _current_exception_frame
    
    if vector not in _interrupt_handlers:
        return False
    
    # Set up exception frame context
    if frame is None:
        frame = ExceptionFrame()
        frame.vector = vector
    
    _current_exception_frame = frame
    
    try:
        # Invoke the handler with the frame
        handler = _interrupt_handlers[vector]
        handler(frame.to_dict())
    finally:
        # Clear exception frame context
        _current_exception_frame = None
    
    return True


# ============================================================================
# DMA (DIRECT MEMORY ACCESS) CONTROL
# ============================================================================

# DMA Channel Numbers (x86 PC Architecture)
class DMAChannel(IntEnum):
    """DMA channel numbers (0-7)"""
    CHANNEL_0 = 0  # Available
    CHANNEL_1 = 1  # Available
    CHANNEL_2 = 2  # Floppy disk controller
    CHANNEL_3 = 3  # Available
    CHANNEL_4 = 4  # CASCADE (DMA controller 1 to controller 2)
    CHANNEL_5 = 5  # Available
    CHANNEL_6 = 6  # Available
    CHANNEL_7 = 7  # Available


# DMA Transfer Modes
class DMAMode(IntEnum):
    """DMA transfer modes"""
    DEMAND = 0    # Demand transfer mode
    SINGLE = 1    # Single transfer mode (one byte/word per request)
    BLOCK = 2     # Block transfer mode (entire block in one burst)
    CASCADE = 3   # Cascade mode (for channel 4 only)


# DMA Transfer Direction
class DMADirection(IntEnum):
    """DMA transfer direction"""
    VERIFY = 0     # Verify transfer (no actual transfer)
    WRITE = 1      # Write to memory (read from device)
    READ = 2       # Read from memory (write to device)
    INVALID = 3    # Invalid


# DMA Port Addresses (x86 PC)
# Controller 1 (channels 0-3)
DMA1_STATUS_REG = 0x08
DMA1_COMMAND_REG = 0x08
DMA1_REQUEST_REG = 0x09
DMA1_MASK_REG = 0x0A
DMA1_MODE_REG = 0x0B
DMA1_CLEAR_FF_REG = 0x0C
DMA1_TEMP_REG = 0x0D
DMA1_MASTER_CLEAR_REG = 0x0D
DMA1_CLEAR_MASK_REG = 0x0E
DMA1_WRITE_MASK_REG = 0x0F

# Controller 2 (channels 4-7)
DMA2_STATUS_REG = 0xD0
DMA2_COMMAND_REG = 0xD0
DMA2_REQUEST_REG = 0xD2
DMA2_MASK_REG = 0xD4
DMA2_MODE_REG = 0xD6
DMA2_CLEAR_FF_REG = 0xD8
DMA2_TEMP_REG = 0xDA
DMA2_MASTER_CLEAR_REG = 0xDA
DMA2_CLEAR_MASK_REG = 0xDC
DMA2_WRITE_MASK_REG = 0xDE

# Channel address and count registers
DMA_CHANNEL_ADDR = [0x00, 0x02, 0x04, 0x06, 0xC0, 0xC4, 0xC8, 0xCC]
DMA_CHANNEL_COUNT = [0x01, 0x03, 0x05, 0x07, 0xC2, 0xC6, 0xCA, 0xCE]
DMA_CHANNEL_PAGE = [0x87, 0x83, 0x81, 0x82, 0x8F, 0x8B, 0x89, 0x8A]


# DMA Channel State
class DMAChannelState:
    """State information for a DMA channel"""
    def __init__(self, channel: int):
        self.channel = channel
        self.allocated = False
        self.source_address = 0
        self.destination_address = 0
        self.count = 0
        self.mode = DMAMode.SINGLE
        self.direction = DMADirection.READ
        self.active = False
        self.page = 0
    
    def to_dict(self) -> dict:
        """Convert to dictionary for NexusLang access"""
        return {
            "channel": self.channel,
            "allocated": self.allocated,
            "source_address": self.source_address,
            "destination_address": self.destination_address,
            "count": self.count,
            "mode": self.mode,
            "direction": self.direction,
            "active": self.active,
            "page": self.page
        }


def _init_dma_controller():
    """Initialize DMA controller state"""
    global _dma_initialized, _dma_channels
    
    if _dma_initialized:
        return
    
    # Initialize all 8 channels
    for channel in range(8):
        _dma_channels[channel] = DMAChannelState(channel)
    
    _dma_initialized = True


def allocate_dma_channel(runtime, channel: int) -> bool:
    """
    Allocate a DMA channel for use
    
    Args:
        channel: DMA channel number (0-7)
        
    Returns:
        True if allocated successfully, False if already allocated
        
    Raises:
        PrivilegeError: If insufficient privileges
        DMAError: If channel number is invalid or channel 4 (cascade)
        
    Example:
        set success to allocate_dma_channel with channel: 2
        if success
            print text "Floppy DMA channel allocated"
        end
    """
    _require_privileges()
    _init_dma_controller()
    
    if not (0 <= channel <= 7):
        raise DMAError(f"Invalid DMA channel: {channel}. Must be 0-7.")
    
    if channel == 4:
        raise DMAError("Channel 4 is reserved for cascade mode (controller linking).")
    
    if _dma_channels[channel].allocated:
        return False
    
    _dma_channels[channel].allocated = True
    return True


def release_dma_channel(runtime, channel: int) -> bool:
    """
    Release a previously allocated DMA channel
    
    Args:
        channel: DMA channel number (0-7)
        
    Returns:
        True if released successfully, False if not allocated
        
    Raises:
        PrivilegeError: If insufficient privileges
        DMAError: If channel number is invalid
        
    Example:
        release_dma_channel with channel: 2
        print text "Floppy DMA channel released"
    """
    _require_privileges()
    _init_dma_controller()
    
    if not (0 <= channel <= 7):
        raise DMAError(f"Invalid DMA channel: {channel}. Must be 0-7.")
    
    if channel == 4:
        raise DMAError("Channel 4 (cascade) cannot be released.")
    
    if not _dma_channels[channel].allocated:
        return False
    
    # Stop transfer if active
    if _dma_channels[channel].active:
        stop_dma_transfer(runtime, channel)
    
    _dma_channels[channel].allocated = False
    _dma_channels[channel].source_address = 0
    _dma_channels[channel].destination_address = 0
    _dma_channels[channel].count = 0
    return True


def get_channel_status(runtime, channel: int) -> dict:
    """
    Get status of a DMA channel
    
    Args:
        channel: DMA channel number (0-7)
        
    Returns:
        Dictionary with channel status:
        - channel: Channel number
        - allocated: Whether channel is allocated
        - source_address: Source address
        - destination_address: Destination address
        - count: Transfer count
        - mode: Transfer mode
        - direction: Transfer direction
        - active: Whether transfer is active
        - page: Page register value
        
    Raises:
        DMAError: If channel number is invalid
        
    Example:
        set status to get_channel_status with channel: 2
        print text "Channel allocated:"
        print text status["allocated"]
        print text "Transfer count:"
        print text status["count"]
    """
    _init_dma_controller()
    
    if not (0 <= channel <= 7):
        raise DMAError(f"Invalid DMA channel: {channel}. Must be 0-7.")
    
    return _dma_channels[channel].to_dict()


def list_allocated_channels(runtime) -> list:
    """
    List all allocated DMA channels
    
    Returns:
        List of channel numbers that are currently allocated
        
    Example:
        set channels to list_allocated_channels
        print text "Allocated DMA channels:"
        for each ch in channels
            print text ch
        end
    """
    _init_dma_controller()
    
    result = []
    for channel in range(8):
        if _dma_channels[channel].allocated:
            result.append(channel)
    return result


def configure_dma_transfer(runtime, channel: int, source: int, destination: int,
                           count: int, mode: int = DMAMode.SINGLE,
                           direction: int = DMADirection.READ) -> bool:
    """
    Configure a DMA transfer
    
    Args:
        channel: DMA channel number (0-7)
        source: Source address (24-bit physical address)
        destination: Destination address (or device port for I/O)
        count: Transfer count in bytes (max 65535 for 8-bit, 131072 for 16-bit)
        mode: Transfer mode (DEMAND=0, SINGLE=1, BLOCK=2, CASCADE=3)
        direction: Transfer direction (VERIFY=0, WRITE=1, READ=2)
        
    Returns:
        True if configured successfully
        
    Raises:
        PrivilegeError: If insufficient privileges
        DMAError: If channel not allocated or parameters invalid
        
    Example:
        # Configure floppy disk read (1KB)
        configure_dma_transfer with channel: 2 and source: 0 and destination: 524288 
                                and count: 1024 and mode: 1 and direction: 1
    """
    _require_privileges()
    _init_dma_controller()
    
    if not (0 <= channel <= 7):
        raise DMAError(f"Invalid DMA channel: {channel}. Must be 0-7.")
    
    if channel == 4:
        raise DMAError("Channel 4 is cascade mode only.")
    
    if not _dma_channels[channel].allocated:
        raise DMAError(f"Channel {channel} is not allocated. Call allocate_dma_channel first.")
    
    if not (0 <= mode <= 3):
        raise DMAError(f"Invalid DMA mode: {mode}. Must be 0-3.")
    
    if not (0 <= direction <= 2):
        raise DMAError(f"Invalid DMA direction: {direction}. Must be 0-2.")
    
    # Validate count (max 64KB for 8-bit channels 0-3, 128KB for 16-bit channels 5-7)
    max_count = 65536 if channel < 4 else 131072
    if not (1 <= count <= max_count):
        raise DMAError(f"Invalid count: {count}. Must be 1-{max_count} for channel {channel}.")
    
    # Validate 24-bit addresses
    if not (0 <= source <= 0xFFFFFF):
        raise DMAError(f"Invalid source address: {source}. Must be 0-16777215 (24-bit).")
    
    # Store configuration
    state = _dma_channels[channel]
    state.source_address = source
    state.destination_address = destination
    state.count = count
    state.mode = mode
    state.direction = direction
    state.page = (source >> 16) & 0xFF  # Extract page from address
    
    return True


def set_dma_address(runtime, channel: int, address: int) -> bool:
    """
    Set DMA transfer address
    
    Args:
        channel: DMA channel number (0-7)
        address: 24-bit physical address
        
    Returns:
        True if set successfully
        
    Raises:
        PrivilegeError: If insufficient privileges
        DMAError: If channel not allocated or address invalid
    """
    _require_privileges()
    _init_dma_controller()
    
    if not (0 <= channel <= 7):
        raise DMAError(f"Invalid DMA channel: {channel}. Must be 0-7.")
    
    if channel == 4:
        raise DMAError("Channel 4 is cascade mode only.")
    
    if not _dma_channels[channel].allocated:
        raise DMAError(f"Channel {channel} is not allocated.")
    
    if not (0 <= address <= 0xFFFFFF):
        raise DMAError(f"Invalid address: {address}. Must be 0-16777215 (24-bit).")
    
    _dma_channels[channel].source_address = address
    _dma_channels[channel].page = (address >> 16) & 0xFF
    return True


def set_dma_count(runtime, channel: int, count: int) -> bool:
    """
    Set DMA transfer count
    
    Args:
        channel: DMA channel number (0-7)
        count: Transfer count in bytes
        
    Returns:
        True if set successfully
        
    Raises:
        PrivilegeError: If insufficient privileges
        DMAError: If channel not allocated or count invalid
    """
    _require_privileges()
    _init_dma_controller()
    
    if not (0 <= channel <= 7):
        raise DMAError(f"Invalid DMA channel: {channel}. Must be 0-7.")
    
    if channel == 4:
        raise DMAError("Channel 4 is cascade mode only.")
    
    if not _dma_channels[channel].allocated:
        raise DMAError(f"Channel {channel} is not allocated.")
    
    max_count = 65536 if channel < 4 else 131072
    if not (1 <= count <= max_count):
        raise DMAError(f"Invalid count: {count}. Must be 1-{max_count}.")
    
    _dma_channels[channel].count = count
    return True


def set_dma_mode(runtime, channel: int, mode: int, direction: int) -> bool:
    """
    Set DMA transfer mode and direction
    
    Args:
        channel: DMA channel number (0-7)
        mode: Transfer mode (DEMAND=0, SINGLE=1, BLOCK=2)
        direction: Transfer direction (VERIFY=0, WRITE=1, READ=2)
        
    Returns:
        True if set successfully
        
    Raises:
        PrivilegeError: If insufficient privileges
        DMAError: If channel not allocated or parameters invalid
    """
    _require_privileges()
    _init_dma_controller()
    
    if not (0 <= channel <= 7):
        raise DMAError(f"Invalid DMA channel: {channel}. Must be 0-7.")
    
    if channel == 4:
        raise DMAError("Channel 4 is cascade mode only.")
    
    if not _dma_channels[channel].allocated:
        raise DMAError(f"Channel {channel} is not allocated.")
    
    if not (0 <= mode <= 2):  # CASCADE mode not allowed via set_dma_mode
        raise DMAError(f"Invalid mode: {mode}. Must be 0-2.")
    
    if not (0 <= direction <= 2):
        raise DMAError(f"Invalid direction: {direction}. Must be 0-2.")
    
    _dma_channels[channel].mode = mode
    _dma_channels[channel].direction = direction
    return True


def start_dma_transfer(runtime, channel: int) -> bool:
    """
    Start a DMA transfer
    
    Unmasks the DMA channel and begins the transfer. The transfer will
    proceed asynchronously with hardware requests.
    
    Args:
        channel: DMA channel number (0-7)
        
    Returns:
        True if started successfully
        
    Raises:
        PrivilegeError: If insufficient privileges
        DMAError: If channel not configured or already active
        
    Example:
        # Start floppy disk DMA transfer
        start_dma_transfer with channel: 2
        print text "DMA transfer started"
        
        # Wait for completion (would use interrupt handler in real code)
        # ...
    """
    _require_privileges()
    _init_dma_controller()
    
    if not (0 <= channel <= 7):
        raise DMAError(f"Invalid DMA channel: {channel}. Must be 0-7.")
    
    if channel == 4:
        raise DMAError("Channel 4 is cascade mode only.")
    
    state = _dma_channels[channel]
    
    if not state.allocated:
        raise DMAError(f"Channel {channel} is not allocated.")
    
    if state.count == 0:
        raise DMAError(f"Channel {channel} not configured. Set transfer parameters first.")
    
    if state.active:
        raise DMAError(f"Channel {channel} transfer already active.")
    
    # In real implementation, would program DMA controller registers:
    # 1. Disable channel (mask)
    # 2. Clear flip-flop
    # 3. Write mode register
    # 4. Write address register
    # 5. Write count register
    # 6. Write page register
    # 7. Enable channel (unmask)
    
    state.active = True
    return True


def stop_dma_transfer(runtime, channel: int) -> bool:
    """
    Stop an active DMA transfer
    
    Masks the DMA channel and halts the transfer.
    
    Args:
        channel: DMA channel number (0-7)
        
    Returns:
        True if stopped successfully
        
    Raises:
        PrivilegeError: If insufficient privileges
        DMAError: If channel invalid
        
    Example:
        stop_dma_transfer with channel: 2
        print text "DMA transfer stopped"
    """
    _require_privileges()
    _init_dma_controller()
    
    if not (0 <= channel <= 7):
        raise DMAError(f"Invalid DMA channel: {channel}. Must be 0-7.")
    
    if channel == 4:
        raise DMAError("Channel 4 is cascade mode only.")
    
    if not _dma_channels[channel].allocated:
        return False
    
    _dma_channels[channel].active = False
    return True


def reset_dma_controller(runtime) -> bool:
    """
    Reset DMA controller to initial state
    
    Resets both DMA controllers (1 and 2) and clears all channel state.
    All transfers are stopped and all channels are released.
    
    Returns:
        True if reset successfully
        
    Raises:
        PrivilegeError: If insufficient privileges
        
    Example:
        reset_dma_controller
        print text "DMA controller reset"
    """
    _require_privileges()
    _init_dma_controller()
    
    # Stop all active transfers
    for channel in range(8):
        if channel == 4:
            continue
        if _dma_channels[channel].active:
            _dma_channels[channel].active = False
        _dma_channels[channel].allocated = False
        _dma_channels[channel].source_address = 0
        _dma_channels[channel].destination_address = 0
        _dma_channels[channel].count = 0
    
    # In real implementation, would write to master clear registers:
    # - Write to DMA1_MASTER_CLEAR_REG (0x0D)
    # - Write to DMA2_MASTER_CLEAR_REG (0xDA)
    
    return True


def mask_dma_channel(runtime, channel: int) -> bool:
    """
    Mask (disable) a DMA channel
    
    Prevents the channel from servicing DMA requests without stopping
    the transfer configuration.
    
    Args:
        channel: DMA channel number (0-7)
        
    Returns:
        True if masked successfully
        
    Raises:
        PrivilegeError: If insufficient privileges
        DMAError: If channel invalid
    """
    _require_privileges()
    _init_dma_controller()
    
    if not (0 <= channel <= 7):
        raise DMAError(f"Invalid DMA channel: {channel}. Must be 0-7.")
    
    if channel == 4:
        raise DMAError("Channel 4 is cascade mode only.")
    
    # In real implementation, would write to mask register
    # Mask bit = 1 (disable), bit 2 = channel select
    
    _dma_channels[channel].active = False
    return True


def unmask_dma_channel(runtime, channel: int) -> bool:
    """
    Unmask (enable) a DMA channel
    
    Allows the channel to service DMA requests.
    
    Args:
        channel: DMA channel number (0-7)
        
    Returns:
        True if unmasked successfully
        
    Raises:
        PrivilegeError: If insufficient privileges
        DMAError: If channel invalid or not configured
    """
    _require_privileges()
    _init_dma_controller()
    
    if not (0 <= channel <= 7):
        raise DMAError(f"Invalid DMA channel: {channel}. Must be 0-7.")
    
    if channel == 4:
        raise DMAError("Channel 4 is cascade mode only.")
    
    if not _dma_channels[channel].allocated:
        raise DMAError(f"Channel {channel} is not allocated.")
    
    if _dma_channels[channel].count == 0:
        raise DMAError(f"Channel {channel} not configured.")
    
    # In real implementation, would write to mask register
    # Mask bit = 0 (enable), bit 2 = channel select
    
    _dma_channels[channel].active = True
    return True


def get_dma_status(runtime, channel: int) -> dict:
    """
    Get detailed DMA channel status
    
    Reads status from the DMA controller and returns current state.
    
    Args:
        channel: DMA channel number (0-7)
        
    Returns:
        Dictionary with status:
        - channel: Channel number
        - allocated: Whether allocated
        - active: Whether transfer active
        - terminal_count: Whether terminal count reached
        - request_pending: Whether device has pending request
        
    Raises:
        DMAError: If channel invalid
        
    Example:
        set status to get_dma_status with channel: 2
        if status["terminal_count"]
            print text "Transfer complete"
        end
    """
    _init_dma_controller()
    
    if not (0 <= channel <= 7):
        raise DMAError(f"Invalid DMA channel: {channel}. Must be 0-7.")
    
    state = _dma_channels[channel]
    
    # In real implementation, would read from status register
    # Bit 0-3: Terminal count flags for channels 0-3
    # Bit 4-7: Request flags for channels 0-3
    
    return {
        "channel": channel,
        "allocated": state.allocated,
        "active": state.active,
        "terminal_count": False,  # Would read from hardware
        "request_pending": False   # Would read from hardware
    }


def get_transfer_count(runtime, channel: int) -> int:
    """
    Get current transfer count (remaining bytes)
    
    Reads the current count register to determine how many bytes
    remain in the transfer.
    
    Args:
        channel: DMA channel number (0-7)
        
    Returns:
        Remaining transfer count in bytes
        
    Raises:
        DMAError: If channel invalid
        
    Example:
        set remaining to get_transfer_count with channel: 2
        print text "Bytes remaining:"
        print text remaining
    """
    _init_dma_controller()
    
    if not (0 <= channel <= 7):
        raise DMAError(f"Invalid DMA channel: {channel}. Must be 0-7.")
    
    if channel == 4:
        raise DMAError("Channel 4 is cascade mode only.")
    
    # In real implementation, would read from count register
    # Must read twice (low byte, high byte) due to 8-bit bus
    
    return _dma_channels[channel].count


def is_transfer_complete(runtime, channel: int) -> bool:
    """
    Check if DMA transfer is complete
    
    Checks the terminal count (TC) flag to determine if the transfer
    has completed.
    
    Args:
        channel: DMA channel number (0-7)
        
    Returns:
        True if transfer is complete, False if still in progress
        
    Raises:
        DMAError: If channel invalid
        
    Example:
        set complete to is_transfer_complete with channel: 2
        if complete
            print text "Floppy read complete"
        end
    """
    _init_dma_controller()
    
    if not (0 <= channel <= 7):
        raise DMAError(f"Invalid DMA channel: {channel}. Must be 0-7.")
    
    if channel == 4:
        raise DMAError("Channel 4 is cascade mode only.")
    
    # In real implementation, would check terminal count bit in status register
    
    return not _dma_channels[channel].active


def get_dma_registers(runtime, channel: int) -> dict:
    """
    Get DMA channel register values
    
    Reads all registers associated with the specified channel.
    
    Args:
        channel: DMA channel number (0-7)
        
    Returns:
        Dictionary with register values:
        - address: Current address register
        - count: Current count register
        - page: Page register
        - mode: Mode register
        
    Raises:
        DMAError: If channel invalid
        
    Example:
        set regs to get_dma_registers with channel: 2
        print text "Current address:"
        print text regs["address"]
        print text "Current count:"
        print text regs["count"]
    """
    _init_dma_controller()
    
    if not (0 <= channel <= 7):
        raise DMAError(f"Invalid DMA channel: {channel}. Must be 0-7.")
    
    state = _dma_channels[channel]
    
    # In real implementation, would read from hardware registers
    
    return {
        "address": state.source_address & 0xFFFF,
        "count": state.count,
        "page": state.page,
        "mode": (state.direction << 2) | state.mode
    }


# ============================================================================
# CPU CONTROL REGISTERS AND FEATURES
# ============================================================================

# Control Register Numbers
class ControlRegister(IntEnum):
    """x86/x64 control register numbers"""
    CR0 = 0  # Processor control flags
    CR2 = 2  # Page fault linear address
    CR3 = 3  # Page directory base (PDBR)
    CR4 = 4  # Architecture extensions


# CR0 Control Flags (x86/x64)
class CR0Flags(IntEnum):
    """CR0 control register flags"""
    PE = 0        # Protected Mode Enable (bit 0)
    MP = 1        # Monitor Coprocessor (bit 1)
    EM = 2        # Emulation (bit 2)
    TS = 3        # Task Switched (bit 3)
    ET = 4        # Extension Type (bit 4)
    NE = 5        # Numeric Error (bit 5)
    WP = 16       # Write Protect (bit 16)
    AM = 18       # Alignment Mask (bit 18)
    NW = 29       # Not Write-through (bit 29)
    CD = 30       # Cache Disable (bit 30)
    PG = 31       # Paging Enable (bit 31)


# CR4 Control Flags (x86/x64)
class CR4Flags(IntEnum):
    """CR4 control register flags"""
    VME = 0       # Virtual-8086 Mode Extensions (bit 0)
    PVI = 1       # Protected-Mode Virtual Interrupts (bit 1)
    TSD = 2       # Time Stamp Disable (bit 2)
    DE = 3        # Debugging Extensions (bit 3)
    PSE = 4       # Page Size Extensions (bit 4)
    PAE = 5       # Physical Address Extension (bit 5)
    MCE = 6       # Machine-Check Enable (bit 6)
    PGE = 7       # Page Global Enable (bit 7)
    PCE = 8       # Performance-Monitoring Counter Enable (bit 8)
    OSFXSR = 9    # OS Support for FXSAVE/FXRSTOR (bit 9)
    OSXMMEXCPT = 10  # OS Support for Unmasked SIMD FP Exceptions (bit 10)
    UMIP = 11     # User-Mode Instruction Prevention (bit 11)
    LA57 = 12     # 57-bit Linear Addresses (bit 12)
    VMXE = 13     # VMX Enable (bit 13)
    SMXE = 14     # SMX Enable (bit 14)
    FSGSBASE = 16 # FS/GS Base Access (bit 16)
    PCIDE = 17    # PCID Enable (bit 17)
    OSXSAVE = 18  # XSAVE and Processor Extended States Enable (bit 18)
    SMEP = 20     # Supervisor Mode Execution Prevention (bit 20)
    SMAP = 21     # Supervisor Mode Access Prevention (bit 21)
    PKE = 22      # Protection Key Enable (bit 22)


# Common MSR Addresses
class MSRAddress(IntEnum):
    """Common Model-Specific Register addresses"""
    IA32_APIC_BASE = 0x0000001B      # APIC base address
    IA32_FEATURE_CONTROL = 0x0000003A # Feature control
    IA32_TSC = 0x00000010             # Time Stamp Counter
    IA32_MTRRCAP = 0x000000FE         # MTRR capabilities
    IA32_SYSENTER_CS = 0x00000174     # SYSENTER CS
    IA32_SYSENTER_ESP = 0x00000175    # SYSENTER ESP
    IA32_SYSENTER_EIP = 0x00000176    # SYSENTER EIP
    IA32_EFER = 0xC0000080            # Extended Feature Enable Register
    IA32_STAR = 0xC0000081            # SYSCALL target
    IA32_LSTAR = 0xC0000082           # Long mode SYSCALL target
    IA32_CSTAR = 0xC0000083           # Compatibility mode SYSCALL target
    IA32_FMASK = 0xC0000084           # SYSCALL flag mask
    IA32_FS_BASE = 0xC0000100         # FS base address
    IA32_GS_BASE = 0xC0000101         # GS base address
    IA32_KERNEL_GS_BASE = 0xC0000102  # Kernel GS base


# CPUID Feature Flags (from EAX=1, ECX and EDX)
class CPUIDFeature(IntEnum):
    """CPUID feature flags"""
    # EDX features (EAX=1)
    FPU = 0       # Floating Point Unit
    VME = 1       # Virtual 8086 Mode Extensions
    DE = 2        # Debugging Extensions
    PSE = 3       # Page Size Extension
    TSC = 4       # Time Stamp Counter
    MSR = 5       # Model Specific Registers
    PAE = 6       # Physical Address Extension
    MCE = 7       # Machine Check Exception
    CX8 = 8       # CMPXCHG8B instruction
    APIC = 9      # APIC on chip
    SEP = 11      # SYSENTER/SYSEXIT
    MTRR = 12     # Memory Type Range Registers
    PGE = 13      # Page Global Enable
    MCA = 14      # Machine Check Architecture
    CMOV = 15     # Conditional Move instruction
    PAT = 16      # Page Attribute Table
    PSE36 = 17    # 36-bit Page Size Extension
    PSN = 18      # Processor Serial Number
    CLFSH = 19    # CLFLUSH instruction
    DS = 21       # Debug Store
    ACPI = 22     # Thermal Monitor and Clock Control
    MMX = 23      # MMX technology
    FXSR = 24     # FXSAVE/FXRSTOR
    SSE = 25      # SSE extensions
    SSE2 = 26     # SSE2 extensions
    SS = 27       # Self-Snoop
    HTT = 28      # Hyper-Threading Technology
    TM = 29       # Thermal Monitor
    PBE = 31      # Pending Break Enable
    
    # ECX features (EAX=1) - offset by 32 for uniqueness
    SSE3 = 32 + 0      # SSE3 extensions
    PCLMULQDQ = 32 + 1 # PCLMULQDQ instruction
    DTES64 = 32 + 2    # 64-bit DS Area
    MONITOR = 32 + 3   # MONITOR/MWAIT
    DS_CPL = 32 + 4    # CPL Qualified Debug Store
    VMX = 32 + 5       # Virtual Machine Extensions
    SMX = 32 + 6       # Safer Mode Extensions
    EIST = 32 + 7      # Enhanced SpeedStep
    TM2 = 32 + 8       # Thermal Monitor 2
    SSSE3 = 32 + 9     # SSSE3 extensions
    CNXT_ID = 32 + 10  # L1 Context ID
    FMA = 32 + 12      # Fused Multiply-Add
    CMPXCHG16B = 32 + 13  # CMPXCHG16B instruction
    PDCM = 32 + 15     # Perfmon and Debug Capability
    PCID = 32 + 17     # Process-Context Identifiers
    DCA = 32 + 18      # Direct Cache Access
    SSE4_1 = 32 + 19   # SSE4.1 extensions
    SSE4_2 = 32 + 20   # SSE4.2 extensions
    X2APIC = 32 + 21   # x2APIC support
    MOVBE = 32 + 22    # MOVBE instruction
    POPCNT = 32 + 23   # POPCNT instruction
    AES = 32 + 25      # AES instruction set
    XSAVE = 32 + 26    # XSAVE/XRSTOR/XSETBV/XGETBV
    OSXSAVE = 32 + 27  # XSAVE enabled by OS
    AVX = 32 + 28      # AVX extensions
    F16C = 32 + 29     # F16C (half-precision) FP support
    RDRAND = 32 + 30   # RDRAND instruction


def read_cr0(runtime) -> int:
    """
    Read CR0 control register
    
    CR0 contains system control flags including:
    - PE (Protected Mode Enable)
    - PG (Paging Enable)
    - WP (Write Protect)
    - AM (Alignment Mask)
    - NW/CD (Cache control)
    
    Returns:
        CR0 register value (32/64-bit)
        
    Raises:
        PrivilegeError: If insufficient privileges
        CPUControlError: If read operation fails
        
    Example:
        set cr0_value to read_cr0
        print text "CR0:"
        print text cr0_value
        
        # Check if paging is enabled (bit 31)
        set paging_enabled to cr0_value bitwise_and 2147483648
        if paging_enabled
            print text "Paging is enabled"
        end
    """
    _require_privileges()
    
    # In real implementation, would execute:
    # mov rax, cr0
    # This requires inline assembly or privileged instruction execution
    
    # Simulated for interpreter mode
    # In compiled mode, would generate actual MOV CR0 instruction
    raise CPUControlError("CR0 access requires compiled mode with inline assembly")


def read_cr2(runtime) -> int:
    """
    Read CR2 control register (Page Fault Linear Address)
    
    CR2 contains the linear address that caused the last page fault.
    This is critical for page fault handlers to determine which address
    triggered the fault.
    
    Returns:
        CR2 register value (linear address of page fault)
        
    Raises:
        PrivilegeError: If insufficient privileges
        CPUControlError: If read operation fails
        
    Example:
        # In page fault handler
        function handle_page_fault
            set fault_address to read_cr2
            print text "Page fault at address:"
            print text fault_address
        end
    """
    _require_privileges()
    
    raise CPUControlError("CR2 access requires compiled mode with inline assembly")


def read_cr3(runtime) -> int:
    """
    Read CR3 control register (Page Directory Base Register)
    
    CR3 contains the physical address of the page directory base.
    This register is central to virtual memory management and is
    updated on every context switch.
    
    Returns:
        CR3 register value (page directory physical address)
        
    Raises:
        PrivilegeError: If insufficient privileges
        CPUControlError: If read operation fails
        
    Example:
        set page_dir to read_cr3
        print text "Page directory base:"
        print text page_dir
    """
    _require_privileges()
    
    raise CPUControlError("CR3 access requires compiled mode with inline assembly")


def write_cr0(runtime, value: int) -> bool:
    """
    Write CR0 control register
    
    WARNING: Modifying CR0 can crash the system if done incorrectly.
    Common operations:
    - Enable/disable paging (bit 31)
    - Enable/disable write protection (bit 16)
    - Enable/disable cache (bits 29-30)
    
    Args:
        value: New CR0 value
        
    Returns:
        True if successful
        
    Raises:
        PrivilegeError: If insufficient privileges
        CPUControlError: If write operation fails
        
    Example:
        # Enable paging (set bit 31)
        set cr0_value to read_cr0
        set cr0_value to cr0_value bitwise_or 2147483648
        write_cr0 with value: cr0_value
    """
    _require_privileges()
    
    if not isinstance(value, int):
        raise CPUControlError("CR0 value must be an integer")
    
    raise CPUControlError("CR0 write requires compiled mode with inline assembly")


def write_cr3(runtime, value: int) -> bool:
    """
    Write CR3 control register (Page Directory Base)
    
    WARNING: Changing CR3 switches the active page directory.
    This is used for:
    - Process context switches
    - Address space changes
    - TLB flushing (writing same value flushes TLB)
    
    Args:
        value: Physical address of page directory (must be page-aligned)
        
    Returns:
        True if successful
        
    Raises:
        PrivilegeError: If insufficient privileges
        CPUControlError: If write operation fails or address not page-aligned
        
    Example:
        # Switch to new page directory
        write_cr3 with value: 4096  # Physical address
        
        # Flush TLB (write same value)
        set cr3 to read_cr3
        write_cr3 with value: cr3
    """
    _require_privileges()
    
    if not isinstance(value, int):
        raise CPUControlError("CR3 value must be an integer")
    
    # Must be page-aligned (4KB = 4096 bytes)
    if value % 4096 != 0:
        raise CPUControlError(f"CR3 value must be page-aligned (multiple of 4096), got {value}")
    
    raise CPUControlError("CR3 write requires compiled mode with inline assembly")


def write_cr4(runtime, value: int) -> bool:
    """
    Write CR4 control register (Architecture Extensions)
    
    WARNING: Modifying CR4 can crash the system if done incorrectly.
    Common operations:
    - Enable/disable PAE (bit 5)
    - Enable/disable PSE (bit 4)
    - Enable/disable PGE (bit 7)
    - Enable/disable OSFXSR (bit 9)
    - Enable/disable OSXMMEXCPT (bit 10)
    
    Args:
        value: New CR4 value
        
    Returns:
        True if successful
        
    Raises:
        PrivilegeError: If insufficient privileges
        CPUControlError: If write operation fails
        
    Example:
        # Enable PAE (Physical Address Extension)
        set cr4_value to read_cr4
        set cr4_value to cr4_value bitwise_or 32  # Bit 5
        write_cr4 with value: cr4_value
    """
    _require_privileges()
    
    if not isinstance(value, int):
        raise CPUControlError("CR4 value must be an integer")
    
    raise CPUControlError("CR4 write requires compiled mode with inline assembly")


def read_cr4(runtime) -> int:
    """
    Read CR4 control register (Architecture Extensions)
    
    CR4 contains flags for various CPU extensions:
    - PAE: Physical Address Extension
    - PSE: Page Size Extensions
    - PGE: Page Global Enable
    - OSFXSR: OS support for FXSAVE/FXRSTOR
    - OSXMMEXCPT: OS support for SIMD exceptions
    - VMXE: VMX (virtualization) enable
    - SMEP/SMAP: Security features
    
    Returns:
        CR4 register value
        
    Raises:
        PrivilegeError: If insufficient privileges
        CPUControlError: If read operation fails
        
    Example:
        set cr4_value to read_cr4
        print text "CR4:"
        print text cr4_value
        
        # Check if PAE is enabled (bit 5)
        set pae_enabled to cr4_value bitwise_and 32
        if pae_enabled
            print text "PAE is enabled"
        end
    """
    _require_privileges()
    
    raise CPUControlError("CR4 access requires compiled mode with inline assembly")


def read_msr(runtime, msr_address: int) -> int:
    """
    Read Model-Specific Register (MSR)
    
    MSRs are special registers unique to x86/x64 processors for:
    - Performance monitoring
    - Feature configuration
    - System call setup (SYSENTER/SYSCALL)
    - Extended features (EFER)
    - APIC configuration
    
    Args:
        msr_address: MSR address (e.g., 0xC0000080 for IA32_EFER)
        
    Returns:
        64-bit MSR value
        
    Raises:
        PrivilegeError: If insufficient privileges
        CPUControlError: If MSR doesn't exist or read fails
        
    Example:
        # Read Extended Feature Enable Register
        set efer to read_msr with msr_address: 3221225600  # 0xC0000080
        print text "EFER:"
        print text efer
        
        # Check if long mode is active (bit 10)
        set long_mode to efer bitwise_and 1024
        if long_mode
            print text "CPU in long mode (64-bit)"
        end
    """
    _require_privileges()
    
    if not isinstance(msr_address, int):
        raise CPUControlError("MSR address must be an integer")
    
    if msr_address < 0:
        raise CPUControlError(f"Invalid MSR address: {msr_address}")
    
    # In real implementation, would execute:
    # rdmsr (reads MSR specified in ECX into EDX:EAX)
    raise CPUControlError("MSR read requires compiled mode with inline assembly")


def write_msr(runtime, msr_address: int, value: int) -> bool:
    """
    Write Model-Specific Register (MSR)
    
    WARNING: Writing incorrect values to MSRs can crash the system.
    
    Args:
        msr_address: MSR address
        value: 64-bit value to write
        
    Returns:
        True if successful
        
    Raises:
        PrivilegeError: If insufficient privileges
        CPUControlError: If MSR doesn't exist or write fails
        
    Example:
        # Enable SYSCALL/SYSRET support in EFER
        set efer to read_msr with msr_address: 3221225600  # IA32_EFER
        set efer to efer bitwise_or 1  # Set SCE bit
        write_msr with msr_address: 3221225600 and value: efer
    """
    _require_privileges()
    
    if not isinstance(msr_address, int):
        raise CPUControlError("MSR address must be an integer")
    
    if not isinstance(value, int):
        raise CPUControlError("MSR value must be an integer")
    
    if msr_address < 0:
        raise CPUControlError(f"Invalid MSR address: {msr_address}")
    
    # In real implementation, would execute:
    # wrmsr (writes EDX:EAX to MSR specified in ECX)
    raise CPUControlError("MSR write requires compiled mode with inline assembly")


def check_msr_support(runtime, msr_address: int) -> bool:
    """
    Check if a specific MSR is supported by the CPU
    
    Args:
        msr_address: MSR address to check
        
    Returns:
        True if MSR is supported, False otherwise
        
    Example:
        set has_efer to check_msr_support with msr_address: 3221225600
        if has_efer
            print text "EFER register is supported"
        end
    """
    _require_privileges()
    
    # In real implementation, would attempt to read MSR
    # and catch general protection fault
    
    # Common MSRs are generally supported on modern x86/x64
    if msr_address == MSRAddress.IA32_EFER:
        return True  # Present on all 64-bit capable CPUs
    
    return False  # Conservative default


def cpuid(runtime, leaf: int, subleaf: int = 0) -> dict:
    """
    Execute CPUID instruction
    
    CPUID provides processor identification and feature information.
    Different leaf values return different information.
    
    Args:
        leaf: CPUID leaf (EAX value)
        subleaf: CPUID subleaf (ECX value), default 0
        
    Returns:
        Dictionary with keys: eax, ebx, ecx, edx (32-bit values)
        
    Raises:
        CPUControlError: If CPUID execution fails
        
    Common leaf values:
        0: Maximum supported leaf and vendor ID
        1: Processor info and feature flags
        7: Extended features
        0x80000000: Maximum extended leaf
        0x80000001: Extended processor info
        
    Example:
        # Get vendor ID
        set result to cpuid with leaf: 0
        print text "Max leaf:"
        print text result["eax"]
        
        # Get feature flags
        set features to cpuid with leaf: 1
        print text "Feature flags (EDX):"
        print text features["edx"]
        print text "Feature flags (ECX):"
        print text features["ecx"]
    """
    if not isinstance(leaf, int):
        raise CPUControlError("CPUID leaf must be an integer")
    
    if not isinstance(subleaf, int):
        raise CPUControlError("CPUID subleaf must be an integer")
    
    # In real implementation, would execute:
    # cpuid instruction with EAX=leaf, ECX=subleaf
    # Returns EAX, EBX, ECX, EDX
    
    # Simulated values for interpreter mode
    if leaf == 0:
        # Return vendor ID: "GenuineIntel" or "AuthenticAMD"
        return {
            "eax": 13,  # Maximum supported leaf
            "ebx": 0x756e6547,  # "Genu"
            "ecx": 0x6c65746e,  # "ntel"
            "edx": 0x49656e69   # "ineI"
        }
    elif leaf == 1:
        # Return feature flags (simulated)
        return {
            "eax": 0x000806EA,  # Family, model, stepping
            "ebx": 0x00000800,  # Brand index, CLFLUSH, etc.
            "ecx": 0x7FFAFBBF,  # Feature flags (SSE3, AVX, etc.)
            "edx": 0xBFEBFBFF   # Feature flags (FPU, SSE2, etc.)
        }
    
    return {"eax": 0, "ebx": 0, "ecx": 0, "edx": 0}


def get_cpu_vendor(runtime) -> str:
    """
    Get CPU vendor string
    
    Returns:
        Vendor string: "GenuineIntel", "AuthenticAMD", "Unknown", etc.
        
    Example:
        set vendor to get_cpu_vendor
        print text "CPU vendor:"
        print text vendor
    """
    result = cpuid(runtime, 0)
    
    # Extract vendor string from EBX, EDX, ECX
    ebx = result["ebx"]
    edx = result["edx"]
    ecx = result["ecx"]
    
    # Convert to string (4 bytes each)
    vendor = ""
    for val in [ebx, edx, ecx]:
        for i in range(4):
            byte = (val >> (i * 8)) & 0xFF
            if byte != 0:
                vendor += chr(byte)
    
    return vendor if vendor else "Unknown"


def get_cpu_features(runtime) -> dict:
    """
    Get CPU feature flags
    
    Returns:
        Dictionary with feature categories:
        - basic: Features from CPUID leaf 1 (EDX)
        - extended: Features from CPUID leaf 1 (ECX)
        - has_sse: Boolean for SSE support
        - has_sse2: Boolean for SSE2 support
        - has_sse3: Boolean for SSE3 support
        - has_ssse3: Boolean for SSSE3 support
        - has_sse4_1: Boolean for SSE4.1 support
        - has_sse4_2: Boolean for SSE4.2 support
        - has_avx: Boolean for AVX support
        - has_avx2: Boolean for AVX2 support (requires leaf 7)
        - has_fma: Boolean for FMA support
        - has_aes: Boolean for AES-NI support
        
    Example:
        set features to get_cpu_features
        print text "CPU Features:"
        
        if features["has_sse2"]
            print text "SSE2 supported"
        end
        
        if features["has_avx"]
            print text "AVX supported"
        end
    """
    # Get feature flags from leaf 1
    result = cpuid(runtime, 1)
    edx = result["edx"]
    ecx = result["ecx"]
    
    # Extract feature bits
    features = {
        "basic": edx,
        "extended": ecx,
        # EDX features
        "has_fpu": bool(edx & (1 << 0)),
        "has_tsc": bool(edx & (1 << 4)),
        "has_msr": bool(edx & (1 << 5)),
        "has_apic": bool(edx & (1 << 9)),
        "has_cmov": bool(edx & (1 << 15)),
        "has_mmx": bool(edx & (1 << 23)),
        "has_fxsr": bool(edx & (1 << 24)),
        "has_sse": bool(edx & (1 << 25)),
        "has_sse2": bool(edx & (1 << 26)),
        "has_htt": bool(edx & (1 << 28)),
        # ECX features
        "has_sse3": bool(ecx & (1 << 0)),
        "has_pclmulqdq": bool(ecx & (1 << 1)),
        "has_monitor": bool(ecx & (1 << 3)),
        "has_vmx": bool(ecx & (1 << 5)),
        "has_ssse3": bool(ecx & (1 << 9)),
        "has_fma": bool(ecx & (1 << 12)),
        "has_cmpxchg16b": bool(ecx & (1 << 13)),
        "has_sse4_1": bool(ecx & (1 << 19)),
        "has_sse4_2": bool(ecx & (1 << 20)),
        "has_movbe": bool(ecx & (1 << 22)),
        "has_popcnt": bool(ecx & (1 << 23)),
        "has_aes": bool(ecx & (1 << 25)),
        "has_xsave": bool(ecx & (1 << 26)),
        "has_osxsave": bool(ecx & (1 << 27)),
        "has_avx": bool(ecx & (1 << 28)),
        "has_f16c": bool(ecx & (1 << 29)),
        "has_rdrand": bool(ecx & (1 << 30)),
    }
    
    # Check for AVX2 (requires leaf 7, subleaf 0)
    try:
        result7 = cpuid(runtime, 7, 0)
        ebx7 = result7["ebx"]
        features["has_avx2"] = bool(ebx7 & (1 << 5))
        features["has_bmi1"] = bool(ebx7 & (1 << 3))
        features["has_bmi2"] = bool(ebx7 & (1 << 8))
        features["has_avx512f"] = bool(ebx7 & (1 << 16))
    except:
        features["has_avx2"] = False
        features["has_bmi1"] = False
        features["has_bmi2"] = False
        features["has_avx512f"] = False
    
    return features


def check_feature(runtime, feature_name: str) -> bool:
    """
    Check if a specific CPU feature is supported
    
    Args:
        feature_name: Feature name (e.g., "sse2", "avx", "fma")
        
    Returns:
        True if feature is supported, False otherwise
        
    Supported feature names:
        - fpu, tsc, msr, apic, cmov, mmx, fxsr
        - sse, sse2, sse3, ssse3, sse4_1, sse4_2
        - avx, avx2, avx512f
        - fma, aes, pclmulqdq
        - vmx, htt, f16c, rdrand
        - bmi1, bmi2, popcnt, movbe
        
    Example:
        set has_avx to check_feature with feature_name: "avx"
        if has_avx
            print text "AVX is supported"
        else
            print text "AVX not supported"
        end
    """
    if not isinstance(feature_name, str):
        raise CPUControlError("Feature name must be a string")
    
    features = get_cpu_features(runtime)
    feature_key = f"has_{feature_name.lower()}"
    
    if feature_key in features:
        return features[feature_key]
    
    return False


def register_stdlib(runtime):
    """Register hardware module functions with NexusLang runtime"""
    from nexuslang.runtime.runtime import Runtime
    
    # Port I/O operations
    runtime.register_function("read_port_byte", read_port_byte)
    runtime.register_function("read_port_word", read_port_word)
    runtime.register_function("read_port_dword", read_port_dword)
    runtime.register_function("write_port_byte", write_port_byte)
    runtime.register_function("write_port_word", write_port_word)
    runtime.register_function("write_port_dword", write_port_dword)
    
    # Legacy names for backward compatibility
    runtime.register_function("read_port", read_port_byte)
    runtime.register_function("write_port", write_port_byte)
    
    # Memory-mapped I/O - mapping
    runtime.register_function("map_memory", map_memory)
    runtime.register_function("unmap_memory", unmap_memory)
    runtime.register_function("get_mapping_info", get_mapping_info)
    runtime.register_function("list_mappings", list_mappings)
    
    # Memory-mapped I/O - read operations
    runtime.register_function("read_mmio_byte", read_mmio_byte)
    runtime.register_function("read_mmio_word", read_mmio_word)
    runtime.register_function("read_mmio_dword", read_mmio_dword)
    runtime.register_function("read_mmio_qword", read_mmio_qword)
    
    # Memory-mapped I/O - write operations
    runtime.register_function("write_mmio_byte", write_mmio_byte)
    runtime.register_function("write_mmio_word", write_mmio_word)
    runtime.register_function("write_mmio_dword", write_mmio_dword)
    runtime.register_function("write_mmio_qword", write_mmio_qword)
    
    # Interrupt and exception handling - IDT management
    runtime.register_function("setup_idt", setup_idt)
    runtime.register_function("get_idt_entry", get_idt_entry)
    runtime.register_function("set_idt_entry", set_idt_entry)
    runtime.register_function("get_idt_base", get_idt_base)
    runtime.register_function("get_idt_limit", get_idt_limit)
    
    # Interrupt and exception handling - handler registration
    runtime.register_function("register_interrupt_handler", register_interrupt_handler)
    runtime.register_function("unregister_interrupt_handler", unregister_interrupt_handler)
    runtime.register_function("list_interrupt_handlers", list_interrupt_handlers)
    
    # Interrupt and exception handling - interrupt control
    runtime.register_function("enable_interrupts", enable_interrupts)
    runtime.register_function("disable_interrupts", disable_interrupts)
    runtime.register_function("get_interrupt_flag", get_interrupt_flag)
    runtime.register_function("set_interrupt_flag", set_interrupt_flag)
    
    # Interrupt and exception handling - exception frame access
    runtime.register_function("get_exception_frame", get_exception_frame)
    runtime.register_function("get_error_code", get_error_code)
    runtime.register_function("get_instruction_pointer", get_instruction_pointer)
    runtime.register_function("get_stack_pointer", get_stack_pointer)
    runtime.register_function("get_cpu_flags", get_cpu_flags)
    
    # DMA Control - channel allocation
    runtime.register_function("allocate_dma_channel", allocate_dma_channel)
    runtime.register_function("release_dma_channel", release_dma_channel)
    runtime.register_function("get_channel_status", get_channel_status)
    runtime.register_function("list_allocated_channels", list_allocated_channels)
    
    # DMA Control - transfer configuration
    runtime.register_function("configure_dma_transfer", configure_dma_transfer)
    runtime.register_function("set_dma_address", set_dma_address)
    runtime.register_function("set_dma_count", set_dma_count)
    runtime.register_function("set_dma_mode", set_dma_mode)
    
    # DMA Control - transfer control
    runtime.register_function("start_dma_transfer", start_dma_transfer)
    runtime.register_function("stop_dma_transfer", stop_dma_transfer)
    runtime.register_function("reset_dma_controller", reset_dma_controller)
    runtime.register_function("mask_dma_channel", mask_dma_channel)
    runtime.register_function("unmask_dma_channel", unmask_dma_channel)
    
    # DMA Control - status monitoring
    runtime.register_function("get_dma_status", get_dma_status)
    runtime.register_function("get_transfer_count", get_transfer_count)
    runtime.register_function("is_transfer_complete", is_transfer_complete)
    runtime.register_function("get_dma_registers", get_dma_registers)
    
    # CPU Control - control registers
    runtime.register_function("read_cr0", read_cr0)
    runtime.register_function("read_cr2", read_cr2)
    runtime.register_function("read_cr3", read_cr3)
    runtime.register_function("read_cr4", read_cr4)
    runtime.register_function("write_cr0", write_cr0)
    runtime.register_function("write_cr3", write_cr3)
    runtime.register_function("write_cr4", write_cr4)
    
    # CPU Control - MSRs
    runtime.register_function("read_msr", read_msr)
    runtime.register_function("write_msr", write_msr)
    runtime.register_function("check_msr_support", check_msr_support)
    
    # CPU Control - CPUID
    runtime.register_function("cpuid", cpuid)
    runtime.register_function("get_cpu_vendor", get_cpu_vendor)
    runtime.register_function("get_cpu_features", get_cpu_features)
    runtime.register_function("check_feature", check_feature)

