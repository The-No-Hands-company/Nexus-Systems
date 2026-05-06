"""
Numeric limits and platform constants (C limits.h/float.h equivalent).

Provides system-dependent constants for numeric types.

Features:
- Integer limits (INT_MAX, LONG_MAX, etc.)
- Floating point limits (FLT_MAX, DBL_EPSILON, etc.)
- Character limits (CHAR_BIT, UCHAR_MAX)
- Platform information (word size, endianness)

Example usage in NexusLang:
    # Integer limits
    set max_int to limits_INT_MAX()
    set min_int to limits_INT_MIN()
    
    # Float limits
    set max_float to limits_FLT_MAX()
    set epsilon to limits_FLT_EPSILON()
    
    # Platform info
    set word_size to limits_get_word_size()
    set is_little_endian to limits_is_little_endian()
"""

from typing import Type
from ...runtime.runtime import Runtime
import sys
import struct


# Character limits
CHAR_BIT = 8
CHAR_MIN = -128
CHAR_MAX = 127
UCHAR_MAX = 255

# Short int limits (16-bit)
SHRT_MIN = -32768
SHRT_MAX = 32767
USHRT_MAX = 65535

# Integer limits (32-bit on most platforms)
INT_MIN = -2147483648
INT_MAX = 2147483647
UINT_MAX = 4294967295

# Long int limits (platform-dependent, typically 64-bit)
LONG_MIN = -9223372036854775808
LONG_MAX = 9223372036854775807
ULONG_MAX = 18446744073709551615

# Long long limits (64-bit)
LLONG_MIN = -9223372036854775808
LLONG_MAX = 9223372036854775807
ULLONG_MAX = 18446744073709551615

# Floating point limits (IEEE 754)
FLT_RADIX = 2
FLT_MANT_DIG = 24
FLT_DIG = 6
FLT_MIN_EXP = -125
FLT_MAX_EXP = 128
FLT_MIN_10_EXP = -37
FLT_MAX_10_EXP = 38

# Float values
FLT_EPSILON = 1.1920928955078125e-07
FLT_MIN = 1.1754943508222875e-38
FLT_MAX = 3.4028234663852886e+38

# Double limits
DBL_MANT_DIG = 53
DBL_DIG = 15
DBL_MIN_EXP = -1021
DBL_MAX_EXP = 1024
DBL_MIN_10_EXP = -307
DBL_MAX_10_EXP = 308

# Double values
DBL_EPSILON = 2.220446049250313e-16
DBL_MIN = 2.2250738585072014e-308
DBL_MAX = 1.7976931348623157e+308


def limits_CHAR_BIT():
    """Bits in a char."""
    return CHAR_BIT


def limits_CHAR_MIN():
    """Minimum value for a signed char."""
    return CHAR_MIN


def limits_CHAR_MAX():
    """Maximum value for a signed char."""
    return CHAR_MAX


def limits_UCHAR_MAX():
    """Maximum value for an unsigned char."""
    return UCHAR_MAX


def limits_SHRT_MIN():
    """Minimum value for a short int."""
    return SHRT_MIN


def limits_SHRT_MAX():
    """Maximum value for a short int."""
    return SHRT_MAX


def limits_USHRT_MAX():
    """Maximum value for an unsigned short."""
    return USHRT_MAX


def limits_INT_MIN():
    """Minimum value for an int."""
    return INT_MIN


def limits_INT_MAX():
    """Maximum value for an int."""
    return INT_MAX


def limits_UINT_MAX():
    """Maximum value for an unsigned int."""
    return UINT_MAX


def limits_LONG_MIN():
    """Minimum value for a long int."""
    return LONG_MIN


def limits_LONG_MAX():
    """Maximum value for a long int."""
    return LONG_MAX


def limits_ULONG_MAX():
    """Maximum value for an unsigned long."""
    return ULONG_MAX


def limits_LLONG_MIN():
    """Minimum value for a long long int."""
    return LLONG_MIN


def limits_LLONG_MAX():
    """Maximum value for a long long int."""
    return LLONG_MAX


