"""
Interrupt Handling Module

Provides interrupt handling framework inspired by x86 interrupt architecture.
This module simulates hardware and software interrupts for educational purposes
and low-level system programming concepts.

Interrupt Types:
- Hardware interrupts: Simulated hardware events (timer, keyboard, disk, etc.)
- Software interrupts: User-triggered interrupts (INT instruction equivalent)
- Exceptions: CPU-generated interrupts (divide by zero, page fault, etc.)

Common x86 Interrupt Vectors:
- 0x00: Divide by zero
- 0x01: Debug
- 0x03: Breakpoint
- 0x04: Overflow
- 0x06: Invalid opcode
- 0x08: Double fault
- 0x0D: General protection fault
- 0x0E: Page fault
- 0x20-0x2F: Hardware IRQs (timer, keyboard, etc.)
- 0x80: System call (Linux)

Note: This is a simulation for learning purposes. Real interrupt handling
requires kernel-level privileges and hardware access.
"""

from ...runtime.runtime import Runtime
import sys
import traceback


# Global interrupt state
_interrupt_enabled = True
_interrupt_handlers = {}  # vector -> handler function
_interrupt_vectors = {}   # name -> vector number
_interrupt_history = []   # List of triggered interrupts
_pending_interrupts = []  # Queue of pending interrupts


# Standard x86 interrupt vectors
INTERRUPT_VECTORS = {
    "DIVIDE_BY_ZERO": 0x00,
    "DEBUG": 0x01,
    "NMI": 0x02,  # Non-maskable interrupt
    "BREAKPOINT": 0x03,
    "OVERFLOW": 0x04,
    "BOUND_RANGE_EXCEEDED": 0x05,
    "INVALID_OPCODE": 0x06,
    "DEVICE_NOT_AVAILABLE": 0x07,
    "DOUBLE_FAULT": 0x08,
    "COPROCESSOR_SEGMENT_OVERRUN": 0x09,
    "INVALID_TSS": 0x0A,
    "SEGMENT_NOT_PRESENT": 0x0B,
    "STACK_SEGMENT_FAULT": 0x0C,
    "GENERAL_PROTECTION_FAULT": 0x0D,
    "PAGE_FAULT": 0x0E,
    "X87_FLOATING_POINT": 0x10,
    "ALIGNMENT_CHECK": 0x11,
    "MACHINE_CHECK": 0x12,
    "SIMD_FLOATING_POINT": 0x13,
    "VIRTUALIZATION": 0x14,
    "SECURITY_EXCEPTION": 0x1E,
    # Hardware IRQs (typically start at 0x20)
    "IRQ_TIMER": 0x20,
    "IRQ_KEYBOARD": 0x21,
    "IRQ_CASCADE": 0x22,
    "IRQ_COM2": 0x23,
    "IRQ_COM1": 0x24,
    "IRQ_LPT2": 0x25,
    "IRQ_FLOPPY": 0x26,
    "IRQ_LPT1": 0x27,
    "IRQ_RTC": 0x28,
    "IRQ_MOUSE": 0x2C,
    "IRQ_MATH_COPROCESSOR": 0x2D,
    "IRQ_PRIMARY_ATA": 0x2E,
    "IRQ_SECONDARY_ATA": 0x2F,
    # Software interrupts
    "SYSCALL": 0x80,
}


def interrupt_init(runtime: Runtime):
    """Initialize interrupt system.
    
    Resets all interrupt state, handlers, and history.
    """
    global _interrupt_enabled, _interrupt_handlers, _interrupt_vectors
    global _interrupt_history, _pending_interrupts
    
    _interrupt_enabled = True
    _interrupt_handlers = {}
    _interrupt_vectors = INTERRUPT_VECTORS.copy()
    _interrupt_history = []
    _pending_interrupts = []
    
    return True


def interrupt_enable(runtime: Runtime):
    """Enable interrupt handling (equivalent to STI instruction).
    
    When enabled, pending interrupts will be processed.
    """
    global _interrupt_enabled
    _interrupt_enabled = True
    return True


def interrupt_disable(runtime: Runtime):
    """Disable interrupt handling (equivalent to CLI instruction).
    
    When disabled, interrupts are queued but not processed.
    """
    global _interrupt_enabled
    _interrupt_enabled = False
    return True


