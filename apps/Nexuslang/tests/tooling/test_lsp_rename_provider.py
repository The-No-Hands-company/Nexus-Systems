from pathlib import Path

import pytest

from nexuslang.lsp.rename import RenameProvider


class _Pos:
    def __init__(self, line: int, character: int):
        self.line = line
        self.character = character


class _Server:
    def __init__(self):
        self.documents = {}
        self.workspace_index = None


class _WorkspaceIndex:
    def __init__(self, indexed_files):
        self.indexed_files = indexed_files

    def _uri_to_path(self, uri: str) -> str:
        if uri.startswith("file://"):
            return uri.replace("file://", "", 1)
        return uri


class _FakeLocation:
    def __init__(self, line: int, column: int):
        self.line = line
        self.column = column


class _FakeSymbol:
    def __init__(self, name: str, line: int, column: int):
        self.name = name
        self.location = _FakeLocation(line, column)


class _FakeSymbolTable:
    def __init__(self, symbol):
        self._symbol = symbol

    def get_symbol_at_position(self, _uri, _line, _character):
        return self._symbol


@pytest.fixture
def provider():
    return RenameProvider(_Server())


def test_get_or_build_symbol_table_caches_result(provider):
    text = "set value to 1\nset result to value\n"
    uri = "file:///main.nxl"

    table = provider._get_or_build_symbol_table(text, uri)

    assert table is not None
    assert provider.symbol_tables[uri] is table


def test_get_or_build_symbol_table_falls_back_to_cached_on_recoverable_error(provider, monkeypatch):
    uri = "file:///main.nxl"
    sentinel = object()
    provider.symbol_tables[uri] = sentinel

    class _BadLexer:
        def __init__(self, _text):
            pass

        def tokenize(self):
            raise ValueError("tokenize failed")

    monkeypatch.setattr("nexuslang.lsp.rename.Lexer", _BadLexer)

    table = provider._get_or_build_symbol_table("set value to 1", uri)

    assert table is sentinel


def test_prepare_rename_uses_ast_symbol_table(provider, monkeypatch):
    uri = "file:///main.nxl"
    symbol = _FakeSymbol("value", 0, 4)
    table = _FakeSymbolTable(symbol)

    monkeypatch.setattr(provider, "_get_or_build_symbol_table", lambda _t, _u: table)

    out = provider.prepare_rename("set value to 1", _Pos(0, 5), uri)

    assert out["placeholder"] == "value"
    assert out["range"]["start"] == {"line": 0, "character": 4}
    assert out["range"]["end"] == {"line": 0, "character": 9}


def test_prepare_rename_falls_back_when_ast_symbol_missing(provider, monkeypatch):
    uri = "file:///main.nxl"
    table = _FakeSymbolTable(None)
    monkeypatch.setattr(provider, "_get_or_build_symbol_table", lambda _t, _u: table)
    monkeypatch.setattr(provider, "_fallback_prepare_rename", lambda *_a, **_k: {"placeholder": "value"})

    out = provider.prepare_rename("set value to 1", _Pos(0, 5), uri)

    assert out == {"placeholder": "value"}


def test_fallback_prepare_rename_builds_symbol_range(provider):
    text = "set my_var to 1\n"

    out = provider._fallback_prepare_rename(text, _Pos(0, 6), "file:///main.nxl")

    assert out["placeholder"] == "my_var"
    assert out["range"]["start"] == {"line": 0, "character": 4}
    assert out["range"]["end"] == {"line": 0, "character": 10}


def test_rename_validates_name_and_symbol_resolution(provider, monkeypatch):
    text = "set value to 1\n"

    assert provider.rename(text, _Pos(0, 5), "file:///main.nxl", "1bad") is None

    monkeypatch.setattr(provider, "_is_valid_identifier", lambda _n: True)
    monkeypatch.setattr(provider, "_get_symbol_at_position", lambda *_a, **_k: None)
    assert provider.rename(text, _Pos(0, 5), "file:///main.nxl", "good_name") is None


def test_rename_returns_none_when_not_renameable(provider, monkeypatch):
    text = "set value to 1\n"

    monkeypatch.setattr(provider, "_is_valid_identifier", lambda _n: True)
    monkeypatch.setattr(provider, "_get_symbol_at_position", lambda *_a, **_k: "value")
    monkeypatch.setattr(provider, "_is_renameable", lambda *_a, **_k: False)

    assert provider.rename(text, _Pos(0, 5), "file:///main.nxl", "renamed") is None