def limits_ULLONG_MAX():
    """Maximum value for an unsigned long long."""
    return ULLONG_MAX


def limits_FLT_RADIX():
    """Radix of exponent representation."""
    return FLT_RADIX


def limits_FLT_MANT_DIG():
    """Number of base-FLT_RADIX digits in mantissa."""
    return FLT_MANT_DIG


def limits_FLT_DIG():
    """Number of decimal digits of precision."""
    return FLT_DIG


def limits_FLT_EPSILON():
    """Smallest number such that 1.0 + FLT_EPSILON != 1.0."""
    return FLT_EPSILON


def limits_FLT_MIN():
    """Minimum normalized positive float."""
    return FLT_MIN


def limits_FLT_MAX():
    """Maximum representable finite float."""
    return FLT_MAX


def limits_DBL_MANT_DIG():
    """Number of base-FLT_RADIX digits in double mantissa."""
    return DBL_MANT_DIG


def limits_DBL_DIG():
    """Number of decimal digits of precision for double."""
    return DBL_DIG


def limits_DBL_EPSILON():
    """Smallest number such that 1.0 + DBL_EPSILON != 1.0."""
    return DBL_EPSILON


def limits_DBL_MIN():
    """Minimum normalized positive double."""
    return DBL_MIN


def limits_DBL_MAX():
    """Maximum representable finite double."""
    return DBL_MAX


def limits_get_word_size():
    """
    Get platform word size in bits.
    
    Returns:
        32 or 64 (platform word size)
    """
    return 64 if sys.maxsize > 2**32 else 32


def limits_is_little_endian():
    """
    Check if platform is little endian.
    
    Returns:
        True if little endian, False if big endian
    """
    return sys.byteorder == 'little'


def limits_is_big_endian():
    """
    Check if platform is big endian.
    
    Returns:
        True if big endian, False if little endian
    """
    return sys.byteorder == 'big'


def limits_get_pointer_size():
    """
    Get pointer size in bytes.
    
    Returns:
        Pointer size (typically 4 or 8)
    """
    return struct.calcsize("P")


def limits_get_alignment(typename):
    """
    Get alignment requirement for type.
    
    Args:
        typename: Type name ('char', 'short', 'int', 'long', 'float', 'double', 'pointer')
    
    Returns:
        Alignment in bytes
    """
    alignments = {
        'char': struct.calcsize("c"),
        'short': struct.calcsize("h"),
        'int': struct.calcsize("i"),
        'long': struct.calcsize("l"),
        'longlong': struct.calcsize("q"),
        'float': struct.calcsize("f"),
        'double': struct.calcsize("d"),
        'pointer': struct.calcsize("P"),
    }
    
    return alignments.get(str(typename).lower(), 1)


def limits_get_size(typename):
    """
    Get size of type in bytes.
    
    Args:
        typename: Type name ('char', 'short', 'int', 'long', 'float', 'double', 'pointer')
    
    Returns:
        Size in bytes
    """
    sizes = {
        'char': 1,
        'short': 2,
        'int': 4,
        'long': 8,
        'longlong': 8,
        'float': 4,
        'double': 8,
        'pointer': struct.calcsize("P"),
    }
    
    return sizes.get(str(typename).lower(), 0)


