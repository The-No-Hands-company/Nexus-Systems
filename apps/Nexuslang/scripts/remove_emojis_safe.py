#!/usr/bin/env python3
"""
Safe emoji removal script - removes emojis while preserving indentation.
Only removes the emoji characters themselves, doesn't touch any whitespace.
"""

import os
import re
from pathlib import Path

# Comprehensive emoji pattern
EMOJI_PATTERN = re.compile(
    "["
    "\U0001F600-\U0001F64F"  # emoticons
    "\U0001F300-\U0001F5FF"  # symbols & pictographs
    "\U0001F680-\U0001F6FF"  # transport & map symbols
    "\U0001F1E0-\U0001F1FF"  # flags
    "\U00002702-\U000027B0"  # dingbats
    "\U000024C2-\U0001F251"  # enclosed characters
    "\U0001F900-\U0001F9FF"  # supplemental symbols
    "\U0001FA00-\U0001FA6F"  # chess symbols
    "\U00002600-\U000026FF"  # miscellaneous symbols
    "\U0001F780-\U0001F7FF"  # geometric shapes extended
    "]+",
    flags=re.UNICODE
)

def remove_emojis_safe(text: str) -> str:
    """Remove emojis while preserving all whitespace and formatting."""
    # Simply remove emoji characters, nothing else
    return EMOJI_PATTERN.sub('', text)

def process_file(filepath: Path) -> bool:
    """Process a single file, return True if modified."""
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            original = f.read()
        
        # Check if file has emojis
        if not EMOJI_PATTERN.search(original):
            return False
        
        # Remove emojis
        cleaned = remove_emojis_safe(original)
        
        # Only write if something changed
        if cleaned != original:
            with open(filepath, 'w', encoding='utf-8') as f:
                f.write(cleaned)
            return True
        
        return False
    
    except Exception as e:
        print(f"ERROR processing {filepath}: {e}")
        return False

def main():
    """Remove emojis from all project files."""
    root = Path(__file__).parent.parent
    
    # Only scan specific directories to avoid slowness
    scan_dirs = ['src', 'tests', 'dev_tools', 'examples', 'test_programs', 'docs']
    
    # File extensions to process
    extensions = {'.py', '.nxl', '.md'}
    
    # Directories to skip
    skip_dirs = {'.git', '.venv', '__pycache__', 'node_modules', 'build', '.pytest_cache'}
    
    print("Safe Emoji Removal")
    print("=" * 60)
    print("Removing emojis while preserving indentation...\n")
    
    modified_files = []
    total_files = 0
    
    for scan_dir in scan_dirs:
        dir_path = root / scan_dir
        if not dir_path.exists():
            continue
        
        for ext in extensions:
            for filepath in dir_path.rglob(f'*{ext}'):
                # Skip excluded directories
                if any(skip_dir in filepath.parts for skip_dir in skip_dirs):
                    continue
                
                total_files += 1
                
                if process_file(filepath):
                    rel_path = filepath.relative_to(root)
                    print(f"MODIFIED: {rel_path}")
                    modified_files.append(rel_path)
    
    print("\n" + "=" * 60)
    print(f"Total files scanned: {total_files}")
    print(f"Files modified: {len(modified_files)}")
    print(f"Files unchanged: {total_files - len(modified_files)}")
    
    if modified_files:
        print(f"\nSUCCESS: Removed emojis from {len(modified_files)} files.")
    else:
        print("\nNo emojis found in project files.")

if __name__ == '__main__':
    main()
