"""
String manipulation functions for NexusLang.

Provides comprehensive string operations including:
- Transformation (upper, lower, capitalize, trim)
- Searching (contains, starts_with, ends_with, index_of)
- Splitting/Joining (split, split_lines, join)
- Inspection (length, is_empty, char_at)
"""

import re
from ...runtime.runtime import Runtime


def register_string_functions(runtime: Runtime) -> None:
    # Basic string operations
    runtime.register_function("length", length)
    runtime.register_function("len", length)  # Short alias
    runtime.register_function("substring", substring)
    runtime.register_function("concatenate", concatenate)
    runtime.register_function("str_concat", concatenate)  # Alias (concatenate is a reserved token)
    runtime.register_function("str_contains", contains)    # Alias (contains is a reserved token)
    runtime.register_function("str_starts_with", starts_with)  # Alias (starts_with/starts with are reserved)
    runtime.register_function("str_ends_with", ends_with)      # Alias (ends_with/ends with are reserved)
    runtime.register_function("str_split", split)  # Alias (split is a reserved token)
    runtime.register_function("str_join", join)    # Alias (join is a reserved token)

    # Case conversion
    runtime.register_function("uppercase", uppercase)
    runtime.register_function("lowercase", lowercase)
    runtime.register_function("capitalize", capitalize)
    runtime.register_function("title_case", title_case)

    # Trimming
    runtime.register_function("strip", strip)
    runtime.register_function("lstrip", lstrip)
    runtime.register_function("rstrip", rstrip)
    runtime.register_function("trim", trim)

    # Searching
    runtime.register_function("contains", contains)
    runtime.register_function("starts_with", starts_with)
    runtime.register_function("ends_with", ends_with)
    runtime.register_function("find", find)
    runtime.register_function("index_of", index_of)
    runtime.register_function("count_occurrences", count_occurrences)

    # Manipulation
    runtime.register_function("replace", replace)
    runtime.register_function("reverse", reverse)
    runtime.register_function("repeat", repeat)
    runtime.register_function("str_repeat", repeat)  # Alias for discoverability
    runtime.register_function("split", split)
    runtime.register_function("split_lines", split_lines)
    runtime.register_function("join", join)

    # String validation
    runtime.register_function("is_numeric", is_numeric)
    runtime.register_function("is_alphabetic", is_alphabetic)
    runtime.register_function("is_alphanumeric", is_alphanumeric)
    runtime.register_function("is_lowercase", is_lowercase)
    runtime.register_function("is_uppercase", is_uppercase)

    # Conversion
    runtime.register_function("to_string", to_string)
    runtime.register_function("str", to_string)  # Short alias

    # Regular expressions
    runtime.register_function("match", match)
    runtime.register_function("replace_regex", replace_regex)

    # Conversion functions
    runtime.register_function("to_number", to_number)
    runtime.register_function("to_int", to_int)
    runtime.register_function("to_float", to_float)
    runtime.register_function("int", to_int)
    runtime.register_function("float", to_float)
    runtime.register_function("number", to_number)
    runtime.register_function("split_string", split_string)
    runtime.register_function("str_split", split_string)
    runtime.register_function("trim_string", trim_string)

# Basic string operations
def length(obj):
    """Return the length of a string, list, dict, bytes, or any iterable.
    
    Args:
        obj: String, list, dict, bytes, tuple, set, or any object with __len__
        
    Returns:
        Length of the object
        
    Examples:
        length("hello") -> 5
        length([1, 2, 3]) -> 3
        length({"a": 1, "b": 2}) -> 2
        length(b"bytes") -> 5
    """
    try:
        return len(obj)
    except TypeError:
        raise TypeError(f"length() requires an object with length, got {type(obj).__name__}")

def concatenate(*args):
    """Concatenate multiple strings."""
    result = ""
    for arg in args:
        if not isinstance(arg, str):
            arg = str(arg)
        result += arg
    return result

def substring(s, start, end=None):
    """Return a substring from start to end (exclusive)."""
    if not isinstance(s, str):
        raise TypeError("substring() requires a string as first argument")
    
    if end is None:
        return s[start:]
    return s[start:end]

# Case conversion
def uppercase(s):
    """Convert string to uppercase."""
    if not isinstance(s, str):
        raise TypeError("uppercase() requires a string argument")
    return s.upper()

def lowercase(s):
    """Convert string to lowercase."""
    if not isinstance(s, str):
        raise TypeError("lowercase() requires a string argument")
    return s.lower()

def capitalize(s):
    """Capitalize the first letter of the string."""
    if not isinstance(s, str):
        raise TypeError("capitalize() requires a string argument")
    return s.capitalize()

# String searching
def contains(s, substring):
    """Check if string contains substring."""
    if not isinstance(s, str) or not isinstance(substring, str):
        raise TypeError("contains() requires string arguments")
    return substring in s

def starts_with(s, prefix):
    """Check if string starts with prefix."""
    if not isinstance(s, str) or not isinstance(prefix, str):
        raise TypeError("starts_with() requires string arguments")
    return s.startswith(prefix)

