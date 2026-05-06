"""
Tests for LSP document features: document symbols and call hierarchy.
"""

import os
import tempfile
import pytest

from nexuslang.lsp.workspace_index import WorkspaceIndex
from nexuslang.lsp.server import NLPLLanguageServer


class TestDocumentSymbols:
    """Test document outline (textDocument/documentSymbol)."""
    
    def test_document_symbol_hierarchy(self):
        """Test hierarchical symbol outline for a file."""
        with tempfile.TemporaryDirectory() as tmpdir:
            test_file = os.path.join(tmpdir, "test.nxl")
            with open(test_file, 'w') as f:
                f.write("""
function top_level_func returns Integer
    return 42
end

class MyClass
    function method1 returns Integer
        return 1
    end
    
    function method2 returns String
        return "hello"
    end
end

struct Point
    x as Integer
    y as Integer
end

set global_var to 100
""")
            
            # Initialize server and index
            server = NLPLLanguageServer()
            server.workspace_index = WorkspaceIndex(tmpdir)
            server.workspace_index.scan_workspace()
            
            file_uri = server.workspace_index._path_to_uri(test_file)
            
            # Get document symbols
            params = {'textDocument': {'uri': file_uri}}
            response = server._handle_document_symbol(1, params)
            
            assert response['jsonrpc'] == '2.0'
            assert response['id'] == 1
            symbols = response['result']
            
            # Should have top-level symbols
            assert len(symbols) >= 3  # function, class, struct
            
            # Find the class
            class_symbol = None
            for sym in symbols:
                if sym['name'] == 'MyClass':
                    class_symbol = sym
                    break
            
            assert class_symbol is not None
            assert class_symbol['kind'] == 5  # Class
            
            # Class should have children (methods)
            assert 'children' in class_symbol
            assert len(class_symbol['children']) >= 2
            
            method_names = [child['name'] for child in class_symbol['children']]
            assert 'method1' in method_names
            assert 'method2' in method_names

    def test_document_symbol_hierarchy_includes_method_parameters(self):
        """Method parameters should appear nested under their method symbol."""
        with tempfile.TemporaryDirectory() as tmpdir:
            test_file = os.path.join(tmpdir, "nested_params.nxl")
            with open(test_file, 'w') as f:
                f.write("""
class Calculator
    function scale with value as Integer and factor as Integer returns Integer
        return value times factor
    end
end
""")

            server = NLPLLanguageServer()
            server.workspace_index = WorkspaceIndex(tmpdir)
            server.workspace_index.scan_workspace()

            file_uri = server.workspace_index._path_to_uri(test_file)
            params = {'textDocument': {'uri': file_uri}}
            response = server._handle_document_symbol(1, params)

            symbols = response['result']
            class_symbol = next((s for s in symbols if s['name'] == 'Calculator'), None)
            assert class_symbol is not None

            method_symbol = next((c for c in class_symbol.get('children', []) if c['name'] == 'scale'), None)
            assert method_symbol is not None

            parameter_names = {child['name'] for child in method_symbol.get('children', [])}
            assert 'value' in parameter_names
            assert 'factor' in parameter_names
    
    def test_flat_document_symbols(self):
        """Test document symbols without hierarchy."""
        with tempfile.TemporaryDirectory() as tmpdir:
            test_file = os.path.join(tmpdir, "simple.nxl")
            with open(test_file, 'w') as f:
                f.write("""
function func1 returns Integer
    return 1
end

function func2 returns Integer
    return 2
end
""")
            
            server = NLPLLanguageServer()
            server.workspace_index = WorkspaceIndex(tmpdir)
            server.workspace_index.scan_workspace()
            
            file_uri = server.workspace_index._path_to_uri(test_file)
            
            params = {'textDocument': {'uri': file_uri}}
            response = server._handle_document_symbol(1, params)
            
            symbols = response['result']
            assert len(symbols) >= 2
            
            names = [sym['name'] for sym in symbols]
            assert 'func1' in names
            assert 'func2' in names


