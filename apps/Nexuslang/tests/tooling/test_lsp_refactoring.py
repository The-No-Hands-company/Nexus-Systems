"""
Tests: Advanced Code Actions and Refactoring
=============================================

Validates the 4 new refactoring actions added in Week 5:
  - organize_imports  (sort contiguous import blocks)
  - toggle_comment    (add / remove # prefix from selected lines)
  - extract_variable  (wrap selected expression in a named variable)
  - inline_variable   (replace all uses of a variable with its value)
"""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent.parent.parent / "src"))

import pytest
from nexuslang.lsp.code_actions import CodeActionsProvider


class MockServer:
    def __init__(self):
        self.documents = {}
        self.workspace_index = None


@pytest.fixture
def provider():
    return CodeActionsProvider(MockServer())


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------


def _range(sl, sc, el, ec):
    return {
        "start": {"line": sl, "character": sc},
        "end": {"line": el, "character": ec},
    }


def _call(provider, uri, text, sl=0, sc=0, el=0, ec=0, diagnostics=None):
    """Call get_code_actions with the correct 4-arg signature."""
    return provider.get_code_actions(
        uri,
        text,
        _range(sl, sc, el, ec),
        diagnostics or [],
    )

def _params(uri, text, sl=0, sc=0, el=0, ec=0, diagnostics=None):
    """Return the LSP range dict (passed as range_params arg)."""
    return _range(sl, sc, el, ec)

def _find_action(actions, title_fragment):
    """Return the first action whose title contains title_fragment."""
    for a in actions:
        if title_fragment.lower() in a.get("title", "").lower():
            return a
    return None


# ---------------------------------------------------------------------------
# Organize Imports
# ---------------------------------------------------------------------------


UNSORTED_IMPORTS = """\
import math
import string
import collections
import io

set x to 1
"""


SORTED_IMPORTS = """\
import collections
import io
import math
import string

set x to 1
"""


def test_organize_imports_returns_action(provider):
    params = _params("file:///t.nxl", UNSORTED_IMPORTS)
    actions = provider.get_code_actions("file:///t.nxl", UNSORTED_IMPORTS, params, [])
    action = _find_action(actions, "organize import")
    assert action is not None


def test_organize_imports_sorts_alphabetically(provider):
    params = _params("file:///t.nxl", UNSORTED_IMPORTS)
    actions = provider.get_code_actions("file:///t.nxl", UNSORTED_IMPORTS, params, [])
    action = _find_action(actions, "organize import")
    assert action is not None
    edits = action["edit"]["changes"]["file:///t.nxl"]
    # The new text for the import block should list them in alpha order
    new_text = edits[0]["newText"]
    lines = [l for l in new_text.splitlines() if l.startswith("import ")]
    assert lines == sorted(lines)


def test_organize_imports_no_action_when_already_sorted(provider):
    src = "import collections\nimport io\nimport math\nimport string\n\nset x to 1\n"
    params = _params("file:///t.nxl", src)
    actions = provider.get_code_actions("file:///t.nxl", src, params, [])
    action = _find_action(actions, "organize import")
    assert action is None


def test_organize_imports_no_action_when_no_imports(provider):
    src = "set x to 1\nset y to 2\n"
    params = _params("file:///t.nxl", src)
    actions = provider.get_code_actions("file:///t.nxl", src, params, [])
    action = _find_action(actions, "organize import")
    assert action is None


def test_organize_imports_multiple_blocks(provider):
    # Two separate import blocks should both be sorted
    src = (
        "import z_module\nimport a_module\n\n"
        "# some code\nset x to 1\n\n"
        "import zz_lib\nimport aa_lib\n"
    )
    params = _params("file:///t.nxl", src)
    actions = provider.get_code_actions("file:///t.nxl", src, params, [])
    action = _find_action(actions, "organize import")
    assert action is not None


# ---------------------------------------------------------------------------
# Toggle Comment
# ---------------------------------------------------------------------------


UNCOMMENTED_LINES = """\
set x to 1
set y to 2
set z to 3
"""


