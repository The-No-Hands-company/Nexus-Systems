"""Server-level semantic token integration tests for LSP capabilities."""

from nexuslang.lsp.server import NLPLLanguageServer


def test_initialize_reports_semantic_tokens_capability():
    server = NLPLLanguageServer()

    response = server._handle_initialize(
        1,
        {
            "workspaceFolders": [],
            "initializationOptions": {},
        },
    )

    capabilities = response["result"]["capabilities"]
    semantic = capabilities.get("semanticTokensProvider")

    assert semantic is not None
    assert semantic.get("full") is True
    legend = semantic.get("legend", {})
    assert "tokenTypes" in legend
    assert "tokenModifiers" in legend
    assert "keyword" in legend["tokenTypes"]


def test_semantic_tokens_full_returns_delta_encoded_data():
    server = NLPLLanguageServer()
    uri = "file:///tmp/semantic_server.nxl"
    source = (
        "function greet with name as String returns String\n"
        "    return \"Hello, \" plus name\n"
        "end\n"
    )
    server.documents[uri] = source

    response = server._handle_semantic_tokens_full(
        7,
        {
            "textDocument": {
                "uri": uri,
            }
        },
    )

    data = response["result"]["data"]
    assert isinstance(data, list)
    assert len(data) > 0
    assert len(data) % 5 == 0
