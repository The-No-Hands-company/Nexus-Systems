"""
Terminal color utilities for NexusLang error messages.
Uses colorama for cross-platform color support.
"""

import sys
from colorama import Fore, Back, Style, init

# Initialize colorama
init(autoreset=True)


def should_use_colors() -> bool:
    """Check if we should use colors (only for TTY)."""
    return sys.stdout.isatty()


def red(text: str) -> str:
    """Red text for errors."""
    if should_use_colors():
        return f"{Fore.RED}{text}{Style.RESET_ALL}"
    return text


def yellow(text: str) -> str:
    """Yellow text for warnings."""
    if should_use_colors():
        return f"{Fore.YELLOW}{text}{Style.RESET_ALL}"
    return text


def green(text: str) -> str:
    """Green text for success/suggestions."""
    if should_use_colors():
        return f"{Fore.GREEN}{text}{Style.RESET_ALL}"
    return text


def cyan(text: str) -> str:
    """Cyan text for code/identifiers."""
    if should_use_colors():
        return f"{Fore.CYAN}{text}{Style.RESET_ALL}"
    return text


def magenta(text: str) -> str:
    """Magenta text for types."""
    if should_use_colors():
        return f"{Fore.MAGENTA}{text}{Style.RESET_ALL}"
    return text


def bold(text: str) -> str:
    """Bold text for emphasis."""
    if should_use_colors():
        return f"{Style.BRIGHT}{text}{Style.RESET_ALL}"
    return text


def dim(text: str) -> str:
    """Dim text for less important info."""
    if should_use_colors():
        return f"{Style.DIM}{text}{Style.RESET_ALL}"
    return text
