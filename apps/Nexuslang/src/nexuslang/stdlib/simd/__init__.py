"""
SIMD (Single Instruction, Multiple Data) Operations Module

Provides vector operations inspired by x86 SIMD instruction sets:
- MMX: 64-bit packed integers
- SSE: 128-bit packed integers and floats
- AVX: 256-bit packed integers and floats

This module emulates SIMD operations using Python lists to represent
packed data vectors. Each operation processes multiple data elements
in parallel (conceptually).

Vector Formats:
- i8: 8-bit signed integers
- i16: 16-bit signed integers
- i32: 32-bit signed integers
- i64: 64-bit signed integers
- f32: 32-bit floats
- f64: 64-bit floats

Register Sizes:
- MMX: 64-bit (8 bytes)
- SSE: 128-bit (16 bytes)
- AVX: 256-bit (32 bytes)
"""

from ...runtime.runtime import Runtime
import math
import struct


# Vector arithmetic operations
def simd_add(runtime: Runtime, vector_a, vector_b):
    """Add two vectors element-wise.
    
    Example: simd_add([1, 2, 3], [4, 5, 6]) -> [5, 7, 9]
    """
    if not isinstance(vector_a, list) or not isinstance(vector_b, list):
        raise TypeError("Both arguments must be lists (vectors)")
    
    if len(vector_a) != len(vector_b):
        raise ValueError(f"Vector length mismatch: {len(vector_a)} vs {len(vector_b)}")
    
    return [a + b for a, b in zip(vector_a, vector_b)]


def simd_sub(runtime: Runtime, vector_a, vector_b):
    """Subtract two vectors element-wise.
    
    Example: simd_sub([10, 20, 30], [3, 5, 7]) -> [7, 15, 23]
    """
    if not isinstance(vector_a, list) or not isinstance(vector_b, list):
        raise TypeError("Both arguments must be lists (vectors)")
    
    if len(vector_a) != len(vector_b):
        raise ValueError(f"Vector length mismatch: {len(vector_a)} vs {len(vector_b)}")
    
    return [a - b for a, b in zip(vector_a, vector_b)]


def simd_mul(runtime: Runtime, vector_a, vector_b):
    """Multiply two vectors element-wise.
    
    Example: simd_mul([2, 3, 4], [5, 6, 7]) -> [10, 18, 28]
    """
    if not isinstance(vector_a, list) or not isinstance(vector_b, list):
        raise TypeError("Both arguments must be lists (vectors)")
    
    if len(vector_a) != len(vector_b):
        raise ValueError(f"Vector length mismatch: {len(vector_a)} vs {len(vector_b)}")
    
    return [a * b for a, b in zip(vector_a, vector_b)]


def simd_div(runtime: Runtime, vector_a, vector_b):
    """Divide two vectors element-wise.
    
    Example: simd_div([10, 20, 30], [2, 4, 5]) -> [5.0, 5.0, 6.0]
    """
    if not isinstance(vector_a, list) or not isinstance(vector_b, list):
        raise TypeError("Both arguments must be lists (vectors)")
    
    if len(vector_a) != len(vector_b):
        raise ValueError(f"Vector length mismatch: {len(vector_a)} vs {len(vector_b)}")
    
    result = []
    for a, b in zip(vector_a, vector_b):
        if b == 0:
            raise ZeroDivisionError("Division by zero in SIMD operation")
        result.append(a / b)
    
    return result


# Vector logical operations
def simd_and(runtime: Runtime, vector_a, vector_b):
    """Bitwise AND of two integer vectors.
    
    Example: simd_and([0xFF, 0xAA], [0x0F, 0x55]) -> [15, 0]
    """
    if not isinstance(vector_a, list) or not isinstance(vector_b, list):
        raise TypeError("Both arguments must be lists (vectors)")
    
    if len(vector_a) != len(vector_b):
        raise ValueError(f"Vector length mismatch: {len(vector_a)} vs {len(vector_b)}")
    
    return [int(a) & int(b) for a, b in zip(vector_a, vector_b)]