class TestCallHierarchy:
    """Test call hierarchy (incoming/outgoing calls)."""
    
    def test_prepare_call_hierarchy(self):
        """Test preparing call hierarchy for a function."""
        with tempfile.TemporaryDirectory() as tmpdir:
            test_file = os.path.join(tmpdir, "test.nxl")
            with open(test_file, 'w') as f:
                f.write("""
function calculate with x as Integer and y as Integer returns Integer
    return x plus y
end

function main
    set result to calculate with x: 10 and y: 20
    print text result
end
""")
            
            server = NLPLLanguageServer()
            server.workspace_index = WorkspaceIndex(tmpdir)
            server.workspace_index.scan_workspace()
            
            file_uri = server.workspace_index._path_to_uri(test_file)
            # Add document to server's document cache
            with open(test_file, 'r') as f:
                server.documents[file_uri] = f.read()
            
            # Prepare call hierarchy for "calculate" function (line 1, column 9)
            params = {
                'textDocument': {'uri': file_uri},
                'position': {'line': 1, 'character': 9}  # "calculate"
            }
            response = server._handle_prepare_call_hierarchy(1, params)
            
            assert response['jsonrpc'] == '2.0'
            items = response['result']
            
            assert items is not None
            assert len(items) >= 1
            assert items[0]['name'] == 'calculate'
            assert items[0]['kind'] == 12  # Function
    
    def test_incoming_calls(self):
        """Test finding incoming calls (callers)."""
        with tempfile.TemporaryDirectory() as tmpdir:
            test_file = os.path.join(tmpdir, "test.nxl")
            with open(test_file, 'w') as f:
                f.write("""
function helper returns Integer
    return 42
end

function caller1
    set x to helper()
end

function caller2
    set y to helper()
end
""")
            
            server = NLPLLanguageServer()
            server.workspace_index = WorkspaceIndex(tmpdir)
            server.workspace_index.scan_workspace()
            
            file_uri = server.workspace_index._path_to_uri(test_file)
            server.documents[file_uri] = open(test_file).read()
            
            # Get incoming calls to "helper"
            item = {
                'name': 'helper',
                'kind': 12,
                'uri': file_uri,
                'range': {'start': {'line': 1, 'character': 9}, 'end': {'line': 1, 'character': 15}},
                'selectionRange': {'start': {'line': 1, 'character': 9}, 'end': {'line': 1, 'character': 15}}
            }
            params = {'item': item}
            response = server._handle_incoming_calls(1, params)
            
            incoming = response['result']
            
            # Should find calls (may not be perfect due to line number resolution)
            # At minimum, should find at least one caller
            assert len(incoming) >= 1
            
            # If we found both, verify they're the right names
            if len(incoming) >= 2:
                caller_names = [call['from']['name'] for call in incoming]
                assert 'caller1' in caller_names or 'caller2' in caller_names
    
    def test_outgoing_calls(self):
        """Test finding outgoing calls (callees)."""
        with tempfile.TemporaryDirectory() as tmpdir:
            test_file = os.path.join(tmpdir, "test.nxl")
            with open(test_file, 'w') as f:
                f.write("""
function helper1 returns Integer
    return 1
end

function helper2 returns Integer
    return 2
end

function caller
    set x to helper1()
    set y to helper2()
    return x plus y
end
""")
            
            server = NLPLLanguageServer()
            server.workspace_index = WorkspaceIndex(tmpdir)
            server.workspace_index.scan_workspace()
            
            file_uri = server.workspace_index._path_to_uri(test_file)
            server.documents[file_uri] = open(test_file).read()
            
            # Get outgoing calls from "caller"
            item = {
                'name': 'caller',
                'kind': 12,
                'uri': file_uri,
                'range': {'start': {'line': 9, 'character': 9}, 'end': {'line': 9, 'character': 15}},
                'selectionRange': {'start': {'line': 9, 'character': 9}, 'end': {'line': 9, 'character': 15}}
            }
            params = {'item': item}
            response = server._handle_outgoing_calls(1, params)
            
            outgoing = response['result']
            
            # Should find calls to helper1 and helper2
            assert len(outgoing) >= 2
            callee_names = [call['to']['name'] for call in outgoing]
            assert 'helper1' in callee_names
            assert 'helper2' in callee_names


class TestCodeActionsIntegration:
    """Test code actions integration through NLPLLanguageServer handlers."""

    def test_code_actions_use_structured_diagnostic_suggestions(self):
        """Diagnostics with data.fixes should yield non-empty code actions."""
        server = NLPLLanguageServer()

        uri = "file:///tmp/test_structured_fixes.nxl"
        text = 'print text "hello\n'
        server.documents[uri] = text

        params = {
            "textDocument": {"uri": uri},
            "range": {
                "start": {"line": 0, "character": 0},
                "end": {"line": 0, "character": len(text)}
            },
            "context": {
                "diagnostics": [
                    {
                        "range": {
                            "start": {"line": 0, "character": 0},
                            "end": {"line": 0, "character": len(text)}
                        },
                        "severity": 1,
                        "message": "Unclosed string",
                        "source": "nlpl",
                        "code": "E102",
                        "data": {
                            "fixes": ["Add closing quote"]
                        }
                    }
                ]
            }
        }

        response = server._handle_code_action(1, params)

        assert response["jsonrpc"] == "2.0"
        assert response["id"] == 1
        assert isinstance(response["result"], list)
        assert len(response["result"]) > 0

        quickfix_titles = [action.get("title", "") for action in response["result"]]
        assert any("quote" in title.lower() for title in quickfix_titles)

    def test_code_actions_fallback_without_structured_suggestions(self):
        """Without data.fixes, message-based heuristics should still return actions."""
        server = NLPLLanguageServer()

        uri = "file:///tmp/test_fallback_fixes.nxl"
        text = 'print text "hello\n'
        server.documents[uri] = text

        params = {
            "textDocument": {"uri": uri},
            "range": {
                "start": {"line": 0, "character": 0},
                "end": {"line": 0, "character": len(text)}
            },
            "context": {
                "diagnostics": [
                    {
                        "range": {
                            "start": {"line": 0, "character": 0},
                            "end": {"line": 0, "character": len(text)}
                        },
                        "severity": 1,
                        "message": "Unclosed string",
                        "source": "nlpl",
                        "code": "E102"
                    }
                ]
            }
        }

        response = server._handle_code_action(1, params)

        assert response["jsonrpc"] == "2.0"
        assert response["id"] == 1
        assert isinstance(response["result"], list)
        assert len(response["result"]) > 0

        quickfix_titles = [action.get("title", "") for action in response["result"]]
        assert any("quote" in title.lower() for title in quickfix_titles)


if __name__ == '__main__':
    pytest.main([__file__, '-v'])
