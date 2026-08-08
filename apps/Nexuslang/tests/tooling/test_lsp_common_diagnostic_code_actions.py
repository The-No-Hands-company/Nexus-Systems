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


def test_ownership_structured_fix_uses_specialized_move_fixes_over_generic_drop_insert():
    provider = _provider()
    uri = "file:///ownership_drop_fix.nxl"
    code = "set x to 10\nset b to borrow x\nset y to move x\n"
    diagnostic = {
        "range": {"start": {"line": 2, "character": 9}, "end": {"line": 2, "character": 10}},
        "severity": 1,
        "message": "Ownership error: Cannot move 'x' while borrowed",
        "source": "nlpl",
        "code": "E201",
        "data": {
            "fixes": [
                "Drop active borrows before move or assignment",
            ],
            "ownership": {
                "variable": "x",
                "kind": "borrow",
                "line": 2,
                "operation": "move",
            },
        },
    }

    actions = provider.get_code_actions(uri, code, _range(2), [diagnostic])
    narrow_action = _find_action(actions, "Narrow borrow scope of 'x' before move")
    assert narrow_action is not None
    generic_drop_action = _find_action(actions, "Drop active borrow of 'x'")
    assert generic_drop_action is None


def test_ownership_structured_fix_falls_back_to_generic_drop_insert_when_no_specialized_move_fix():
    provider = _provider()
    uri = "file:///ownership_drop_fallback_fix.nxl"
    code = "set y to move x\n"
    diagnostic = {
        "range": {"start": {"line": 0, "character": 9}, "end": {"line": 0, "character": 10}},
        "severity": 1,
        "message": "Ownership error: Cannot move 'x' while borrowed",
        "source": "nlpl",
        "code": "E201",
        "data": {
            "fixes": [
                "Drop active borrows before move or assignment",
            ],
            "ownership": {
                "variable": "x",
                "kind": "borrow",
                "line": 0,
                "operation": "move",
            },
        },
    }

    actions = provider.get_code_actions(uri, code, _range(0), [diagnostic])
    action = _find_action(actions, "Drop active borrow of 'x'")
    assert action is not None
    edit = action["edit"]["changes"][uri][0]
    assert edit["newText"] == "drop borrow x\n"
    assert edit["range"]["start"]["line"] == 0


def test_ownership_structured_fix_converts_mutable_borrow_to_immutable():
    provider = _provider()
    uri = "file:///ownership_mutable_fix.nxl"
    code = "set x to 0\nset m to borrow mutable x\nset n to borrow x\n"
    diagnostic = {
        "range": {"start": {"line": 1, "character": 10}, "end": {"line": 1, "character": 24}},
        "severity": 1,
        "message": "Ownership error: Cannot take mutable borrow of 'x' while immutable borrow exists",
        "source": "nlpl",
        "code": "E201",
        "data": {
            "fixes": [
                "Avoid mutable borrow while immutable borrows are active",
            ],
            "ownership": {
                "variable": "x",
                "kind": "borrow",
                "line": 1,
                "operation": "borrow_mutable",
            },
        },
    }

    actions = provider.get_code_actions(uri, code, _range(1), [diagnostic])
    action = _find_action(actions, "Convert mutable borrow of 'x' to immutable borrow")
    assert action is not None
    edit = action["edit"]["changes"][uri][0]
    assert edit["newText"] == "set m to borrow x"


def test_ownership_structured_fix_inserts_drop_mutable_borrow_action():
    provider = _provider()
    uri = "file:///ownership_drop_mutable_fix.nxl"
    code = "set x to 0\nset m to borrow mutable x\nset n to borrow x\n"
    diagnostic = {
        "range": {"start": {"line": 2, "character": 10}, "end": {"line": 2, "character": 11}},
        "severity": 1,
        "message": "Ownership error: Cannot take mutable borrow of 'x' while immutable borrow exists",
        "source": "nlpl",
        "code": "E201",
        "data": {
            "fixes": [
                "Drop active borrows before move or assignment",
            ],
            "ownership": {
                "variable": "x",
                "kind": "borrow",
                "line": 2,
                "operation": "borrow_mutable",
            },
        },
    }

    actions = provider.get_code_actions(uri, code, _range(2), [diagnostic])
    action = _find_action(actions, "Drop mutable borrow of 'x' before this operation")
    assert action is not None
    edit = action["edit"]["changes"][uri][0]
    assert edit["newText"] == "drop borrow mutable x\n"
    assert edit["range"]["start"]["line"] == 2


