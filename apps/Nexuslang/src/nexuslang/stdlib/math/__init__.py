"""
Math module for the NexusLang standard library.
This module provides mathematical functions and constants.
"""

import math
from ...runtime.runtime import Runtime


def pct(part, total, decimals: int = 1) -> float:
    """Return part/total as a percentage (0-100). Returns 0.0 if total is 0."""
    if total == 0:
        return 0.0
    return round(float(part) / float(total) * 100.0, decimals)


def clamp(value, minimum, maximum):
    """Clamp value between minimum and maximum."""
    return max(minimum, min(maximum, value))


def lerp(a, b, t: float):
    """Linear interpolation between a and b by factor t (0..1)."""
    return a + (b - a) * float(t)

def register_math_functions(runtime: Runtime) -> None:
    """Register math functions with the runtime."""
    # Register constants
    runtime.register_constant("PI", math.pi)
    runtime.register_constant("E", math.e)
    runtime.register_constant("TAU", 2 * math.pi)
    runtime.register_constant("INF", math.inf)
    runtime.register_constant("NAN", math.nan)

    # Also register constants as zero-arg functions so invoke_function() can access them
    runtime.register_function("PI", lambda: math.pi)
    runtime.register_function("E", lambda: math.e)
    runtime.register_function("TAU", lambda: 2 * math.pi)
    runtime.register_function("INF", lambda: math.inf)
    runtime.register_function("NAN", lambda: math.nan)
    
    # Register basic arithmetic functions
    runtime.register_function("absolute", absolute)
    runtime.register_function("abs", absolute)  # Short alias
    runtime.register_function("square_root", sqrt)
    runtime.register_function("sqrt", sqrt)  # Short alias
    runtime.register_function("power", power)
    runtime.register_function("pow", power)  # Short alias
    runtime.register_function("pct", pct)
    runtime.register_function("percent", pct)
    runtime.register_function("clamp", clamp)
    runtime.register_function("lerp", lerp)
    runtime.register_function("floor", floor)
    runtime.register_function("ceiling", ceiling)
    runtime.register_function("ceil", ceiling)  # Short alias
    runtime.register_function("round", round_number)
    runtime.register_function("truncate", truncate)
    runtime.register_function("trunc", truncate)  # Short alias
    runtime.register_function("sign", sign)
    runtime.register_function("gcd", gcd)
    runtime.register_function("lcm", lcm)
    runtime.register_function("factorial", factorial)
    
    # Register trigonometric functions
    runtime.register_function("sine", sine)
    runtime.register_function("sin", sine)  # Short alias
    runtime.register_function("cosine", cosine)
    runtime.register_function("cos", cosine)  # Short alias
    runtime.register_function("tangent", tangent)
    runtime.register_function("tan", tangent)  # Short alias
    runtime.register_function("arcsine", arcsine)
    runtime.register_function("asin", arcsine)  # Short alias
    runtime.register_function("arccosine", arccosine)
    runtime.register_function("acos", arccosine)  # Short alias
    runtime.register_function("arctangent", arctangent)
    runtime.register_function("atan", arctangent)  # Short alias
    runtime.register_function("arctangent2", arctangent2)
    runtime.register_function("atan2", arctangent2)  # Short alias
    
    # Register angle conversion
    runtime.register_function("degrees", degrees)
    runtime.register_function("radians", radians)
    
    # Register hyperbolic functions
    runtime.register_function("sinh", sinh)
    runtime.register_function("cosh", cosh)
    runtime.register_function("tanh", tanh)
    
    # Register logarithmic functions
    runtime.register_function("logarithm", logarithm)
    runtime.register_function("log", logarithm)  # Short alias
    runtime.register_function("natural_logarithm", natural_logarithm)
    runtime.register_function("ln", natural_logarithm)  # Short alias
    runtime.register_function("log2", log2)
    runtime.register_function("log10", log10)
    runtime.register_function("exponential", exponential)
    runtime.register_function("exp", exponential)  # Short alias
    
    # Register statistical functions
    runtime.register_function("maximum", maximum)
    runtime.register_function("max", maximum)  # Short alias
    runtime.register_function("minimum", minimum)
    runtime.register_function("min", minimum)  # Short alias
    runtime.register_function("sum", sum_numbers)
    runtime.register_function("average", average)
    runtime.register_function("mean", average)  # Short alias
    
    # Register special checks
    runtime.register_function("is_nan", is_nan)
    runtime.register_function("is_infinite", is_infinite)
    runtime.register_function("is_finite", is_finite)

# Basic arithmetic functions
def absolute(x):
    """Return the absolute value of x."""
    return abs(x)