def simd_or(runtime: Runtime, vector_a, vector_b):
    """Bitwise OR of two integer vectors.
    
    Example: simd_or([0x0F, 0x00], [0xF0, 0xFF]) -> [255, 255]
    """
    if not isinstance(vector_a, list) or not isinstance(vector_b, list):
        raise TypeError("Both arguments must be lists (vectors)")
    
    if len(vector_a) != len(vector_b):
        raise ValueError(f"Vector length mismatch: {len(vector_a)} vs {len(vector_b)}")
    
    return [int(a) | int(b) for a, b in zip(vector_a, vector_b)]


def simd_xor(runtime: Runtime, vector_a, vector_b):
    """Bitwise XOR of two integer vectors.
    
    Example: simd_xor([0xFF, 0xAA], [0x0F, 0xAA]) -> [240, 0]
    """
    if not isinstance(vector_a, list) or not isinstance(vector_b, list):
        raise TypeError("Both arguments must be lists (vectors)")
    
    if len(vector_a) != len(vector_b):
        raise ValueError(f"Vector length mismatch: {len(vector_a)} vs {len(vector_b)}")
    
    return [int(a) ^ int(b) for a, b in zip(vector_a, vector_b)]


def simd_not(runtime: Runtime, vector, width=32):
    """Bitwise NOT of an integer vector with specified bit width.
    
    Example: simd_not([0, 15], width=8) -> [255, 240]
    """
    if not isinstance(vector, list):
        raise TypeError("Argument must be a list (vector)")
    
    width = int(width)
    if width not in [8, 16, 32, 64]:
        raise ValueError("Width must be 8, 16, 32, or 64")
    
    mask = (1 << width) - 1
    return [(~int(v)) & mask for v in vector]


# Vector comparison operations
def simd_min(runtime: Runtime, vector_a, vector_b):
    """Element-wise minimum of two vectors.
    
    Example: simd_min([1, 5, 3], [2, 4, 6]) -> [1, 4, 3]
    """
    if not isinstance(vector_a, list) or not isinstance(vector_b, list):
        raise TypeError("Both arguments must be lists (vectors)")
    
    if len(vector_a) != len(vector_b):
        raise ValueError(f"Vector length mismatch: {len(vector_a)} vs {len(vector_b)}")
    
    return [min(a, b) for a, b in zip(vector_a, vector_b)]


def simd_max(runtime: Runtime, vector_a, vector_b):
    """Element-wise maximum of two vectors.
    
    Example: simd_max([1, 5, 3], [2, 4, 6]) -> [2, 5, 6]
    """
    if not isinstance(vector_a, list) or not isinstance(vector_b, list):
        raise TypeError("Both arguments must be lists (vectors)")
    
    if len(vector_a) != len(vector_b):
        raise ValueError(f"Vector length mismatch: {len(vector_a)} vs {len(vector_b)}")
    
    return [max(a, b) for a, b in zip(vector_a, vector_b)]


def simd_cmp_eq(runtime: Runtime, vector_a, vector_b):
    """Element-wise equality comparison (returns 1 for true, 0 for false).
    
    Example: simd_cmp_eq([1, 2, 3], [1, 5, 3]) -> [1, 0, 1]
    """
    if not isinstance(vector_a, list) or not isinstance(vector_b, list):
        raise TypeError("Both arguments must be lists (vectors)")
    
    if len(vector_a) != len(vector_b):
        raise ValueError(f"Vector length mismatch: {len(vector_a)} vs {len(vector_b)}")
    
    return [1 if a == b else 0 for a, b in zip(vector_a, vector_b)]