COMMENTED_LINES = """\
# set x to 1
# set y to 2
# set z to 3
"""


def test_toggle_comment_adds_hash_to_uncommented(provider):
    params = _params("file:///t.nxl", UNCOMMENTED_LINES, sl=0, sc=0, el=2, ec=10)
    actions = provider.get_code_actions("file:///t.nxl", UNCOMMENTED_LINES, params, [])
    action = _find_action(actions, "comment")
    assert action is not None


def test_toggle_comment_produces_hash_prefix(provider):
    params = _params("file:///t.nxl", UNCOMMENTED_LINES, sl=0, sc=0, el=2, ec=10)
    actions = provider.get_code_actions("file:///t.nxl", UNCOMMENTED_LINES, params, [])
    action = _find_action(actions, "comment")
    assert action is not None
    edits = action["edit"]["changes"]["file:///t.nxl"]
    assert any("#" in e["newText"] for e in edits)


def test_toggle_comment_removes_hash_when_all_commented(provider):
    params = _params("file:///t.nxl", COMMENTED_LINES, sl=0, sc=0, el=2, ec=14)
    actions = provider.get_code_actions("file:///t.nxl", COMMENTED_LINES, params, [])
    action = _find_action(actions, "uncomment")
    assert action is not None


def test_toggle_comment_produces_uncommented_text(provider):
    params = _params("file:///t.nxl", COMMENTED_LINES, sl=0, sc=0, el=2, ec=14)
    actions = provider.get_code_actions("file:///t.nxl", COMMENTED_LINES, params, [])
    action = _find_action(actions, "uncomment")
    assert action is not None
    edits = action["edit"]["changes"]["file:///t.nxl"]
    for edit in edits:
        assert not edit["newText"].lstrip().startswith("#")


def test_toggle_comment_skips_blank_lines(provider):
    src = "set x to 1\n\nset y to 2\n"
    params = _params("file:///t.nxl", src, sl=0, sc=0, el=2, ec=10)
    actions = provider.get_code_actions("file:///t.nxl", src, params, [])
    action = _find_action(actions, "comment")
    if action is not None:
        edits = action["edit"]["changes"]["file:///t.nxl"]
        # Blank line should not be included in edits
        for edit in edits:
            assert edit["newText"].strip() != ""


def test_toggle_comment_single_line(provider):
    params = _params("file:///t.nxl", "set x to 1\n", sl=0, sc=0, el=0, ec=10)
    actions = provider.get_code_actions("file:///t.nxl", "set x to 1\n", params, [])
    action = _find_action(actions, "comment")
    assert action is not None


# ---------------------------------------------------------------------------
# Extract Variable
# ---------------------------------------------------------------------------


EXTRACT_SRC = "set result to compute x plus y plus z\n"


def test_extract_variable_returns_action_on_selection(provider):
    # Select a portion of line 0
    params = _params("file:///t.nxl", EXTRACT_SRC, sl=0, sc=18, el=0, ec=37)
    actions = provider.get_code_actions("file:///t.nxl", EXTRACT_SRC, params, [])
    action = _find_action(actions, "to variable")
    assert action is not None


def test_extract_variable_creates_set_line(provider):
    params = _params("file:///t.nxl", EXTRACT_SRC, sl=0, sc=18, el=0, ec=37)
    actions = provider.get_code_actions("file:///t.nxl", EXTRACT_SRC, params, [])
    action = _find_action(actions, "to variable")
    assert action is not None
    edits = action["edit"]["changes"]["file:///t.nxl"]
    new_texts = [e["newText"] for e in edits]
    # One of the edits should have "set newValue to"
    assert any("set newValue to" in t for t in new_texts)


def test_extract_variable_replaces_selection(provider):
    params = _params("file:///t.nxl", EXTRACT_SRC, sl=0, sc=18, el=0, ec=37)
    actions = provider.get_code_actions("file:///t.nxl", EXTRACT_SRC, params, [])
    action = _find_action(actions, "to variable")
    assert action is not None
    edits = action["edit"]["changes"]["file:///t.nxl"]
    assert any("newValue" in e.get("newText", "") and "set newValue to" not in e.get("newText", "") for e in edits)


