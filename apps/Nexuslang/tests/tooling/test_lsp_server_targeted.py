import io
import json
import tempfile
from pathlib import Path

import pytest

import nexuslang.lsp.server as lsp_server
from nexuslang.lsp.server import Location, NexusLangLanguageServer, Position, Range


class _FakeSymbol:
    def __init__(self, name, kind, line, column, file_uri, signature=None, scope=""):
        self.name = name
        self.kind = kind
        self.line = line
        self.column = column
        self.file_uri = file_uri
        self.signature = signature
        self.scope = scope


class _FakeWorkspaceIndex:
    def __init__(self, root):
        self.root = root
        self.scanned = False
        self.indexed_files = []

    def scan_workspace(self):
        self.scanned = True

    def get_statistics(self):
        return {"files_indexed": 1, "total_symbols": 2}

    def _uri_to_path(self, uri):
        return uri.replace("file://", "", 1)

    def get_symbols_in_file(self, _uri):
        return [
            _FakeSymbol("Outer", "class", 0, 6, "file:///doc.nxl"),
            _FakeSymbol("inner", "method", 1, 11, "file:///doc.nxl", scope="Outer"),
        ]

    def get_symbol(self, name):
        return [
            _FakeSymbol(name, "function", 0, 9, "file:///calls.nxl", signature=f"{name}()")
        ]


class _NoopThread:
    def __init__(self, target, daemon):
        self.target = target
        self.daemon = daemon

    def start(self):
        self.target()


def _new_server_without_init():
    server = NexusLangLanguageServer.__new__(NexusLangLanguageServer)
    server.documents = {}
    server.initialization_options = {}
    server.workspace_index = None
    server._parse_cache = {}
    server.semantic_tokens_provider = type(
        "_SemProvider", (), {"get_semantic_tokens_legend": staticmethod(lambda: {"tokenTypes": [], "tokenModifiers": []})}
    )()
    return server


def test_position_range_location_to_dict():
    pos = Position(3, 8)
    rng = Range(pos, Position(3, 12))
    loc = Location("file:///x.nxl", rng)

    assert pos.to_dict() == {"line": 3, "character": 8}
    assert rng.to_dict()["start"] == {"line": 3, "character": 8}
    assert loc.to_dict()["uri"] == "file:///x.nxl"


def test_read_message_parses_headers_and_body(monkeypatch):
    payload = {"jsonrpc": "2.0", "id": 5, "method": "initialize", "params": {}}
    body = json.dumps(payload).encode("utf-8")
    raw = b"Ignore-This-Header\r\nContent-Length: " + str(len(body)).encode("ascii") + b"\r\n\r\n" + body

    fake_stdin = type("_Stdin", (), {"buffer": io.BytesIO(raw)})()
    monkeypatch.setattr(lsp_server.sys, "stdin", fake_stdin)

    server = _new_server_without_init()
    message = server._read_message()

    assert message == payload


def test_read_message_handles_empty_and_decode_errors(monkeypatch):
    server = _new_server_without_init()

    empty_stdin = type("_Stdin", (), {"buffer": io.BytesIO(b"Content-Length: 0\r\n\r\n")})()
    monkeypatch.setattr(lsp_server.sys, "stdin", empty_stdin)
    assert server._read_message() is None

    bad_utf8 = type("_Stdin", (), {"buffer": io.BytesIO(b"\xff")})()
    monkeypatch.setattr(lsp_server.sys, "stdin", bad_utf8)
    assert server._read_message() is None


def test_write_message_serializes_lsp_frame(monkeypatch):
    class _Out:
        def __init__(self):
            self.buffer = io.BytesIO()

    fake_stdout = _Out()
    monkeypatch.setattr(lsp_server.sys, "stdout", fake_stdout)

    server = _new_server_without_init()
    server._write_message({"jsonrpc": "2.0", "id": 1, "result": None})

    wire = fake_stdout.buffer.getvalue().decode("utf-8")
    assert wire.startswith("Content-Length: ")
    assert "\r\n\r\n" in wire
    assert '"jsonrpc": "2.0"' in wire


def test_write_message_handles_stdout_failures(monkeypatch):
    class _Buffer:
        def write(self, _data):
            raise OSError("broken pipe")

        def flush(self):
            pass

    fake_stdout = type("_Out", (), {"buffer": _Buffer()})()
    monkeypatch.setattr(lsp_server.sys, "stdout", fake_stdout)

    server = _new_server_without_init()
    server._write_message({"jsonrpc": "2.0"})


