"""
LSP diagnostic payload validation tests
========================================

Validates that _build_diagnostic() in DiagnosticsProvider produces the
canonical LSP payload shape required by the error-code integration checklist:

  - stable ``code`` field (E###)
  - ``source`` always "nlpl"
  - ``data.fixes`` populated from registry
  - ``data.explainHint`` = "nxl --explain E###"
  - ``data.title`` and ``data.category`` from registry
  - ``range`` shape is valid
  - Fallback to E309 for "runtime_error" generic type key

Checklist codes exercised: E001, E100, E200, E301, E309.
"""

import pytest
from unittest.mock import MagicMock

from nexuslang.lsp.diagnostics import DiagnosticsProvider
from nexuslang.error_codes import get_error_info, get_error_code_for_type


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def make_provider() -> DiagnosticsProvider:
    """Return a DiagnosticsProvider with a stub server (no real LSP needed)."""
    return DiagnosticsProvider(server=MagicMock())


def _range(line: int, start: int, end: int) -> dict:
    return {
        "start": {"line": line, "character": start},
        "end": {"line": line, "character": end},
    }


# ---------------------------------------------------------------------------
# Core payload shape contract
# ---------------------------------------------------------------------------

class TestBuildDiagnosticShape:
    """Invariants that must hold for every diagnostic produced by _build_diagnostic."""

    def test_range_present_and_valid(self):
        p = make_provider()
        d = p._build_diagnostic(line=3, start_char=5, end_char=10,
                                severity=1, message="test")
        r = d["range"]
        assert r["start"]["line"] == 3
        assert r["start"]["character"] == 5
        assert r["end"]["line"] == 3
        assert r["end"]["character"] == 10

    def test_source_is_always_nxl(self):
        p = make_provider()
        d = p._build_diagnostic(line=0, start_char=0, end_char=1,
                                severity=1, message="x")
        assert d["source"] == "nlpl"

    def test_custom_source_propagated(self):
        p = make_provider()
        d = p._build_diagnostic(line=0, start_char=0, end_char=1,
                                severity=2, message="x", source="nlpl-test")
        assert d["source"] == "nlpl-test"

    def test_severity_propagated(self):
        p = make_provider()
        for sev in (1, 2, 3, 4):
            d = p._build_diagnostic(line=0, start_char=0, end_char=1,
                                    severity=sev, message="m")
            assert d["severity"] == sev

    def test_fallback_code_when_no_type_key(self):
        p = make_provider()
        d = p._build_diagnostic(line=0, start_char=0, end_char=1,
                                severity=1, message="no code")
        assert d.get("code") == "E309"

    def test_no_data_field_when_registry_has_no_suggestions_and_none_given(self):
        """If a code resolves but the registry entry has no fixes, data may be omitted."""
        p = make_provider()
        # Pass an explicit empty fixes list -> data.fixes is [] which is filtered
        d = p._build_diagnostic(line=0, start_char=0, end_char=1,
                                severity=1, message="m", fixes=[])
        # data key may be absent or present without fixes key
        if "data" in d:
            assert "fixes" not in d["data"] or d["data"].get("fixes") == []

    def test_start_char_clamped_to_zero(self):
        p = make_provider()
        d = p._build_diagnostic(line=0, start_char=-5, end_char=-2,
                                severity=1, message="m")
        assert d["range"]["start"]["character"] == 0
        assert d["range"]["end"]["character"] == 0  # max(0, max(0,-5)) == 0


# ---------------------------------------------------------------------------
# Checklist code: E001 — Unexpected token (syntax)
# ---------------------------------------------------------------------------

