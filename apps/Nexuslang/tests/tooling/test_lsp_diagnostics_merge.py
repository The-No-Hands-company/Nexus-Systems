"""Deterministic merge/dedup tests for LSP diagnostics provider."""

from unittest.mock import MagicMock

from nexuslang.lsp.diagnostics import DiagnosticsProvider


def _provider() -> DiagnosticsProvider:
    return DiagnosticsProvider(server=MagicMock())


def _diag(line: int, character: int, message: str, code: str | None = None, source: str = "nexuslang") -> dict:
    payload = {
        "range": {
            "start": {"line": line, "character": character},
            "end": {"line": line, "character": character + 1},
        },
        "severity": 1,
        "message": message,
        "source": source,
    }
    if code is not None:
        payload["code"] = code
    return payload


def test_merge_and_dedupe_removes_exact_duplicates():
    provider = _provider()
    diagnostics = provider.merge_and_dedupe_diagnostics(
        [_diag(1, 1, "Type error", "E200")],
        [_diag(1, 1, "Type error", "E200")],
    )

    assert len(diagnostics) == 1


def test_merge_and_dedupe_is_deterministically_sorted():
    provider = _provider()
    diagnostics = provider.merge_and_dedupe_diagnostics(
        [_diag(3, 4, "later", "E200")],
        [_diag(1, 2, "earlier", "E001")],
        [_diag(2, 0, "middle", "E150")],
    )

    assert [d["message"] for d in diagnostics] == ["earlier", "middle", "later"]


def test_merge_and_dedupe_normalizes_legacy_source_and_missing_code():
    provider = _provider()
    diagnostics = provider.merge_and_dedupe_diagnostics(
        [_diag(0, 0, "legacy-source-and-missing-code", code=None, source="nexuslang")],
    )

    assert len(diagnostics) == 1
    diag = diagnostics[0]
    assert diag["source"] == "nlpl"
    assert diag["code"] == "E309"
    assert diag.get("data", {}).get("explainHint") == "nxl --explain E309"
    assert diag.get("data", {}).get("reference") == "docs/reference/error-codes.md#e309"


def test_merge_and_dedupe_normalizes_invalid_code_to_runtime_error():
    provider = _provider()
    diagnostics = provider.merge_and_dedupe_diagnostics(
        [_diag(0, 0, "bad-code", code="NOT_A_CODE", source="nexuslang")],
    )

    assert len(diagnostics) == 1
    diag = diagnostics[0]
    assert diag["code"] == "E309"
    assert diag.get("data", {}).get("reference") == "docs/reference/error-codes.md#e309"


def test_merge_and_dedupe_preserves_and_merges_origins():
    provider = _provider()
    parser_diag = _diag(1, 0, "duplicate", "E200", source="parser")
    type_diag = _diag(1, 0, "duplicate", "E200", source="typechecker")

    diagnostics = provider.merge_and_dedupe_diagnostics(
        provider._tag_origin([parser_diag], "parser"),
        provider._tag_origin([type_diag], "typechecker"),
    )

    assert len(diagnostics) == 1
    data = diagnostics[0].get("data", {})
    assert diagnostics[0]["source"] == "nlpl"
    assert data.get("origins") == ["parser", "typechecker"]


def test_get_diagnostics_output_is_deduplicated_and_sorted():
    provider = _provider()

    duplicate = _diag(0, 0, "dup", "E200", source="nexuslang")
    later = _diag(3, 0, "later", "E005", source="nexuslang")

    provider._check_parser_syntax_enhanced = lambda text, uri: [later, duplicate]
    provider._check_syntax = lambda text: []
    provider._check_type_errors_enhanced = lambda text, uri: [duplicate]
    provider._check_imports = lambda text, uri: []
    provider._check_channel_operations = lambda text: []
    provider._check_macro_comptime_operations = lambda text: []
    provider._check_exception_scope_and_unreachable_catch = lambda text: []
    provider._check_async_await_spawn_contexts = lambda text: []
    provider._check_unused_vars = lambda text: []

    diagnostics = provider.get_diagnostics("file:///dedup.nxl", "set x to 1", check_imports=False)

    assert len(diagnostics) == 2
    assert diagnostics[0]["message"] == "dup"
    assert diagnostics[1]["message"] == "later"
