"""
Tests for Workspace Symbol Indexing
====================================

Tests the WorkspaceIndex class for symbol extraction, lookup, and workspace scanning.
"""

import os
import tempfile
import pytest
from pathlib import Path

from nexuslang.lsp.workspace_index import WorkspaceIndex, SymbolInfo


class TestSymbolInfo:
    """Test SymbolInfo dataclass."""
    
    def test_symbol_info_creation(self):
        """Test creating a SymbolInfo object."""
        symbol = SymbolInfo(
            name="test_func",
            kind="function",
            file_uri="file:///test.nxl",
            line=10,
            column=0,
            signature="with x as Integer returns Integer"
        )
        
        assert symbol.name == "test_func"
        assert symbol.kind == "function"
        assert symbol.line == 10
        assert symbol.signature == "with x as Integer returns Integer"
    
    def test_symbol_info_equality(self):
        """Test SymbolInfo equality comparison."""
        s1 = SymbolInfo("func", "function", "file:///test.nxl", 10, 0)
        s2 = SymbolInfo("func", "function", "file:///test.nxl", 10, 0)
        s3 = SymbolInfo("func", "function", "file:///other.nxl", 10, 0)
        
        assert s1 == s2
        assert s1 != s3
    
    def test_symbol_info_hashable(self):
        """Test that SymbolInfo can be used in sets."""
        s1 = SymbolInfo("func", "function", "file:///test.nxl", 10, 0)
        s2 = SymbolInfo("func", "function", "file:///test.nxl", 10, 0)
        
        symbol_set = {s1, s2}
        assert len(symbol_set) == 1  # Same symbol, only one in set


