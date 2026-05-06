"""
Character classification and conversion (C ctype.h equivalent).

Provides character type checking and case conversion functions.

Features:
- Character classification (isalpha, isdigit, etc.)
- Case conversion (toupper, tolower)
- Unicode-aware operations

Example usage in NexusLang:
    # Character classification
    set is_letter to char_isalpha with "A"
    set is_number to char_isdigit with "5"
    set is_alphanumeric to char_isalnum with "A5"
    
    # Case conversion
    set upper to char_toupper with "hello"
    set lower to char_tolower with "WORLD"
    
    # Whitespace checking
    set is_ws to char_isspace with " "
"""

from ...runtime.runtime import Runtime


def char_isalpha(char):
    """
    Check if character is alphabetic (a-z, A-Z).
    
    Args:
        char: Character or string (checks first character)
    
    Returns:
        True if alphabetic, False otherwise
    """
    if not char:
        return False
    
    c = str(char)[0]
    return c.isalpha()


def char_isdigit(char):
    """
    Check if character is a decimal digit (0-9).
    
    Args:
        char: Character or string
    
    Returns:
        True if digit, False otherwise
    """
    if not char:
        return False
    
    c = str(char)[0]
    return c.isdigit()


def char_isalnum(char):
    """
    Check if character is alphanumeric (letter or digit).
    
    Args:
        char: Character or string
    
    Returns:
        True if alphanumeric, False otherwise
    """
    if not char:
        return False
    
    c = str(char)[0]
    return c.isalnum()


def char_isspace(char):
    """
    Check if character is whitespace (space, tab, newline, etc.).
    
    Args:
        char: Character or string
    
    Returns:
        True if whitespace, False otherwise
    """
    if not char:
        return False
    
    c = str(char)[0]
    return c.isspace()


def char_isupper(char):
    """
    Check if character is uppercase letter.
    
    Args:
        char: Character or string
    
    Returns:
        True if uppercase, False otherwise
    """
    if not char:
        return False
    
    c = str(char)[0]
    return c.isupper()


def char_islower(char):
    """
    Check if character is lowercase letter.
    
    Args:
        char: Character or string
    
    Returns:
        True if lowercase, False otherwise
    """
    if not char:
        return False
    
    c = str(char)[0]
    return c.islower()


def char_isprint(char):
    """
    Check if character is printable (not control character).
    
    Args:
        char: Character or string
    
    Returns:
        True if printable, False otherwise
    """
    if not char:
        return False
    
    c = str(char)[0]
    return c.isprintable()


def char_ispunct(char):
    """
    Check if character is punctuation.
    
    Args:
        char: Character or string
    
    Returns:
        True if punctuation, False otherwise
    """
    if not char:
        return False
    
    c = str(char)[0]
    # Punctuation: printable but not alphanumeric or space
    return c.isprintable() and not c.isalnum() and not c.isspace()


def char_iscntrl(char):
    """
    Check if character is control character.
    
    Args:
        char: Character or string
    
    Returns:
        True if control character, False otherwise
    """
    if not char:
        return False
    
    c = str(char)[0]
    # Control characters are not printable (except space)
    return not c.isprintable() or ord(c) < 32 or ord(c) == 127


def char_isxdigit(char):
    """
    Check if character is hexadecimal digit (0-9, a-f, A-F).
    
    Args:
        char: Character or string
    
    Returns:
        True if hex digit, False otherwise
    """
    if not char:
        return False
    
    c = str(char)[0]
    return c in '0123456789abcdefABCDEF'


def char_isascii(char):
    """
    Check if character is ASCII (0-127).
    
    Args:
        char: Character or string
    
    Returns:
        True if ASCII, False otherwise
    """
    if not char:
        return False
    
    c = str(char)[0]
    return ord(c) < 128


def char_toupper(text):
    """
    Convert string to uppercase.
    
    Args:
        text: String to convert
    
    Returns:
        Uppercase string
    """
    return str(text).upper()


def char_tolower(text):
    """
    Convert string to lowercase.
    
    Args:
        text: String to convert
    
    Returns:
        Lowercase string
    """
    return str(text).lower()


def char_toascii(char):
    """
    Convert character to ASCII (mask to 7 bits).
    
    Args:
        char: Character or string
    
    Returns:
        ASCII value (0-127)
    """
    if not char:
        return 0
    
    c = str(char)[0]
    return ord(c) & 0x7F


def char_ord(char):
    """
    Get Unicode code point of character.
    
    Args:
        char: Character or string
    
    Returns:
        Unicode code point (integer)
    """
    if not char:
        return 0
    
    return ord(str(char)[0])


def char_chr(code):
    """
    Create character from Unicode code point.
    
    Args:
        code: Unicode code point (integer)
    
    Returns:
        Character string
    """
    try:
        return chr(int(code))
    except (ValueError, OverflowError):
        return ""


def register_ctype_functions(runtime: Runtime) -> None:
    """Register character classification functions with the runtime."""
    
    # Character classification
    runtime.register_function("char_isalpha", char_isalpha)
    runtime.register_function("char_isdigit", char_isdigit)
    runtime.register_function("char_isalnum", char_isalnum)
    runtime.register_function("char_isspace", char_isspace)
    runtime.register_function("char_isupper", char_isupper)
    runtime.register_function("char_islower", char_islower)
    runtime.register_function("char_isprint", char_isprint)
    runtime.register_function("char_ispunct", char_ispunct)
    runtime.register_function("char_iscntrl", char_iscntrl)
    runtime.register_function("char_isxdigit", char_isxdigit)
    runtime.register_function("char_isascii", char_isascii)
    
    # Case conversion
    runtime.register_function("char_toupper", char_toupper)
    runtime.register_function("char_tolower", char_tolower)
    
    # ASCII conversion
    runtime.register_function("char_toascii", char_toascii)
    
    # Character code conversion
    runtime.register_function("char_ord", char_ord)
    runtime.register_function("char_chr", char_chr)