def test_start_handles_recoverable_message_errors():
    server = _new_server_without_init()
    messages = iter([
        {"method": "ok", "id": 1, "params": {}},
        {"method": "boom", "id": 2, "params": {}},
        None,
    ])

    seen = []

    def _read_message():
        return next(messages)

    def _handle_message(msg):
        if msg["method"] == "boom":
            raise RuntimeError("recoverable")
        return {"jsonrpc": "2.0", "id": msg["id"], "result": {"ok": True}}

    def _write_message(resp):
        seen.append(resp["id"])

    server._read_message = _read_message
    server._handle_message = _handle_message
    server._write_message = _write_message

    server.start()
    assert seen == [1]


def test_handle_message_dispatch_and_exit(monkeypatch):
    server = _new_server_without_init()

    request_handlers = {
        "initialize": "_handle_initialize",
        "textDocument/completion": "_handle_completion",
        "textDocument/definition": "_handle_definition",
        "textDocument/hover": "_handle_hover",
        "textDocument/references": "_handle_references",
        "textDocument/prepareRename": "_handle_prepare_rename",
        "textDocument/rename": "_handle_rename",
        "textDocument/codeAction": "_handle_code_action",
        "textDocument/signatureHelp": "_handle_signature_help",
        "textDocument/formatting": "_handle_formatting",
        "workspace/symbol": "_handle_workspace_symbol",
        "textDocument/documentSymbol": "_handle_document_symbol",
        "textDocument/prepareCallHierarchy": "_handle_prepare_call_hierarchy",
        "callHierarchy/incomingCalls": "_handle_incoming_calls",
        "callHierarchy/outgoingCalls": "_handle_outgoing_calls",
        "textDocument/semanticTokens/full": "_handle_semantic_tokens_full",
        "textDocument/codeLens": "_handle_code_lens",
        "codeLens/resolve": "_handle_code_lens_resolve",
        "textDocument/inlayHint": "_handle_inlay_hint",
    }

    called = []

    def _mk(name):
        def _handler(msg_id, params):
            called.append((name, msg_id, params.get("x")))
            return {"jsonrpc": "2.0", "id": msg_id, "result": name}

        return _handler

    for method, handler_name in request_handlers.items():
        setattr(server, handler_name, _mk(handler_name))

    server._handle_did_open = lambda params: called.append(("didOpen", params.get("x")))
    server._handle_did_change = lambda params: called.append(("didChange", params.get("x")))
    server._handle_did_close = lambda params: called.append(("didClose", params.get("x")))

    for method in request_handlers:
        resp = server._handle_message({"method": method, "id": 9, "params": {"x": 1}})
        assert resp["id"] == 9

    assert server._handle_message({"method": "initialized", "id": 3, "params": {}}) is None
    assert server._handle_message({"method": "textDocument/didOpen", "params": {"x": 2}}) is None
    assert server._handle_message({"method": "textDocument/didChange", "params": {"x": 3}}) is None
    assert server._handle_message({"method": "textDocument/didClose", "params": {"x": 4}}) is None

    shutdown = server._handle_message({"method": "shutdown", "id": 11, "params": {}})
    assert shutdown == {"jsonrpc": "2.0", "id": 11, "result": None}

    monkeypatch.setattr(lsp_server.sys, "exit", lambda code: (_ for _ in ()).throw(SystemExit(code)))
    with pytest.raises(SystemExit) as exc:
        server._handle_message({"method": "exit", "id": 12, "params": {}})
    assert exc.value.code == 0

    assert server._handle_message({"method": "unknown/custom", "id": 15, "params": {}}) is None


def test_handle_initialize_workspace_folder_and_rooturi(monkeypatch):
    server = _new_server_without_init()

    monkeypatch.setattr("nexuslang.lsp.workspace_index.WorkspaceIndex", _FakeWorkspaceIndex)
    monkeypatch.setattr("threading.Thread", _NoopThread)

    response = server._handle_initialize(
        1,
        {
            "workspaceFolders": [{"uri": "file:///tmp/ws-folder"}],
            "initializationOptions": {"linting": {"enabled": True}},
        },
    )

    assert response["id"] == 1
    assert response["result"]["capabilities"]["renameProvider"]["prepareProvider"] is True
    assert isinstance(server.workspace_index, _FakeWorkspaceIndex)
    assert server.workspace_index.scanned is True

    server2 = _new_server_without_init()
    monkeypatch.setattr("nexuslang.lsp.workspace_index.WorkspaceIndex", _FakeWorkspaceIndex)
    monkeypatch.setattr("threading.Thread", _NoopThread)

    response2 = server2._handle_initialize(
        2,
        {
            "workspaceFolders": [],
            "rootUri": "file:///tmp/ws-root-uri",
            "initializationOptions": {},
        },
    )
    assert response2["id"] == 2
    assert isinstance(server2.workspace_index, _FakeWorkspaceIndex)


