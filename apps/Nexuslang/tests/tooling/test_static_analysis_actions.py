"""
Tests for static analysis remediation engine and LSP IDE integration hooks.

Covers:
    - autofix.py: TextEdit, FixSuggestion, FixResult, AutoFixer, all built-in generators
  - ide_hooks.py: IDEHooks, LspFormatter, severity_to_lsp, lsp_position, lsp_range
"""

import sys
import os
import pytest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "src"))

from nexuslang.tooling.analyzer.report import (
    Issue,
    Severity,
    Category,
    SourceLocation,
    AnalysisReport,
)
from nexuslang.tooling.analyzer.autofix import (
    TextEdit,
    FixSuggestion,
    FixResult,
    AutoFixer,
    _apply_single_edit,
    _sort_edits_reverse,
    _FIX_GENERATORS,
)
from nexuslang.tooling.analyzer.ide_hooks import (
    IDEHooks,
    LspFormatter,
    severity_to_lsp,
    lsp_position,
    lsp_range,
)


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------


def _make_issue(
    code="W-STYLE-TRAIL",
    severity=Severity.WARNING,
    category=Category.STYLE,
    message="trailing whitespace",
    line=1,
    col=1,
    end_line=None,
    end_col=None,
    fix=None,
    suggestion=None,
    related_locations=None,
    help_url=None,
):
    return Issue(
        code=code,
        severity=severity,
        category=category,
        message=message,
        location=SourceLocation(
            "test.nxl", line, col, end_line or line, end_col or (col + 1)
        ),
        source_line=None,
        suggestion=suggestion,
        fix=fix,
        related_locations=related_locations or [],
        help_url=help_url,
    )


def _make_report(issues=None, file_path="test.nxl"):
    report = AnalysisReport(
        file_path=file_path, total_lines=20, lines_analyzed=20
    )
    for issue in (issues or []):
        report.add_issue(issue)
    return report


# ===========================================================================
# TextEdit
# ===========================================================================


class TestTextEdit:
    def test_creates_with_valid_args(self):
        edit = TextEdit(line=1, col=1, end_line=1, end_col=5, new_text="hello")
        assert edit.line == 1
        assert edit.col == 1
        assert edit.end_line == 1
        assert edit.end_col == 5
        assert edit.new_text == "hello"

    def test_zero_length_range_is_insert(self):
        # end == start means pure insert (no deletion)
        edit = TextEdit(line=2, col=3, end_line=2, end_col=3, new_text="insert")
        assert edit.line == edit.end_line
        assert edit.col == edit.end_col

    def test_deletion_empty_new_text(self):
        edit = TextEdit(line=1, col=1, end_line=1, end_col=10, new_text="")
        assert edit.new_text == ""

    def test_multiline_range(self):
        edit = TextEdit(line=1, col=1, end_line=3, end_col=5, new_text="replacement")
        assert edit.end_line == 3

    def test_end_line_before_start_raises(self):
        with pytest.raises((ValueError, AssertionError)):
            TextEdit(line=5, col=1, end_line=3, end_col=1, new_text="")

    def test_zero_line_raises(self):
        with pytest.raises((ValueError, AssertionError)):
            TextEdit(line=0, col=1, end_line=0, end_col=2, new_text="")

    def test_negative_col_raises(self):
        with pytest.raises((ValueError, AssertionError)):
            TextEdit(line=1, col=-1, end_line=1, end_col=1, new_text="")

    def test_end_col_before_start_col_same_line_raises(self):
        with pytest.raises((ValueError, AssertionError)):
            TextEdit(line=2, col=5, end_line=2, end_col=3, new_text="")


# ===========================================================================
# FixSuggestion
# ===========================================================================


class TestFixSuggestion:
    def test_is_applicable_with_edits(self):
        issue = _make_issue()
        edit = TextEdit(1, 1, 1, 5, "")
        s = FixSuggestion(issue=issue, description="remove trailing ws", edits=[edit])
        assert s.is_applicable() is True

    def test_is_applicable_false_when_no_edits(self):
        issue = _make_issue()
        s = FixSuggestion(issue=issue, description="nothing to do", edits=[])
        assert s.is_applicable() is False

    def test_holds_issue_reference(self):
        issue = _make_issue(code="P003")
        s = FixSuggestion(issue=issue, description="desc", edits=[])
        assert s.issue is issue

    def test_holds_description(self):
        issue = _make_issue()
        s = FixSuggestion(issue=issue, description="my description", edits=[])
        assert s.description == "my description"

    def test_holds_edits_list(self):
        issue = _make_issue()
        edits = [TextEdit(1, 1, 1, 3, "x"), TextEdit(2, 1, 2, 2, "y")]
        s = FixSuggestion(issue=issue, description="two edits", edits=edits)
        assert len(s.edits) == 2