def sqrt(x):
    """Return the square root of x."""
    if x < 0:
        raise ValueError("Cannot take square root of a negative number")
    return math.sqrt(x)

def power(x, y):
    """Return x raised to the power of y."""
    return math.pow(x, y)

def floor(x):
    """Return the largest integer less than or equal to x."""
    return math.floor(x)

def ceiling(x):
    """Return the smallest integer greater than or equal to x."""
    return math.ceil(x)

def round_number(x, digits=0):
    """Round x to the specified number of decimal places."""
    return round(x, digits)

# Trigonometric functions
def sine(x):
    """Return the sine of x (measured in radians)."""
    return math.sin(x)

def cosine(x):
    """Return the cosine of x (measured in radians)."""
    return math.cos(x)

def tangent(x):
    """Return the tangent of x (measured in radians)."""
    return math.tan(x)

def arcsine(x):
    """Return the arcsine of x."""
    if x < -1 or x > 1:
        raise ValueError("Domain error: arcsine requires -1 <= x <= 1")
    return math.asin(x)

def arccosine(x):
    """Return the arccosine of x."""
    if x < -1 or x > 1:
        raise ValueError("Domain error: arccosine requires -1 <= x <= 1")
    return math.acos(x)

def arctangent(x):
    """Return the arctangent of x."""
    return math.atan(x)

# Logarithmic functions
def logarithm(x, base=10):
    """Return the logarithm of x to the given base."""
    if x <= 0:
        raise ValueError("Domain error: logarithm requires x > 0")
    return math.log(x, base)

def natural_logarithm(x):
    """Return the natural logarithm of x."""
    if x <= 0:
        raise ValueError("Domain error: natural logarithm requires x > 0")
    return math.log(x)

# Statistical functions
def maximum(*args):
    """Return the maximum value from the arguments."""
    if not args:
        raise ValueError("maximum() requires at least one argument")
    return max(args)

def minimum(*args):
    """Return the minimum value from the arguments."""
    if not args:
        raise ValueError("minimum() requires at least one argument")
    return min(args)

def sum_numbers(*args):
    """Return the sum of the arguments."""
    return sum(args)

def average(*args):
    """Return the average of the arguments.

    Accepts either a single list/tuple or multiple numeric arguments.
    """
    if not args:
        raise ValueError("average() requires at least one argument")
    # Handle single list/tuple argument: average([1, 2, 3])
    if len(args) == 1 and isinstance(args[0], (list, tuple)):
        vals = list(args[0])
    else:
        vals = list(args)
    if not vals:
        raise ValueError("average() requires at least one element")
    return sum(vals) / len(vals)

# Additional math functions

def truncate(x):
    """Truncate x to an integer (towards zero)."""
    return math.trunc(x)

def sign(x):
    """Return the sign of x (-1, 0, or 1)."""
    if x > 0:
        return 1
    elif x < 0:
        return -1
    else:
        return 0

def gcd(a, b):
    """Return the greatest common divisor of a and b."""
    return math.gcd(int(a), int(b))

def lcm(a, b):
    """Return the least common multiple of a and b."""
    return math.lcm(int(a), int(b))

def factorial(n):
    """Return the factorial of n."""
    if n < 0:
        raise ValueError("factorial() requires a non-negative integer")
    return math.factorial(int(n))

def arctangent2(y, x):
    """Return atan2(y, x) in radians."""
    return math.atan2(y, x)

def degrees(x):
    """Convert angle x from radians to degrees."""
    return math.degrees(x)

def radians(x):
    """Convert angle x from degrees to radians."""
    return math.radians(x)

def sinh(x):
    """Return the hyperbolic sine of x."""
    return math.sinh(x)

def cosh(x):
    """Return the hyperbolic cosine of x."""
    return math.cosh(x)

def tanh(x):
    """Return the hyperbolic tangent of x."""
    return math.tanh(x)

def log2(x):
    """Return the base-2 logarithm of x."""
    if x <= 0:
        raise ValueError("Domain error: log2 requires x > 0")
    return math.log2(x)

def log10(x):
    """Return the base-10 logarithm of x."""
    if x <= 0:
        raise ValueError("Domain error: log10 requires x > 0")
    return math.log10(x)

def exponential(x):
    """Return e raised to the power of x."""
    return math.exp(x)

def is_nan(x):
    """Check if x is NaN (Not a Number)."""
    return math.isnan(x)

def is_infinite(x):
    """Check if x is infinite."""
    return math.isinf(x)

def is_finite(x):
    """Check if x is finite (not infinite and not NaN)."""
    return math.isfinite(x)