def interrupt_is_enabled(runtime: Runtime):
    """Check if interrupts are currently enabled.
    
    Returns: True if enabled, False otherwise
    """
    return _interrupt_enabled


def interrupt_register_handler(runtime: Runtime, vector, handler):
    """Register a handler function for an interrupt vector.
    
    Args:
        vector: Interrupt vector number (0-255) or name
        handler: Function to call when interrupt occurs
    
    Example:
        interrupt_register_handler(0x03, my_breakpoint_handler)
        interrupt_register_handler("DIVIDE_BY_ZERO", my_div_handler)
    """
    # Convert vector name to number if needed
    if isinstance(vector, str):
        if vector not in _interrupt_vectors:
            raise ValueError(f"Unknown interrupt vector name: {vector}")
        vector_num = _interrupt_vectors[vector]
    else:
        vector_num = int(vector)
    
    if vector_num < 0 or vector_num > 255:
        raise ValueError(f"Interrupt vector must be 0-255, got {vector_num}")
    
    _interrupt_handlers[vector_num] = handler
    return True


def interrupt_unregister_handler(runtime: Runtime, vector):
    """Unregister a handler for an interrupt vector.
    
    Args:
        vector: Interrupt vector number or name
    """
    # Convert vector name to number if needed
    if isinstance(vector, str):
        if vector not in _interrupt_vectors:
            raise ValueError(f"Unknown interrupt vector name: {vector}")
        vector_num = _interrupt_vectors[vector]
    else:
        vector_num = int(vector)
    
    if vector_num in _interrupt_handlers:
        del _interrupt_handlers[vector_num]
    
    return True


def interrupt_get_handler(runtime: Runtime, vector):
    """Get the handler function for an interrupt vector.
    
    Args:
        vector: Interrupt vector number or name
    
    Returns: Handler function or None if not registered
    """
    # Convert vector name to number if needed
    if isinstance(vector, str):
        if vector not in _interrupt_vectors:
            raise ValueError(f"Unknown interrupt vector name: {vector}")
        vector_num = _interrupt_vectors[vector]
    else:
        vector_num = int(vector)
    
    return _interrupt_handlers.get(vector_num)


def interrupt_trigger(runtime: Runtime, vector, data=None):
    """Trigger an interrupt (software interrupt - INT instruction equivalent).
    
    Args:
        vector: Interrupt vector number or name
        data: Optional data to pass to handler
    
    Returns: Handler return value or None
    """
    # Convert vector name to number if needed
    if isinstance(vector, str):
        if vector not in _interrupt_vectors:
            raise ValueError(f"Unknown interrupt vector name: {vector}")
        vector_num = _interrupt_vectors[vector]
    else:
        vector_num = int(vector)
    
    if vector_num < 0 or vector_num > 255:
        raise ValueError(f"Interrupt vector must be 0-255, got {vector_num}")
    
    # Record interrupt in history
    _interrupt_history.append({
        "vector": vector_num,
        "data": data,
        "enabled": _interrupt_enabled
    })
    
    # If interrupts are disabled, queue the interrupt
    if not _interrupt_enabled:
        _pending_interrupts.append({"vector": vector_num, "data": data})
        return None
    
    # Call handler if registered
    if vector_num in _interrupt_handlers:
        handler = _interrupt_handlers[vector_num]
        try:
            # Call handler with runtime and data
            if callable(handler):
                if data is not None:
                    return handler(runtime, data)
                else:
                    return handler(runtime)
            else:
                raise TypeError(f"Handler for interrupt {vector_num} is not callable")
        except Exception as e:
            # Handler error - print to stderr but don't crash
            print(f"Error in interrupt handler {vector_num}: {e}", file=sys.stderr)
            traceback.print_exc(file=sys.stderr)
            return None
    
    # No handler registered
    return None


def interrupt_process_pending(runtime: Runtime):
    """Process all pending interrupts (if interrupts are enabled).
    
    Returns: Number of interrupts processed
    """
    if not _interrupt_enabled:
        return 0
    
    count = 0
    while _pending_interrupts:
        pending = _pending_interrupts.pop(0)
        interrupt_trigger(runtime, pending["vector"], pending["data"])
        count += 1
    
    return count


