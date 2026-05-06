#!/usr/bin/env python3
"""
Fix absolute imports to relative imports throughout the NexusLang codebase.

This converts all 'from nexuslang.X import Y' to 'from .X import Y' or 'from ..X import Y'
based on the module's depth in the package hierarchy.
"""

import os
import re
from pathlib import Path


def calculate_relative_import(file_path: Path, root_path: Path) -> dict[str, str]:
    """
    Calculate how to convert nlpl.X imports to relative imports.
    
    Returns a mapping from 'nexuslang.module' to relative import path.
    """
    # Get the file's package depth (how many levels down from src/nlpl/)
    relative_to_nxl = file_path.relative_to(root_path / 'src' / 'nlpl')
    depth = len(relative_to_nxl.parts) - 1  # Subtract 1 for the file itself
    
    conversions = {}
    
    # If in src/nlpl/ directly (depth 0)
    if depth == 0:
        # from nexuslang.parser -> from .parser
        conversions['from nexuslang.'] = 'from .'
    else:
        # If in src/nlpl/compiler/ (depth 1)
        # from nexuslang.parser -> from ..parser
        # from nexuslang.compiler -> from .
        
        # For same-level packages (nlpl.X where X is current package)
        current_package = relative_to_nxl.parts[0] if depth > 0 else None
        
        # Most imports: go up to nlpl level then down
        conversions[f'from nexuslang.'] = f'from {"." * (depth + 1)}'
        
        # Imports from current package
        if current_package:
            conversions[f'from nexuslang.{current_package}.'] = 'from .'
    
    return conversions


def fix_imports_in_file(file_path: Path, root_path: Path) -> bool:
    """Fix imports in a single file. Returns True if changes were made."""
    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            content = f.read()
        
        original_content = content
        lines = content.split('\n')
        
        # Get conversion rules
        conversions = calculate_relative_import(file_path, root_path)
        
        # Process each line
        new_lines = []
        for line in lines:
            modified_line = line
            
            # Check each conversion pattern
            for old_pattern, new_pattern in conversions.items():
                if modified_line.strip().startswith(old_pattern):
                    # Replace the import prefix
                    modified_line = modified_line.replace(old_pattern, new_pattern, 1)
                    break
            
            new_lines.append(modified_line)
        
        new_content = '\n'.join(new_lines)
        
        # Only write if changed
        if new_content != original_content:
            with open(file_path, 'w', encoding='utf-8') as f:
                f.write(new_content)
            return True
        
        return False
        
    except Exception as e:
        print(f"Error processing {file_path}: {e}")
        return False


def main():
    root_path = Path(__file__).parent.parent
    src_path = root_path / 'src' / 'nlpl'
    
    if not src_path.exists():
        print(f"Error: {src_path} not found")
        return 1
    
    # Find all Python files
    python_files = list(src_path.rglob('*.py'))
    
    print(f"Found {len(python_files)} Python files")
    print("Converting absolute imports to relative imports...\n")
    
    changed_count = 0
    for file_path in sorted(python_files):
        if fix_imports_in_file(file_path, root_path):
            print(f" Fixed: {file_path.relative_to(root_path)}")
            changed_count += 1
    
    print(f"\n{'='*60}")
    print(f"Conversion complete!")
    print(f"Modified {changed_count} files")
    print(f"{'='*60}")
    
    return 0


if __name__ == '__main__':
    exit(main())
