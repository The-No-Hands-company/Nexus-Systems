"""
Error-surface hardening tests for errors.py.

Day 13 sprint gate: error messages must include line and column context and
at least one actionable suggestion wherever applicable.  This suite also
validates that error-formatting helpers handle adversarial inputs without
raising, leaking internal state, or producing empty/truncated output.

Coverage areas:
  - format_source_context: OOB line, OOB column, empty source, single-line
    source, large context_lines, column 1 and end-of-line
  - NxlSyntaxError, NxlRuntimeError, NxlNameError, NxlTypeError:
    formatted output includes location and suggestion fields
  - NxlNameError "did you mean" matching: correct candidate, no candidates,
    empty name, empty available_names
  - get_close_matches: normal match, empty word, empty possibilities, very
    large possibility list (performance cap), identical input
  - suggest_correction: all documented keys return strings, unknown key
    returns None
"""

import pytest
from nexuslang.errors import (
    NxlSyntaxError,
    NxlRuntimeError,
    NxlNameError,
    NxlTypeError,
    NxlContractError,
    get_close_matches,
    format_source_context,
    suggest_correction,
)


SAMPLE_SOURCE = """\
set x to 10
set y to 0
set result to x divided by y
print text result
"""

# =============================================================================
# format_source_context: resilience matrix
# =============================================================================

class TestFormatSourceContextRobustness:

    def test_normal_case_returns_string(self):
        result = format_source_context(SAMPLE_SOURCE, line=3, column=18)
        assert isinstance(result, str)
        assert len(result) > 0

    def test_line_beyond_end_does_not_raise(self):
        result = format_source_context(SAMPLE_SOURCE, line=9999, column=1)
        assert isinstance(result, str)

    def test_line_one_always_works(self):
        result = format_source_context(SAMPLE_SOURCE, line=1, column=1)
        assert isinstance(result, str)
        assert len(result) > 0

    def test_column_one_shows_caret_at_start(self):
        result = format_source_context(SAMPLE_SOURCE, line=1, column=1)
        assert "^" in result

    def test_empty_source_does_not_raise(self):
        result = format_source_context("", line=1, column=1)
        assert isinstance(result, str)

    def test_single_line_source(self):
        result = format_source_context("set x to 1", line=1, column=5)
        assert isinstance(result, str)
        assert "^" in result

    def test_context_lines_zero(self):
        result = format_source_context(SAMPLE_SOURCE, line=2, column=1, context_lines=0)
        assert isinstance(result, str)
        assert "^" in result

    def test_large_context_lines_does_not_raise(self):
        result = format_source_context(SAMPLE_SOURCE, line=2, column=1, context_lines=100)
        assert isinstance(result, str)

    def test_column_beyond_line_length_does_not_raise(self):
        result = format_source_context("set x to 1", line=1, column=9999)
        assert isinstance(result, str)


# =============================================================================
# Error class: location fields in formatted output
# =============================================================================

class TestErrorLocationInOutput:

    def test_syntax_error_includes_line_number(self):
        err = NxlSyntaxError("unexpected token", line=5, column=3,
                              source_line="set x to ??")
        formatted = err.format_error()
        assert "5" in formatted

    def test_syntax_error_includes_column(self):
        err = NxlSyntaxError("unexpected token", line=5, column=3,
                              source_line="set x to ??")
        formatted = err.format_error()
        assert "3" in formatted

    def test_syntax_error_includes_suggestion(self):
        err = NxlSyntaxError("unexpected token", line=2, column=1,
                              suggestion="Did you forget 'end'?")
        formatted = err.format_error()
        assert "end" in formatted

    def test_syntax_error_includes_expected_got(self):
        err = NxlSyntaxError("token mismatch", line=1, column=4,
                              expected="identifier", got="number")
        formatted = err.format_error()
        assert "identifier" in formatted
        assert "number" in formatted

    def test_runtime_error_includes_line(self):
        err = NxlRuntimeError("division by zero", line=7)
        formatted = err.format_error()
        assert "7" in formatted

    def test_runtime_error_includes_stack_trace(self):
        err = NxlRuntimeError("stack overflow", line=10,
                               stack_trace=["main()", "fib(5)", "fib(4)"])
        formatted = err.format_error()
        assert "main()" in formatted

    def test_runtime_error_includes_variable_context(self):
        err = NxlRuntimeError("value error", line=3,
                               variable_context={"x": 42, "y": "hello"})
        formatted = err.format_error()
        assert "x" in formatted
        assert "42" in formatted

    def test_name_error_includes_line(self):
        err = NxlNameError(name="myVaraible", line=4)
        formatted = err.format_error()
        assert "4" in formatted

    def test_name_error_did_you_mean(self):
        err = NxlNameError(name="functin",
                           available_names=["function", "variable", "print"])
        formatted = err.format_error()
        assert "function" in formatted

    def test_type_error_includes_types(self):
        err = NxlTypeError("cannot add Integer and String", line=6,
                           expected_type="Integer", got_type="String")
        formatted = err.format_error()
        assert "Integer" in formatted
        assert "String" in formatted

    def test_contract_error_formats_without_raise(self):
        err = NxlContractError("precondition failed", line=8,
                               contract_kind="require")
        formatted = err.format_error()
        assert "precondition" in formatted