def interrupt_get_pending_count(runtime: Runtime):
    """Get number of pending interrupts.
    
    Returns: Number of interrupts waiting to be processed
    """
    return len(_pending_interrupts)


def interrupt_clear_pending(runtime: Runtime):
    """Clear all pending interrupts without processing them.
    
    Returns: Number of interrupts cleared
    """
    count = len(_pending_interrupts)
    _pending_interrupts.clear()
    return count


def interrupt_get_vector(runtime: Runtime, name):
    """Get interrupt vector number by name.
    
    Example: interrupt_get_vector("DIVIDE_BY_ZERO") -> 0
    """
    name = str(name)
    if name not in _interrupt_vectors:
        raise ValueError(f"Unknown interrupt vector name: {name}")
    
    return _interrupt_vectors[name]


def interrupt_get_vector_name(runtime: Runtime, vector):
    """Get interrupt vector name by number.
    
    Example: interrupt_get_vector_name(0) -> "DIVIDE_BY_ZERO"
    """
    vector = int(vector)
    
    for name, num in _interrupt_vectors.items():
        if num == vector:
            return name
    
    return None


def interrupt_register_vector(runtime: Runtime, name, vector):
    """Register a custom interrupt vector name.
    
    Args:
        name: Name for the interrupt vector
        vector: Vector number (0-255)
    
    Example: interrupt_register_vector("MY_INTERRUPT", 0x90)
    """
    name = str(name)
    vector = int(vector)
    
    if vector < 0 or vector > 255:
        raise ValueError(f"Interrupt vector must be 0-255, got {vector}")
    
    _interrupt_vectors[name] = vector
    return True


def interrupt_get_all_vectors(runtime: Runtime):
    """Get all registered interrupt vectors.
    
    Returns: Dictionary mapping names to vector numbers
    """
    return _interrupt_vectors.copy()


def interrupt_get_history(runtime: Runtime, count=None):
    """Get interrupt history.
    
    Args:
        count: Optional number of recent interrupts to return (default: all)
    
    Returns: List of interrupt records
    """
    if count is None:
        return _interrupt_history.copy()
    
    count = int(count)
    if count < 0:
        raise ValueError("Count must be non-negative")
    
    return _interrupt_history[-count:] if count > 0 else []


def interrupt_clear_history(runtime: Runtime):
    """Clear interrupt history.
    
    Returns: Number of records cleared
    """
    count = len(_interrupt_history)
    _interrupt_history.clear()
    return count


def interrupt_get_stats(runtime: Runtime):
    """Get interrupt statistics.
    
    Returns: Dictionary with statistics
    """
    total = len(_interrupt_history)
    pending = len(_pending_interrupts)
    handlers = len(_interrupt_handlers)
    
    # Count interrupts by vector
    by_vector = {}
    for record in _interrupt_history:
        vec = record["vector"]
        by_vector[vec] = by_vector.get(vec, 0) + 1
    
    return {
        "enabled": _interrupt_enabled,
        "total_triggered": total,
        "pending": pending,
        "handlers_registered": handlers,
        "by_vector": by_vector
    }


# Common interrupt simulation helpers
def interrupt_simulate_divide_by_zero(runtime: Runtime):
    """Simulate divide-by-zero exception (INT 0x00).
    
    Returns: Handler result or None
    """
    return interrupt_trigger(runtime, 0x00, {"error": "Division by zero"})


def interrupt_simulate_breakpoint(runtime: Runtime, address=None):
    """Simulate breakpoint interrupt (INT 0x03).
    
    Args:
        address: Optional instruction address where breakpoint occurred
    
    Returns: Handler result or None
    """
    data = {"type": "breakpoint"}
    if address is not None:
        data["address"] = address
    
    return interrupt_trigger(runtime, 0x03, data)


def interrupt_simulate_overflow(runtime: Runtime):
    """Simulate overflow exception (INT 0x04).
    
    Returns: Handler result or None
    """
    return interrupt_trigger(runtime, 0x04, {"error": "Arithmetic overflow"})


