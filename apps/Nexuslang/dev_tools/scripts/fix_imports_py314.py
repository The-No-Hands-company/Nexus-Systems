#!/usr/bin/env python3
"""
Fix Python 3.14 Import Bug

Replace relative imports with absolute imports to avoid Python 3.14's
broken import system that hangs on package imports.
"""

import os
import re
from pathlib import Path

def fix_relative_imports(file_path):
    """Convert relative imports to absolute imports in a Python file."""
    with open(file_path, 'r', encoding='utf-8') as f:
        content = f.read()
    
    original_content = content
    
    # Pattern to match relative imports
    # Examples: "from ..module import X" -> "from nexuslang.module import X"
    #           "from ..parser.lexer import X" -> "from nexuslang.parser.lexer import X"
    
    # Replace ..xxx with nlpl.xxx
    content = re.sub(r'^from \.\.([\w.]+) import', r'from nexuslang.\1 import', content, flags=re.MULTILINE)
    
    # Replace .. standalone (from parent package)
    content = re.sub(r'^from \.\. import', r'from nlpl import', content, flags=re.MULTILINE)
    
    # Replace single dot relative imports in subpackages
    # This is more complex and context-dependent, so we'll skip for now
    
    if content != original_content:
        print(f"Fixed: {file_path}")
        with open(file_path, 'w', encoding='utf-8') as f:
            f.write(content)
        return True
    return False

def main():
    src_dir = Path(__file__).parent.parent.parent / 'src' / 'nlpl'
    
    if not src_dir.exists():
        print(f"Error: {src_dir} not found")
        return
    
    fixed_count = 0
    for py_file in src_dir.rglob('*.py'):
        if fix_relative_imports(py_file):
            fixed_count += 1
    
    print(f"\nFixed {fixed_count} files")

if __name__ == '__main__':
    main()