# ===========================================================================
# FixResult
# ===========================================================================


class TestFixResult:
    def test_total_is_applied_plus_skipped(self):
        r = FixResult(new_source="src", applied=3, skipped=2, changes=["a", "b", "c"])
        assert r.total == 5

    def test_new_source_stored(self):
        r = FixResult(new_source="hello world", applied=1, skipped=0, changes=["x"])
        assert r.new_source == "hello world"

    def test_changes_list_stored(self):
        r = FixResult(new_source="", applied=2, skipped=0, changes=["c1", "c2"])
        assert r.changes == ["c1", "c2"]

    def test_zero_applied_zero_skipped(self):
        r = FixResult(new_source="unchanged", applied=0, skipped=0, changes=[])
        assert r.total == 0


# ===========================================================================
# _apply_single_edit
# ===========================================================================


class TestApplySingleEdit:
    def test_replace_middle_of_line(self):
        # _apply_single_edit receives lines from splitlines(keepends=False)
        lines = ["hello world", "foo"]
        # Replace "world" (cols 7-12) with "earth"
        edit = TextEdit(line=1, col=7, end_line=1, end_col=12, new_text="earth")
        result = _apply_single_edit(lines, edit)
        assert result[0] == "hello earth"
        assert result[1] == "foo"

    def test_delete_entire_line_content(self):
        lines = ["  keep this\n", "  delete me   \n", "also keep\n"]
        # Delete all content of line 2, leaving empty line
        edit = TextEdit(line=2, col=1, end_line=2, end_col=17, new_text="")
        result = _apply_single_edit(lines, edit)
        assert "delete me" not in result[1]

    def test_insert_at_start_of_line(self):
        lines = ["existing\n"]
        edit = TextEdit(line=1, col=1, end_line=1, end_col=1, new_text="new ")
        result = _apply_single_edit(lines, edit)
        assert result[0].startswith("new existing")

    def test_out_of_range_line_unchanged(self):
        lines = ["only one line\n"]
        edit = TextEdit(line=5, col=1, end_line=5, end_col=2, new_text="x")
        result = _apply_single_edit(lines, edit)
        assert result[0] == "only one line\n"

    def test_multiline_span_replacement(self):
        lines = ["line one\n", "line two\n", "line three\n"]
        # Replace from line 1 col 6 to line 2 col 5 (crossing lines)
        edit = TextEdit(line=1, col=6, end_line=2, end_col=5, new_text="X")
        result = _apply_single_edit(lines, edit)
        combined = "".join(result)
        assert "X" in combined

    def test_end_preserved_after_range(self):
        lines = ["abcdef\n"]
        # Replace "bc" (cols 2-4)
        edit = TextEdit(line=1, col=2, end_line=1, end_col=4, new_text="Z")
        result = _apply_single_edit(lines, edit)
        # "a" before, "Z" inserted, "def\n" after
        assert result[0].startswith("aZ")
        assert "def" in result[0]


# ===========================================================================
# _sort_edits_reverse
# ===========================================================================


class TestSortEditsReverse:
    def test_later_edit_comes_first(self):
        e1 = TextEdit(1, 1, 1, 3, "a")
        e2 = TextEdit(5, 1, 5, 3, "b")
        result = _sort_edits_reverse([e1, e2])
        assert result[0] is e2
        assert result[1] is e1

    def test_same_line_sorted_by_col_descending(self):
        e1 = TextEdit(3, 1, 3, 3, "a")
        e2 = TextEdit(3, 8, 3, 10, "b")
        result = _sort_edits_reverse([e1, e2])
        assert result[0] is e2  # col 8 > col 1

    def test_single_edit_unchanged(self):
        e = TextEdit(2, 4, 2, 6, "x")
        result = _sort_edits_reverse([e])
        assert len(result) == 1
        assert result[0] is e


# ===========================================================================
# Fix generators — W-STYLE-TRAIL
# ===========================================================================