def simd_cmp_gt(runtime: Runtime, vector_a, vector_b):
    """Element-wise greater-than comparison (returns 1 for true, 0 for false).
    
    Example: simd_cmp_gt([5, 2, 7], [3, 4, 6]) -> [1, 0, 1]
    """
    if not isinstance(vector_a, list) or not isinstance(vector_b, list):
        raise TypeError("Both arguments must be lists (vectors)")
    
    if len(vector_a) != len(vector_b):
        raise ValueError(f"Vector length mismatch: {len(vector_a)} vs {len(vector_b)}")
    
    return [1 if a > b else 0 for a, b in zip(vector_a, vector_b)]


def simd_cmp_lt(runtime: Runtime, vector_a, vector_b):
    """Element-wise less-than comparison (returns 1 for true, 0 for false).
    
    Example: simd_cmp_lt([3, 5, 2], [4, 4, 1]) -> [1, 0, 0]
    """
    if not isinstance(vector_a, list) or not isinstance(vector_b, list):
        raise TypeError("Both arguments must be lists (vectors)")
    
    if len(vector_a) != len(vector_b):
        raise ValueError(f"Vector length mismatch: {len(vector_a)} vs {len(vector_b)}")
    
    return [1 if a < b else 0 for a, b in zip(vector_a, vector_b)]


# Vector math operations
def simd_sqrt(runtime: Runtime, vector):
    """Element-wise square root.
    
    Example: simd_sqrt([4, 9, 16, 25]) -> [2.0, 3.0, 4.0, 5.0]
    """
    if not isinstance(vector, list):
        raise TypeError("Argument must be a list (vector)")
    
    result = []
    for v in vector:
        if v < 0:
            raise ValueError(f"Cannot compute square root of negative number: {v}")
        result.append(math.sqrt(v))
    
    return result


def simd_abs(runtime: Runtime, vector):
    """Element-wise absolute value.
    
    Example: simd_abs([-1, 2, -3, 4]) -> [1, 2, 3, 4]
    """
    if not isinstance(vector, list):
        raise TypeError("Argument must be a list (vector)")
    
    return [abs(v) for v in vector]


def simd_neg(runtime: Runtime, vector):
    """Element-wise negation.
    
    Example: simd_neg([1, -2, 3, -4]) -> [-1, 2, -3, 4]
    """
    if not isinstance(vector, list):
        raise TypeError("Argument must be a list (vector)")
    
    return [-v for v in vector]


# Horizontal operations (reduce across vector)
def simd_hadd(runtime: Runtime, vector):
    """Horizontal add - sum all elements in vector.
    
    Example: simd_hadd([1, 2, 3, 4]) -> 10
    """
    if not isinstance(vector, list):
        raise TypeError("Argument must be a list (vector)")
    
    if not vector:
        return 0
    
    return sum(vector)


def simd_hmul(runtime: Runtime, vector):
    """Horizontal multiply - product of all elements.
    
    Example: simd_hmul([2, 3, 4]) -> 24
    """
    if not isinstance(vector, list):
        raise TypeError("Argument must be a list (vector)")
    
    if not vector:
        return 1
    
    result = 1
    for v in vector:
        result *= v
    
    return result


def simd_hmin(runtime: Runtime, vector):
    """Horizontal minimum - smallest element.
    
    Example: simd_hmin([5, 2, 8, 1, 9]) -> 1
    """
    if not isinstance(vector, list):
        raise TypeError("Argument must be a list (vector)")
    
    if not vector:
        raise ValueError("Cannot find minimum of empty vector")
    
    return min(vector)


def simd_hmax(runtime: Runtime, vector):
    """Horizontal maximum - largest element.
    
    Example: simd_hmax([5, 2, 8, 1, 9]) -> 9
    """
    if not isinstance(vector, list):
        raise TypeError("Argument must be a list (vector)")
    
    if not vector:
        raise ValueError("Cannot find maximum of empty vector")
    
    return max(vector)


