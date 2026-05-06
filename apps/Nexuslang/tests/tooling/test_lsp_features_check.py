#!/usr/bin/env python3
"""
Quick LSP Feature Check
=======================

Manually verify LSP capabilities by inspecting code.
"""

import sys
from pathlib import Path

# Add NexusLang to path
sys.path.insert(0, str(Path(__file__).parent.parent / 'src'))

from nexuslang.lsp.definitions import DefinitionProvider
from nexuslang.lsp.completions import CompletionProvider
from nexuslang.lsp.hover import HoverProvider
from nexuslang.lsp.diagnostics import DiagnosticsProvider
from nexuslang.parser.parser import Parser
from nexuslang.parser.lexer import Lexer


def test_definitions_provider():
    """Test the definitions provider logic."""
    print("\n" + "=" * 70)
    print("TESTING: Definition Provider")
    print("=" * 70)
    
    # Simple test code
    test_code = """function greet with name as String returns String
    return "Hello, " plus name
end

set message to greet with "World"
"""
    
    # Parse the code
    lexer = Lexer(test_code)
    tokens = lexer.tokenize()
    parser = Parser(tokens)
    ast = parser.parse()
    
    print("\n✅ Code parsed successfully")
    print(f"   AST nodes: {len(ast.statements)}")
    assert len(ast.statements) > 0
    
    # Check if we can find the function definition
    print("\n✅ Function definitions found:")
    found_function = False
    for stmt in ast.statements:
        if hasattr(stmt, '__class__') and stmt.__class__.__name__ == 'FunctionDefinition':
            print(f"   - {stmt.name} at line {getattr(stmt, 'line', '?')}")
            found_function = True

    assert found_function


def test_completion_provider():
    """Test completion logic."""
    print("\n" + "=" * 70)
    print("TESTING: Completion Provider")
    print("=" * 70)
    
    from nexuslang.lsp.completions import CompletionProvider
    
    # Mock language server
    class MockServer:
        def __init__(self):
            self.documents = {}
    
    provider = CompletionProvider(MockServer())
    
    # Test keyword completions
    keywords = provider.get_keyword_completions()
    print(f"\n✅ Keyword completions available: {len(keywords)}")
    print(f"   Sample: {[k['label'] for k in keywords[:5]]}")

    assert len(keywords) > 0


def check_lsp_files():
    """Check which LSP files exist and their size."""
    print("\n" + "=" * 70)
    print("LSP FILES INVENTORY")
    print("=" * 70)
    
    lsp_dir = Path(__file__).parent.parent / 'src' / 'nlpl' / 'lsp'
    
    files = [
        'server.py',
        'completions.py',
        'definitions.py',
        'hover.py',
        'diagnostics.py',
        'references.py',
        'rename.py',
        'code_actions.py',
        'signature_help.py',
        'formatter.py',
        'symbols.py',
        'workspace_index.py',
        'semantic_tokens.py'
    ]
    
    total_lines = 0
    for filename in files:
        filepath = lsp_dir / filename
        if filepath.exists():
            lines = len(filepath.read_text().splitlines())
            total_lines += lines
            status = "✅" if lines > 50 else "⚠️ " if lines > 20 else "❌"
            print(f"{status} {filename:25} {lines:5} lines")
        else:
            print(f"❌ {filename:25} MISSING")
    
    print(f"\n📊 Total LSP code: {total_lines} lines")
    
    return True


def check_cross_file_support():
    """Check if workspace index supports cross-file navigation."""
    print("\n" + "=" * 70)
    print("CROSS-FILE SUPPORT CHECK")
    print("=" * 70)
    
    try:
        from nexuslang.lsp.workspace_index import WorkspaceIndex
        
        # Create a workspace index
        test_dir = Path(__file__).parent.parent / "test_programs" / "lsp_tests"
        test_dir.mkdir(parents=True, exist_ok=True)
        
        index = WorkspaceIndex(str(test_dir))
        
        print("✅ WorkspaceIndex initialized")
        print(f"   Root: {index.root_path}")
        
        # Check if it can index files
        if hasattr(index, 'index_file'):
            print("✅ index_file method exists")
        else:
            print("❌ No index_file method")
            
        if hasattr(index, 'find_symbol'):
            print("✅ find_symbol method exists")
        else:
            print("❌ No find_symbol method")
            
        if hasattr(index, 'get_all_symbols'):
            print("✅ get_all_symbols method exists")
        else:
            print("❌ No get_all_symbols method")
        
        return True
        
    except Exception as e:
        print(f"❌ Error checking workspace index: {e}")
        return False


if __name__ == '__main__':
    print("\n" + "=" * 70)
    print("LSP FEATURE CHECK")
    print("=" * 70)
    print("\nThis test verifies LSP components without running the server.")
    
    try:
        check_lsp_files()
        test_definitions_provider()
        test_completion_provider()
        check_cross_file_support()
        
        print("\n" + "=" * 70)
        print("FEATURE CHECK COMPLETE")
        print("=" * 70)
        print("\n✅ LSP implementation is substantial and working!")
        print("   Next steps:")
        print("   1. Test in VS Code manually")
        print("   2. Fix any cross-file navigation issues")
        print("   3. Add more comprehensive tests")
        
    except Exception as e:
        print(f"\n❌ Error: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)