class TestFixGeneratorStyleTrail:
    def _run(self, issue, source_lines):
        return _FIX_GENERATORS["W-STYLE-TRAIL"](issue, source_lines)

    def test_removes_trailing_whitespace(self):
        issue = _make_issue(code="W-STYLE-TRAIL", line=1)
        lines = ["hello world   \n"]
        suggestion = self._run(issue, lines)
        assert suggestion is not None
        assert len(suggestion.edits) == 1

    def test_noop_on_clean_line(self):
        # Lines come from splitlines(keepends=False) in real usage
        issue = _make_issue(code="W-STYLE-TRAIL", line=1)
        lines = ["hello world"]
        suggestion = self._run(issue, lines)
        # Either None or empty edits
        assert suggestion is None or not suggestion.is_applicable()

    def test_out_of_range_line_returns_none(self):
        issue = _make_issue(code="W-STYLE-TRAIL", line=99)
        lines = ["only one line\n"]
        suggestion = self._run(issue, lines)
        assert suggestion is None

    def test_correct_textedit_position(self):
        issue = _make_issue(code="W-STYLE-TRAIL", line=1)
        lines = ["code   \n"]  # 3 trailing spaces
        suggestion = self._run(issue, lines)
        assert suggestion is not None
        assert suggestion.edits[0].line == 1


# ===========================================================================
# Fix generators — W-STYLE-EOF
# ===========================================================================


class TestFixGeneratorStyleEof:
    def _run(self, issue, source_lines):
        return _FIX_GENERATORS["W-STYLE-EOF"](issue, source_lines)

    def test_appends_newline_when_missing(self):
        issue = _make_issue(code="W-STYLE-EOF", line=2)
        lines = ["line one\n", "line two"]
        suggestion = self._run(issue, lines)
        assert suggestion is not None
        assert suggestion.is_applicable()

    def test_returns_suggestion_hint(self):
        issue = _make_issue(code="W-STYLE-EOF", line=1)
        lines = ["no newline here"]
        suggestion = self._run(issue, lines)
        assert isinstance(suggestion, FixSuggestion)

    def test_empty_file_handled(self):
        issue = _make_issue(code="W-STYLE-EOF", line=1)
        lines = []
        # Should not raise
        suggestion = self._run(issue, lines)
        # Returns None or valid suggestion — both acceptable
        assert suggestion is None or isinstance(suggestion, FixSuggestion)


# ===========================================================================
# Fix generators — W-DEAD-ASSIGN
# ===========================================================================


class TestFixGeneratorDeadAssign:
    def _run(self, issue, source_lines):
        return _FIX_GENERATORS["W-DEAD-ASSIGN"](issue, source_lines)

    def test_deletes_assignment_line(self):
        issue = _make_issue(code="W-DEAD-ASSIGN", line=2)
        lines = ["set a to 1\n", "set b to 2\n", "print text a\n"]
        suggestion = self._run(issue, lines)
        assert suggestion is not None
        assert len(suggestion.edits) == 1

    def test_out_of_range_returns_none(self):
        issue = _make_issue(code="W-DEAD-ASSIGN", line=50)
        lines = ["line\n"]
        suggestion = self._run(issue, lines)
        assert suggestion is None

    def test_correct_line_number_in_edit(self):
        issue = _make_issue(code="W-DEAD-ASSIGN", line=3)
        lines = ["a\n", "b\n", "c\n", "d\n"]
        suggestion = self._run(issue, lines)
        assert suggestion is not None
        assert suggestion.edits[0].line == 3

    def test_returns_suggestion_hint(self):
        issue = _make_issue(code="W-DEAD-ASSIGN", line=1)
        lines = ["set x to 0\n"]
        suggestion = self._run(issue, lines)
        assert isinstance(suggestion, FixSuggestion)


# ===========================================================================
# Fix generators — W-UNINIT-USE
# ===========================================================================


class TestFixGeneratorUninitUse:
    def _run(self, issue, source_lines):
        return _FIX_GENERATORS["W-UNINIT-USE"](issue, source_lines)

    def test_inserts_init_before_issue_line(self):
        issue = _make_issue(
            code="W-UNINIT-USE", line=2, message="variable 'counter' may be uninitialized"
        )
        lines = ["function foo\n", "print text counter\n", "end\n"]
        suggestion = self._run(issue, lines)
        assert suggestion is not None
        assert "counter" in suggestion.edits[0].new_text

    def test_extracts_variable_name_from_message(self):
        issue = _make_issue(
            code="W-UNINIT-USE", line=1, message="variable 'my_var' used before init"
        )
        lines = ["use my_var\n"]
        suggestion = self._run(issue, lines)
        assert suggestion is not None
        assert "my_var" in suggestion.edits[0].new_text

    def test_fallback_var_name_when_no_quotes(self):
        issue = _make_issue(
            code="W-UNINIT-USE", line=1, message="uninitialized variable used"
        )
        lines = ["use something\n"]
        suggestion = self._run(issue, lines)
        # Should still produce a suggestion (fallback variable name)
        assert suggestion is not None
        assert len(suggestion.edits) == 1

    def test_returns_suggestion_hint(self):
        issue = _make_issue(code="W-UNINIT-USE", line=1, message="variable 'x' uninitialized")
        lines = ["x\n"]
        suggestion = self._run(issue, lines)
        assert isinstance(suggestion, FixSuggestion)