class TestWorkspaceIndex:
    """Test WorkspaceIndex class."""
    
    def test_workspace_index_creation(self):
        """Test creating a WorkspaceIndex."""
        with tempfile.TemporaryDirectory() as tmpdir:
            index = WorkspaceIndex(tmpdir)
            assert index.workspace_root == tmpdir
            assert len(index.symbols) == 0
            assert len(index.files) == 0
    
    def test_extract_function_symbols(self):
        """Test extracting function symbols from code."""
        with tempfile.TemporaryDirectory() as tmpdir:
            # Create test file
            test_file = os.path.join(tmpdir, "test.nxl")
            with open(test_file, 'w') as f:
                f.write("""
function greet with name as String returns String
    return "Hello " + name
end

function add with a as Integer and b as Integer returns Integer
    return a + b
end
""")
            
            # Index the file
            index = WorkspaceIndex(tmpdir)
            file_uri = index._path_to_uri(test_file)
            symbols = index.index_file(file_uri, test_file)
            
            # Check function symbols
            func_symbols = [s for s in symbols if s.kind == 'function']
            assert len(func_symbols) == 2
            
            # Check 'greet' function
            greet = next(s for s in func_symbols if s.name == 'greet')
            assert greet.name == 'greet'
            assert greet.kind == 'function'
            assert 'String' in greet.signature
            
            # Check 'add' function
            add = next(s for s in func_symbols if s.name == 'add')
            assert add.name == 'add'
            assert 'Integer' in add.signature
    
    def test_extract_class_symbols(self):
        """Test extracting class and method symbols."""
        with tempfile.TemporaryDirectory() as tmpdir:
            test_file = os.path.join(tmpdir, "test.nxl")
            with open(test_file, 'w') as f:
                f.write("""
class Person
    function init with name as String and age as Integer
        set self.name to name
        set self.age to age
    end
    
    function greet returns String
        return "Hello, I am " + self.name
    end
end
""")
            
            index = WorkspaceIndex(tmpdir)
            file_uri = index._path_to_uri(test_file)
            symbols = index.index_file(file_uri, test_file)
            
            # Check class symbol
            class_symbols = [s for s in symbols if s.kind == 'class']
            assert len(class_symbols) == 1
            assert class_symbols[0].name == 'Person'
            
            # Check method symbols
            method_symbols = [s for s in symbols if s.kind == 'method']
            assert len(method_symbols) == 2
            
            method_names = {s.name for s in method_symbols}
            assert 'init' in method_names
            assert 'greet' in method_names
            
            # Check method scope
            for method in method_symbols:
                assert method.scope == 'Person'
    
    def test_extract_struct_symbols(self):
        """Test extracting struct symbols."""
        with tempfile.TemporaryDirectory() as tmpdir:
            test_file = os.path.join(tmpdir, "test.nxl")
            with open(test_file, 'w') as f:
                f.write("""
struct Point
    x as Integer
    y as Integer
end

struct Rectangle
    top_left as Point
    width as Integer
    height as Integer
end
""")
            
            index = WorkspaceIndex(tmpdir)
            file_uri = index._path_to_uri(test_file)
            symbols = index.index_file(file_uri, test_file)
            
            # Check struct symbols
            struct_symbols = [s for s in symbols if s.kind == 'struct']
            assert len(struct_symbols) == 2
            
            struct_names = {s.name for s in struct_symbols}
            assert 'Point' in struct_names
            assert 'Rectangle' in struct_names
    
    def test_get_symbol(self):
        """Test getting symbols by name."""
        with tempfile.TemporaryDirectory() as tmpdir:
            test_file = os.path.join(tmpdir, "test.nxl")
            with open(test_file, 'w') as f:
                f.write("""
function greet with name as String returns String
    return "Hello " + name
end
""")
            
            index = WorkspaceIndex(tmpdir)
            file_uri = index._path_to_uri(test_file)
            index.index_file(file_uri, test_file)
            
            # Get symbol by name
            symbols = index.get_symbol("greet")
            assert len(symbols) == 1
            assert symbols[0].name == "greet"
            assert symbols[0].kind == "function"
            
            # Non-existent symbol
            assert len(index.get_symbol("nonexistent")) == 0
    
    def test_get_symbols_in_file(self):
        """Test getting all symbols from a file."""
        with tempfile.TemporaryDirectory() as tmpdir:
            test_file = os.path.join(tmpdir, "test.nxl")
            with open(test_file, 'w') as f:
                f.write("""
function func1 returns Integer
    return 42
end

function func2 returns String
    return "hello"
end

class MyClass
    function method1 returns Integer
        return 1
    end
end
""")
            
            index = WorkspaceIndex(tmpdir)
            file_uri = index._path_to_uri(test_file)
            index.index_file(file_uri, test_file)
            
            # Get all symbols from file
            symbols = index.get_symbols_in_file(file_uri)
            
            # Should have: 2 functions, 1 class, 1 method, parameters
            func_and_class = [s for s in symbols if s.kind in ('function', 'class', 'method')]
            assert len(func_and_class) >= 4  # At least 2 functions + 1 class + 1 method
    
    def test_scan_workspace(self):
        """Test scanning entire workspace."""
        with tempfile.TemporaryDirectory() as tmpdir:
            # Create multiple files
            file1 = os.path.join(tmpdir, "file1.nxl")
            with open(file1, 'w') as f:
                f.write("function func1 returns Integer\n    return 1\nend\n")
            
            file2 = os.path.join(tmpdir, "file2.nxl")
            with open(file2, 'w') as f:
                f.write("function func2 returns Integer\n    return 2\nend\n")
            
            # Create subdirectory with file
            subdir = os.path.join(tmpdir, "subdir")
            os.makedirs(subdir)
            file3 = os.path.join(subdir, "file3.nxl")
            with open(file3, 'w') as f:
                f.write("function func3 returns Integer\n    return 3\nend\n")
            
            # Scan workspace
            index = WorkspaceIndex(tmpdir)
            index.scan_workspace()
            
            # Check that all files were indexed
            assert len(index.indexed_files) == 3
            
            # Check that all functions are found
            assert len(index.get_symbol("func1")) == 1
            assert len(index.get_symbol("func2")) == 1
            assert len(index.get_symbol("func3")) == 1
    
    def test_skip_hidden_directories(self):
        """Test that hidden directories are skipped."""
        with tempfile.TemporaryDirectory() as tmpdir:
            # Create .git directory with NexusLang file (should be skipped)
            gitdir = os.path.join(tmpdir, ".git")
            os.makedirs(gitdir)
            git_file = os.path.join(gitdir, "hidden.nxl")
            with open(git_file, 'w') as f:
                f.write("function hidden returns Integer\n    return 0\nend\n")
            
            # Create normal file
            normal_file = os.path.join(tmpdir, "normal.nxl")
            with open(normal_file, 'w') as f:
                f.write("function visible returns Integer\n    return 1\nend\n")
            
            # Scan workspace
            index = WorkspaceIndex(tmpdir)
            index.scan_workspace()
            
            # Should only index normal file, not hidden
            assert len(index.indexed_files) == 1
            assert len(index.get_symbol("visible")) == 1
            assert len(index.get_symbol("hidden")) == 0
    
    def test_incremental_reindex(self):
        """Test re-indexing a file after changes."""
        with tempfile.TemporaryDirectory() as tmpdir:
            test_file = os.path.join(tmpdir, "test.nxl")
            
            # Initial content
            with open(test_file, 'w') as f:
                f.write("function old_func returns Integer\n    return 1\nend\n")
            
            index = WorkspaceIndex(tmpdir)
            file_uri = index._path_to_uri(test_file)
            index.index_file(file_uri, test_file)
            
            # Check old function exists
            assert len(index.get_symbol("old_func")) == 1
            
            # Update file content
            with open(test_file, 'w') as f:
                f.write("function new_func returns Integer\n    return 2\nend\n")
            
            # Re-index
            index.index_file(file_uri, test_file)
            
            # Check old function removed, new function added
            assert len(index.get_symbol("old_func")) == 0
            assert len(index.get_symbol("new_func")) == 1
    
    def test_find_symbols_fuzzy(self):
        """Test fuzzy symbol search."""
        with tempfile.TemporaryDirectory() as tmpdir:
            test_file = os.path.join(tmpdir, "test.nxl")
            with open(test_file, 'w') as f:
                f.write("""
function calculate_total returns Integer
    return 42
end

function calculate_average returns Float
    return 3.14
end

function process_data returns String
    return "done"
end
""")
            
            index = WorkspaceIndex(tmpdir)
            file_uri = index._path_to_uri(test_file)
            index.index_file(file_uri, test_file)
            
            # Search for "calculate" (should match 2 functions)
            results = index.find_symbols("calculate")
            func_results = [r for r in results if r.kind == 'function']
            assert len(func_results) >= 2
            
            # Search for "total" (should match 1 function)
            results = index.find_symbols("total")
            assert len(results) >= 1
            assert any(s.name == "calculate_total" for s in results)
    
    def test_find_symbols_by_kind(self):
        """Test filtering symbol search by kind."""
        with tempfile.TemporaryDirectory() as tmpdir:
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
""")
            
            index = WorkspaceIndex(tmpdir)
            file_uri = index._path_to_uri(test_file)
            index.index_file(file_uri, test_file)
            
            # Search for "my" with kind filter
            functions = index.find_symbols("my", kind="function")
            classes = index.find_symbols("my", kind="class")
            methods = index.find_symbols("my", kind="method")
            
            assert len(functions) >= 1
            assert len(classes) >= 1
            assert len(methods) >= 1
            
            assert functions[0].kind == "function"
            assert classes[0].kind == "class"
            assert methods[0].kind == "method"
    
    def test_get_statistics(self):
        """Test getting indexing statistics."""
        with tempfile.TemporaryDirectory() as tmpdir:
            test_file = os.path.join(tmpdir, "test.nxl")
            with open(test_file, 'w') as f:
                f.write("""
