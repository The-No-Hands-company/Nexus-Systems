"""Tests for opt-in real-time linter diagnostics in the LSP server."""

from nexuslang.lsp.server import NLPLLanguageServer


def test_realtime_lint_diagnostics_emitted_when_enabled():
    server = NLPLLanguageServer()
    server.initialization_options = {
        "linting": {
            "enabled": True,
            "strict": True,
        }
    }

    published = {}

    def _capture(uri, diagnostics):
        published["uri"] = uri
        published["diagnostics"] = diagnostics

    server._publish_diagnostics = _capture

    uri = "file:///tmp/lint_enabled.nxl"
    text = """# TODO: remove debug output\nset value to 10\nprint text value\n"""

    server._handle_did_open(
        {
            "textDocument": {
                "uri": uri,
                "text": text,
            }
        }
    )

    diagnostics = published.get("diagnostics", [])
    assert any(d.get("source") == "nlpl-lint" for d in diagnostics)
    assert any("S018" in d.get("message", "") for d in diagnostics)


def test_realtime_lint_diagnostics_disabled_by_default():
    server = NLPLLanguageServer()

    published = {}

    def _capture(uri, diagnostics):
        published["uri"] = uri
        published["diagnostics"] = diagnostics

    server._publish_diagnostics = _capture

    uri = "file:///tmp/lint_disabled.nxl"
    text = """# TODO: remove debug output\nset value to 10\nprint text value\n"""

    server._handle_did_open(
        {
            "textDocument": {
                "uri": uri,
                "text": text,
            }
        }
    )

    diagnostics = published.get("diagnostics", [])
    assert not any(d.get("source") == "nlpl-lint" for d in diagnostics)
