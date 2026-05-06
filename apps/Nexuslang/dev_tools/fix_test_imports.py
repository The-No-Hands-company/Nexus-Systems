#!/usr/bin/env python3
"""Fix test imports from src.nlpl.* to nlpl.* and update sys.path."""

import os
import re
from pathlib import Path

# Get test directory
test_dir = Path(__file__).parent.parent / "tests"

# Files to update
test_files = [
    "test_type_system_features.py",
    "test_comprehensive_errors.py",
    "test_error_reporting.py",
    "test_errors.py",
    "test_lexer.py",
    "test_parser.py",
    "test_interpreter.py",
    "test_integration.py",
    "test_stdlib.py",
]

for filename in test_files:
    filepath = test_dir / filename
    if not filepath.exists():
        print(f"Skipping {filename} - file not found")
        continue
    
    print(f"Updating {filename}...")
    
    # Read file
    with open(filepath, 'r') as f:
        content = f.read()
    
    # Replace imports
    original_content = content
    content = re.sub(r'from src\.nxl\.', 'from nexuslang.', content)
    content = re.sub(r'from src\.parser\.', 'from nexuslang.parser.', content)
    content = re.sub(r'from src\.interpreter\.', 'from nexuslang.interpreter.', content)
    content = re.sub(r'from src\.runtime\.', 'from nexuslang.runtime.', content)
    content = re.sub(r'from src\.typesystem\.', 'from nexuslang.typesystem.', content)
    content = re.sub(r'from src\.stdlib\.', 'from nexuslang.stdlib.', content)
    
    # Fix sys.path if needed
    if 'sys.path.insert' in content:
        # Replace sys.path.insert lines to use correct path
        content = re.sub(
            r"sys\.path\.insert\(0, os\.path\.join\(os\.path\.dirname\(__file__\), '\.\.'\)\)",
            "sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'src'))",
            content
        )
        content = re.sub(
            r"sys\.path\.insert\(0, os\.path\.dirname\(os\.path\.dirname\(__file__\)\)\)",
            "sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(__file__)), 'src'))",
            content
        )
    else:
        # Add sys.path.insert at the top if not present
        if 'import sys' not in content and 'from sys import' not in content:
            # Find first import
            first_import = re.search(r'^(import |from )', content, re.MULTILINE)
            if first_import:
                pos = first_import.start()
                content = (
                    content[:pos] +
                    "import sys\n" +
                    "import os\n" +
                    "sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'src'))\n\n" +
                    content[pos:]
                )
    
    # Write back only if changed
    if content != original_content:
        with open(filepath, 'w') as f:
            f.write(content)
        print(f"   Updated {filename}")
    else:
        print(f"  - No changes needed for {filename}")

print("\nDone!")