def test_did_open_change_close_and_document_symbol_fallback_and_index_paths(tmp_path):
    server = _new_server_without_init()

    class _DiagnosticsProvider:
        def get_diagnostics(self, _uri, _text):
            return [{"source": "parser"}]

        def merge_and_dedupe_diagnostics(self, diagnostics, dead, lint):
            return diagnostics + dead + lint

    class _DeadCodeProvider:
        def get_diagnostics(self, _uri, _text):
            return [{"source": "dead-code"}]

    published = []
    server.diagnostics_provider = _DiagnosticsProvider()
    server.dead_code_provider = _DeadCodeProvider()
    server._get_realtime_lint_diagnostics = lambda _u, _t: [{"source": "lint"}]
    server._publish_diagnostics = lambda uri, diags: published.append((uri, diags))

    index = _FakeWorkspaceIndex(str(tmp_path))
    server.workspace_index = index

    uri = "file:///tmp/doc.nxl"
    opened = {"textDocument": {"uri": uri, "text": "set value to 1\n"}}
    server._handle_did_open(opened)

    changed = {
        "textDocument": {"uri": uri},
        "contentChanges": [{"text": "set value to 2\n"}],
    }
    server._handle_did_change(changed)

    server._parse_cache[uri] = ("abc", object())
    server._handle_did_close({"textDocument": {"uri": uri}})

    assert uri not in server.documents
    assert uri not in server._parse_cache
    assert len(published) == 2
    assert {d["source"] for d in published[0][1]} == {"parser", "dead-code", "lint"}

    indexed_doc_symbols = server._handle_document_symbol(7, {"textDocument": {"uri": uri}})
    assert indexed_doc_symbols["result"][0]["name"] == "Outer"
    assert indexed_doc_symbols["result"][0]["children"][0]["name"] == "inner"

    server.workspace_index = None
    server.documents[uri] = "function make with x\nclass Box\nstruct Pair\nset top to 1\n"
    fallback_doc_symbols = server._handle_document_symbol(8, {"textDocument": {"uri": uri}})
    names = {s["name"] for s in fallback_doc_symbols["result"]}
    assert {"make", "Box", "Pair", "top"}.issubset(names)


def test_call_hierarchy_handlers_incoming_and_outgoing(tmp_path):
    server = _new_server_without_init()
    ws = _FakeWorkspaceIndex(str(tmp_path))

    calls_file = tmp_path / "calls.nxl"
    calls_file.write_text(
        "function target\n"
        "end\n"
        "function caller\n"
        "    target with 1\n"
        "end\n",
        encoding="utf-8",
    )

    ws.indexed_files = [f"file://{calls_file}"]
    server.workspace_index = ws
    uri = f"file://{calls_file}"
    server.documents[uri] = calls_file.read_text(encoding="utf-8")

    ws.get_symbols_in_file = lambda _uri: [
        _FakeSymbol("target", "function", 0, 9, uri, signature="target()"),
        _FakeSymbol("caller", "function", 2, 9, uri, signature="caller()"),
    ]

    prepared = server._handle_prepare_call_hierarchy(
        1,
        {"textDocument": {"uri": uri}, "position": {"line": 0, "character": 10}},
    )
    assert prepared["result"] is not None
    assert prepared["result"][0]["name"] == "target"

    incoming = server._handle_incoming_calls(2, {"item": {"name": "target"}})
    assert incoming["result"]
    assert incoming["result"][0]["from"]["name"] == "caller"

    outgoing = server._handle_outgoing_calls(
        3,
        {
            "item": {
                "uri": uri,
                "range": {"start": {"line": 2, "character": 0}},
            }
        },
    )
    assert outgoing["result"]
    assert outgoing["result"][0]["to"]["name"] == "target"


