import logging

from nexuslang.lsp.rename import RenameProvider
from nexuslang.lsp.references import ReferencesProvider
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