# Vector manipulation operations
def simd_shuffle(runtime: Runtime, vector, indices):
    """Shuffle vector elements according to index list.
    
    Example: simd_shuffle([10, 20, 30, 40], [3, 1, 2, 0]) -> [40, 20, 30, 10]
    """
    if not isinstance(vector, list):
        raise TypeError("First argument must be a list (vector)")
    
    if not isinstance(indices, list):
        raise TypeError("Second argument must be a list (indices)")
    
    result = []
    for idx in indices:
        idx = int(idx)
        if idx < 0 or idx >= len(vector):
            raise IndexError(f"Shuffle index {idx} out of range [0, {len(vector)-1}]")
        result.append(vector[idx])
    
    return result


def simd_broadcast(runtime: Runtime, value, count):
    """Broadcast a scalar value to create a vector.
    
    Example: simd_broadcast(42, 4) -> [42, 42, 42, 42]
    """
    count = int(count)
    if count < 1:
        raise ValueError("Count must be at least 1")
    
    return [value] * count


def simd_splat(runtime: Runtime, vector, index):
    """Splat (replicate) element at index across entire vector.
    
    Example: simd_splat([10, 20, 30, 40], 2) -> [30, 30, 30, 30]
    """
    if not isinstance(vector, list):
        raise TypeError("First argument must be a list (vector)")
    
    index = int(index)
    if index < 0 or index >= len(vector):
        raise IndexError(f"Index {index} out of range [0, {len(vector)-1}]")
    
    value = vector[index]
    return [value] * len(vector)


def simd_extract(runtime: Runtime, vector, index):
    """Extract single element from vector.
    
    Example: simd_extract([10, 20, 30, 40], 2) -> 30
    """
    if not isinstance(vector, list):
        raise TypeError("First argument must be a list (vector)")
    
    index = int(index)
    if index < 0 or index >= len(vector):
        raise IndexError(f"Index {index} out of range [0, {len(vector)-1}]")
    
    return vector[index]


def simd_insert(runtime: Runtime, vector, index, value):
    """Insert value at index in vector (returns new vector).
    
    Example: simd_insert([10, 20, 30, 40], 2, 99) -> [10, 20, 99, 40]
    """
    if not isinstance(vector, list):
        raise TypeError("First argument must be a list (vector)")
    
    index = int(index)
    if index < 0 or index >= len(vector):
        raise IndexError(f"Index {index} out of range [0, {len(vector)-1}]")
    
    result = vector.copy()
    result[index] = value
    return result


# Vector packing/unpacking
def simd_pack_i8(runtime: Runtime, *values):
    """Pack values into 8-bit signed integer vector.
    
    Example: simd_pack_i8(10, 20, -5, 127) -> [10, 20, -5, 127]
    """
    result = []
    for v in values:
        v = int(v)
        # Clamp to i8 range [-128, 127]
        if v < -128:
            v = -128
        elif v > 127:
            v = 127
        result.append(v)
    
    return result


def simd_pack_i16(runtime: Runtime, *values):
    """Pack values into 16-bit signed integer vector.
    
    Example: simd_pack_i16(1000, -2000, 32767) -> [1000, -2000, 32767]
    """
    result = []
    for v in values:
        v = int(v)
        # Clamp to i16 range [-32768, 32767]
        if v < -32768:
            v = -32768
        elif v > 32767:
            v = 32767
        result.append(v)
    
    return result


def simd_pack_i32(runtime: Runtime, *values):
    """Pack values into 32-bit signed integer vector.
    
    Example: simd_pack_i32(100000, -200000, 2147483647)
    """
    result = []
    for v in values:
        v = int(v)
        # Clamp to i32 range
        if v < -2147483648:
            v = -2147483648
        elif v > 2147483647:
            v = 2147483647
        result.append(v)
    
    return result


def simd_pack_f32(runtime: Runtime, *values):
    """Pack values into 32-bit float vector.
    
    Example: simd_pack_f32(1.5, 2.7, -3.14) -> [1.5, 2.7, -3.14]
    """
    return [float(v) for v in values]


