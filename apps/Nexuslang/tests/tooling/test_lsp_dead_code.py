"""
Tests: Dead Code Provider
==========================

Validates DeadCodeProvider detects unreachable statements, empty
function bodies, and constant conditions.
"""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent.parent.parent / "src"))

import pytest
from nexuslang.lsp.dead_code import DeadCodeProvider


class MockServer:
    def __init__(self):
        self.documents = {}
        self.workspace_index = None


@pytest.fixture
def provider():
    return DeadCodeProvider(MockServer())


# ---------------------------------------------------------------------------
# 1. Unreachable code after return
# ---------------------------------------------------------------------------


UNREACHABLE_RETURN = """\
function compute with x as Integer returns Integer
    if x is greater than 0
        return x
        set y to 1
        print text y
    end
    return 0
end
"""


def test_unreachable_after_return_detected(provider):
    diags = provider.get_diagnostics("file:///t.nxl", UNREACHABLE_RETURN)
    dead = [d for d in diags if d["code"] == "dead-unreachable"]
    assert len(dead) >= 2  # 'set y to 1' and 'print text y' are unreachable


def test_unreachable_after_return_line_numbers(provider):
    diags = provider.get_diagnostics("file:///t.nxl", UNREACHABLE_RETURN)
    dead = [d for d in diags if d["code"] == "dead-unreachable"]
    lines = {d["range"]["start"]["line"] for d in dead}
    # 'set y to 1' is line 3, 'print text y' is line 4
    assert 3 in lines
    assert 4 in lines


def test_unreachable_after_return_severity_is_warning(provider):
    diags = provider.get_diagnostics("file:///t.nxl", UNREACHABLE_RETURN)
    dead = [d for d in diags if d["code"] == "dead-unreachable"]
    for d in dead:
        assert d["severity"] == 2  # WARNING


def test_unreachable_after_return_has_suggestion(provider):
    diags = provider.get_diagnostics("file:///t.nxl", UNREACHABLE_RETURN)
    dead = [d for d in diags if d["code"] == "dead-unreachable"]
    for d in dead:
        assert d["data"]["fixes"]


UNREACHABLE_BREAK = """\
while counter is less than 10
    set counter to counter plus 1
    break
    print text "never"
end
"""


def test_unreachable_after_break(provider):
    diags = provider.get_diagnostics("file:///t.nxl", UNREACHABLE_BREAK)
    dead = [d for d in diags if d["code"] == "dead-unreachable"]
    assert len(dead) >= 1
    lines = {d["range"]["start"]["line"] for d in dead}
    assert 3 in lines  # 'print text "never"'


UNREACHABLE_CONTINUE = """\
for each item in items
    continue
    process item
end
"""


def test_unreachable_after_continue(provider):
    diags = provider.get_diagnostics("file:///t.nxl", UNREACHABLE_CONTINUE)
    dead = [d for d in diags if d["code"] == "dead-unreachable"]
    assert any(d["range"]["start"]["line"] == 2 for d in dead)


def test_no_false_positive_after_conditional_return(provider):
    # The return is inside an 'if' body; code after the if is still reachable
    src = """\
function check with x as Integer returns Integer
    if x is greater than 0
        return x
    end
    return 0
end
"""
    diags = provider.get_diagnostics("file:///t.nxl", src)
    dead = [d for d in diags if d["code"] == "dead-unreachable"]
    # `return 0` should NOT be flagged — the `end` resets the dead zone
    assert len(dead) == 0


# ---------------------------------------------------------------------------
# 2. Empty function bodies
# ---------------------------------------------------------------------------


EMPTY_FUNC = """\
function empty_fn with x as Integer returns Integer
end

function populated with x as Integer returns Integer
    return x plus 1
end
"""


def test_empty_function_detected(provider):
    diags = provider.get_diagnostics("file:///t.nxl", EMPTY_FUNC)
    empty = [d for d in diags if d["code"] == "dead-empty-function"]
    assert len(empty) == 1