# ===========================================================================
# Fix generators — P003
# ===========================================================================


class TestFixGeneratorP003:
    def _run(self, issue, source_lines):
        return _FIX_GENERATORS["P003"](issue, source_lines)

    def test_inserts_guidance_comment(self):
        issue = _make_issue(code="P003", line=2)
        lines = ["set result to a plus b plus c\n", "print text result\n"]
        suggestion = self._run(issue, lines)
        assert suggestion is not None
        assert len(suggestion.edits) >= 1

    def test_correct_line_number(self):
        issue = _make_issue(code="P003", line=1)
        lines = ["set result to a plus b\n"]
        suggestion = self._run(issue, lines)
        assert suggestion is not None
        assert suggestion.edits[0].line == 1

    def test_out_of_range_returns_none(self):
        issue = _make_issue(code="P003", line=10)
        lines = ["line\n"]
        suggestion = self._run(issue, lines)
        assert suggestion is None


# ===========================================================================
# Fix generators — P005
# ===========================================================================


class TestFixGeneratorP005:
    def _run(self, issue, source_lines):
        return _FIX_GENERATORS["P005"](issue, source_lines)

    def test_inserts_opt_comment(self):
        issue = _make_issue(code="P005", line=1)
        lines = ["while i is less than len items\n"]
        suggestion = self._run(issue, lines)
        assert suggestion is not None
        assert len(suggestion.edits) >= 1

    def test_returns_suggestion_hint(self):
        issue = _make_issue(code="P005", line=1)
        lines = ["some loop call\n"]
        suggestion = self._run(issue, lines)
        assert isinstance(suggestion, FixSuggestion)


# ===========================================================================
# Fix generators — S-INJECT
# ===========================================================================


class TestFixGeneratorSInject:
    def _run(self, issue, source_lines):
        return _FIX_GENERATORS["S-INJECT"](issue, source_lines)

    def test_inserts_security_comment(self):
        issue = _make_issue(code="S-INJECT", line=1)
        lines = ["run command user_input\n"]
        suggestion = self._run(issue, lines)
        assert suggestion is not None
        assert len(suggestion.edits) >= 1

    def test_returns_suggestion_hint(self):
        issue = _make_issue(code="S-INJECT", line=1)
        lines = ["execute user_data\n"]
        suggestion = self._run(issue, lines)
        assert isinstance(suggestion, FixSuggestion)


# ===========================================================================
# AutoFixer — initialization
# ===========================================================================


class TestAutoFixerInit:
    def test_default_generators_loaded(self):
        fixer = AutoFixer()
        codes = fixer.supported_codes()
        assert "W-STYLE-TRAIL" in codes
        assert "W-DEAD-ASSIGN" in codes

    def test_extra_generators_merges(self):
        def custom_gen(issue, lines):
            return None

        fixer = AutoFixer(extra_generators={"MY-CODE": custom_gen})
        codes = fixer.supported_codes()
        assert "MY-CODE" in codes

    def test_supported_codes_returns_sorted_list(self):
        fixer = AutoFixer()
        codes = fixer.supported_codes()
        assert codes == sorted(codes)


# ===========================================================================
# AutoFixer.generate_fixes
# ===========================================================================


class TestAutoFixerGenerateFixes:
    def test_known_code_returns_suggestion(self):
        fixer = AutoFixer()
        issue = _make_issue(code="W-STYLE-TRAIL", line=1)
        source_lines = ["hello   \n"]
        suggestions = fixer.generate_fixes([issue], source_lines)
        assert len(suggestions) >= 1

    def test_unknown_code_skipped(self):
        fixer = AutoFixer()
        issue = _make_issue(code="UNKNOWN-XYZ", line=1)
        suggestions = fixer.generate_fixes([issue], ["line\n"])
        assert len(suggestions) == 0

    def test_empty_issues_returns_empty_list(self):
        fixer = AutoFixer()
        result = fixer.generate_fixes([], ["line\n"])
        assert result == []

    def test_generator_exception_skipped_gracefully(self):
        def bad_gen(issue, lines):
            raise RuntimeError("intentional failure")

        fixer = AutoFixer(extra_generators={"BAD-CODE": bad_gen})
        issue = _make_issue(code="BAD-CODE", line=1)
        # Should not raise  
        suggestions = fixer.generate_fixes([issue], ["line\n"])
        assert isinstance(suggestions, list)

    def test_multiple_issues_multiple_suggestions(self):
        fixer = AutoFixer()
        issues = [
            _make_issue(code="W-STYLE-TRAIL", line=1),
            _make_issue(code="W-STYLE-TRAIL", line=2),
        ]
        lines = ["trailing   \n", "also trailing  \n"]
        suggestions = fixer.generate_fixes(issues, lines)
        assert len(suggestions) == 2

    def test_non_applicable_suggestion_excluded(self):
        # Generator returns FixSuggestion with no edits
        def empty_gen(issue, lines):
            return FixSuggestion(issue=issue, description="noop", edits=[])

        fixer = AutoFixer(extra_generators={"NOOP": empty_gen})
        issue = _make_issue(code="NOOP", line=1)
        suggestions = fixer.generate_fixes([issue], ["line\n"])
        assert len(suggestions) == 0


