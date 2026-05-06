"""
Regular Expressions for NexusLang.
"""

import re
from typing import List, Optional, Any
from ...runtime.runtime import Runtime


def regex_match(pattern: str, text: str, flags: int = 0) -> bool:
    """Check if pattern matches text."""
    try:
        return re.search(pattern, text, flags) is not None
    except re.error as e:
        raise ValueError(f"Invalid regex pattern: {e}")


def regex_find(pattern: str, text: str, flags: int = 0) -> Optional[str]:
    """Find first match of pattern in text."""
    try:
        match = re.search(pattern, text, flags)
        return match.group(0) if match else None
    except re.error as e:
        raise ValueError(f"Invalid regex pattern: {e}")


def regex_find_all(pattern: str, text: str, flags: int = 0) -> List[str]:
    """Find all matches of pattern in text."""
    try:
        return re.findall(pattern, text, flags)
    except re.error as e:
        raise ValueError(f"Invalid regex pattern: {e}")


def regex_replace(pattern: str, replacement: str, text: str, count: int = 0, flags: int = 0) -> str:
    """Replace pattern matches with replacement string."""
    try:
        return re.sub(pattern, replacement, text, count=count, flags=flags)
    except re.error as e:
        raise ValueError(f"Invalid regex pattern: {e}")


def regex_split(pattern: str, text: str, maxsplit: int = 0, flags: int = 0) -> List[str]:
    """Split text by pattern."""
    try:
        return re.split(pattern, text, maxsplit=maxsplit, flags=flags)
    except re.error as e:
        raise ValueError(f"Invalid regex pattern: {e}")


def regex_groups(pattern: str, text: str, flags: int = 0) -> Optional[tuple]:
    """Get capture groups from first match."""
    try:
        match = re.search(pattern, text, flags)
        return match.groups() if match else None
    except re.error as e:
        raise ValueError(f"Invalid regex pattern: {e}")


def regex_find_iter(pattern: str, text: str, flags: int = 0) -> List[dict]:
    """Find all matches with position info."""
    try:
        matches = []
        for match in re.finditer(pattern, text, flags):
            matches.append({
                'match': match.group(0),
                'start': match.start(),
                'end': match.end(),
                'groups': match.groups()
            })
        return matches
    except re.error as e:
        raise ValueError(f"Invalid regex pattern: {e}")


def regex_escape(text: str) -> str:
    """Escape special regex characters in text."""
    return re.escape(text)


def regex_compile(pattern: str, flags: int = 0) -> Any:
    """Compile regex pattern for reuse."""
    try:
        return re.compile(pattern, flags)
    except re.error as e:
        raise ValueError(f"Invalid regex pattern: {e}")


# Regex flags constants
REGEX_IGNORECASE = re.IGNORECASE
REGEX_MULTILINE = re.MULTILINE
REGEX_DOTALL = re.DOTALL
REGEX_VERBOSE = re.VERBOSE
REGEX_ASCII = re.ASCII


def register_regex_functions(runtime: Runtime) -> None:
    """Register regex functions with the runtime."""
    runtime.register_function("regex_match", regex_match)
    runtime.register_function("regex_find", regex_find)
    runtime.register_function("regex_find_all", regex_find_all)
    runtime.register_function("regex_replace", regex_replace)
    runtime.register_function("regex_split", regex_split)
    runtime.register_function("regex_groups", regex_groups)
    runtime.register_function("regex_find_iter", regex_find_iter)
    runtime.register_function("regex_escape", regex_escape)
    runtime.register_function("regex_compile", regex_compile)
    
    # Regex flag constants
    runtime.constants["REGEX_IGNORECASE"] = REGEX_IGNORECASE
    runtime.constants["REGEX_MULTILINE"] = REGEX_MULTILINE
    runtime.constants["REGEX_DOTALL"] = REGEX_DOTALL
    runtime.constants["REGEX_VERBOSE"] = REGEX_VERBOSE
    runtime.constants["REGEX_ASCII"] = REGEX_ASCII
    
    # Short aliases (not overriding string stdlib's replace)
    runtime.register_function("match", regex_match)
    runtime.register_function("find", regex_find)
    runtime.register_function("find_all", regex_find_all)
    runtime.register_function("regex_replace", regex_replace)