def interrupt_simulate_page_fault(runtime: Runtime, address, access_type="read"):
    """Simulate page fault exception (INT 0x0E).
    
    Args:
        address: Memory address that caused fault
        access_type: "read", "write", or "execute"
    
    Returns: Handler result or None
    """
    return interrupt_trigger(runtime, 0x0E, {
        "error": "Page fault",
        "address": address,
        "access_type": access_type
    })


def interrupt_simulate_general_protection_fault(runtime: Runtime, segment=None):
    """Simulate general protection fault (INT 0x0D).
    
    Args:
        segment: Optional segment selector that caused fault
    
    Returns: Handler result or None
    """
    data = {"error": "General protection fault"}
    if segment is not None:
        data["segment"] = segment
    
    return interrupt_trigger(runtime, 0x0D, data)


def interrupt_simulate_timer_tick(runtime: Runtime, ticks=None):
    """Simulate timer interrupt (IRQ 0 / INT 0x20).
    
    Args:
        ticks: Optional tick count
    
    Returns: Handler result or None
    """
    data = {"type": "timer"}
    if ticks is not None:
        data["ticks"] = ticks
    
    return interrupt_trigger(runtime, 0x20, data)


def interrupt_simulate_keyboard(runtime: Runtime, scancode):
    """Simulate keyboard interrupt (IRQ 1 / INT 0x21).
    
    Args:
        scancode: Keyboard scancode
    
    Returns: Handler result or None
    """
    return interrupt_trigger(runtime, 0x21, {
        "type": "keyboard",
        "scancode": scancode
    })


def interrupt_syscall(runtime: Runtime, syscall_number, *args):
    """Simulate system call interrupt (INT 0x80).
    
    Args:
        syscall_number: System call number
        *args: System call arguments
    
    Returns: Handler result or None
    """
    return interrupt_trigger(runtime, 0x80, {
        "syscall": syscall_number,
        "args": args
    })


def register_interrupt_functions(runtime: Runtime) -> None:
    """Register interrupt handling functions with the runtime."""
    # Core interrupt management
    runtime.register_function("interrupt_init", interrupt_init)
    runtime.register_function("interrupt_enable", interrupt_enable)
    runtime.register_function("interrupt_disable", interrupt_disable)
    runtime.register_function("interrupt_is_enabled", interrupt_is_enabled)
    
    # Handler management
    runtime.register_function("interrupt_register_handler", interrupt_register_handler)
    runtime.register_function("interrupt_unregister_handler", interrupt_unregister_handler)
    runtime.register_function("interrupt_get_handler", interrupt_get_handler)
    
    # Interrupt triggering
    runtime.register_function("interrupt_trigger", interrupt_trigger)
    runtime.register_function("interrupt_process_pending", interrupt_process_pending)
    runtime.register_function("interrupt_get_pending_count", interrupt_get_pending_count)
    runtime.register_function("interrupt_clear_pending", interrupt_clear_pending)
    
    # Vector management
    runtime.register_function("interrupt_get_vector", interrupt_get_vector)
    runtime.register_function("interrupt_get_vector_name", interrupt_get_vector_name)
    runtime.register_function("interrupt_register_vector", interrupt_register_vector)
    runtime.register_function("interrupt_get_all_vectors", interrupt_get_all_vectors)
    
    # History and statistics
    runtime.register_function("interrupt_get_history", interrupt_get_history)
    runtime.register_function("interrupt_clear_history", interrupt_clear_history)
    runtime.register_function("interrupt_get_stats", interrupt_get_stats)
    
    # Common interrupt simulations
    runtime.register_function("interrupt_simulate_divide_by_zero", interrupt_simulate_divide_by_zero)
    runtime.register_function("interrupt_simulate_breakpoint", interrupt_simulate_breakpoint)
    runtime.register_function("interrupt_simulate_overflow", interrupt_simulate_overflow)
    runtime.register_function("interrupt_simulate_page_fault", interrupt_simulate_page_fault)
    runtime.register_function("interrupt_simulate_general_protection_fault", interrupt_simulate_general_protection_fault)
    runtime.register_function("interrupt_simulate_timer_tick", interrupt_simulate_timer_tick)
    runtime.register_function("interrupt_simulate_keyboard", interrupt_simulate_keyboard)
    runtime.register_function("interrupt_syscall", interrupt_syscall)
