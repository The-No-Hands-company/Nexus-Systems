#!/usr/bin/env python3
"""
NLPL Error Code Explainer
==========================

Explains NexusLang error codes in detail, similar to `rustc --explain`.

Usage:
    python explain_error.py E001
    python explain_error.py E100
    nxl --explain E001
"""

import sys
from nexuslang.error_codes import get_error_info, list_all_error_codes, search_errors


def explain_error(code: str) -> None:
    """Explain a specific error code."""
    error_info = get_error_info(code)
    
    if error_info:
        print(error_info.format_help())
    else:
        print(f"Error code '{code}' not found.")
        print(f"\nAvailable error codes: {', '.join(list_all_error_codes()[:10])}...")
        print(f"Use 'nlpl --list-errors' to see all error codes.")


def list_errors() -> None:
    """List all error codes."""
    print("NLPL Error Codes")
    print("=" * 80)
    print()
    
    from nexuslang.error_codes import ERROR_CODES
    
    categories = {}
    for code, info in ERROR_CODES.items():
        if info.category not in categories:
            categories[info.category] = []
        categories[info.category].append((code, info.title))
    
    for category, errors in sorted(categories.items()):
        print(f"{category.upper()} ERRORS:")
        for code, title in sorted(errors):
            print(f"  {code}: {title}")
        print()


def search_error(query: str) -> None:
    """Search for errors matching query."""
    results = search_errors(query)
    
    if results:
        print(f"Found {len(results)} error(s) matching '{query}':")
        print()
        for error_info in results:
            print(f"{error_info.code}: {error_info.title}")
            print(f"  {error_info.description}")
            print()
    else:
        print(f"No errors found matching '{query}'")


def main():
    if len(sys.argv) < 2:
        print("Usage: python explain_error.py <error_code>")
        print("       python explain_error.py --list")
        print("       python explain_error.py --search <query>")
        sys.exit(1)
    
    arg = sys.argv[1]
    
    if arg == "--list":
        list_errors()
    elif arg == "--search":
        if len(sys.argv) < 3:
            print("Usage: python explain_error.py --search <query>")
            sys.exit(1)
        search_error(sys.argv[2])
    else:
        explain_error(arg.upper())


if __name__ == "__main__":
    main()
