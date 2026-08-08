"""Dedicated tooling coverage for LSP signature-help behavior."""

import types

from nexuslang.lsp.server import NLPLLanguageServer
from nexuslang.lsp.signature_help import SignatureHelpProvider


def _position(line: int, character: int):
    return types.SimpleNamespace(line=line, character=character)


def test_signature_help_provider_stdlib_signature_payload():
    provider = SignatureHelpProvider(server=types.SimpleNamespace(workspace_index=None))
    text = "set result to sqrt with number: "

    result = provider.get_signature_help(text, _position(0, len(text)))

    assert result is not None
    assert result["activeSignature"] == 0
    assert result["activeParameter"] == 0
    assert len(result["signatures"]) == 1
    assert "sqrt" in result["signatures"][0]["label"].lower()


def test_signature_help_provider_active_parameter_from_comma_separated_args():
    provider = SignatureHelpProvider(server=types.SimpleNamespace(workspace_index=None))
    text = "set value to max with 10, "

    result = provider.get_signature_help(text, _position(0, len(text)))

    assert result is not None
    assert result["activeParameter"] == 1
    assert "max" in result["signatures"][0]["label"].lower()


def test_signature_help_provider_returns_none_outside_call_context():
    provider = SignatureHelpProvider(server=types.SimpleNamespace(workspace_index=None))
    text = "set value to 42"

    result = provider.get_signature_help(text, _position(0, len(text)))

    assert result is None


def test_signature_help_provider_extracts_user_defined_function_signature():
    provider = SignatureHelpProvider(server=types.SimpleNamespace(workspace_index=None))
    text = (
        "function compute that takes left as Integer, right as Integer returns Integer\n"
        "    return left plus right\n"
        "end\n"
        "\n"
        "set answer to compute with 1, \n"
    )

    result = provider.get_signature_help(text, _position(4, len("set answer to compute with 1, ")))

    assert result is not None
    signature = result["signatures"][0]
    assert "function compute" in signature["label"].lower()
    assert len(signature["parameters"]) == 2
    assert result["activeParameter"] == 1


def test_lsp_server_signature_help_handler_returns_expected_payload_shape():
    server = NLPLLanguageServer()
    uri = "file:///tmp/signature_help_test.nxl"
    text = "set value to split with text: \"a,b\", "
    server.documents[uri] = text

    response = server._handle_signature_help(
        101,
        {
            "textDocument": {"uri": uri},
            "position": {"line": 0, "character": len(text)},
        },
    )

    assert response["jsonrpc"] == "2.0"
    assert response["id"] == 101
    assert response["result"] is not None
    assert "signatures" in response["result"]
    assert response["result"]["activeSignature"] == 0


def test_lsp_server_signature_help_returns_null_for_unknown_symbol():
    server = NLPLLanguageServer()
    uri = "file:///tmp/signature_help_unknown.nxl"
    text = "set value to unknown_thing with "
    server.documents[uri] = text

    response = server._handle_signature_help(
        102,
        {
            "textDocument": {"uri": uri},
            "position": {"line": 0, "character": len(text)},
        },
    )

    assert response["jsonrpc"] == "2.0"
    assert response["id"] == 102
    assert response["result"] is None
