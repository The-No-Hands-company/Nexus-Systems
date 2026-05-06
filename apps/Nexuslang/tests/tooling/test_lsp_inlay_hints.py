"""
Tests: Inlay Hints Provider
============================

Validates InlayHintsProvider against NexusLang source patterns.
"""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent.parent.parent / "src"))

import pytest
from nexuslang.lsp.inlay_hints import InlayHintsProvider, INLAY_HINT_TYPE, INLAY_HINT_PARAMETER


class MockServer:
    """Minimal server stub for provider tests."""

    def __init__(self):
        self.documents = {}
        self.workspace_index = None


# ---------------------------------------------------------------------------
# Fixtures
# ---------------------------------------------------------------------------


@pytest.fixture
def provider():
    return InlayHintsProvider(MockServer())


# ---------------------------------------------------------------------------
# Type inference tests
# ---------------------------------------------------------------------------


def test_infer_integer(provider):
    assert provider._infer_type("42") == "Integer"


def test_infer_negative_integer(provider):
    assert provider._infer_type("-7") == "Integer"


def test_infer_float(provider):
    assert provider._infer_type("3.14") == "Float"


def test_infer_negative_float(provider):
    assert provider._infer_type("-2.718") == "Float"


def test_infer_string_double(provider):
    assert provider._infer_type('"hello"') == "String"


def test_infer_string_single(provider):
    assert provider._infer_type("'world'") == "String"


def test_infer_boolean_true(provider):
    assert provider._infer_type("true") == "Boolean"


def test_infer_boolean_false(provider):
    assert provider._infer_type("false") == "Boolean"


def test_infer_boolean_case_insensitive(provider):
    assert provider._infer_type("True") == "Boolean"
    assert provider._infer_type("FALSE") == "Boolean"


def test_infer_list(provider):
    assert provider._infer_type("[1, 2, 3]") == "List"


def test_infer_dict(provider):
    assert provider._infer_type('{"key": "value"}') == "Dict"


def test_infer_rc(provider):
    assert provider._infer_type("Rc of MyStruct") == "Rc"


def test_infer_arc(provider):
    assert provider._infer_type("Arc of Node") == "Arc"


def test_infer_unknown_returns_none(provider):
    assert provider._infer_type("some_variable") is None


def test_infer_empty_returns_none(provider):
    assert provider._infer_type("") is None


# ---------------------------------------------------------------------------
# Variable declaration type hints
# ---------------------------------------------------------------------------


def test_hint_integer_declaration(provider):
    lines = ["set count to 0"]
    hints = provider.get_inlay_hints("file:///t.nxl", "\n".join(lines))
    type_hints = [h for h in hints if h["kind"] == INLAY_HINT_TYPE]
    assert len(type_hints) == 1
    assert type_hints[0]["label"] == ": Integer"


def test_hint_float_declaration(provider):
    hints = provider.get_inlay_hints("file:///t.nxl", 'set pi to 3.14159')
    type_hints = [h for h in hints if h["kind"] == INLAY_HINT_TYPE]
    assert len(type_hints) == 1
    assert type_hints[0]["label"] == ": Float"


def test_hint_string_declaration(provider):
    hints = provider.get_inlay_hints("file:///t.nxl", 'set name to "Alice"')
    type_hints = [h for h in hints if h["kind"] == INLAY_HINT_TYPE]
    assert len(type_hints) == 1
    assert type_hints[0]["label"] == ": String"


def test_hint_boolean_declaration(provider):
    hints = provider.get_inlay_hints("file:///t.nxl", "set flag to true")
    type_hints = [h for h in hints if h["kind"] == INLAY_HINT_TYPE]
    assert len(type_hints) == 1
    assert type_hints[0]["label"] == ": Boolean"


def test_hint_list_declaration(provider):
    hints = provider.get_inlay_hints("file:///t.nxl", "set items to [1, 2, 3]")
    type_hints = [h for h in hints if h["kind"] == INLAY_HINT_TYPE]
    assert len(type_hints) == 1
    assert type_hints[0]["label"] == ": List"


def test_hint_position_after_variable_name(provider):
    line = "set counter to 0"
    hints = provider.get_inlay_hints("file:///t.nxl", line)
    type_hints = [h for h in hints if h["kind"] == INLAY_HINT_TYPE]
    assert type_hints
    # "set " = 4 chars, "counter" = 7 chars -> hint at col 11
    assert type_hints[0]["position"]["character"] == 11