def simd_pack_f64(runtime: Runtime, *values):
    """Pack values into 64-bit double vector.
    
    Example: simd_pack_f64(1.5, 2.7, -3.14) -> [1.5, 2.7, -3.14]
    """
    return [float(v) for v in values]


# Vector conversion operations
def simd_cvt_i32_f32(runtime: Runtime, vector):
    """Convert 32-bit integer vector to 32-bit float vector.
    
    Example: simd_cvt_i32_f32([1, 2, 3]) -> [1.0, 2.0, 3.0]
    """
    if not isinstance(vector, list):
        raise TypeError("Argument must be a list (vector)")
    
    return [float(int(v)) for v in vector]


def simd_cvt_f32_i32(runtime: Runtime, vector):
    """Convert 32-bit float vector to 32-bit integer vector (truncate).
    
    Example: simd_cvt_f32_i32([1.9, 2.1, -3.7]) -> [1, 2, -3]
    """
    if not isinstance(vector, list):
        raise TypeError("Argument must be a list (vector)")
    
    return [int(float(v)) for v in vector]


# Vector load/store operations
def simd_load(runtime: Runtime, data, offset=0, count=None):
    """Load data into vector from list/array starting at offset.
    
    Example: simd_load([10, 20, 30, 40, 50], 1, 3) -> [20, 30, 40]
    """
    if not isinstance(data, list):
        raise TypeError("Data must be a list")
    
    offset = int(offset)
    if offset < 0 or offset >= len(data):
        raise IndexError(f"Offset {offset} out of range [0, {len(data)-1}]")
    
    if count is None:
        return data[offset:]
    
    count = int(count)
    if count < 1:
        raise ValueError("Count must be at least 1")
    
    end = min(offset + count, len(data))
    return data[offset:end]


def simd_store(runtime: Runtime, vector, data, offset=0):
    """Store vector into list/array starting at offset.
    
    Example: simd_store([99, 88], [1, 2, 3, 4, 5], 2) -> [1, 2, 99, 88, 5]
    """
    if not isinstance(vector, list):
        raise TypeError("Vector must be a list")
    
    if not isinstance(data, list):
        raise TypeError("Data must be a list")
    
    offset = int(offset)
    if offset < 0 or offset >= len(data):
        raise IndexError(f"Offset {offset} out of range [0, {len(data)-1}]")
    
    result = data.copy()
    for i, v in enumerate(vector):
        if offset + i >= len(result):
            break
        result[offset + i] = v
    
    return result


