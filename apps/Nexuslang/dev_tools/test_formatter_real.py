"""
Test formatter on real NexusLang file
"""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent.parent / 'src'))

from nexuslang.lsp.formatter import NLPLFormatter


def main():
    formatter = NLPLFormatter()
    
    # Read an example file
    example_file = Path(__file__).parent.parent / 'examples' / '01_basic_concepts.nxl'
    
    with open(example_file, 'r') as f:
        original = f.read()
    
    print("=" * 80)
    print("Formatting: examples/01_basic_concepts.nxl")
    print("=" * 80)
    print("\nOriginal file (first 500 chars):")
    print(original[:500])
    print("\n" + "=" * 80)
    
    formatted = formatter.format(original)
    
    print("\nFormatted file (first 500 chars):")
    print(formatted[:500])
    print("\n" + "=" * 80)
    
    # Check if changes were made
    if original == formatted:
        print("\n File is already properly formatted!")
    else:
        print("\n File was reformatted")
        print(f"\nOriginal length: {len(original)} chars")
        print(f"Formatted length: {len(formatted)} chars")
    
    print("\n" + "=" * 80)


if __name__ == '__main__':
    main()