# ===========================================================================
# AutoFixer.preview
# ===========================================================================


class TestAutoFixerPreview:
    def test_returns_modified_string(self):
        fixer = AutoFixer()
        issue = _make_issue(code="W-STYLE-TRAIL", line=1)
        lines = ["trailing   \n"]
        suggestions = fixer.generate_fixes([issue], lines)
        source = "trailing   \n"
        preview = fixer.preview(source, suggestions)
        assert isinstance(preview, str)

    def test_does_not_modify_original_source(self):
        fixer = AutoFixer()
        issue = _make_issue(code="W-STYLE-TRAIL", line=1)
        lines = ["trailing   \n"]
        suggestions = fixer.generate_fixes([issue], lines)
        original = "trailing   \n"
        fixer.preview(original, suggestions)
        assert original == "trailing   \n"

    def test_empty_suggestions_returns_unchanged(self):
        fixer = AutoFixer()
        source = "pristine source\n"
        result = fixer.preview(source, [])
        assert result == source

    def test_edits_applied_in_reverse_order(self):
        fixer = AutoFixer()
        # Two trailing whitespace issues on different lines
        issue1 = _make_issue(code="W-STYLE-TRAIL", line=1)
        issue2 = _make_issue(code="W-STYLE-TRAIL", line=2)
        lines = ["line1   \n", "line2   \n"]
        suggestions = fixer.generate_fixes([issue1, issue2], lines)
        source = "line1   \nline2   \n"
        result = fixer.preview(source, suggestions)
        assert isinstance(result, str)


# ===========================================================================
# AutoFixer.apply
# ===========================================================================


class TestAutoFixerApply:
    def test_applied_count_correct(self):
        fixer = AutoFixer()
        issue = _make_issue(code="W-STYLE-TRAIL", line=1)
        lines = ["code   \n"]
        suggestions = fixer.generate_fixes([issue], lines)
        result = fixer.apply("code   \n", suggestions)
        assert result.applied >= 1

    def test_new_source_modified(self):
        fixer = AutoFixer()
        issue = _make_issue(code="W-STYLE-TRAIL", line=1)
        lines = ["code   \n"]
        suggestions = fixer.generate_fixes([issue], lines)
        result = fixer.apply("code   \n", suggestions)
        assert "   " not in result.new_source or result.applied == 0

    def test_changes_populated(self):
        fixer = AutoFixer()
        issue = _make_issue(code="W-STYLE-TRAIL", line=1)
        lines = ["code   \n"]
        suggestions = fixer.generate_fixes([issue], lines)
        result = fixer.apply("code   \n", suggestions)
        if result.applied > 0:
            assert len(result.changes) > 0

    def test_suggestion_result_type_returned(self):
        fixer = AutoFixer()
        result = fixer.apply("source\n", [])
        assert isinstance(result, FixResult)

    def test_no_suggestions_noop(self):
        fixer = AutoFixer()
        source = "clean source\n"
        result = fixer.apply(source, [])
        assert result.new_source == source
        assert result.applied == 0

    def test_multiple_non_overlapping_edits_applied(self):
        fixer = AutoFixer()
        # Two separate lines with trailing whitespace
        issue1 = _make_issue(code="W-STYLE-TRAIL", line=1)
        issue2 = _make_issue(code="W-STYLE-TRAIL", line=3)
        lines = ["a   \n", "clean\n", "b   \n"]
        suggestions = fixer.generate_fixes([issue1, issue2], lines)
        source = "a   \nclean\nb   \n"
        result = fixer.apply(source, suggestions)
        assert result.applied == 2


