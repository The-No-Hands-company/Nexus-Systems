"""Coverage for common parser/type diagnostic quick-fix code actions."""

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


def _range(line: int = 0):
    return {
        "start": {"line": line, "character": 0},
        "end": {"line": line, "character": 0},
    }


def _find_action(actions, title_fragment: str):
    for action in actions:
        if title_fragment.lower() in action.get("title", "").lower():
            return action
    return None


def test_parser_quick_fix_adds_missing_end():
    provider = _provider()
    uri = "file:///missing_end.nxl"
    code = "if true\n    print text \"ok\"\n"
    diagnostic = {
        "range": {"start": {"line": 1, "character": 0}, "end": {"line": 2, "character": 0}},
        "severity": 1,
        "message": "Syntax Error: Expected 'end' before EOF",
        "source": "nlpl",
        "code": "E001",
    }

    actions = provider.get_code_actions(uri, code, _range(1), [diagnostic])
    action = _find_action(actions, "Add missing end")
    assert action is not None
    edit = action["edit"]["changes"][uri][0]
    assert edit["newText"] == "end\n"


def test_parser_quick_fix_adds_closing_paren():
    provider = _provider()
    uri = "file:///missing_paren.nxl"
    code = "print text (value\n"
    diagnostic = {
        "range": {"start": {"line": 0, "character": 0}, "end": {"line": 0, "character": 17}},
        "severity": 1,
        "message": "Syntax Error: expected ')'",
        "source": "nlpl",
        "code": "E001",
    }

    actions = provider.get_code_actions(uri, code, _range(0), [diagnostic])
    action = _find_action(actions, "Add closing )")
    assert action is not None
    edit = action["edit"]["changes"][uri][0]
    assert edit["newText"] == ")"


def test_type_quick_fix_adds_annotation_without_structured_fixes():
    provider = _provider()
    uri = "file:///type_annot.nxl"
    code = "set count to 1\n"
    diagnostic = {
        "range": {"start": {"line": 0, "character": 0}, "end": {"line": 0, "character": 10}},
        "severity": 1,
        "message": "Type error: expected 'Integer', got 'String'",
        "source": "nlpl",
        "code": "E201",
    }

    actions = provider.get_code_actions(uri, code, _range(0), [diagnostic])
    action = _find_action(actions, "Add type annotation: Integer")
    assert action is not None
    edit = action["edit"]["changes"][uri][0]
    assert "as Integer" in edit["newText"]


def test_boolean_quick_fix_for_if_condition():
    provider = _provider()
    uri = "file:///bool_fix.nxl"
    code = "if count\n    print text \"ok\"\nend\n"
    diagnostic = {
        "range": {"start": {"line": 0, "character": 0}, "end": {"line": 0, "character": 8}},
        "severity": 1,
        "message": "Type error: if condition must be a boolean, got 'Integer'",
        "source": "nlpl",
        "code": "E201",
    }

    actions = provider.get_code_actions(uri, code, _range(0), [diagnostic])
    action = _find_action(actions, "explicit boolean check")
    assert action is not None
    edit = action["edit"]["changes"][uri][0]
    assert edit["newText"] == "if count is true"


def test_closed_channel_send_structured_fix_creates_recreate_action():
    provider = _provider()
    uri = "file:///closed_channel_send_action.nxl"
    code = "set ch to create channel\nclose ch\nsend 1 to ch\n"
    diagnostic = {
        "range": {"start": {"line": 2, "character": 10}, "end": {"line": 2, "character": 12}},
        "severity": 2,
        "message": "Potential send to closed channel 'ch'",
        "source": "nlpl",
        "code": "E201",
        "data": {
            "fixes": [
                "Recreate channel before send: set ch to create channel",
                "Move this send before close ch",
            ]
        },
    }

    actions = provider.get_code_actions(uri, code, _range(2), [diagnostic])
    action = _find_action(actions, "Recreate channel 'ch' before operation")
    assert action is not None
    edit = action["edit"]["changes"][uri][0]
    assert edit["newText"] == "set ch to create channel\n"
    assert edit["range"]["start"]["line"] == 2