def test_empty_function_line_number(provider):
    diags = provider.get_diagnostics("file:///t.nxl", EMPTY_FUNC)
    empty = [d for d in diags if d["code"] == "dead-empty-function"]
    assert empty[0]["range"]["start"]["line"] == 0


def test_empty_function_severity_is_hint(provider):
    diags = provider.get_diagnostics("file:///t.nxl", EMPTY_FUNC)
    empty = [d for d in diags if d["code"] == "dead-empty-function"]
    assert empty[0]["severity"] == 4  # HINT


def test_non_empty_function_not_flagged(provider):
    src = """\
function add with a as Integer and b as Integer returns Integer
    return a plus b
end
"""
    diags = provider.get_diagnostics("file:///t.nxl", src)
    empty = [d for d in diags if d["code"] == "dead-empty-function"]
    assert len(empty) == 0


def test_comment_only_body_detected_as_empty(provider):
    src = """\
function stub with x as Integer returns Integer
    # TODO implement
end
"""
    diags = provider.get_diagnostics("file:///t.nxl", src)
    empty = [d for d in diags if d["code"] == "dead-empty-function"]
    assert len(empty) == 1


# ---------------------------------------------------------------------------
# 3. Constant conditions
# ---------------------------------------------------------------------------


def test_constant_true_condition(provider):
    src = "if true\n    print text \"always\"\nend\n"
    diags = provider.get_diagnostics("file:///t.nxl", src)
    cc = [d for d in diags if d["code"] == "dead-constant-condition"]
    assert len(cc) == 1
    assert "always taken" in cc[0]["message"]


def test_constant_false_condition(provider):
    src = "if false\n    print text \"never\"\nend\n"
    diags = provider.get_diagnostics("file:///t.nxl", src)
    cc = [d for d in diags if d["code"] == "dead-constant-condition"]
    assert len(cc) == 1
    assert "never taken" in cc[0]["message"]


def test_constant_zero_condition(provider):
    src = "if 0\n    print text \"dead\"\nend\n"
    diags = provider.get_diagnostics("file:///t.nxl", src)
    cc = [d for d in diags if d["code"] == "dead-constant-condition"]
    assert len(cc) == 1


def test_constant_one_condition(provider):
    src = "if 1\n    print text \"always\"\nend\n"
    diags = provider.get_diagnostics("file:///t.nxl", src)
    cc = [d for d in diags if d["code"] == "dead-constant-condition"]
    assert len(cc) == 1


def test_normal_condition_not_flagged(provider):
    src = "if count is greater than 5\n    print text \"big\"\nend\n"
    diags = provider.get_diagnostics("file:///t.nxl", src)
    cc = [d for d in diags if d["code"] == "dead-constant-condition"]
    assert len(cc) == 0


# ---------------------------------------------------------------------------
# Edge cases
# ---------------------------------------------------------------------------


def test_empty_document_returns_empty_list(provider):
    assert provider.get_diagnostics("file:///empty.nxl", "") == []


def test_comment_only_document_returns_empty_list(provider):
    assert provider.get_diagnostics("file:///c.nxl", "# just a comment\n") == []


def test_diagnostic_has_required_lsp_fields(provider):
    src = "if true\n    print text \"x\"\nend\n"
    diags = provider.get_diagnostics("file:///t.nxl", src)
    for d in diags:
        assert "range" in d
        assert "start" in d["range"]
        assert "end" in d["range"]
        assert "severity" in d
        assert "message" in d
        assert "code" in d
        assert "source" in d
        assert d["source"] == "nlpl-dead-code"


def test_diagnostic_has_suggestions_in_data(provider):
    src = "if false\n    print text \"x\"\nend\n"
    diags = provider.get_diagnostics("file:///t.nxl", src)
    for d in diags:
        assert "data" in d
        assert "fixes" in d["data"]
        assert isinstance(d["data"]["fixes"], list)
        assert len(d["data"]["fixes"]) > 0