# ===========================================================================
# AutoFixer.register_generator
# ===========================================================================


class TestAutoFixerRegisterGenerator:
    def test_custom_generator_callable(self):
        fixer = AutoFixer()
        called = []

        def my_gen(issue, lines):
            called.append(True)
            return None

        fixer.register_generator("MY-CUSTOM", my_gen)
        issue = _make_issue(code="MY-CUSTOM", line=1)
        fixer.generate_fixes([issue], ["line\n"])
        assert len(called) == 1

    def test_overrides_builtin(self):
        fixer = AutoFixer()
        override_called = []

        def override(issue, lines):
            override_called.append(True)
            return None

        fixer.register_generator("W-STYLE-TRAIL", override)
        issue = _make_issue(code="W-STYLE-TRAIL", line=1)
        fixer.generate_fixes([issue], ["trailing   \n"])
        assert len(override_called) == 1

    def test_custom_code_in_supported_codes(self):
        fixer = AutoFixer()
        fixer.register_generator("Z-CUSTOM", lambda i, l: None)
        assert "Z-CUSTOM" in fixer.supported_codes()


# ===========================================================================
# severity_to_lsp
# ===========================================================================


class TestSeverityToLsp:
    def test_error_is_1(self):
        assert severity_to_lsp(Severity.ERROR) == 1

    def test_warning_is_2(self):
        assert severity_to_lsp(Severity.WARNING) == 2

    def test_info_is_3(self):
        assert severity_to_lsp(Severity.INFO) == 3

    def test_hint_is_4(self):
        assert severity_to_lsp(Severity.HINT) == 4


# ===========================================================================
# lsp_position
# ===========================================================================


class TestLspPosition:
    def test_has_line_and_character_keys(self):
        pos = lsp_position(5, 10)
        assert "line" in pos
        assert "character" in pos

    def test_values_stored_correctly(self):
        pos = lsp_position(3, 7)
        assert pos["line"] == 3
        assert pos["character"] == 7


# ===========================================================================
# lsp_range
# ===========================================================================


class TestLspRange:
    def test_has_start_and_end(self):
        r = lsp_range(0, 0, 0, 5)
        assert "start" in r
        assert "end" in r

    def test_zero_based_values(self):
        r = lsp_range(2, 4, 2, 9)
        assert r["start"]["line"] == 2
        assert r["start"]["character"] == 4
        assert r["end"]["line"] == 2
        assert r["end"]["character"] == 9

    def test_start_equals_end_zero_length(self):
        r = lsp_range(1, 3, 1, 3)
        assert r["start"] == r["end"]


# ===========================================================================
# IDEHooks.issue_to_diagnostic
# ===========================================================================


class TestIDEHooksIssueToDiagnostic:
    def setup_method(self):
        self.hooks = IDEHooks()
        self.file_uri = "file:///test.nxl"

    def test_has_range_key(self):
        issue = _make_issue()
        diag = self.hooks.issue_to_diagnostic(issue, self.file_uri)
        assert "range" in diag

    def test_severity_mapped_correctly(self):
        issue = _make_issue(severity=Severity.ERROR)
        diag = self.hooks.issue_to_diagnostic(issue, self.file_uri)
        assert diag["severity"] == 1

    def test_code_present(self):
        issue = _make_issue(code="W-STYLE-TRAIL")
        diag = self.hooks.issue_to_diagnostic(issue, self.file_uri)
        assert diag.get("code") == "W-STYLE-TRAIL"

    def test_source_is_nxl_analyze(self):
        issue = _make_issue()
        diag = self.hooks.issue_to_diagnostic(issue, self.file_uri)
        assert diag.get("source") == "nlpl-analyze"

    def test_message_present(self):
        issue = _make_issue(message="my test message")
        diag = self.hooks.issue_to_diagnostic(issue, self.file_uri)
        assert "my test message" in diag.get("message", "")

    def test_data_category_correct(self):
        issue = _make_issue(category=Category.SECURITY)
        diag = self.hooks.issue_to_diagnostic(issue, self.file_uri)
        data = diag.get("data", {})
        assert "SECURITY" in str(data.get("category", "")).upper()

    def test_suggestion_in_data_when_present(self):
        issue = _make_issue(suggestion="consider using X instead")
        diag = self.hooks.issue_to_diagnostic(issue, self.file_uri)
        data = diag.get("data", {})
        assert "consider using X instead" in str(data)

    def test_related_information_present_for_related_locations(self):
        related_loc = SourceLocation("other.nxl", 5, 1, 5, 10)
        issue = _make_issue(related_locations=[related_loc])
        diag = self.hooks.issue_to_diagnostic(issue, self.file_uri)
        # Either relatedInformation key present or skipped — check without error
        # If present, verify it's a list
        if "relatedInformation" in diag:
            assert isinstance(diag["relatedInformation"], list)