def ends_with(s, suffix):
    """Check if string ends with suffix."""
    if not isinstance(s, str) or not isinstance(suffix, str):
        raise TypeError("ends_with() requires string arguments")
    return s.endswith(suffix)

def find(s, substring, start=0, end=None):
    """Find the index of substring in string."""
    if not isinstance(s, str) or not isinstance(substring, str):
        raise TypeError("find() requires string arguments")
    
    if end is None:
        return s.find(substring, start)
    return s.find(substring, start, end)

# String manipulation
def replace(s, old, new, count=-1):
    """Replace occurrences of old with new in string."""
    if not isinstance(s, str):
        raise TypeError("replace() requires a string as first argument")
    return s.replace(old, new, count)

def trim(s):
    """Remove leading and trailing whitespace."""
    if not isinstance(s, str):
        raise TypeError("trim() requires a string argument")
    return s.strip()

def split(s, separator=None, maxsplit=-1):
    """Split string by separator."""
    if not isinstance(s, str):
        raise TypeError("split() requires a string as first argument")
    return s.split(separator, maxsplit)

def join(separator, iterable):
    """Join elements of iterable with separator."""
    if not isinstance(separator, str):
        raise TypeError("join() requires a string as first argument")
    return separator.join(str(item) for item in iterable)

# Regular expressions
def match(s, pattern):
    """Check if string matches pattern."""
    if not isinstance(s, str) or not isinstance(pattern, str):
        raise TypeError("match() requires string arguments")
    return bool(re.match(pattern, s))

def replace_regex(s, pattern, replacement, count=0):
    """Replace pattern with replacement in string."""
    if not isinstance(s, str):
        raise TypeError("replace_regex() requires a string as first argument")
    return re.sub(pattern, replacement, s, count=count)

# Additional string functions

def strip(s, chars=None):
    """Remove leading and trailing characters (default: whitespace)."""
    if not isinstance(s, str):
        raise TypeError("strip() requires a string argument")
    return s.strip(chars)

def lstrip(s, chars=None):
    """Remove leading characters (default: whitespace)."""
    if not isinstance(s, str):
        raise TypeError("lstrip() requires a string argument")
    return s.lstrip(chars)

def rstrip(s, chars=None):
    """Remove trailing characters (default: whitespace)."""
    if not isinstance(s, str):
        raise TypeError("rstrip() requires a string argument")
    return s.rstrip(chars)

def title_case(s):
    """Convert string to title case."""
    if not isinstance(s, str):
        raise TypeError("title_case() requires a string argument")
    return s.title()

def reverse(s):
    """Reverse a string."""
    if not isinstance(s, str):
        raise TypeError("reverse() requires a string argument")
    return s[::-1]

def repeat(s, count):
    """Repeat a string count times."""
    if not isinstance(s, str):
        raise TypeError("repeat() requires a string as first argument")
    return s * count

def count_occurrences(s, substring):
    """Count occurrences of substring in string."""
    if not isinstance(s, str) or not isinstance(substring, str):
        raise TypeError("count_occurrences() requires string arguments")
    return s.count(substring)

def index_of(s, substring):
    """Find the index of substring in string, return -1 if not found."""
    if not isinstance(s, str) or not isinstance(substring, str):
        raise TypeError("index_of() requires string arguments")
    try:
        return s.index(substring)
    except ValueError:
        return -1

def split_lines(s):
    """Split string into lines."""
    if not isinstance(s, str):
        raise TypeError("split_lines() requires a string argument")
    return s.splitlines()

def is_numeric(s):
    """Check if string contains only numeric characters."""
    if not isinstance(s, str):
        raise TypeError("is_numeric() requires a string argument")
    return s.isnumeric()

def is_alphabetic(s):
    """Check if string contains only alphabetic characters."""
    if not isinstance(s, str):
        raise TypeError("is_alphabetic() requires a string argument")
    return s.isalpha()

def is_alphanumeric(s):
    """Check if string contains only alphanumeric characters."""
    if not isinstance(s, str):
        raise TypeError("is_alphanumeric() requires a string argument")
    return s.isalnum()

def is_lowercase(s):
    """Check if all cased characters in string are lowercase."""
    if not isinstance(s, str):
        raise TypeError("is_lowercase() requires a string argument")
    return s.islower()

def is_uppercase(s):
    """Check if all cased characters in string are uppercase."""
    if not isinstance(s, str):
        raise TypeError("is_uppercase() requires a string argument")
    return s.isupper()

def to_string(value):
    """Convert any value to string representation."""
    return str(value)


def to_number(value) -> float:
    """Convert a string or other value to a number (float)."""
    try:
        return float(str(value).strip())
    except (ValueError, TypeError):
        return 0.0


def to_int(value) -> int:
    """Convert a value to an integer."""
    try:
        return int(float(str(value).strip()))
    except (ValueError, TypeError):
        return 0


def to_float(value) -> float:
    """Convert a value to a float."""
    return to_number(value)


def split_string(s: str, delimiter: str = " ") -> list:
    """Split string by delimiter. Alias for split()."""
    return str(s).split(delimiter)


def trim_string(s: str) -> str:
    """Strip leading/trailing whitespace. Alias for trim()."""
    return str(s).strip()