class TestDiagnosticE001:
    CODE = "E001"

    def test_code_via_error_type_key(self):
        p = make_provider()
        d = p._build_diagnostic(line=0, start_char=0, end_char=5,
                                severity=1, message="Syntax error",
                                error_type_key="unexpected_token")
        assert d.get("code") == self.CODE

    def test_data_title_matches_registry(self):
        p = make_provider()
        d = p._build_diagnostic(line=0, start_char=0, end_char=5,
                                severity=1, message="Syntax error",
                                error_type_key="unexpected_token")
        info = get_error_info(self.CODE)
        assert d["data"]["title"] == info.title

    def test_data_category(self):
        p = make_provider()
        d = p._build_diagnostic(line=0, start_char=0, end_char=5,
                                severity=1, message="Syntax error",
                                error_type_key="unexpected_token")
        assert d["data"]["category"] == "syntax"

    def test_data_suggestions_non_empty(self):
        p = make_provider()
        d = p._build_diagnostic(line=0, start_char=0, end_char=5,
                                severity=1, message="Syntax error",
                                error_type_key="unexpected_token")
        assert len(d["data"]["fixes"]) > 0

    def test_data_explain_hint(self):
        p = make_provider()
        d = p._build_diagnostic(line=0, start_char=0, end_char=5,
                                severity=1, message="Syntax error",
                                error_type_key="unexpected_token")
        assert d["data"]["explainHint"] == f"nxl --explain {self.CODE}"

    def test_code_via_direct_error_code(self):
        p = make_provider()
        d = p._build_diagnostic(line=0, start_char=0, end_char=5,
                                severity=1, message="Syntax error",
                                error_code=self.CODE)
        assert d.get("code") == self.CODE


# ---------------------------------------------------------------------------
# Checklist code: E100 — Undefined variable (name)
# ---------------------------------------------------------------------------

class TestDiagnosticE100:
    CODE = "E100"

    def test_code_via_error_type_key(self):
        p = make_provider()
        d = p._build_diagnostic(line=2, start_char=4, end_char=11,
                                severity=1, message="Undefined: foo",
                                error_type_key="undefined_variable")
        assert d.get("code") == self.CODE

    def test_category_is_name(self):
        p = make_provider()
        d = p._build_diagnostic(line=2, start_char=4, end_char=11,
                                severity=1, message="Undefined: foo",
                                error_type_key="undefined_variable")
        assert d["data"]["category"] == "name"

    def test_suggestions_include_set_declaration_hint(self):
        p = make_provider()
        d = p._build_diagnostic(line=2, start_char=4, end_char=11,
                                severity=1, message="Undefined: foo",
                                error_type_key="undefined_variable")
        fixes_text = " ".join(d["data"]["fixes"]).lower()
        assert "set" in fixes_text or "declare" in fixes_text or "variable" in fixes_text

    def test_explain_hint(self):
        p = make_provider()
        d = p._build_diagnostic(line=2, start_char=4, end_char=11,
                                severity=1, message="Undefined: foo",
                                error_type_key="undefined_variable")
        assert d["data"]["explainHint"] == f"nxl --explain {self.CODE}"


# ---------------------------------------------------------------------------
# Checklist code: E200 — Type mismatch (type)
# ---------------------------------------------------------------------------

class TestDiagnosticE200:
    CODE = "E200"

    def test_code_via_type_key(self):
        p = make_provider()
        d = p._build_diagnostic(line=5, start_char=8, end_char=15,
                                severity=1, message="Type error",
                                error_type_key="type_mismatch")
        assert d.get("code") == self.CODE

    def test_category_is_type(self):
        p = make_provider()
        d = p._build_diagnostic(line=5, start_char=8, end_char=15,
                                severity=1, message="Type error",
                                error_type_key="type_mismatch")
        assert d["data"]["category"] == "type"

    def test_suggestions_mention_conversion(self):
        p = make_provider()
        d = p._build_diagnostic(line=5, start_char=8, end_char=15,
                                severity=1, message="Type error",
                                error_type_key="type_mismatch")
        fixes_text = " ".join(d["data"]["fixes"]).lower()
        assert "type" in fixes_text or "convert" in fixes_text

    def test_doc_link_present(self):
        p = make_provider()
        d = p._build_diagnostic(line=5, start_char=8, end_char=15,
                                severity=1, message="Type error",
                                error_type_key="type_mismatch")
        assert d["data"].get("docLink")

    def test_suggestions_capped_at_3(self):
        p = make_provider()
        d = p._build_diagnostic(line=5, start_char=8, end_char=15,
                                severity=1, message="Type error",
                                error_type_key="type_mismatch")
        assert len(d["data"]["fixes"]) <= 3


# ---------------------------------------------------------------------------
# Checklist code: E301 — Index out of range (runtime)
# ---------------------------------------------------------------------------

