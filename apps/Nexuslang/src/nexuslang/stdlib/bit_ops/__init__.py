"""
Bit manipulation operations for NexusLang.

Provides low-level bit operations found in Assembly and C/C++.

Features:
- Bitwise logical operations (AND, OR, XOR, NOT)
- Bit shifts (left, right, arithmetic, logical)
- Bit rotation (ROL, ROR)
- Bit counting (popcount, leading zeros, trailing zeros)
- Bit extraction and masking
- Bit field manipulation

Example usage in NexusLang:
    # Bitwise operations
    set result to bit_and with 0x0F and 0xF0
    set result to bit_or with 0x0F and 0xF0
    set result to bit_xor with 0xFF and 0xAA
    set result to bit_not with 0xFF
    
    # Bit shifts
    set shifted to bit_shl with 1 and 4  # 1 << 4 = 16
    set shifted to bit_shr with 16 and 2  # 16 >> 2 = 4
    
    # Bit rotation
    set rotated to bit_rol with 0x01 and 1 and 8  # Rotate left in 8-bit
    
    # Bit counting
    set count to bit_popcount with 0xFF  # Count set bits = 8
    set zeros to bit_clz with 0x0F and 32  # Leading zeros in 32-bit
"""

from ...runtime.runtime import Runtime


def bit_and(a, b):
    """
    Bitwise AND operation.
    
    Args:
        a: First integer
        b: Second integer
    
    Returns:
        a & b
    """
    return int(a) & int(b)


def bit_or(a, b):
    """
    Bitwise OR operation.
    
    Args:
        a: First integer
        b: Second integer
    
    Returns:
        a | b
    """
    return int(a) | int(b)


def bit_xor(a, b):
    """
    Bitwise XOR (exclusive OR) operation.
    
    Args:
        a: First integer
        b: Second integer
    
    Returns:
        a ^ b
    """
    return int(a) ^ int(b)


def bit_not(a, width=None):
    """
    Bitwise NOT operation.
    
    Args:
        a: Integer to invert
        width: Bit width (8, 16, 32, 64) - if None, inverts all bits
    
    Returns:
        ~a (masked to width if specified)
    """
    result = ~int(a)
    
    if width is not None:
        # Mask to specified width
        mask = (1 << width) - 1
        result = result & mask
    
    return result


def bit_shl(a, n):
    """
    Bitwise left shift (logical).
    
    Args:
        a: Integer to shift
        n: Number of positions to shift
    
    Returns:
        a << n
    """
    return int(a) << int(n)


def bit_shr(a, n):
    """
    Bitwise right shift (logical).
    
    Args:
        a: Integer to shift (treated as unsigned)
        n: Number of positions to shift
    
    Returns:
        a >> n (logical shift)
    """
    # Python's >> is arithmetic shift for negative numbers
    # For logical shift, ensure unsigned
    value = int(a)
    if value < 0:
        # Convert to unsigned representation
        # Assuming 64-bit
        value = value & 0xFFFFFFFFFFFFFFFF
    
    return value >> int(n)


def bit_asr(a, n, width=32):
    """
    Arithmetic right shift (preserves sign bit).
    
    Args:
        a: Integer to shift
        n: Number of positions to shift
        width: Bit width (default: 32)
    
    Returns:
        a >> n (arithmetic shift)
    """
    value = int(a)
    shift = int(n)
    
    # Python's >> is already arithmetic for negative numbers
    return value >> shift


def bit_rol(a, n, width):
    """
    Rotate left (circular shift left).
    
    Args:
        a: Integer to rotate
        n: Number of positions to rotate
        width: Bit width (8, 16, 32, 64)
    
    Returns:
        Rotated value
    """
    value = int(a) & ((1 << width) - 1)  # Mask to width
    n = int(n) % width  # Normalize rotation count
    
    return ((value << n) | (value >> (width - n))) & ((1 << width) - 1)


def bit_ror(a, n, width):
    """
    Rotate right (circular shift right).
    
    Args:
        a: Integer to rotate
        n: Number of positions to rotate
        width: Bit width (8, 16, 32, 64)
    
    Returns:
        Rotated value
    """
    value = int(a) & ((1 << width) - 1)  # Mask to width
    n = int(n) % width  # Normalize rotation count
    
    return ((value >> n) | (value << (width - n))) & ((1 << width) - 1)


def bit_popcount(a):
    """
    Count the number of set bits (1s) - population count.
    
    Args:
        a: Integer
    
    Returns:
        Number of set bits
    """
    count = 0
    value = abs(int(a))
    
    while value:
        count += value & 1
        value >>= 1
    
    return count


def bit_clz(a, width=32):
    """
    Count leading zeros.
    
    Args:
        a: Integer (treated as unsigned)
        width: Bit width (default: 32)
    
    Returns:
        Number of leading zero bits
    """
    if a == 0:
        return width
    
    value = int(a) & ((1 << width) - 1)
    count = 0
    
    for i in range(width - 1, -1, -1):
        if value & (1 << i):
            break
        count += 1
    
    return count


