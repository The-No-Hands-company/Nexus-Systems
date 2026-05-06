"""LSP tooling coverage for switch/case/label semantic tokens and outline symbols."""

import os
import tempfile
from unittest.mock import MagicMock

from nexuslang.lsp.semantic_tokens import SemanticTokensProvider
from nexuslang.lsp.server import NLPLLanguageServer
from nexuslang.lsp.workspace_index import WorkspaceIndex


_SWITCH_LABEL_SOURCE = """
function main returns Integer
    set x to 1
    label outer: while x is less than 3
        switch x
            case 1
                set x to x plus 1
                continue outer
            case 2
                set x to x plus 1
                fallthrough
            case 3
                break outer
            default
                break
    end
    return x
end
"""


def _decode_semantic_tokens(data):
    """Decode LSP delta-encoded semantic token array into absolute tuples."""
    decoded = []
    line = 0
    char = 0
    for i in range(0, len(data), 5):
        delta_line, delta_char, length, token_type, modifiers = data[i:i + 5]
        line += delta_line
        if delta_line == 0:
            char += delta_char
        else:
            char = delta_char
        decoded.append((line, char, length, token_type, modifiers))
    return decoded


def test_semantic_tokens_include_switch_case_label_keywords():
    provider = SemanticTokensProvider(server=MagicMock())

    data = provider.get_semantic_tokens(_SWITCH_LABEL_SOURCE, "file:///switch_tokens.nxl")
    decoded = _decode_semantic_tokens(data)

    keyword_type = provider.TOKEN_TYPES.index("keyword")
    keyword_tokens = [t for t in decoded if t[3] == keyword_type]

    assert keyword_tokens, "Expected keyword semantic tokens"

    lines = _SWITCH_LABEL_SOURCE.split("\n")
    found_keywords = set()
    for line, char, length, _, _ in keyword_tokens:
        if line < len(lines):
            found_keywords.add(lines[line][char:char + length].lower())

    assert "switch" in found_keywords
    assert "case" in found_keywords
    assert "default" in found_keywords
    assert "label" in found_keywords
    assert "fallthrough" in found_keywords


def test_workspace_index_extracts_switch_case_and_label_symbols():
    with tempfile.TemporaryDirectory() as tmpdir:
        test_file = os.path.join(tmpdir, "switch_symbols.nxl")
        with open(test_file, "w", encoding="utf-8") as f:
            f.write(_SWITCH_LABEL_SOURCE)

        index = WorkspaceIndex(tmpdir)
        index.scan_workspace()

        file_uri = index._path_to_uri(test_file)
        symbols = index.get_symbols_in_file(file_uri)

        kinds = {s.kind for s in symbols}
        names = {s.name for s in symbols}

        assert "label" in kinds
        assert "switch" in kinds
        assert "case" in kinds
        assert "outer" in names
        assert "switch" in names


def test_document_symbols_include_switch_label_children_under_function():
    with tempfile.TemporaryDirectory() as tmpdir:
        test_file = os.path.join(tmpdir, "switch_outline.nxl")
        with open(test_file, "w", encoding="utf-8") as f:
            f.write(_SWITCH_LABEL_SOURCE)

        server = NLPLLanguageServer()
        server.workspace_index = WorkspaceIndex(tmpdir)
        server.workspace_index.scan_workspace()

        file_uri = server.workspace_index._path_to_uri(test_file)
        params = {"textDocument": {"uri": file_uri}}
        response = server._handle_document_symbol(1, params)

        symbols = response["result"]
        main_symbol = next((s for s in symbols if s["name"] == "main"), None)
        assert main_symbol is not None, "Expected function symbol for main"
        assert "children" in main_symbol and main_symbol["children"], "Expected child symbols under main"

        child_names = {child["name"] for child in main_symbol["children"]}
        assert "outer" in child_names
        assert "switch" in child_names
        assert "default" in child_names
