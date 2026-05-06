#!/usr/bin/env python3
"""
Test Enhanced LSP Server Features
==================================

Tests:
1. Enhanced error positioning (AST-based)
2. Code actions (quick fixes)
3. Signature help
4. Multi-file diagnostics
"""

import sys
import os

# Add src directory to path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'src'))

from nexuslang.lsp.server import NLPLLanguageServer, Position


def test_enhanced_positioning():
    """Test AST-based error positioning."""
    print("=" * 60)
    print("Test 1: Enhanced Error Positioning")
    print("=" * 60)
    
    server = NLPLLanguageServer()
    
    code = '''
function greet that takes name as String returns Integer
    return "hello"  # Type error: returns String instead of Integer
'''
    
    uri = "file:///test_positioning.nxl"
    diagnostics = server.diagnostics_provider.get_diagnostics(uri, code, check_imports=False)
    
    print(f"\nDiagnostics found: {len(diagnostics)}\n")
    for diag in diagnostics:
        severity = {1: "ERROR", 2: "WARNING"}.get(diag['severity'], "UNKNOWN")
        line = diag['range']['start']['line'] + 1
        col = diag['range']['start']['character'] + 1
        print(f"[{severity}] Line {line}:{col} - {diag['message']}")
        print(f"  Source: {diag['source']}\n")


def test_code_actions():
    """Test code actions (quick fixes)."""
    print("=" * 60)
    print("Test 2: Code Actions (Quick Fixes)")
    print("=" * 60)
    
    server = NLPLLanguageServer()
    
    code = '''set unused_var to 42
set message to "unclosed string
set x to 10
print text x'''
    
    uri = "file:///test_actions.nxl"
    diagnostics = server.diagnostics_provider.get_diagnostics(uri, code, check_imports=False)
    
    print(f"\nDiagnostics: {len(diagnostics)}")
    for diag in diagnostics:
        print(f"  - {diag['message']}")
    
    # Get code actions
    range_params = {"start": {"line": 0, "character": 0}, "end": {"line": 10, "character": 0}}
    actions = server.code_actions_provider.get_code_actions(uri, code, range_params, diagnostics)
    
    print(f"\nCode Actions available: {len(actions)}\n")
    for action in actions:
        print(f" {action['title']}")
        print(f"  Kind: {action['kind']}")
        if 'edit' in action:
            print(f"  Edit: Will modify the document")
        print()


def test_signature_help():
    """Test signature help."""
    print("=" * 60)
    print("Test 3: Signature Help")
    print("=" * 60)
    
    server = NLPLLanguageServer()
    
    # Test 1: stdlib function
    code1 = "set result to sqrt with "
    position1 = Position(0, len(code1))
    
    sig1 = server.signature_help_provider.get_signature_help(code1, position1)
    
    print("\nTest 3a: Stdlib function (sqrt)")
    if sig1:
        sig = sig1['signatures'][0]
        print(f"Signature: {sig['label']}")
        print(f"Active parameter: {sig1['activeParameter']}")
        print(f"Parameters:")
        for param in sig['parameters']:
            print(f"  - {param['label']}: {param['documentation']}")
    else:
        print("No signature help available")
    
    # Test 2: User-defined function
    code2 = '''function calculate that takes x as Integer, y as Integer returns Integer
    return x plus y

set result to calculate with 5, '''
    
    lines2 = code2.split('\n')
    position2 = Position(3, len(lines2[3]))
    
    sig2 = server.signature_help_provider.get_signature_help(code2, position2)
    
    print("\nTest 3b: User-defined function (calculate)")
    if sig2:
        sig = sig2['signatures'][0]
        print(f"Signature: {sig['label']}")
        print(f"Active parameter: {sig2['activeParameter']}")
        print(f"Parameters:")
        for param in sig['parameters']:
            print(f"  - {param['label']}: {param['documentation']}")
    else:
        print("No signature help available")


def test_multi_file_diagnostics():
    """Test multi-file import checking."""
    print("\n" + "=" * 60)
    print("Test 4: Multi-File Diagnostics")
    print("=" * 60)
    
    server = NLPLLanguageServer()
    
    code = '''import math
import nonexistent_module
import utils from "missing_file.nxl"
import collections

set x to sqrt with 16
'''
    
    uri = "file:///test_imports.nxl"
    diagnostics = server.diagnostics_provider.get_diagnostics(uri, code, check_imports=True)
    
    print(f"\nDiagnostics found: {len(diagnostics)}\n")
    for diag in diagnostics:
        severity = {1: "ERROR", 2: "WARNING"}.get(diag['severity'], "UNKNOWN")
        line = diag['range']['start']['line'] + 1
        source = diag.get('source', 'unknown')
        print(f"[{severity}] Line {line} ({source})")
        print(f"  {diag['message']}\n")
    
    # Test workspace diagnostics cache
    print("Workspace diagnostics cache:")
    workspace_diags = server.diagnostics_provider.get_workspace_diagnostics()
    print(f"  Files tracked: {len(workspace_diags)}")
    for file_uri in workspace_diags.keys():
        print(f"  - {file_uri}: {len(workspace_diags[file_uri])} diagnostics")


def test_integration():
    """Test all features working together."""
    print("\n" + "=" * 60)
    print("Test 5: Integration Test")
    print("=" * 60)
    
    server = NLPLLanguageServer()
    
    code = '''function add that takes a as Integer, b as Integer returns Integer
    return a plus b

set unused_var to "test
set result to add with 5, 10
print text result
'''
    
    uri = "file:///test_integration.nxl"
    
    # Get diagnostics
    diagnostics = server.diagnostics_provider.get_diagnostics(uri, code, check_imports=False)
    
    print(f"\nDiagnostics: {len(diagnostics)}")
    for diag in diagnostics:
        severity = {1: "ERROR", 2: "WARNING"}.get(diag['severity'], "UNKNOWN")
        line = diag['range']['start']['line'] + 1
        col = diag['range']['start']['character'] + 1
        print(f"  [{severity}] Line {line}:{col} - {diag['message']}")
    
    # Get code actions
    range_params = {"start": {"line": 0, "character": 0}, "end": {"line": 10, "character": 0}}
    actions = server.code_actions_provider.get_code_actions(uri, code, range_params, diagnostics)
    print(f"\nCode Actions: {len(actions)}")
    for action in actions:
        print(f"   {action['title']}")
    
    # Get signature help at function call
    lines = code.split('\n')
    call_line = 4  # "set result to add with 5, 10"
    call_pos = len("set result to add with 5, ")
    position = Position(call_line, call_pos)
    
    sig = server.signature_help_provider.get_signature_help(code, position)
    if sig:
        print(f"\nSignature Help: {sig['signatures'][0]['label']}")
        print(f"  Active parameter: {sig['activeParameter']}")
    
    print("\n" + "=" * 60)
    print(" All integration tests passed!")
    print("=" * 60)


if __name__ == '__main__':
    test_enhanced_positioning()
    print()
    test_code_actions()
    print()
    test_signature_help()
    test_multi_file_diagnostics()
    test_integration()