class TestDiagnosticE301:
    CODE = "E301"

    def test_code_via_type_key(self):
        p = make_provider()
        d = p._build_diagnostic(line=10, start_char=0, end_char=20,
                                severity=1, message="Index out of range",
                                error_type_key="index_out_of_range")
        assert d.get("code") == self.CODE

    def test_category_is_runtime(self):
        p = make_provider()
        d = p._build_diagnostic(line=10, start_char=0, end_char=20,
                                severity=1, message="Index out of range",
                                error_type_key="index_out_of_range")
        assert d["data"]["category"] == "runtime"

    def test_explain_hint(self):
        p = make_provider()
        d = p._build_diagnostic(line=10, start_char=0, end_char=20,
                                severity=1, message="Index out of range",
                                error_type_key="index_out_of_range")
        assert d["data"]["explainHint"] == f"nxl --explain {self.CODE}"


# ---------------------------------------------------------------------------
# Checklist code: E309 — General runtime error (fallback)
# ---------------------------------------------------------------------------

class TestDiagnosticE309:
    CODE = "E309"

    def test_code_via_runtime_error_key(self):
        p = make_provider()
        d = p._build_diagnostic(line=7, start_char=0, end_char=10,
                                severity=1, message="Runtime failure",
                                error_type_key="runtime_error")
        assert d.get("code") == self.CODE

    def test_title_is_general_runtime(self):
        p = make_provider()
        d = p._build_diagnostic(line=7, start_char=0, end_char=10,
                                severity=1, message="Runtime failure",
                                error_type_key="runtime_error")
        info = get_error_info(self.CODE)
        assert d["data"]["title"] == info.title

    def test_explain_hint(self):
        p = make_provider()
        d = p._build_diagnostic(line=7, start_char=0, end_char=10,
                                severity=1, message="Runtime failure",
                                error_type_key="runtime_error")
        assert d["data"]["explainHint"] == f"nxl --explain {self.CODE}"


# ---------------------------------------------------------------------------
# Cross-cutting: explicit overrides beat registry defaults
# ---------------------------------------------------------------------------

class TestExplicitOverrides:
    """Caller-supplied values must win over registry lookups."""

    def test_explicit_suggestions_override_registry(self):
        p = make_provider()
        custom_fixes = ["Do this first", "Then do that"]
        d = p._build_diagnostic(line=0, start_char=0, end_char=5,
                                severity=1, message="m",
                                error_type_key="type_mismatch",
                                fixes=custom_fixes)
        assert d["data"]["fixes"] == custom_fixes

    def test_explicit_title_overrides_registry(self):
        p = make_provider()
        d = p._build_diagnostic(line=0, start_char=0, end_char=5,
                                severity=1, message="m",
                                error_type_key="type_mismatch",
                                title="My custom title")
        assert d["data"]["title"] == "My custom title"

    def test_explicit_error_code_beats_type_key(self):
        """If both error_code and error_type_key given, error_code wins."""
        p = make_provider()
        d = p._build_diagnostic(line=0, start_char=0, end_char=5,
                                severity=1, message="m",
                                error_code="E301",
                                error_type_key="type_mismatch")
        assert d.get("code") == "E301"

    def test_explicit_doc_link_overrides_registry(self):
        p = make_provider()
        d = p._build_diagnostic(line=0, start_char=0, end_char=5,
                                severity=1, message="m",
                                error_type_key="type_mismatch",
                                doc_link="https://example.com/custom")
        assert d["data"]["docLink"] == "https://example.com/custom"


# ---------------------------------------------------------------------------
# get_error_code_for_type registry smoke-test
# ---------------------------------------------------------------------------

class TestErrorCodeRegistry:
    """Verify the five checklist codes resolve correctly from the type-key map."""

    @pytest.mark.parametrize("type_key,expected_code", [
        ("unexpected_token", "E001"),
        ("undefined_variable", "E100"),
        ("type_mismatch", "E200"),
        ("index_out_of_range", "E301"),
        ("runtime_error", "E309"),
    ])
    def test_type_key_resolves_to_correct_code(self, type_key, expected_code):
        assert get_error_code_for_type(type_key) == expected_code

    @pytest.mark.parametrize("code", ["E001", "E100", "E200", "E301", "E309"])
    def test_registry_entry_has_required_fields(self, code):
        info = get_error_info(code)
        assert info is not None, f"{code} missing from registry"
        assert info.title
        assert info.category
        assert isinstance(info.fixes, list)
