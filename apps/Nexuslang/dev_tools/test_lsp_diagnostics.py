#!/usr/bin/env python3
"""
Test the NexusLang LSP Server diagnostics integration.

This script simulates what the LSP client does:
1. Opens a document
2. Gets diagnostics
3. Displays results
"""

import sys
import os

# Add src directory to path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'src'))

from nexuslang.lsp.server import NLPLLanguageServer


def test_diagnostics():
    """Test diagnostics on a sample NexusLang file."""
    server = NLPLLanguageServer()
    
    # Read test file
    root_dir = os.path.join(os.path.dirname(__file__), '..')
    test_file = os.path.join(root_dir, 'test_programs', 'lsp_test.nxl')
    
    if not os.path.exists(test_file):
        print(f"Test file not found: {test_file}")
        return
    
    with open(test_file, 'r') as f:
        text = f.read()
    
    print("=" * 60)
    print("Testing NexusLang LSP Server Diagnostics")
    print("=" * 60)
    print(f"\nTest file: {test_file}\n")
    print("File contents:")
    print("-" * 60)
    print(text)
    print("-" * 60)
    
    # Get diagnostics
    uri = f"file://{test_file}"
    diagnostics = server.diagnostics_provider.get_diagnostics(uri, text)
    
    print(f"\n\nDiagnostics found: {len(diagnostics)}\n")
    
    if diagnostics:
        for i, diag in enumerate(diagnostics, 1):
            severity = {1: "ERROR", 2: "WARNING", 3: "INFO", 4: "HINT"}.get(
                diag['severity'], "UNKNOWN"
            )
            line = diag['range']['start']['line'] + 1
            col = diag['range']['start']['character'] + 1
            msg = diag['message']
            source = diag.get('source', 'unknown')
            
            print(f"{i}. [{severity}] Line {line}:{col}")
            print(f"   Source: {source}")
            print(f"   {msg}")
            print()
    else:
        print(" No diagnostics found - code looks good!")
    
    print("=" * 60)
    print("Testing parser integration...")
    print("=" * 60)
    
    # Test with intentionally broken syntax
    broken_code = '''
set x to "unclosed string
set y to 123
'''
    
    print("\nTesting broken code:")
    print("-" * 60)
    print(broken_code)
    print("-" * 60)
    
    broken_diagnostics = server.diagnostics_provider.get_diagnostics("file://broken.nxl", broken_code)
    
    print(f"\n\nDiagnostics found: {len(broken_diagnostics)}\n")
    
    for i, diag in enumerate(broken_diagnostics, 1):
        severity = {1: "ERROR", 2: "WARNING", 3: "INFO", 4: "HINT"}.get(
            diag['severity'], "UNKNOWN"
        )
        line = diag['range']['start']['line'] + 1
        col = diag['range']['start']['character'] + 1
        msg = diag['message']
        source = diag.get('source', 'unknown')
        
        print(f"{i}. [{severity}] Line {line}:{col}")
        print(f"   Source: {source}")
        print(f"   {msg}")
        print()


if __name__ == '__main__':
    test_diagnostics()