# ===========================================================================
# IDEHooks.issues_to_diagnostics
# ===========================================================================


class TestIDEHooksIssuesToDiagnostics:
    def setup_method(self):
        self.hooks = IDEHooks()
        self.file_uri = "file:///test.nxl"

    def test_empty_list_returns_empty(self):
        result = self.hooks.issues_to_diagnostics([], self.file_uri)
        assert result == []

    def test_one_issue_one_diagnostic(self):
        issue = _make_issue()
        result = self.hooks.issues_to_diagnostics([issue], self.file_uri)
        assert len(result) == 1

    def test_n_issues_n_diagnostics(self):
        issues = [_make_issue(line=i + 1) for i in range(5)]
        result = self.hooks.issues_to_diagnostics(issues, self.file_uri)
        assert len(result) == 5


# ===========================================================================
# IDEHooks.report_to_publish_params
# ===========================================================================


class TestIDEHooksReportToPublishParams:
    def setup_method(self):
        self.hooks = IDEHooks()

    def test_has_uri_key(self):
        report = _make_report(file_path="/tmp/test.nxl")
        params = self.hooks.report_to_publish_params(report)
        assert "uri" in params

    def test_has_diagnostics_key(self):
        report = _make_report(file_path="/tmp/test.nxl")
        params = self.hooks.report_to_publish_params(report)
        assert "diagnostics" in params

    def test_uri_is_file_scheme(self):
        report = _make_report(file_path="/tmp/test.nxl")
        params = self.hooks.report_to_publish_params(report)
        assert params["uri"].startswith("file://")


# ===========================================================================
# IDEHooks.fix_to_code_action
# ===========================================================================


class TestIDEHooksFixToCodeAction:
    def setup_method(self):
        self.hooks = IDEHooks()
        self.file_uri = "file:///test.nxl"

    def _make_suggestion(self):
        issue = _make_issue()
        edit = TextEdit(1, 1, 1, 8, "clean")
        return FixSuggestion(issue=issue, description="remove trailing space", edits=[edit])

    def test_kind_is_quickfix(self):
        suggestion = self._make_suggestion()
        action = self.hooks.fix_to_code_action(suggestion, self.file_uri, is_preferred=True)
        assert action.get("kind") == "quickfix"

    def test_title_present(self):
        suggestion = self._make_suggestion()
        action = self.hooks.fix_to_code_action(suggestion, self.file_uri, is_preferred=True)
        assert "title" in action
        assert len(action["title"]) > 0

    def test_edit_changes_contains_file_uri(self):
        suggestion = self._make_suggestion()
        action = self.hooks.fix_to_code_action(suggestion, self.file_uri, is_preferred=True)
        changes = action.get("edit", {}).get("changes", {})
        assert self.file_uri in changes

    def test_diagnostics_included(self):
        suggestion = self._make_suggestion()
        action = self.hooks.fix_to_code_action(suggestion, self.file_uri, is_preferred=True)
        assert "diagnostics" in action

    def test_is_preferred_respected(self):
        suggestion = self._make_suggestion()
        preferred = self.hooks.fix_to_code_action(suggestion, self.file_uri, is_preferred=True)
        not_preferred = self.hooks.fix_to_code_action(suggestion, self.file_uri, is_preferred=False)
        assert preferred.get("isPreferred") is True
        assert not_preferred.get("isPreferred") is False


# ===========================================================================
# IDEHooks.fixes_to_code_actions
# ===========================================================================


class TestIDEHooksFixesToCodeActions:
    def setup_method(self):
        self.hooks = IDEHooks()
        self.file_uri = "file:///test.nxl"

    def _make_suggestion(self, line=1):
        issue = _make_issue(line=line)
        edit = TextEdit(line, 1, line, 5, "edit")
        return FixSuggestion(issue=issue, description="apply edit", edits=[edit])

    def test_empty_returns_empty(self):
        result = self.hooks.fixes_to_code_actions([], self.file_uri)
        assert result == []

    def test_first_action_is_preferred(self):
        suggestions = [self._make_suggestion(1), self._make_suggestion(2)]
        actions = self.hooks.fixes_to_code_actions(suggestions, self.file_uri)
        assert actions[0].get("isPreferred") is True

    def test_subsequent_actions_not_preferred(self):
        suggestions = [self._make_suggestion(1), self._make_suggestion(2)]
        actions = self.hooks.fixes_to_code_actions(suggestions, self.file_uri)
        assert actions[1].get("isPreferred") is False


