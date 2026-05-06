"""Tooling coverage for contract diagnostics and LSP quick-fix code actions."""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent.parent.parent / "src"))

from nexuslang.lsp.code_actions import CodeActionsProvider
from nexuslang.lsp.diagnostics import DiagnosticsProvider


class MockServer:
    def __init__(self):
        self.documents = {}
        self.workspace_index = None


def _provider() -> CodeActionsProvider:
    return CodeActionsProvider(MockServer())


def _diag_provider() -> DiagnosticsProvider:
    return DiagnosticsProvider(MockServer())


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


def test_contract_boolean_diagnostic_includes_fix_suggestions():
    code = "set n to 5\nrequire n"

    diagnostics = _diag_provider().get_diagnostics("file:///contract_boolean.nxl", code, check_imports=False)

    contract_errors = [
        d for d in diagnostics
        if "require condition must be a boolean" in d.get("message", "").lower()
    ]
    assert contract_errors, "Expected contract boolean diagnostic"

    fixes = contract_errors[0].get("data", {}).get("fixes", [])
    assert any("explicit boolean check" in f.lower() for f in fixes)
    assert any("contract failure message" in f.lower() for f in fixes)


def test_contract_code_action_adds_message_clause():
    provider = _provider()
    uri = "file:///contract_actions.nxl"
    code = "require n"
    diagnostic = {
        "range": {"start": {"line": 0, "character": 0}, "end": {"line": 0, "character": 7}},
        "severity": 1,
        "message": "Type error: Require condition must be a boolean, got 'Integer'",
        "source": "nlpl",
        "code": "E201",
        "data": {
            "fixes": ["Add contract failure message"],
        },
    }

    actions = provider.get_code_actions(uri, code, _range(0), [diagnostic])
    action = _find_action(actions, "Add contract failure message")
    assert action is not None
    edit = action["edit"]["changes"][uri][0]
    assert "message \"contract failed\"" in edit["newText"]


def test_contract_code_action_converts_condition_to_boolean_check():
    provider = _provider()
    uri = "file:///contract_actions_boolean.nxl"
    code = "require n"
    diagnostic = {
        "range": {"start": {"line": 0, "character": 0}, "end": {"line": 0, "character": 7}},
        "severity": 1,
        "message": "Type error: Require condition must be a boolean, got 'Integer'",
        "source": "nlpl",
        "code": "E201",
        "data": {
            "fixes": ["Convert contract condition to explicit boolean check"],
        },
    }

    actions = provider.get_code_actions(uri, code, _range(0), [diagnostic])
    action = _find_action(actions, "explicit boolean check")
    assert action is not None
    edit = action["edit"]["changes"][uri][0]
    assert edit["newText"] == "require n is true"


def test_contract_code_action_converts_message_expression_to_string_literal():
    provider = _provider()
    uri = "file:///contract_actions_message.nxl"
    code = "require n is greater than 0 message 42"
    diagnostic = {
        "range": {"start": {"line": 0, "character": 0}, "end": {"line": 0, "character": 35}},
        "severity": 1,
        "message": "Type error: Require message must be a string, got 'Integer'",
        "source": "nlpl",
        "code": "E201",
        "data": {
            "fixes": ["Use string literal for contract message"],
        },
    }

    actions = provider.get_code_actions(uri, code, _range(0), [diagnostic])
    action = _find_action(actions, "Convert contract message to string literal")
    assert action is not None
    edit = action["edit"]["changes"][uri][0]
    assert edit["newText"] == '"42"'