def test_closed_channel_receive_structured_fix_creates_recreate_action():
    provider = _provider()
    uri = "file:///closed_channel_receive_action.nxl"
    code = "set ch to create channel\nclose ch\nset value to receive from ch\n"
    diagnostic = {
        "range": {"start": {"line": 2, "character": 26}, "end": {"line": 2, "character": 28}},
        "severity": 2,
        "message": "Potential receive from closed channel 'ch'",
        "source": "nlpl",
        "code": "E201",
        "data": {
            "fixes": [
                "Recreate channel before receive: set ch to create channel",
                "Move close ch after this receive",
            ]
        },
    }

    actions = provider.get_code_actions(uri, code, _range(2), [diagnostic])
    action = _find_action(actions, "Recreate channel 'ch' before operation")
    assert action is not None
    edit = action["edit"]["changes"][uri][0]
    assert edit["newText"] == "set ch to create channel\n"
    assert edit["range"]["start"]["line"] == 2


def test_closed_channel_send_structured_fix_creates_safe_move_close_action():
    provider = _provider()
    uri = "file:///closed_channel_send_move_action.nxl"
    code = "set ch to create channel\nclose ch\nsend 1 to ch\n"
    diagnostic = {
        "range": {"start": {"line": 2, "character": 10}, "end": {"line": 2, "character": 12}},
        "severity": 2,
        "message": "Potential send to closed channel 'ch'",
        "source": "nlpl",
        "code": "E201",
        "data": {
            "fixes": [
                "Move this send before close ch",
            ]
        },
    }

    actions = provider.get_code_actions(uri, code, _range(2), [diagnostic])
    action = _find_action(actions, "Move close 'ch' after this operation")
    assert action is not None
    edit = action["edit"]["changes"][uri][0]
    assert edit["newText"] == "send 1 to ch\nclose ch\n"
    assert edit["range"]["start"]["line"] == 1


def test_closed_channel_move_close_action_not_offered_across_block_boundary():
    provider = _provider()
    uri = "file:///closed_channel_send_move_unsafe_action.nxl"
    code = "set ch to create channel\nif ready\n    close ch\nend\nsend 1 to ch\n"
    diagnostic = {
        "range": {"start": {"line": 4, "character": 10}, "end": {"line": 4, "character": 12}},
        "severity": 2,
        "message": "Potential send to closed channel 'ch'",
        "source": "nlpl",
        "code": "E201",
        "data": {
            "fixes": [
                "Move this send before close ch",
            ]
        },
    }

    actions = provider.get_code_actions(uri, code, _range(4), [diagnostic])
    action = _find_action(actions, "Move close 'ch' after this operation")
    assert action is None


def test_closed_channel_move_close_action_suppressed_for_inline_close_comment():
    provider = _provider()
    uri = "file:///closed_channel_send_move_inline_comment.nxl"
    code = "set ch to create channel\nclose ch # keep close here\nsend 1 to ch\n"
    diagnostic = {
        "range": {"start": {"line": 2, "character": 10}, "end": {"line": 2, "character": 12}},
        "severity": 2,
        "message": "Potential send to closed channel 'ch'",
        "source": "nlpl",
        "code": "E201",
        "data": {
            "fixes": [
                "Move this send before close ch",
            ]
        },
    }

    actions = provider.get_code_actions(uri, code, _range(2), [diagnostic])
    action = _find_action(actions, "Move close 'ch' after this operation")
    assert action is None


def test_closed_channel_move_close_action_suppressed_for_attached_directive_comment():
    provider = _provider()
    uri = "file:///closed_channel_send_move_attached_comment.nxl"
    code = "set ch to create channel\n# nolint: keep-close-order\nclose ch\nsend 1 to ch\n"
    diagnostic = {
        "range": {"start": {"line": 3, "character": 10}, "end": {"line": 3, "character": 12}},
        "severity": 2,
        "message": "Potential send to closed channel 'ch'",
        "source": "nlpl",
        "code": "E201",
        "data": {
            "fixes": [
                "Move this send before close ch",
            ]
        },
    }

    actions = provider.get_code_actions(uri, code, _range(3), [diagnostic])
    action = _find_action(actions, "Move close 'ch' after this operation")
    assert action is None
