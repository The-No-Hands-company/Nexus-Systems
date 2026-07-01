#!/usr/bin/env python3
"""
NLPL Project Cleanup Script
Removes all emojis from source files and documentation.
"""

import os
import re
import sys
from pathlib import Path

# Comprehensive emoji pattern - matches all common emojis and special symbols
EMOJI_PATTERN = re.compile(
    "["
    "\U0001F600-\U0001F64F"  # emoticons
    "\U0001F300-\U0001F5FF"  # symbols & pictographs
    "\U0001F680-\U0001F6FF"  # transport & map symbols
    "\U0001F700-\U0001F77F"  # alchemical symbols
    "\U0001F780-\U0001F7FF"  # Geometric Shapes Extended
    "\U0001F800-\U0001F8FF"  # Supplemental Arrows-C
    "\U0001F900-\U0001F9FF"  # Supplemental Symbols and Pictographs
    "\U0001FA00-\U0001FA6F"  # Chess Symbols
    "\U0001FA70-\U0001FAFF"  # Symbols and Pictographs Extended-A
    "\U00002702-\U000027B0"  # Dingbats
    "\U000024C2-\U0001F251"  # Enclosed characters
    "\U0000203C-\U00003299"  # Various symbols
    "\u2600-\u26FF"          # Miscellaneous Symbols
    "\u2700-\u27BF"          # Dingbats
    "\u231A-\u231B"          # Watch symbols
    "\u23E9-\u23F3"          # Additional symbols
    "\u2B50"                 # Star
    "\u2B55"                 # Circle
    "]+", 
    flags=re.UNICODE
)

def clean_emoji_from_text(text):
    """Remove all emojis from text while preserving structure."""
    # Remove emojis
    cleaned = EMOJI_PATTERN.sub('', text)
    
    # Clean up multiple spaces that may result from emoji removal
    cleaned = re.sub(r' {2,}', ' ', cleaned)
    
    # Clean up lines that are now just whitespace after emoji removal
    lines = cleaned.split('\n')
    cleaned_lines = []
    for line in lines:
        stripped = line.strip()
        # Keep line if it has content or if it's intentionally blank (between paragraphs)
        if stripped or (not stripped and cleaned_lines and cleaned_lines[-1].strip()):
            cleaned_lines.append(line)
    
    return '\n'.join(cleaned_lines)

def process_file(filepath):
    """Process a single file to remove emojis."""
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            content = f.read()
        
        # Check if file contains emojis
        if not EMOJI_PATTERN.search(content):
            return False, "No emojis found"
        
        # Clean the content
        cleaned_content = clean_emoji_from_text(content)
        
        # Write back if changed
        if cleaned_content != content:
            with open(filepath, 'w', encoding='utf-8') as f:
                f.write(cleaned_content)
            return True, "Emojis removed"
        
        return False, "No changes needed"
    
    except Exception as e:
        return False, f"Error: {str(e)}"

def main():
    """Main cleanup function."""
    project_root = Path(__file__).parent.parent
    
    # File patterns to process
    patterns = ['**/*.py', '**/*.nxl', '**/*.md']
    
    # Directories to skip
    skip_dirs = {'.git', '.venv', '__pycache__', '.pytest_cache', 'build', 'node_modules'}
    
    print("NLPL Project Emoji Cleanup")
    print("=" * 60)
    print()
    
    total_files = 0
    modified_files = 0
    
    for pattern in patterns:
        for filepath in project_root.glob(pattern):
            # Skip if in excluded directory
            if any(skip_dir in filepath.parts for skip_dir in skip_dirs):
                continue
            
            total_files += 1
            modified, status = process_file(filepath)
            
            if modified:
                modified_files += 1
                relative_path = filepath.relative_to(project_root)
                print(f"MODIFIED: {relative_path}")
    
    print()
    print("=" * 60)
    print(f"Total files scanned: {total_files}")
    print(f"Files modified: {modified_files}")
    print(f"Files unchanged: {total_files - modified_files}")
    print()
    
    if modified_files > 0:
        print("SUCCESS: All emojis have been removed from the project.")
    else:
        print("INFO: No emojis found in project files.")

if __name__ == '__main__':
    main()