def test_ownership_structured_fix_reorders_move_after_nearby_drop_borrow():
    provider = _provider()
    uri = "file:///ownership_reorder_move_after_drop.nxl"
    code = "set x to 10\nset b to borrow x\nset y to move x\ndrop borrow x\n"
    diagnostic = {
        "range": {"start": {"line": 2, "character": 9}, "end": {"line": 2, "character": 10}},
        "severity": 1,
        "message": "Ownership error: Cannot move 'x' while borrowed",
        "source": "nlpl",
        "code": "E201",
        "data": {
            "fixes": [
                "Drop active borrows before move or assignment",
            ],
            "ownership": {
                "variable": "x",
                "kind": "borrow",
                "line": 2,
                "operation": "move",
            },
        },
    }

    actions = provider.get_code_actions(uri, code, _range(2), [diagnostic])
    action = _find_action(actions, "Reorder move of 'x' after drop borrow")
    assert action is not None
    edit = action["edit"]["changes"][uri][0]
    assert edit["newText"] == "drop borrow x\nset y to move x\n"
    assert edit["range"]["start"]["line"] == 2


def test_ownership_specialized_move_fixes_have_deterministic_priority_order():
    provider = _provider()
    uri = "file:///ownership_move_fix_priority_order.nxl"
    code = "set x to 10\nset b to borrow x\nset y to move x\ndrop borrow x\n"
    diagnostic = {
        "range": {"start": {"line": 2, "character": 9}, "end": {"line": 2, "character": 10}},
        "severity": 1,
        "message": "Ownership error: Cannot move 'x' while borrowed",
        "source": "nlpl",
        "code": "E201",
        "data": {
            "fixes": [
                "Drop active borrows before move or assignment",
            ],
            "ownership": {
                "variable": "x",
                "kind": "borrow",
                "line": 2,
                "operation": "move",
            },
        },
    }

    actions = provider.get_code_actions(uri, code, _range(2), [diagnostic])
    specialized_actions = [
        action
        for action in actions
        if "Reorder move of 'x' after drop borrow" in action.get("title", "")
        or "Narrow borrow scope of 'x' before move" in action.get("title", "")
    ]

    specialized_titles = [action.get("title", "") for action in specialized_actions]
    assert specialized_titles == [
        "Reorder move of 'x' after drop borrow (safe reorder)",
        "Narrow borrow scope of 'x' before move",
    ]
    assert specialized_actions[0].get("isPreferred") is True
    assert specialized_actions[1].get("isPreferred") in (None, False)


def test_ownership_reorder_move_action_suppressed_for_inline_drop_comment():
    provider = _provider()
    uri = "file:///ownership_reorder_move_guard_comment.nxl"
    code = "set x to 10\nset b to borrow x\nset y to move x\ndrop borrow x # keep this order\n"
    diagnostic = {
        "range": {"start": {"line": 2, "character": 9}, "end": {"line": 2, "character": 10}},
        "severity": 1,
        "message": "Ownership error: Cannot move 'x' while borrowed",
        "source": "nlpl",
        "code": "E201",
        "data": {
            "fixes": [
                "Drop active borrows before move or assignment",
            ],
            "ownership": {
                "variable": "x",
                "kind": "borrow",
                "line": 2,
                "operation": "move",
            },
        },
    }

    actions = provider.get_code_actions(uri, code, _range(2), [diagnostic])
    action = _find_action(actions, "Reorder move of 'x' after drop borrow")
    assert action is None