# =============================================================================
# NxlNameError: did-you-mean edge cases
# =============================================================================

class TestNxlNameErrorSuggestions:

    def test_exact_typo_suggests_correct_name(self):
        err = NxlNameError(name="prnit",
                           available_names=["print", "printf", "println"])
        assert "print" in err.suggestions

    def test_no_candidates_empty_suggestions(self):
        err = NxlNameError(name="zzzqqqxxx", available_names=["print", "set"])
        assert err.suggestions == []

    def test_empty_available_names(self):
        err = NxlNameError(name="foo", available_names=[])
        assert err.suggestions == []

    def test_no_available_names_kwarg(self):
        err = NxlNameError(name="bar")
        assert err.suggestions == []

    def test_explicit_suggestions_override_matching(self):
        err = NxlNameError(name="foo",
                           available_names=["totally_unrelated"],
                           suggestions=["forced_suggestion"])
        assert "forced_suggestion" in err.suggestions

    def test_unknown_name_message_contains_name(self):
        err = NxlNameError(name="missing_var")
        assert "missing_var" in str(err)

    def test_message_kwarg_preserved(self):
        err = NxlNameError(message="Custom message for 'foobar'")
        assert "Custom message" in str(err)


# =============================================================================
# get_close_matches: edge cases
# =============================================================================

class TestGetCloseMatchesEdgeCases:

    def test_normal_match(self):
        result = get_close_matches("functin", ["function", "variable", "class"])
        assert "function" in result

    def test_empty_word_returns_empty(self):
        result = get_close_matches("", ["function", "variable"])
        assert result == []

    def test_empty_possibilities_returns_empty(self):
        result = get_close_matches("something", [])
        assert result == []

    def test_identical_input_matches_itself(self):
        result = get_close_matches("function", ["function", "variable"])
        assert "function" in result

    def test_large_possibility_list_does_not_raise(self):
        # 1000 candidates — should be capped at 256 internally
        many = [f"var_{i}" for i in range(1000)]
        result = get_close_matches("var_1", many)
        assert isinstance(result, list)

    def test_large_possibility_list_returns_reasonable_result(self):
        many = [f"name_{i}" for i in range(500)] + ["functin"]
        result = get_close_matches("functin", many)
        assert "functin" in result

    def test_returns_at_most_n_results(self):
        many = ["function", "functional", "functor", "func", "funky"]
        result = get_close_matches("functi", many, n=2)
        assert len(result) <= 2

    def test_cutoff_affects_results(self):
        result_strict = get_close_matches("x", ["xyz", "xyzabc"], cutoff=0.99)
        result_loose  = get_close_matches("x", ["xyz", "xyzabc"], cutoff=0.01)
        assert len(result_loose) >= len(result_strict)


# =============================================================================
# suggest_correction: all documented keys return strings
# =============================================================================

DOCUMENTED_ERROR_KEYS = [
    "missing_end",
    "unexpected_token",
    "undefined_variable",
    "undefined_function",
    "type_mismatch",
    "indentation_error",
    "missing_colon",
    "invalid_syntax",
    "division_by_zero",
    "index_out_of_range",
    "key_not_found",
]


@pytest.mark.parametrize("key", DOCUMENTED_ERROR_KEYS)
def test_suggest_correction_returns_nonempty_string(key):
    result = suggest_correction(key, {})
    assert isinstance(result, str)
    assert len(result) > 0


def test_suggest_correction_unknown_key_returns_none():
    assert suggest_correction("totally_unknown_error_type_xyz", {}) is None


def test_suggest_correction_empty_key_returns_none():
    assert suggest_correction("", {}) is None