def test_request_handler_methods_and_fallback_paths(monkeypatch):
    server = _new_server_without_init()
    uri = "file:///handlers.nxl"
    server.documents[uri] = "set value to 1\n"

    class _CompletionProvider:
        def get_completions(self, text, position):
            assert text
            assert position.line == 0
            return [{"label": "set"}]

    class _DefinitionProvider:
        def get_definition(self, text, position, doc_uri):
            assert text
            assert doc_uri == uri
            return Location(doc_uri, Range(Position(0, 4), Position(0, 9)))

    class _HoverProvider:
        def get_hover(self, _text, _position):
            return {"contents": "hover"}

    class _ReferencesProvider:
        def find_references(self, _text, _position, _uri, include_decl):
            return [{"uri": _uri, "include": include_decl}]

    class _RenameProvider:
        def prepare_rename(self, _text, _position, _uri):
            return {"placeholder": "value"}

        def rename(self, _text, _position, _uri, new_name):
            return {"changes": {_uri: [{"newText": new_name}]}}

    class _CodeActionsProvider:
        def get_code_actions(self, _uri, _text, _range, diagnostics):
            return [{"title": "Fix", "diagnostics": diagnostics}]

    class _SignatureProvider:
        def get_signature_help(self, _text, _position):
            return {"signatures": [{"label": "f(x)"}]}

    class _Formatter:
        def get_formatting_edits(self, _text):
            return [{"newText": "formatted"}]

    class _SymbolProvider:
        def find_symbols(self, query, _docs):
            return [{"name": query or "value"}]

    class _SemanticProvider:
        def get_semantic_tokens_legend(self):
            return {"tokenTypes": [], "tokenModifiers": []}

        def get_semantic_tokens(self, _text, _uri):
            return [0, 0, 3, 0, 0]

    class _CodeLensProvider:
        def get_code_lenses(self, _uri, _text):
            return [{"range": {"start": {"line": 0, "character": 0}}}]

        def resolve_code_lens(self, lens):
            out = dict(lens)
            out["resolved"] = True
            return out

    class _InlayProvider:
        def get_inlay_hints(self, _uri, _text, _range):
            return [{"label": "hint"}]

    server.completion_provider = _CompletionProvider()
    server.definition_provider = _DefinitionProvider()
    server.hover_provider = _HoverProvider()
    server.references_provider = _ReferencesProvider()
    server.rename_provider = _RenameProvider()
    server.code_actions_provider = _CodeActionsProvider()
    server.signature_help_provider = _SignatureProvider()
    server.formatter = _Formatter()
    server.symbol_provider = _SymbolProvider()
    server.semantic_tokens_provider = _SemanticProvider()
    server.code_lens_provider = _CodeLensProvider()
    server.inlay_hints_provider = _InlayProvider()

    position_params = {"textDocument": {"uri": uri}, "position": {"line": 0, "character": 5}}

    assert server._handle_completion(1, position_params)["result"]["items"][0]["label"] == "set"
    assert server._handle_definition(2, position_params)["result"]["uri"] == uri
    assert server._handle_hover(3, position_params)["result"]["contents"] == "hover"
    assert server._handle_references(4, {**position_params, "context": {"includeDeclaration": False}})["result"][0]["include"] is False
    assert server._handle_prepare_rename(5, position_params)["result"]["placeholder"] == "value"
    assert server._handle_rename(6, {**position_params, "newName": "renamed"})["result"]["changes"][uri][0]["newText"] == "renamed"

    action_resp = server._handle_code_action(
        7,
        {
            "textDocument": {"uri": uri},
            "range": {"start": {"line": 0, "character": 0}, "end": {"line": 0, "character": 1}},
            "context": {"diagnostics": [{"code": "E1"}]},
        },
    )
    assert action_resp["result"][0]["diagnostics"][0]["code"] == "E1"
    assert server._handle_signature_help(8, position_params)["result"]["signatures"][0]["label"] == "f(x)"
    assert server._handle_formatting(9, {"textDocument": {"uri": uri}})["result"][0]["newText"] == "formatted"
    assert server._handle_workspace_symbol(10, {"query": "val"})["result"][0]["name"] == "val"
    assert server._handle_semantic_tokens_full(11, {"textDocument": {"uri": uri}})["result"]["data"] == [0, 0, 3, 0, 0]

    assert server._handle_code_lens(12, {"textDocument": {"uri": uri}})["result"]
    assert server._handle_code_lens_resolve(13, {"id": 1})["result"]["resolved"] is True
    assert server._handle_inlay_hint(14, {"textDocument": {"uri": uri}, "range": {}})["result"][0]["label"] == "hint"

    server.code_lens_provider = type("_BadLenses", (), {"get_code_lenses": staticmethod(lambda _u, _t: (_ for _ in ()).throw(RuntimeError("lens fail"))), "resolve_code_lens": staticmethod(lambda _p: (_ for _ in ()).throw(RuntimeError("resolve fail")))})()
    server.inlay_hints_provider = type("_BadInlay", (), {"get_inlay_hints": staticmethod(lambda _u, _t, _r: (_ for _ in ()).throw(RuntimeError("hint fail")))})()

    assert server._handle_code_lens(15, {"textDocument": {"uri": uri}})["result"] == []
    assert server._handle_code_lens_resolve(16, {"id": 99})["result"]["id"] == 99
    assert server._handle_inlay_hint(17, {"textDocument": {"uri": uri}, "range": {}})["result"] == []