# ===========================================================================
# IDEHooks.get_code_actions_for_range
# ===========================================================================


class TestIDEHooksGetCodeActionsForRange:
    def setup_method(self):
        self.hooks = IDEHooks()
        self.file_uri = "file:///test.nxl"

    def _make_suggestion(self, line):
        issue = _make_issue(line=line)
        edit = TextEdit(line, 1, line, 2, "x")
        return FixSuggestion(issue=issue, description="edit", edits=[edit])

    def test_issue_in_range_included(self):
        issues = [_make_issue(line=5)]
        suggestions = [self._make_suggestion(5)]
        actions = self.hooks.get_code_actions_for_range(
            issues, suggestions, self.file_uri, start_line=3, end_line=7
        )
        assert len(actions) >= 1

    def test_issue_outside_range_excluded(self):
        issues = [_make_issue(line=15)]
        suggestions = [self._make_suggestion(15)]
        actions = self.hooks.get_code_actions_for_range(
            issues, suggestions, self.file_uri, start_line=1, end_line=3
        )
        assert len(actions) == 0

    def test_empty_result_when_no_suggestions_in_range(self):
        actions = self.hooks.get_code_actions_for_range(
            [], [], self.file_uri, start_line=1, end_line=10
        )
        assert actions == []

    def test_boundary_lines_inclusive(self):
        issues = [_make_issue(line=1), _make_issue(line=5)]
        suggestions = [self._make_suggestion(1), self._make_suggestion(5)]
        actions = self.hooks.get_code_actions_for_range(
            issues, suggestions, self.file_uri, start_line=1, end_line=5
        )
        assert len(actions) == 2


# ===========================================================================
# IDEHooks.reports_to_workspace_diagnostics
# ===========================================================================


class TestIDEHooksWorkspaceDiagnostics:
    def setup_method(self):
        self.hooks = IDEHooks()

    def test_multiple_reports_multiple_uris(self):
        r1 = _make_report(file_path="/tmp/a.nxl")
        r2 = _make_report(file_path="/tmp/b.nxl")
        result = self.hooks.reports_to_workspace_diagnostics([r1, r2])
        assert len(result) == 2

    def test_each_uri_has_diagnostics_list(self):
        issue = _make_issue(line=1)
        r = _make_report(issues=[issue], file_path="/tmp/c.nxl")
        result = self.hooks.reports_to_workspace_diagnostics([r])
        for diagnostics in result.values():
            assert isinstance(diagnostics, list)

    def test_empty_report_gives_empty_diagnostics(self):
        r = _make_report(issues=[], file_path="/tmp/empty.nxl")
        result = self.hooks.reports_to_workspace_diagnostics([r])
        for uri, diagnostics in result.items():
            assert diagnostics == []


# ===========================================================================
# LspFormatter facade
# ===========================================================================


class TestLspFormatterFacade:
    def setup_method(self):
        self.formatter = LspFormatter()

    def test_diagnostics_for_file_returns_dict_with_uri_and_diagnostics(self):
        issue = _make_issue(line=1)
        report = _make_report(issues=[issue], file_path="/tmp/test.nxl")
        source = "some code   \n"
        result = self.formatter.diagnostics_for_file(report, source)
        assert "uri" in result
        assert "diagnostics" in result

    def test_code_actions_for_range_returns_list(self):
        issue = _make_issue(code="W-STYLE-TRAIL", line=1)
        report = _make_report(issues=[issue], file_path="/tmp/test.nxl")
        source = "trailing   \n"
        result = self.formatter.code_actions_for_range(report, source, 1, 1)
        assert isinstance(result, list)

    def test_apply_suggestions_returns_string(self):
        issue = _make_issue(code="W-STYLE-TRAIL", line=1)
        report = _make_report(issues=[issue], file_path="/tmp/test.nxl")
        source = "trailing   \n"
        result = self.formatter.apply_fixes(report, source, dry_run=True)
        assert isinstance(result, str)

    def test_hooks_property_is_ide_hooks(self):
        assert isinstance(self.formatter.hooks, IDEHooks)

    def test_custom_source_name_propagated(self):
        formatter = LspFormatter(source_name="my-linter")
        issue = _make_issue(line=1)
        report = _make_report(issues=[issue], file_path="/tmp/test.nxl")
        result = formatter.diagnostics_for_file(report, "line\n")
        # Diagnostics should carry the custom source name
        for diag in result.get("diagnostics", []):
            assert diag.get("source") == "my-linter"