def limits_get_all_limits():
    """
    Get all numeric limits as a dictionary.
    
    Returns:
        Dictionary with all limits
    """
    return {
        # Character limits
        "CHAR_BIT": CHAR_BIT,
        "CHAR_MIN": CHAR_MIN,
        "CHAR_MAX": CHAR_MAX,
        "UCHAR_MAX": UCHAR_MAX,
        
        # Short limits
        "SHRT_MIN": SHRT_MIN,
        "SHRT_MAX": SHRT_MAX,
        "USHRT_MAX": USHRT_MAX,
        
        # Integer limits
        "INT_MIN": INT_MIN,
        "INT_MAX": INT_MAX,
        "UINT_MAX": UINT_MAX,
        
        # Long limits
        "LONG_MIN": LONG_MIN,
        "LONG_MAX": LONG_MAX,
        "ULONG_MAX": ULONG_MAX,
        
        # Long long limits
        "LLONG_MIN": LLONG_MIN,
        "LLONG_MAX": LLONG_MAX,
        "ULLONG_MAX": ULLONG_MAX,
        
        # Float limits
        "FLT_RADIX": FLT_RADIX,
        "FLT_EPSILON": FLT_EPSILON,
        "FLT_MIN": FLT_MIN,
        "FLT_MAX": FLT_MAX,
        "FLT_DIG": FLT_DIG,
        
        # Double limits
        "DBL_EPSILON": DBL_EPSILON,
        "DBL_MIN": DBL_MIN,
        "DBL_MAX": DBL_MAX,
        "DBL_DIG": DBL_DIG,
        
        # Platform info
        "WORD_SIZE": limits_get_word_size(),
        "POINTER_SIZE": limits_get_pointer_size(),
        "IS_LITTLE_ENDIAN": limits_is_little_endian(),
    }


def register_limits_functions(runtime: Runtime) -> None:
    """Register limits functions with the runtime."""
    
    # Character limits
    runtime.register_function("limits_CHAR_BIT", limits_CHAR_BIT)
    runtime.register_function("limits_CHAR_MIN", limits_CHAR_MIN)
    runtime.register_function("limits_CHAR_MAX", limits_CHAR_MAX)
    runtime.register_function("limits_UCHAR_MAX", limits_UCHAR_MAX)
    
    # Short limits
    runtime.register_function("limits_SHRT_MIN", limits_SHRT_MIN)
    runtime.register_function("limits_SHRT_MAX", limits_SHRT_MAX)
    runtime.register_function("limits_USHRT_MAX", limits_USHRT_MAX)
    
    # Integer limits
    runtime.register_function("limits_INT_MIN", limits_INT_MIN)
    runtime.register_function("limits_INT_MAX", limits_INT_MAX)
    runtime.register_function("limits_UINT_MAX", limits_UINT_MAX)
    
    # Long limits
    runtime.register_function("limits_LONG_MIN", limits_LONG_MIN)
    runtime.register_function("limits_LONG_MAX", limits_LONG_MAX)
    runtime.register_function("limits_ULONG_MAX", limits_ULONG_MAX)
    
    # Long long limits
    runtime.register_function("limits_LLONG_MIN", limits_LLONG_MIN)
    runtime.register_function("limits_LLONG_MAX", limits_LLONG_MAX)
    runtime.register_function("limits_ULLONG_MAX", limits_ULLONG_MAX)
    
    # Float limits
    runtime.register_function("limits_FLT_RADIX", limits_FLT_RADIX)
    runtime.register_function("limits_FLT_MANT_DIG", limits_FLT_MANT_DIG)
    runtime.register_function("limits_FLT_DIG", limits_FLT_DIG)
    runtime.register_function("limits_FLT_EPSILON", limits_FLT_EPSILON)
    runtime.register_function("limits_FLT_MIN", limits_FLT_MIN)
    runtime.register_function("limits_FLT_MAX", limits_FLT_MAX)
    
    # Double limits
    runtime.register_function("limits_DBL_MANT_DIG", limits_DBL_MANT_DIG)
    runtime.register_function("limits_DBL_DIG", limits_DBL_DIG)
    runtime.register_function("limits_DBL_EPSILON", limits_DBL_EPSILON)
    runtime.register_function("limits_DBL_MIN", limits_DBL_MIN)
    runtime.register_function("limits_DBL_MAX", limits_DBL_MAX)
    
    # Platform information
    runtime.register_function("limits_get_word_size", limits_get_word_size)
    runtime.register_function("limits_is_little_endian", limits_is_little_endian)
    runtime.register_function("limits_is_big_endian", limits_is_big_endian)
    runtime.register_function("limits_get_pointer_size", limits_get_pointer_size)
    runtime.register_function("limits_get_alignment", limits_get_alignment)
    runtime.register_function("limits_get_size", limits_get_size)
    runtime.register_function("limits_get_all_limits", limits_get_all_limits)