def test_ownership_reorder_move_action_allows_drop_borrow_mutable_variant():
    provider = _provider()
    uri = "file:///ownership_reorder_move_drop_mutable.nxl"
    code = "set x to 10\nset b to borrow x\nset y to move x\ndrop borrow mutable x\n"
    diagnostic = {
        "range": {"start": {"line": 2, "character": 9}, "end": {"line": 2, "character": 10}},
        "severity": 1,
        "message": "Ownership error: Cannot move 'x' while borrowed",
        "source": "nlpl",
        "code": "E201",
        "data": {
            "fixes": [
                "Drop active borrows before move or assignment",
            ],
            "ownership": {
                "variable": "x",
                "kind": "borrow",
                "line": 2,
                "operation": "move",
            },
        },
    }

    actions = provider.get_code_actions(uri, code, _range(2), [diagnostic])
    action = _find_action(actions, "Reorder move of 'x' after drop borrow")
    assert action is not None
    edit = action["edit"]["changes"][uri][0]
    assert edit["newText"] == "drop borrow mutable x\nset y to move x\n"


def test_ownership_reorder_move_action_suppressed_for_indentation_mismatch():
    provider = _provider()
    uri = "file:///ownership_reorder_move_indent_guard.nxl"
    code = "if true\n    set b to borrow x\n    set y to move x\ndrop borrow x\nend\n"
    diagnostic = {
        "range": {"start": {"line": 2, "character": 13}, "end": {"line": 2, "character": 14}},
        "severity": 1,
        "message": "Ownership error: Cannot move 'x' while borrowed",
        "source": "nlpl",
        "code": "E201",
        "data": {
            "fixes": [
                "Drop active borrows before move or assignment",
            ],
            "ownership": {
                "variable": "x",
                "kind": "borrow",
                "line": 2,
                "operation": "move",
            },
        },
    }

    actions = provider.get_code_actions(uri, code, _range(2), [diagnostic])
    action = _find_action(actions, "Reorder move of 'x' after drop borrow")
    assert action is None


def test_ownership_reorder_move_action_suppressed_for_attached_directive_comment():
    provider = _provider()
    uri = "file:///ownership_reorder_move_directive_guard.nxl"
    code = "set x to 10\nset b to borrow x\n# noqa: keep this tracked\nset y to move x\ndrop borrow x\n"
    diagnostic = {
        "range": {"start": {"line": 3, "character": 9}, "end": {"line": 3, "character": 10}},
        "severity": 1,
        "message": "Ownership error: Cannot move 'x' while borrowed",
        "source": "nlpl",
        "code": "E201",
        "data": {
            "fixes": [
                "Drop active borrows before move or assignment",
            ],
            "ownership": {
                "variable": "x",
                "kind": "borrow",
                "line": 3,
                "operation": "move",
            },
        },
    }

    actions = provider.get_code_actions(uri, code, _range(3), [diagnostic])
    action = _find_action(actions, "Reorder move of 'x' after drop borrow")
    assert action is None


def test_ownership_move_conflict_offers_narrow_borrow_scope_action():
    provider = _provider()
    uri = "file:///ownership_narrow_scope_action.nxl"
    code = "set x to 10\nset b to borrow x\nprint text b\nset y to move x\n"
    diagnostic = {
        "range": {"start": {"line": 3, "character": 9}, "end": {"line": 3, "character": 10}},
        "severity": 1,
        "message": "Ownership error: Cannot move 'x' while borrowed",
        "source": "nlpl",
        "code": "E201",
        "data": {
            "fixes": [
                "Drop active borrows before move or assignment",
            ],
            "ownership": {
                "variable": "x",
                "kind": "borrow",
                "line": 3,
                "operation": "move",
            },
        },
    }

    actions = provider.get_code_actions(uri, code, _range(3), [diagnostic])
    action = _find_action(actions, "Narrow borrow scope of 'x' before move")
    assert action is not None
    edit = action["edit"]["changes"][uri][0]
    assert edit["range"]["start"]["line"] == 1
    assert edit["range"]["end"]["line"] == 3
    assert edit["newText"] == (
        "if true\n"
        "    set b to borrow x\n"
        "    print text b\n"
        "    drop borrow x\n"
        "end\n"
    )