def bit_ctz(a, width=32):
    """
    Count trailing zeros.
    
    Args:
        a: Integer (treated as unsigned)
        width: Bit width (default: 32)
    
    Returns:
        Number of trailing zero bits
    """
    if a == 0:
        return width
    
    value = int(a) & ((1 << width) - 1)
    count = 0
    
    for i in range(width):
        if value & (1 << i):
            break
        count += 1
    
    return count


def bit_set(a, n):
    """
    Set bit at position n to 1.
    
    Args:
        a: Integer
        n: Bit position (0-indexed)
    
    Returns:
        Integer with bit n set
    """
    return int(a) | (1 << int(n))


def bit_clear(a, n):
    """
    Clear bit at position n (set to 0).
    
    Args:
        a: Integer
        n: Bit position (0-indexed)
    
    Returns:
        Integer with bit n cleared
    """
    return int(a) & ~(1 << int(n))


def bit_toggle(a, n):
    """
    Toggle bit at position n.
    
    Args:
        a: Integer
        n: Bit position (0-indexed)
    
    Returns:
        Integer with bit n toggled
    """
    return int(a) ^ (1 << int(n))


def bit_test(a, n):
    """
    Test if bit at position n is set.
    
    Args:
        a: Integer
        n: Bit position (0-indexed)
    
    Returns:
        True if bit is set, False otherwise
    """
    return bool(int(a) & (1 << int(n)))


def bit_extract(a, start, length):
    """
    Extract bit field from value.
    
    Args:
        a: Integer
        start: Starting bit position
        length: Number of bits to extract
    
    Returns:
        Extracted bit field
    """
    mask = (1 << length) - 1
    return (int(a) >> start) & mask


def bit_insert(a, value, start, length):
    """
    Insert bit field into value.
    
    Args:
        a: Target integer
        value: Value to insert
        start: Starting bit position
        length: Number of bits
    
    Returns:
        Integer with inserted bit field
    """
    mask = (1 << length) - 1
    # Clear the field
    result = int(a) & ~(mask << start)
    # Insert the value
    result |= (int(value) & mask) << start
    return result


def bit_reverse(a, width):
    """
    Reverse bit order.
    
    Args:
        a: Integer
        width: Bit width
    
    Returns:
        Integer with reversed bit order
    """
    value = int(a) & ((1 << width) - 1)
    result = 0
    
    for i in range(width):
        if value & (1 << i):
            result |= 1 << (width - 1 - i)
    
    return result


def bit_parity(a):
    """
    Calculate bit parity (even or odd number of set bits).
    
    Args:
        a: Integer
    
    Returns:
        0 for even parity, 1 for odd parity
    """
    return bit_popcount(a) % 2


def bit_swap_bytes(a, width=32):
    """
    Swap byte order (endianness conversion).
    
    Args:
        a: Integer
        width: Bit width (must be multiple of 8)
    
    Returns:
        Integer with swapped byte order
    """
    if width % 8 != 0:
        raise ValueError("Width must be multiple of 8")
    
    value = int(a) & ((1 << width) - 1)
    result = 0
    num_bytes = width // 8
    
    for i in range(num_bytes):
        byte = (value >> (i * 8)) & 0xFF
        result |= byte << ((num_bytes - 1 - i) * 8)
    
    return result


def register_bit_ops_functions(runtime: Runtime) -> None:
    """Register bit manipulation functions with the runtime."""
    
    # Basic bitwise operations
    runtime.register_function("bit_and", bit_and)
    runtime.register_function("bit_or", bit_or)
    runtime.register_function("bit_xor", bit_xor)
    runtime.register_function("bit_not", bit_not)
    
    # Bit shifts
    runtime.register_function("bit_shl", bit_shl)  # Shift left
    runtime.register_function("bit_shr", bit_shr)  # Shift right (logical)
    runtime.register_function("bit_asr", bit_asr)  # Arithmetic shift right
    
    # Bit rotation
    runtime.register_function("bit_rol", bit_rol)  # Rotate left
    runtime.register_function("bit_ror", bit_ror)  # Rotate right
    
    # Bit counting
    runtime.register_function("bit_popcount", bit_popcount)  # Count set bits
    runtime.register_function("bit_clz", bit_clz)  # Count leading zeros
    runtime.register_function("bit_ctz", bit_ctz)  # Count trailing zeros
    
    # Bit manipulation
    runtime.register_function("bit_set", bit_set)  # Set bit
    runtime.register_function("bit_clear", bit_clear)  # Clear bit
    runtime.register_function("bit_toggle", bit_toggle)  # Toggle bit
    runtime.register_function("bit_test", bit_test)  # Test bit
    
    # Bit field operations
    runtime.register_function("bit_extract", bit_extract)  # Extract field
    runtime.register_function("bit_insert", bit_insert)  # Insert field
    
    # Advanced operations
    runtime.register_function("bit_reverse", bit_reverse)  # Reverse bits
    runtime.register_function("bit_parity", bit_parity)  # Bit parity
    runtime.register_function("bit_swap_bytes", bit_swap_bytes)  # Endian swap