def test_realtime_lint_diagnostics_paths(monkeypatch):
    server = _new_server_without_init()
    uri = "file:///lint.nxl"
    text = "set value to 1\n"

    # Disabled linting short-circuits.
    server.initialization_options = {"linting": {"enabled": False}}
    assert server._get_realtime_lint_diagnostics(uri, text) == []

    # Enabled linting with parser failure should return empty diagnostics.
    server.initialization_options = {"linting": {"enabled": True}}
    server.get_or_parse = lambda _u, _t: None
    assert server._get_realtime_lint_diagnostics(uri, text) == []

    class _Severity:
        def __init__(self, value):
            self.value = value

    class _Location:
        def __init__(self, line, column):
            self.line = line
            self.column = column

    class _Issue:
        def __init__(self, code, message, severity, line=1, column=1):
            self.code = code
            self.message = message
            self.severity = _Severity(severity)
            self.location = _Location(line, column)
            self.category = type("_Category", (), {"value": "style"})()

    class _BadChecker:
        def check(self, _ast, _text, _lines):
            raise RuntimeError("checker fail")

    class _Checker:
        def check(self, _ast, _text, _lines):
            return [
                _Issue("W1", "warn", "warning", line=2, column=3),
                _Issue("E1", "err", "error", line=1, column=1),
            ]

    analyzer = type("_Analyzer", (), {"checkers": [_BadChecker(), _Checker()]})()

    server.initialization_options = {"linting": {"enabled": True, "strict": False, "errorsOnly": False}}
    server.get_or_parse = lambda _u, _t: object()
    monkeypatch.setattr(lsp_server, "create_default_analyzer", lambda: analyzer)
    monkeypatch.setattr(lsp_server, "create_strict_analyzer", lambda: analyzer)

    all_diags = server._get_realtime_lint_diagnostics(uri, text)
    assert len(all_diags) == 2
    assert all_diags[0]["severity"] == 2

    server.initialization_options = {"linting": {"enabled": True, "strict": True, "errorsOnly": True}}
    error_diags = server._get_realtime_lint_diagnostics(uri, text)
    assert len(error_diags) == 1
    assert error_diags[0]["message"].startswith("[E1]")


def test_prepare_call_hierarchy_edge_paths_and_cache_hit(monkeypatch):
    server = _new_server_without_init()
    uri = "file:///hierarchy.nxl"
    server.documents[uri] = "function target\nend\n"

    no_index = server._handle_prepare_call_hierarchy(1, {"textDocument": {"uri": uri}, "position": {"line": 0, "character": 1}})
    assert no_index["result"] is None

    server.workspace_index = _FakeWorkspaceIndex("/tmp")
    assert server._handle_prepare_call_hierarchy(2, {"textDocument": {"uri": uri}, "position": {"line": 9, "character": 0}})["result"] is None
    assert server._handle_prepare_call_hierarchy(3, {"textDocument": {"uri": uri}, "position": {"line": 0, "character": 999}})["result"] is None

    server.documents[uri] = "....\n"
    server.workspace_index.get_symbol = lambda _word: []
    assert server._handle_prepare_call_hierarchy(4, {"textDocument": {"uri": uri}, "position": {"line": 0, "character": 0}})["result"] is None

    # Exercise no-workspace shortcuts for call handlers.
    server.workspace_index = None
    assert server._handle_incoming_calls(5, {"item": {"name": "x"}})["result"] == []
    assert server._handle_outgoing_calls(6, {"item": {"uri": uri, "range": {"start": {"line": 0}}}})["result"] == []

    # Verify parse cache hit path.
    server._parse_cache = {}
    text = "set value to 1\n"
    import hashlib

    text_hash = hashlib.md5(text.encode("utf-8")).hexdigest()[:12]
    sentinel = object()
    server._parse_cache[uri] = (text_hash, sentinel)
    assert server.get_or_parse(uri, text) is sentinel

    # publish_diagnostics delegates to write_message.
    sent = []
    server._write_message = lambda msg: sent.append(msg)
    server._publish_diagnostics(uri, [{"code": "E"}])
    assert sent and sent[0]["method"] == "textDocument/publishDiagnostics"
