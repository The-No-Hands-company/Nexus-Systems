"""
Tests for the AST-aware NexusLang formatter pipeline.

Validates:
- Token-based indentation (block keywords drive indent, not string heuristics)
- Canonical operator spacing (multi-word tokens normalised)
- String literal preservation (no regex inside string content)
- Trailing comment preservation
- Comment-only line handling (# and ## forms)
- Fallback to regex path when tokenisation raises
- Idempotency (format(format(x)) == format(x))
- get_formatting_edits returns [] for already-formatted documents
"""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent.parent.parent / 'src'))

import pytest
from nexuslang.lsp.formatter import NLPLFormatter


@pytest.fixture()
def fmt():
    return NLPLFormatter()


# ---------------------------------------------------------------------------
# Indentation: basic blocks
# ---------------------------------------------------------------------------

class TestIndentation:
    def test_function_body_indented(self, fmt):
        code = "function greet\nprint text \"hi\"\nend\n"
        result = fmt.format(code)
        lines = result.strip().splitlines()
        assert lines[0] == "function greet"
        assert lines[1].startswith("    ")   # 4-space indent inside function
        assert lines[2] == "end"

    def test_if_else_indentation(self, fmt):
        code = (
            "if x is greater than 0\n"
            "print text \"pos\"\n"
            "else\n"
            "print text \"neg\"\n"
            "end\n"
        )
        result = fmt.format(code)
        lines = result.strip().splitlines()
        assert lines[0] == "if x is greater than 0"
        assert lines[1] == "    print text \"pos\""
        assert lines[2] == "else"
        assert lines[3] == "    print text \"neg\""
        assert lines[4] == "end"

    def test_nested_blocks(self, fmt):
        code = (
            "function outer\n"
            "if flag\n"
            "set x to 1\n"
            "end\n"
            "end\n"
        )
        result = fmt.format(code)
        lines = result.strip().splitlines()
        assert lines[0] == "function outer"
        assert lines[1] == "    if flag"
        assert lines[2] == "        set x to 1"
        assert lines[3] == "    end"
        # blank separator is inserted after inner end; outer end follows
        outer_end = next(l for l in lines if l == "end")
        assert outer_end == "end"

    def test_class_with_method(self, fmt):
        code = (
            "class Counter\n"
            "set count to 0\n"
            "function increment\n"
            "set count to count plus 1\n"
            "end\n"
            "end\n"
        )
        result = fmt.format(code)
        lines = result.strip().splitlines()
        assert lines[0] == "class Counter"
        assert lines[1] == "    set count to 0"
        assert lines[2].startswith("    function increment")
        assert lines[3].startswith("        set count to")
        assert lines[4] == "    end"

    def test_visibility_modifier_preserves_indent(self, fmt):
        code = (
            "class Foo\n"
            "public function bar\n"
            "set x to 1\n"
            "end\n"
            "end\n"
        )
        result = fmt.format(code)
        lines = result.strip().splitlines()
        assert lines[1].startswith("    public function bar")
        assert lines[2].startswith("        set x to 1")

    def test_while_loop(self, fmt):
        code = "while counter is less than 10\nset counter to counter plus 1\nend\n"
        result = fmt.format(code)
        lines = result.strip().splitlines()
        assert lines[0].startswith("while")
        assert lines[1].startswith("    ")
        assert lines[2] == "end"

    def test_for_each_loop(self, fmt):
        code = "for each item in collection\nprint text item\nend\n"
        result = fmt.format(code)
        lines = result.strip().splitlines()
        assert lines[0].startswith("for each")
        assert lines[1].startswith("    ")
        assert lines[2] == "end"

    def test_try_catch(self, fmt):
        code = (
            "try\n"
            "set x to risky_call\n"
            "catch error\n"
            "print text \"failed\"\n"
            "end\n"
        )
        result = fmt.format(code)
        lines = result.strip().splitlines()
        assert lines[0] == "try"
        assert lines[1].startswith("    ")
        assert lines[2] == "catch error"
        assert lines[3].startswith("    ")
        assert lines[4] == "end"


# ---------------------------------------------------------------------------
# Operator spacing normalisation
# ---------------------------------------------------------------------------

class TestOperatorSpacing:
    def test_extra_spaces_collapsed(self, fmt):
        code = "set   x   to   5\n"
        result = fmt.format(code)
        assert "set x to 5" in result

    def test_comparison_operators_canonical(self, fmt):
        code = "if x  is   greater  than  0\nend\n"
        result = fmt.format(code)
        assert "greater than" in result

    def test_greater_than_or_equal_canonical(self, fmt):
        code = "if score is  greater  than  or  equal  to  100\nend\n"
        result = fmt.format(code)
        assert "greater than or equal to" in result

    def test_arithmetic_operators(self, fmt):
        code = "set result to a  plus  b\n"
        result = fmt.format(code)
        assert "a plus b" in result

    def test_divided_by_canonical(self, fmt):
        code = "set avg to total  divided   by  count\n"
        result = fmt.format(code)
        assert "divided by" in result


# ---------------------------------------------------------------------------
# String literal preservation
# ---------------------------------------------------------------------------

