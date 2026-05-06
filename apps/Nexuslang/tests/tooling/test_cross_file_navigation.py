"""
Tests for cross-file navigation using workspace indexing.
"""

import os
import tempfile
import pytest

from nexuslang.lsp.workspace_index import WorkspaceIndex
from nexuslang.lsp.server import NLPLLanguageServer


class TestCrossFileNavigation:
    """Test cross-file go-to-definition and workspace symbols."""
    
    def test_cross_file_definition_lookup(self):
        """Test finding definitions across multiple files."""
        with tempfile.TemporaryDirectory() as tmpdir:
            # Create module A with a function
            module_a = os.path.join(tmpdir, "module_a.nxl")
            with open(module_a, 'w') as f:
                f.write("""
function calculate with x as Integer and y as Integer returns Integer
    return x plus y
end

class Calculator
    function add with a as Integer and b as Integer returns Integer
        return a plus b
    end
end
""")
            
            # Create module B that uses module A
            module_b = os.path.join(tmpdir, "module_b.nxl")
            with open(module_b, 'w') as f:
                f.write("""
import module_a

function main
    set result to calculate with x: 10 and y: 20
    print text result
end
""")
            
            # Index the workspace
            index = WorkspaceIndex(tmpdir)
            index.scan_workspace()
            
            # Test: Find "calculate" function
            symbols = index.get_symbol("calculate")
            assert len(symbols) >= 1
            calc_symbol = [s for s in symbols if s.kind == 'function'][0]
            assert calc_symbol.name == "calculate"
            assert "module_a.nxl" in calc_symbol.file_uri
            assert calc_symbol.line == 1  # 0-indexed: line 0 is blank, function is on line 1
            
            # Test: Find "Calculator" class
            symbols = index.get_symbol("Calculator")
            assert len(symbols) >= 1
            class_symbol = symbols[0]
            assert class_symbol.name == "Calculator"
            assert class_symbol.kind == "class"
            
            # Test: Find "add" method
            symbols = index.get_symbol("add")
            assert len(symbols) >= 1
            method_symbol = [s for s in symbols if s.kind == 'method'][0]
            assert method_symbol.name == "add"
            assert method_symbol.scope == "Calculator"
    
    def test_workspace_symbol_search(self):
        """Test fuzzy symbol search across workspace."""
        with tempfile.TemporaryDirectory() as tmpdir:
            # Create multiple files with various symbols
            file1 = os.path.join(tmpdir, "math_utils.nxl")
            with open(file1, 'w') as f:
                f.write("""
function calculate_sum with numbers as List of Integer returns Integer
    set total to 0
    for each num in numbers
        set total to total plus num
    end
    return total
end

function calculate_average with numbers as List of Integer returns Float
    set sum to calculate_sum with numbers: numbers
    return sum divided by length of numbers
end
""")
            
            file2 = os.path.join(tmpdir, "string_utils.nxl")
            with open(file2, 'w') as f:
                f.write("""
function join_strings with strings as List of String returns String
    set result to ""
    for each s in strings
        set result to result plus s
    end
    return result
end
""")
            
            # Index workspace
            index = WorkspaceIndex(tmpdir)
            index.scan_workspace()
            
            # Test: Fuzzy search for "calc"
            results = index.find_symbols("calc", kind="function")
            assert len(results) >= 2
            names = [s.name for s in results]
            assert "calculate_sum" in names
            assert "calculate_average" in names
            
            # Test: Search for "join"
            results = index.find_symbols("join", kind="function")
            assert len(results) >= 1
            assert results[0].name == "join_strings"
    
    def test_incremental_reindexing(self):
        """Test that file changes are reflected in the index."""
        with tempfile.TemporaryDirectory() as tmpdir:
            test_file = os.path.join(tmpdir, "test.nxl")
            
            # Write initial content
            with open(test_file, 'w') as f:
                f.write("""
function foo returns Integer
    return 42
end
""")
            
            # Index workspace
            index = WorkspaceIndex(tmpdir)
            index.scan_workspace()
            
            # Verify initial symbol
            symbols = index.get_symbol("foo")
            assert len(symbols) == 1
            assert symbols[0].name == "foo"
            
            # Verify bar doesn't exist
            symbols = index.get_symbol("bar")
            assert len(symbols) == 0
            
            # Update file
            with open(test_file, 'w') as f:
                f.write("""
function foo returns Integer
    return 42
end

function bar returns String
    return "hello"
end
""")
            
            # Re-index the file
            file_uri = index._path_to_uri(test_file)
            index.index_file(file_uri, test_file)
            
            # Verify both symbols now exist
            symbols = index.get_symbol("foo")
            assert len(symbols) == 1
            
            symbols = index.get_symbol("bar")
            assert len(symbols) == 1
            assert symbols[0].name == "bar"
    
    def test_statistics(self):
        """Test workspace indexing statistics."""
        with tempfile.TemporaryDirectory() as tmpdir:
            # Create a file with various symbol types
            test_file = os.path.join(tmpdir, "test.nxl")
            with open(test_file, 'w') as f:
                f.write("""
function my_function returns Integer
    return 1
end

class MyClass
    function my_method returns Integer
        return 2
    end
end

struct Point
    x as Integer
    y as Integer
end

set global_var to 42
""")
            
            index = WorkspaceIndex(tmpdir)
            index.scan_workspace()
            
            stats = index.get_statistics()
            
            assert stats['files_indexed'] == 1
            assert stats['functions'] >= 1
            assert stats['classes'] >= 1
            assert stats['methods'] >= 1
            assert stats['structs'] >= 1
            assert stats['variables'] >= 1
            assert stats['total_symbols'] >= 5


if __name__ == '__main__':
    pytest.main([__file__, '-v'])