# Vector alignment and padding
def simd_align(runtime: Runtime, vector, alignment):
    """Pad vector to specified alignment (power of 2).
    
    Example: simd_align([1, 2, 3], 4) -> [1, 2, 3, 0]
    """
    if not isinstance(vector, list):
        raise TypeError("Vector must be a list")
    
    alignment = int(alignment)
    if alignment < 1 or (alignment & (alignment - 1)) != 0:
        raise ValueError("Alignment must be a power of 2")
    
    current_len = len(vector)
    aligned_len = ((current_len + alignment - 1) // alignment) * alignment
    
    if aligned_len == current_len:
        return vector.copy()
    
    result = vector.copy()
    result.extend([0] * (aligned_len - current_len))
    return result


# Vector utility functions
def simd_length(runtime: Runtime, vector):
    """Get number of elements in vector.
    
    Example: simd_length([1, 2, 3, 4]) -> 4
    """
    if not isinstance(vector, list):
        raise TypeError("Argument must be a list (vector)")
    
    return len(vector)


def simd_reverse(runtime: Runtime, vector):
    """Reverse elements in vector.
    
    Example: simd_reverse([1, 2, 3, 4]) -> [4, 3, 2, 1]
    """
    if not isinstance(vector, list):
        raise TypeError("Argument must be a list (vector)")
    
    return list(reversed(vector))


def simd_zip(runtime: Runtime, vector_a, vector_b):
    """Interleave elements from two vectors.
    
    Example: simd_zip([1, 2, 3], [10, 20, 30]) -> [1, 10, 2, 20, 3, 30]
    """
    if not isinstance(vector_a, list) or not isinstance(vector_b, list):
        raise TypeError("Both arguments must be lists (vectors)")
    
    result = []
    for a, b in zip(vector_a, vector_b):
        result.append(a)
        result.append(b)
    
    return result


def simd_unzip(runtime: Runtime, vector):
    """Deinterleave vector into two vectors (even/odd indices).
    
    Example: simd_unzip([1, 10, 2, 20, 3, 30]) -> {"even": [1, 2, 3], "odd": [10, 20, 30]}
    """
    if not isinstance(vector, list):
        raise TypeError("Argument must be a list (vector)")
    
    even = []
    odd = []
    
    for i, v in enumerate(vector):
        if i % 2 == 0:
            even.append(v)
        else:
            odd.append(v)
    
    return {"even": even, "odd": odd}


def register_simd_functions(runtime: Runtime) -> None:
    """Register SIMD functions with the runtime."""
    # Arithmetic operations
    runtime.register_function("simd_add", simd_add)
    runtime.register_function("simd_sub", simd_sub)
    runtime.register_function("simd_mul", simd_mul)
    runtime.register_function("simd_div", simd_div)
    
    # Logical operations
    runtime.register_function("simd_and", simd_and)
    runtime.register_function("simd_or", simd_or)
    runtime.register_function("simd_xor", simd_xor)
    runtime.register_function("simd_not", simd_not)
    
    # Comparison operations
    runtime.register_function("simd_min", simd_min)
    runtime.register_function("simd_max", simd_max)
    runtime.register_function("simd_cmp_eq", simd_cmp_eq)
    runtime.register_function("simd_cmp_gt", simd_cmp_gt)
    runtime.register_function("simd_cmp_lt", simd_cmp_lt)
    
    # Math operations
    runtime.register_function("simd_sqrt", simd_sqrt)
    runtime.register_function("simd_abs", simd_abs)
    runtime.register_function("simd_neg", simd_neg)
    
    # Horizontal operations
    runtime.register_function("simd_hadd", simd_hadd)
    runtime.register_function("simd_hmul", simd_hmul)
    runtime.register_function("simd_hmin", simd_hmin)
    runtime.register_function("simd_hmax", simd_hmax)
    
    # Manipulation operations
    runtime.register_function("simd_shuffle", simd_shuffle)
    runtime.register_function("simd_broadcast", simd_broadcast)
    runtime.register_function("simd_splat", simd_splat)
    runtime.register_function("simd_extract", simd_extract)
    runtime.register_function("simd_insert", simd_insert)
    
    # Packing operations
    runtime.register_function("simd_pack_i8", simd_pack_i8)
    runtime.register_function("simd_pack_i16", simd_pack_i16)
    runtime.register_function("simd_pack_i32", simd_pack_i32)
    runtime.register_function("simd_pack_f32", simd_pack_f32)
    runtime.register_function("simd_pack_f64", simd_pack_f64)
    
    # Conversion operations
    runtime.register_function("simd_cvt_i32_f32", simd_cvt_i32_f32)
    runtime.register_function("simd_cvt_f32_i32", simd_cvt_f32_i32)
    
    # Load/store operations
    runtime.register_function("simd_load", simd_load)
    runtime.register_function("simd_store", simd_store)
    
    # Utility operations
    runtime.register_function("simd_align", simd_align)
    runtime.register_function("simd_length", simd_length)
    runtime.register_function("simd_reverse", simd_reverse)
    runtime.register_function("simd_zip", simd_zip)
    runtime.register_function("simd_unzip", simd_unzip)