class TestStringLiterals:
    def test_string_content_not_modified(self, fmt):
        code = "print text \"hello   world   with   spaces\"\n"
        result = fmt.format(code)
        assert "hello   world   with   spaces" in result

    def test_hash_inside_string_not_treated_as_comment(self, fmt):
        code = "set tag to \"#important\"\n"
        result = fmt.format(code)
        assert '"#important"' in result

    def test_operator_keywords_inside_string_preserved(self, fmt):
        code = "set msg to \"x plus y equals z\"\n"
        result = fmt.format(code)
        # Must NOT collapse "plus" or alter string contents
        assert '"x plus y equals z"' in result


# ---------------------------------------------------------------------------
# Comment handling
# ---------------------------------------------------------------------------

class TestComments:
    def test_regular_comment_preserved(self, fmt):
        code = "# this is a comment\nset x to 1\n"
        result = fmt.format(code)
        assert "# this is a comment" in result

    def test_regular_comment_indented_correctly(self, fmt):
        code = "function foo\n# inner comment\nset x to 1\nend\n"
        result = fmt.format(code)
        lines = result.strip().splitlines()
        comment_line = next(l for l in lines if l.strip().startswith("#"))
        assert comment_line.startswith("    ")

    def test_doc_comment_preserved(self, fmt):
        code = "## This is a doc comment\nfunction greet\nend\n"
        result = fmt.format(code)
        assert "## This is a doc comment" in result

    def test_trailing_comment_preserved(self, fmt):
        code = "set x to 5  # inline comment\n"
        result = fmt.format(code)
        assert "# inline comment" in result
        assert "set x to 5" in result

    def test_no_double_blank_lines(self, fmt):
        code = "set x to 1\n\n\n\nset y to 2\n"
        result = fmt.format(code)
        assert "\n\n\n" not in result


# ---------------------------------------------------------------------------
# Blank-line insertion after block endings
# ---------------------------------------------------------------------------

class TestBlankLineSeparators:
    def test_blank_after_function_end(self, fmt):
        code = "function foo\nset x to 1\nend\nset y to 2\n"
        result = fmt.format(code)
        lines = result.splitlines()
        end_idx = next(i for i, l in enumerate(lines) if l.strip() == "end")
        assert lines[end_idx + 1] == ""

    def test_no_blank_at_eof(self, fmt):
        code = "function foo\nset x to 1\nend\n"
        result = fmt.format(code)
        assert not result.endswith("\n\n")


# ---------------------------------------------------------------------------
# Idempotency
# ---------------------------------------------------------------------------

class TestIdempotency:
    def test_simple_assignment_idempotent(self, fmt):
        code = "set x to 5\n"
        once = fmt.format(code)
        twice = fmt.format(once)
        assert once == twice

    def test_function_idempotent(self, fmt):
        code = (
            "function add with a and b\n"
            "    return a plus b\n"
            "end\n"
        )
        once = fmt.format(code)
        twice = fmt.format(once)
        assert once == twice

    def test_nested_blocks_idempotent(self, fmt):
        code = (
            "function outer\n"
            "    if flag\n"
            "        set x to 1\n"
            "    end\n"
            "end\n"
        )
        once = fmt.format(code)
        twice = fmt.format(once)
        assert once == twice

    def test_class_idempotent(self, fmt):
        code = (
            "class Animal\n"
            "    set name to \"unnamed\"\n"
            "    function speak\n"
            "        print text name\n"
            "    end\n"
            "end\n"
        )
        once = fmt.format(code)
        twice = fmt.format(once)
        assert once == twice


# ---------------------------------------------------------------------------
# get_formatting_edits
# ---------------------------------------------------------------------------

class TestGetFormattingEdits:
    def test_returns_empty_for_already_formatted(self, fmt):
        code = "set x to 1\n"
        # Format once to get canonical form
        canonical = fmt.format(code)
        edits = fmt.get_formatting_edits(canonical)
        assert edits == []

    def test_returns_edit_for_unformatted_code(self, fmt):
        code = "set   x   to   5\n"
        edits = fmt.get_formatting_edits(code)
        assert len(edits) == 1
        assert edits[0]['newText'] != code

    def test_edit_covers_full_document(self, fmt):
        code = "set   x   to   5\nset y to 10\n"
        edits = fmt.get_formatting_edits(code)
        assert len(edits) == 1
        r = edits[0]['range']
        assert r['start']['line'] == 0
        assert r['start']['character'] == 0

    def test_applying_edit_yields_formatted_output(self, fmt):
        code = "set   x   to   5\n"
        edits = fmt.get_formatting_edits(code)
        assert len(edits) == 1
        assert edits[0]['newText'] == fmt.format(code)


# ---------------------------------------------------------------------------
# Regex fallback
# ---------------------------------------------------------------------------

class TestRegexFallback:
    def test_format_regex_produces_output(self, fmt):
        code = "set   x   to   5\nif x is greater than 3\nprint text \"yes\"\nend\n"
        result = fmt._format_regex(code)
        assert "set x to 5" in result
        assert "    print text" in result

    def test_fallback_indentation(self, fmt):
        code = "function foo\nset x to 1\nend\n"
        result = fmt._format_regex(code)
        lines = result.strip().splitlines()
        assert lines[1].startswith("    ")