def test_rename_scans_open_and_indexed_workspace_files(tmp_path):
    server = _Server()
    provider = RenameProvider(server)

    main_uri = "file:///main.nxl"
    main_text = "set value to 1\nset result to value\n"
    server.documents[main_uri] = main_text

    external_file = tmp_path / "other.nxl"
    external_file.write_text("set value to 2\nprint text value\n", encoding="utf-8")
    ext_uri = f"file://{external_file}"
    server.workspace_index = _WorkspaceIndex([ext_uri])

    edit = provider.rename(main_text, _Pos(0, 5), main_uri, "renamed_value")

    assert edit is not None
    assert main_uri in edit["changes"]
    assert ext_uri in edit["changes"]


def test_get_symbol_at_position_handles_out_of_bounds(provider):
    text = "set value to 1\n"

    assert provider._get_symbol_at_position(text, _Pos(3, 0)) is None
    assert provider._get_symbol_at_position(text, _Pos(0, 100)) is None
    assert provider._get_symbol_at_position(text, _Pos(0, 5)) == "value"


def test_symbol_keyword_and_identifier_rules(provider):
    assert provider._is_renameable("", _Pos(0, 0), "function") is False
    assert provider._is_renameable("", _Pos(0, 0), "custom_name") is True

    assert provider._is_valid_identifier("") is False
    assert provider._is_valid_identifier("1abc") is False
    assert provider._is_valid_identifier("bad-name") is False
    assert provider._is_valid_identifier("class") is False
    assert provider._is_valid_identifier("good_name") is True


def test_get_symbol_type_paths(provider):
    assert provider._get_symbol_type("function add with x\n", _Pos(0, 10), "add") == "function"
    assert provider._get_symbol_type("class Box\n", _Pos(0, 7), "Box") == "class"

    method_text = "\nclass Foo\nmethod run with x\nend\n"
    assert provider._get_symbol_type(method_text, _Pos(2, 10), "run") == "method"

    assert provider._get_symbol_type("set count to 1\n", _Pos(0, 5), "count") == "variable"
    assert provider._get_symbol_type("print text unknown\n", _Pos(0, 11), "unknown") == "variable"


def test_find_and_replace_dispatches_by_symbol_type(provider):
    text = "set value to 1\n"

    function_edits = provider._find_and_replace_in_document(text, "value", "renamed", "function", "file:///main.nxl")
    class_edits = provider._find_and_replace_in_document(text, "value", "renamed", "class", "file:///main.nxl")
    method_edits = provider._find_and_replace_in_document(text, "value", "renamed", "method", "file:///main.nxl")
    variable_edits = provider._find_and_replace_in_document(text, "value", "renamed", "variable", "file:///main.nxl")

    assert isinstance(function_edits, list)
    assert isinstance(class_edits, list)
    assert isinstance(method_edits, list)
    assert isinstance(variable_edits, list)


def test_replace_helpers_and_standalone_reference_filter(provider):
    function_lines = [
        "function add with x",
        "set y to add with 1",
        "call add",
        "set copy to add",
    ]
    function_edits = provider._replace_function_refs(function_lines, "add", "sum")
    assert len(function_edits) >= 4

    class_lines = [
        "class Box",
        "set x to new Box",
        "set y as Box",
    ]
    class_edits = provider._replace_class_refs(class_lines, "Box", "Container")
    assert len(class_edits) == 3

    method_lines = [
        "function run with x",
        "set y to obj.run",
    ]
    method_edits = provider._replace_method_refs(method_lines, "run", "execute")
    assert len(method_edits) == 2

    variable_lines = [
        "set count to 1",
        "set total to count plus count",
        "set account to 2",
        "print text \"count\"",
    ]
    variable_edits = provider._replace_variable_refs(variable_lines, "count", "n")

    starts = {(e["range"]["start"]["line"], e["range"]["start"]["character"]) for e in variable_edits}
    assert (2, 4) not in starts  # account should not match count
    assert (3, 12) not in starts  # quoted string should not be renamed

    assert provider._is_standalone_reference("set account to 1", 4, "count") is False
    assert provider._is_standalone_reference("print text \"count\"", 12, "count") is False
    assert provider._is_standalone_reference("set count to 1", 4, "count") is True