def test_ownership_narrow_scope_action_not_offered_when_alias_used_after_move():
    provider = _provider()
    uri = "file:///ownership_narrow_scope_alias_used_after.nxl"
    code = "set x to 10\nset b to borrow x\nprint text b\nset y to move x\nprint text b\n"
    diagnostic = {
        "range": {"start": {"line": 3, "character": 9}, "end": {"line": 3, "character": 10}},
        "severity": 1,
        "message": "Ownership error: Cannot move 'x' while borrowed",
        "source": "nlpl",
        "code": "E201",
        "data": {
            "fixes": [
                "Drop active borrows before move or assignment",
            ],
            "ownership": {
                "variable": "x",
                "kind": "borrow",
                "line": 3,
                "operation": "move",
            },
        },
    }

    actions = provider.get_code_actions(uri, code, _range(3), [diagnostic])
    action = _find_action(actions, "Narrow borrow scope of 'x' before move")
    assert action is None


def test_ownership_narrow_scope_action_not_offered_when_drop_already_present():
    provider = _provider()
    uri = "file:///ownership_narrow_scope_drop_exists.nxl"
    code = "set x to 10\nset b to borrow x\ndrop borrow x\nset y to move x\n"
    diagnostic = {
        "range": {"start": {"line": 3, "character": 9}, "end": {"line": 3, "character": 10}},
        "severity": 1,
        "message": "Ownership error: Cannot move 'x' while borrowed",
        "source": "nlpl",
        "code": "E201",
        "data": {
            "fixes": [
                "Drop active borrows before move or assignment",
            ],
            "ownership": {
                "variable": "x",
                "kind": "borrow",
                "line": 3,
                "operation": "move",
            },
        },
    }

    actions = provider.get_code_actions(uri, code, _range(3), [diagnostic])
    action = _find_action(actions, "Narrow borrow scope of 'x' before move")
    assert action is None


def test_ownership_narrow_scope_action_not_offered_with_if_delimiter_in_region():
    provider = _provider()
    uri = "file:///ownership_narrow_scope_if_delimiter.nxl"
    code = "set x to 10\nset b to borrow x\nif ready\nprint text b\nset y to move x\n"
    diagnostic = {
        "range": {"start": {"line": 4, "character": 9}, "end": {"line": 4, "character": 10}},
        "severity": 1,
        "message": "Ownership error: Cannot move 'x' while borrowed",
        "source": "nlpl",
        "code": "E201",
        "data": {
            "fixes": [
                "Drop active borrows before move or assignment",
            ],
            "ownership": {
                "variable": "x",
                "kind": "borrow",
                "line": 4,
                "operation": "move",
            },
        },
    }

    actions = provider.get_code_actions(uri, code, _range(4), [diagnostic])
    action = _find_action(actions, "Narrow borrow scope of 'x' before move")
    assert action is None


def test_ownership_narrow_scope_action_not_offered_with_end_delimiter_in_region():
    provider = _provider()
    uri = "file:///ownership_narrow_scope_end_delimiter.nxl"
    code = "set x to 10\nset b to borrow x\nprint text b\nend\nset y to move x\n"
    diagnostic = {
        "range": {"start": {"line": 4, "character": 9}, "end": {"line": 4, "character": 10}},
        "severity": 1,
        "message": "Ownership error: Cannot move 'x' while borrowed",
        "source": "nlpl",
        "code": "E201",
        "data": {
            "fixes": [
                "Drop active borrows before move or assignment",
            ],
            "ownership": {
                "variable": "x",
                "kind": "borrow",
                "line": 4,
                "operation": "move",
            },
        },
    }

    actions = provider.get_code_actions(uri, code, _range(4), [diagnostic])
    action = _find_action(actions, "Narrow borrow scope of 'x' before move")
    assert action is None


def test_ownership_narrow_scope_action_not_offered_when_owned_var_written_in_region():
    provider = _provider()
    uri = "file:///ownership_narrow_scope_owned_var_write.nxl"
    code = "set x to 10\nset b to borrow x\nset x to 20\nprint text b\nset y to move x\n"
    diagnostic = {
        "range": {"start": {"line": 4, "character": 9}, "end": {"line": 4, "character": 10}},
        "severity": 1,
        "message": "Ownership error: Cannot move 'x' while borrowed",
        "source": "nlpl",
        "code": "E201",
        "data": {
            "fixes": [
                "Drop active borrows before move or assignment",
            ],
            "ownership": {
                "variable": "x",
                "kind": "borrow",
                "line": 4,
                "operation": "move",
            },
        },
    }

    actions = provider.get_code_actions(uri, code, _range(4), [diagnostic])
    action = _find_action(actions, "Narrow borrow scope of 'x' before move")
    assert action is None