def test_extract_variable_no_action_when_multiline_selection(provider):
    src = "set x to 1\nset y to 2\n"
    # Select across two lines - _extract_variable returns None for multiline
    params = _params("file:///t.nxl", src, sl=0, sc=5, el=1, ec=10)
    actions = provider.get_code_actions("file:///t.nxl", src, params, [])
    action = _find_action(actions, "to variable")
    assert action is None


def test_extract_variable_no_action_when_no_selection(provider):
    params = _params("file:///t.nxl", EXTRACT_SRC, sl=0, sc=0, el=0, ec=0)
    actions = provider.get_code_actions("file:///t.nxl", EXTRACT_SRC, params, [])
    action = _find_action(actions, "to variable")
    assert action is None


# ---------------------------------------------------------------------------
# Inline Variable
# ---------------------------------------------------------------------------


INLINE_SRC = """\
set multiplier to 3
set result to base times multiplier
set other to multiplier plus 1
"""


def test_inline_variable_returns_action(provider):
    # Cursor on 'multiplier' in 'set multiplier to 3'
    params = _params("file:///t.nxl", INLINE_SRC, sl=0, sc=4, el=0, ec=14)
    actions = provider.get_code_actions("file:///t.nxl", INLINE_SRC, params, [])
    action = _find_action(actions, "inline")
    assert action is not None


def test_inline_variable_produces_edits(provider):
    params = _params("file:///t.nxl", INLINE_SRC, sl=0, sc=4, el=0, ec=14)
    actions = provider.get_code_actions("file:///t.nxl", INLINE_SRC, params, [])
    action = _find_action(actions, "inline")
    assert action is not None
    edits = action["edit"]["changes"]["file:///t.nxl"]
    assert len(edits) >= 1


def test_inline_variable_removes_declaration(provider):
    params = _params("file:///t.nxl", INLINE_SRC, sl=0, sc=4, el=0, ec=14)
    actions = provider.get_code_actions("file:///t.nxl", INLINE_SRC, params, [])
    action = _find_action(actions, "inline")
    assert action is not None
    edits = action["edit"]["changes"]["file:///t.nxl"]
    # One edit should produce empty text for the declaration line
    assert any(e.get("newText", "x") == "" for e in edits)


def test_inline_variable_replaces_uses_with_value(provider):
    params = _params("file:///t.nxl", INLINE_SRC, sl=0, sc=4, el=0, ec=14)
    actions = provider.get_code_actions("file:///t.nxl", INLINE_SRC, params, [])
    action = _find_action(actions, "inline")
    assert action is not None
    edits = action["edit"]["changes"]["file:///t.nxl"]
    # The implementation replaces whole lines; verify 'multiplier' is gone
    # and '3' appears in the replacement lines for lines 1 and 2
    replaced = [
        e for e in edits
        if e.get("newText", "") and "multiplier" not in e["newText"] and "3" in e["newText"]
    ]
    assert len(replaced) == 2


MULTI_ASSIGN_SRC = """\
set count to 0
set count to 1
set result to count plus 5
"""


def test_inline_variable_no_action_when_multiple_assignments(provider):
    # 'count' has 2 assignment statements → ambiguous, no inline
    params = _params("file:///t.nxl", MULTI_ASSIGN_SRC, sl=0, sc=4, el=0, ec=9)
    actions = provider.get_code_actions("file:///t.nxl", MULTI_ASSIGN_SRC, params, [])
    action = _find_action(actions, "inline")
    assert action is None


def test_inline_variable_no_action_when_cursor_not_on_set(provider):
    src = "print text multiplier\n"
    # Cursor is on 'print text ...' which is not a set statement
    params = _params("file:///t.nxl", src, sl=0, sc=11, el=0, ec=21)
    actions = provider.get_code_actions("file:///t.nxl", src, params, [])
    action = _find_action(actions, "inline")
    assert action is None