function func1 returns Integer
    return 1
end

class MyClass
    function method1 returns Integer
        return 2
    end
end

struct Point
    x as Integer
    y as Integer
end
""")
            
            index = WorkspaceIndex(tmpdir)
            file_uri = index._path_to_uri(test_file)
            index.index_file(file_uri, test_file)
            
            stats = index.get_statistics()
            
            assert stats['files_indexed'] == 1
            assert stats['functions'] >= 1
            assert stats['classes'] >= 1
            assert stats['methods'] >= 1
            assert stats['structs'] >= 1
            assert stats['total_symbols'] > 0

    def test_extracts_real_line_info_for_variable_and_struct_fields(self):
        """Global variables and struct fields should use AST line metadata, not line 0 placeholders."""
        with tempfile.TemporaryDirectory() as tmpdir:
            test_file = os.path.join(tmpdir, "test.nxl")
            with open(test_file, 'w') as f:
                f.write(
                    """set global_count to 1
struct Point
    x as Integer
    y as Integer
end
"""
                )

            index = WorkspaceIndex(tmpdir)
            file_uri = index._path_to_uri(test_file)
            symbols = index.index_file(file_uri, test_file)

            global_var = next(s for s in symbols if s.kind == 'variable' and s.name == 'global_count')
            assert global_var.line == 0
            assert global_var.column > 0

            fields = [s for s in symbols if s.kind == 'field' and s.scope == 'Point']
            assert len(fields) == 2
            assert all(field.line > 0 for field in fields)


if __name__ == '__main__':
    pytest.main([__file__, '-v'])