def test_ownership_narrow_scope_action_not_offered_when_owned_var_index_written_in_region():
    provider = _provider()
    uri = "file:///ownership_narrow_scope_owned_var_index_write.nxl"
    code = "set x to [1, 2]\nset b to borrow x\nset x[0] to 20\nprint text b\nset y to move x\n"
    diagnostic = {
        "range": {"start": {"line": 4, "character": 9}, "end": {"line": 4, "character": 10}},
        "severity": 1,
        "message": "Ownership error: Cannot move 'x' while borrowed",
        "source": "nlpl",
        "code": "E201",
        "data": {
            "fixes": [
                "Drop active borrows before move or assignment",
            ],
            "ownership": {
                "variable": "x",
                "kind": "borrow",
                "line": 4,
                "operation": "move",
            },
        },
    }

    actions = provider.get_code_actions(uri, code, _range(4), [diagnostic])
    action = _find_action(actions, "Narrow borrow scope of 'x' before move")
    assert action is None


def test_ownership_narrow_scope_action_not_offered_when_owned_var_property_written_in_region():
    provider = _provider()
    uri = "file:///ownership_narrow_scope_owned_var_property_write.nxl"
    code = "set x to make Point\nset b to borrow x\nset x.value to 20\nprint text b\nset y to move x\n"
    diagnostic = {
        "range": {"start": {"line": 4, "character": 9}, "end": {"line": 4, "character": 10}},
        "severity": 1,
        "message": "Ownership error: Cannot move 'x' while borrowed",
        "source": "nlpl",
        "code": "E201",
        "data": {
            "fixes": [
                "Drop active borrows before move or assignment",
            ],
            "ownership": {
                "variable": "x",
                "kind": "borrow",
                "line": 4,
                "operation": "move",
            },
        },
    }

    actions = provider.get_code_actions(uri, code, _range(4), [diagnostic])
    action = _find_action(actions, "Narrow borrow scope of 'x' before move")
    assert action is None


def test_ownership_narrow_scope_action_not_offered_when_borrow_alias_reassigned_in_region():
    provider = _provider()
    uri = "file:///ownership_narrow_scope_alias_reassign.nxl"
    code = "set x to 10\nset b to borrow x\nset b to 0\nset y to move x\n"
    diagnostic = {
        "range": {"start": {"line": 3, "character": 9}, "end": {"line": 3, "character": 10}},
        "severity": 1,
        "message": "Ownership error: Cannot move 'x' while borrowed",
        "source": "nlpl",
        "code": "E201",
        "data": {
            "fixes": [
                "Drop active borrows before move or assignment",
            ],
            "ownership": {
                "variable": "x",
                "kind": "borrow",
                "line": 3,
                "operation": "move",
            },
        },
    }

    actions = provider.get_code_actions(uri, code, _range(3), [diagnostic])
    action = _find_action(actions, "Narrow borrow scope of 'x' before move")
    assert action is None


def test_ownership_narrow_scope_action_not_offered_when_second_borrow_of_var_exists_in_region():
    provider = _provider()
    uri = "file:///ownership_narrow_scope_second_borrow.nxl"
    code = "set x to 10\nset b to borrow x\nset c to borrow x\nset y to move x\n"
    diagnostic = {
        "range": {"start": {"line": 3, "character": 9}, "end": {"line": 3, "character": 10}},
        "severity": 1,
        "message": "Ownership error: Cannot move 'x' while borrowed",
        "source": "nlpl",
        "code": "E201",
        "data": {
            "fixes": [
                "Drop active borrows before move or assignment",
            ],
            "ownership": {
                "variable": "x",
                "kind": "borrow",
                "line": 3,
                "operation": "move",
            },
        },
    }

    actions = provider.get_code_actions(uri, code, _range(3), [diagnostic])
    action = _find_action(actions, "Narrow borrow scope of 'x' before move")
    assert action is None
