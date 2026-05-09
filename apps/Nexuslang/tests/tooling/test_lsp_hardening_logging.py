import logging
from unittest.mock import patch

from nexuslang.lsp.code_actions import CodeActionsProvider
from nexuslang.lsp.definitions import DefinitionProvider
from nexuslang.lsp.diagnostics import DiagnosticsProvider
from nexuslang.lsp.formatter import NexusLangFormatter
from nexuslang.lsp.rename import RenameProvider
from nexuslang.lsp.references import ReferencesProvider
from nexuslang.lsp.semantic_tokens import SemanticTokensProvider
from nexuslang.lsp.server import NexusLangLanguageServer
from nexuslang.lsp.inlay_hints import InlayHintsProvider


class _Pos:
    def __init__(self, line: int, character: int):
        self.line = line
        self.character = character


class _BadWorkspaceIndex:
    def __init__(self, indexed_files):
        self.indexed_files = indexed_files

    def _uri_to_path(self, _uri: str) -> str:
        return "/definitely/missing/file.nxl"

    def get_symbol(self, _symbol: str):
        return []


class _BrokenSymbolsIndex:
    def get_all_symbols(self):
        raise RuntimeError("workspace index unavailable")


class _Server:
    def __init__(self):
        self.documents = {}
        self.workspace_index = None


def test_rename_logs_workspace_file_scan_failures(caplog):
    server = _Server()
    uri = "file:///main.nxl"
    text = "set value to 1\nset result to value\n"
    server.documents[uri] = text
    server.workspace_index = _BadWorkspaceIndex(["file:///missing.nxl"])

    provider = RenameProvider(server)

    with caplog.at_level(logging.WARNING):
        edit = provider.rename(text, _Pos(0, 5), uri, "renamed_value")

    assert edit is not None
    assert "Skipping rename scan for file:///missing.nxl" in caplog.text


def test_references_logs_workspace_file_scan_failures(caplog):
    server = _Server()
    uri = "file:///main.nxl"
    text = "set value to 1\nset result to value\n"
    server.documents[uri] = text
    server.workspace_index = _BadWorkspaceIndex(["file:///missing.nxl"])

    provider = ReferencesProvider(server)

    with caplog.at_level(logging.WARNING):
        refs = provider.find_references(text, _Pos(0, 5), uri, include_declaration=True)

    assert isinstance(refs, list)
    assert "Skipping references scan for file:///missing.nxl" in caplog.text


def test_inlay_hints_logs_workspace_symbol_enrichment_failures(caplog):
    server = _Server()
    server.workspace_index = _BrokenSymbolsIndex()

    provider = InlayHintsProvider(server)
    src = (
        "function add with first as Integer and second as Integer returns Integer\n"
        "    return first plus second\n"
        "end\n"
        "set result to add with 1 and 2\n"
    )

    with caplog.at_level(logging.DEBUG):
        hints = provider.get_inlay_hints("file:///main.nxl", src)

    assert isinstance(hints, list)
    assert "Skipping workspace-index parameter cache enrichment" in caplog.text


def test_semantic_tokens_returns_cached_symbol_table_on_parse_failure():
    server = _Server()
    provider = SemanticTokensProvider(server)
    uri = "file:///main.nxl"
    sentinel = object()
    provider.symbol_tables[uri] = sentinel

    class _BadLexer:
        def __init__(self, _text):
            pass

        def tokenize(self):
            raise ValueError("tokenization failed")

    with patch("nexuslang.lsp.semantic_tokens.Lexer", _BadLexer):
        result = provider._get_or_build_symbol_table("set x to 1", uri)

    assert result is sentinel


def test_code_actions_returns_cached_symbol_table_on_parse_failure():
    server = _Server()
    provider = CodeActionsProvider(server)
    uri = "file:///main.nxl"
    sentinel = object()
    provider.symbol_tables[uri] = sentinel

    class _BadLexer:
        def __init__(self, _text):
            pass

        def tokenize(self):
            raise ValueError("tokenization failed")

    with patch("nexuslang.lsp.code_actions.Lexer", _BadLexer):
        result = provider._get_or_build_symbol_table("set x to 1", uri)

    assert result is sentinel


def test_definitions_module_text_read_failure_logs_and_returns_none(caplog):
    provider = DefinitionProvider(_Server())

    with caplog.at_level(logging.WARNING):
        text = provider._get_module_text("file:///missing.nxl", "/missing/module.nxl")

    assert text is None
    assert "Failed to read module text" in caplog.text


def test_diagnostics_type_check_failure_logs_and_returns_empty(caplog):
    provider = DiagnosticsProvider(_Server())

    class _BadLexer:
        def __init__(self, _text):
            pass

        def tokenize(self):
            raise ValueError("tokenization failed")

    with patch("nexuslang.parser.lexer.Lexer", _BadLexer):
        with caplog.at_level(logging.DEBUG):
            diags = provider._check_type_errors("set x to 1")

    assert diags == []
    assert "Type checker diagnostics failed" in caplog.text


def test_formatter_falls_back_to_regex_on_token_pass_failure():
    formatter = NexusLangFormatter()

    with patch.object(formatter, "_format_with_tokens", side_effect=ValueError("bad token pass")):
        with patch.object(formatter, "_format_regex", return_value="fallback-result") as fallback:
            result = formatter.format("set value to 1")

    assert result == "fallback-result"
    assert fallback.called


def test_server_get_or_parse_returns_none_on_parse_failure():
    server = NexusLangLanguageServer.__new__(NexusLangLanguageServer)
    server._parse_cache = {}

    class _BadLexer:
        def __init__(self, _text):
            pass

        def tokenize(self):
            raise ValueError("tokenization failed")

    with patch("nexuslang.parser.lexer.Lexer", _BadLexer):
        result = NexusLangLanguageServer.get_or_parse(server, "file:///main.nxl", "set x to 1")

    assert result is None


def test_server_outgoing_calls_open_failure_returns_empty_result():
    class _Index:
        def _uri_to_path(self, _uri):
            return "/definitely/missing/file.nxl"

    server = NexusLangLanguageServer.__new__(NexusLangLanguageServer)
    server.workspace_index = _Index()

    response = NexusLangLanguageServer._handle_outgoing_calls(
        server,
        7,
        {
            "item": {
                "uri": "file:///missing.nxl",
                "range": {"start": {"line": 0, "character": 0}},
            }
        },
    )

    assert response["id"] == 7
    assert response["result"] == []
