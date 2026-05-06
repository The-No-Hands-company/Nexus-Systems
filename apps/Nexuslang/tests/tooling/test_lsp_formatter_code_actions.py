"""Tests for formatter quick-fix integration in LSP code actions."""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent.parent.parent / "src"))

from nexuslang.lsp.code_actions import CodeActionsProvider


class MockServer:
    def __init__(self):
        self.documents = {}
        self.workspace_index = None


def _provider() -> CodeActionsProvider:
    return CodeActionsProvider(MockServer())


def _range():
    return {
        "start": {"line": 0, "character": 0},
        "end": {"line": 0, "character": 0},
    }


def _find_action(actions, title_fragment: str):
    for action in actions:
        if title_fragment.lower() in action.get("title", "").lower():
            return action
    return None


def test_formatter_quick_fix_is_offered_for_unformatted_code():
    provider = _provider()
    uri = "file:///format_action.nxl"
    code = "set   value    to 1\n"

    actions = provider.get_code_actions(uri, code, _range(), [])

    format_action = _find_action(actions, "format document")
    assert format_action is not None
    assert format_action.get("kind") == "quickfix"

    edits = format_action["edit"]["changes"][uri]
    assert len(edits) == 1
    assert "set value to 1" in edits[0]["newText"]


def test_formatter_quick_fix_not_offered_for_already_formatted_code():
    provider = _provider()
    uri = "file:///format_clean.nxl"
    code = "set value to 1\n"

    actions = provider.get_code_actions(uri, code, _range(), [])

    format_action = _find_action(actions, "format document")
    assert format_action is None