def test_hint_indented_declaration(provider):
    line = "    set x to 99"
    hints = provider.get_inlay_hints("file:///t.nxl", line)
    type_hints = [h for h in hints if h["kind"] == INLAY_HINT_TYPE]
    assert len(type_hints) == 1
    assert type_hints[0]["label"] == ": Integer"
    # "    set " = 8 chars, "x" = 1 -> hint at col 9
    assert type_hints[0]["position"]["character"] == 9


def test_no_hint_for_variable_assigned_variable(provider):
    # 'set x to y' — y is a variable name, not a literal
    hints = provider.get_inlay_hints("file:///t.nxl", "set x to y")
    type_hints = [h for h in hints if h["kind"] == INLAY_HINT_TYPE]
    assert len(type_hints) == 0


def test_no_hint_for_comment_lines(provider):
    hints = provider.get_inlay_hints("file:///t.nxl", "# set x to 42")
    assert hints == []


def test_no_hint_for_blank_lines(provider):
    hints = provider.get_inlay_hints("file:///t.nxl", "")
    assert hints == []


def test_multiple_declarations_in_document(provider):
    src = "set a to 1\nset b to 2.0\nset c to \"hi\"\n"
    hints = provider.get_inlay_hints("file:///t.nxl", src)
    type_hints = [h for h in hints if h["kind"] == INLAY_HINT_TYPE]
    assert len(type_hints) == 3
    labels = {h["label"] for h in type_hints}
    assert ": Integer" in labels
    assert ": Float" in labels
    assert ": String" in labels


# ---------------------------------------------------------------------------
# Range filtering
# ---------------------------------------------------------------------------


def test_range_restricts_hints(provider):
    src = "set a to 1\nset b to 2\nset c to 3\n"
    # Request only line 1 (second line)
    range_ = {
        "start": {"line": 1, "character": 0},
        "end": {"line": 1, "character": 100},
    }
    hints = provider.get_inlay_hints("file:///t.nxl", src, range_)
    type_hints = [h for h in hints if h["kind"] == INLAY_HINT_TYPE]
    # Only the second declaration should appear
    assert len(type_hints) == 1
    assert type_hints[0]["position"]["line"] == 1


# ---------------------------------------------------------------------------
# Parameter name hints
# ---------------------------------------------------------------------------


def test_parameter_hints_for_known_function(provider):
    src = (
        "function add with first as Integer and second as Integer returns Integer\n"
        "    return first plus second\n"
        "end\n"
        "\n"
        "set result to add with 3 and 5\n"
    )
    hints = provider.get_inlay_hints("file:///t.nxl", src)
    param_hints = [h for h in hints if h["kind"] == INLAY_HINT_PARAMETER]
    assert len(param_hints) == 2
    labels = {h["label"] for h in param_hints}
    assert "first: " in labels
    assert "second: " in labels


def test_no_parameter_hints_for_unknown_function(provider):
    src = "set x to mystery_func with 1 and 2\n"
    hints = provider.get_inlay_hints("file:///t.nxl", src)
    param_hints = [h for h in hints if h["kind"] == INLAY_HINT_PARAMETER]
    assert len(param_hints) == 0


def test_no_parameter_hints_for_named_args(provider):
    # If args already use ':' syntax, no duplicate hints
    src = (
        "function greet with name as String returns String\n"
        "    return name\n"
        "end\n"
        "set msg to greet with name: \"Alice\"\n"
    )
    hints = provider.get_inlay_hints("file:///t.nxl", src)
    param_hints = [h for h in hints if h["kind"] == INLAY_HINT_PARAMETER]
    assert len(param_hints) == 0


# ---------------------------------------------------------------------------
# LSP protocol compliance
# ---------------------------------------------------------------------------


def test_all_hints_have_required_fields(provider):
    src = "set x to 42\nset y to 3.14\n"
    hints = provider.get_inlay_hints("file:///t.nxl", src)
    for hint in hints:
        assert "position" in hint
        assert "line" in hint["position"]
        assert "character" in hint["position"]
        assert "label" in hint
        assert "kind" in hint
        assert hint["kind"] in (INLAY_HINT_TYPE, INLAY_HINT_PARAMETER)


def test_hint_line_numbers_match_source(provider):
    src = "# intro\nset a to 1\n# middle\nset b to 2\n"
    hints = provider.get_inlay_hints("file:///t.nxl", src)
    type_hints = [h for h in hints if h["kind"] == INLAY_HINT_TYPE]
    lines = {h["position"]["line"] for h in type_hints}
    # 'set a to 1' is line 1, 'set b to 2' is line 3
    assert 1 in lines
    assert 3 in lines
