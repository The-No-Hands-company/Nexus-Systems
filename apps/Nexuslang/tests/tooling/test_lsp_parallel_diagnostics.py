"""Tooling diagnostics coverage for parallel-for unsafe capture patterns."""

from unittest.mock import MagicMock

from nexuslang.lsp.diagnostics import DiagnosticsProvider


def _provider() -> DiagnosticsProvider:
    return DiagnosticsProvider(server=MagicMock())


def _parallel_capture_diagnostics(code: str):
    diagnostics = _provider().get_diagnostics("file:///parallel_capture.nxl", code, check_imports=False)
    return [d for d in diagnostics if "unsafe capture in parallel region" in d.get("message", "").lower()]


def test_parallel_for_reports_outer_variable_write_capture():
    code = """
set values to [1, 2, 3]
set total to 0
parallel for each item in values
    set total to total plus item
end
"""

    captures = _parallel_capture_diagnostics(code)

    assert captures, "Expected unsafe capture diagnostic for outer accumulator write"
    assert any("total" in d.get("message", "") for d in captures)
    assert all(d.get("code") == "E201" for d in captures)


def test_parallel_for_reports_outer_object_member_mutation():
    code = """
set values to [1, 2, 3]
set state to create dictionary
parallel for each item in values
    set state.count to item
end
"""

    captures = _parallel_capture_diagnostics(code)

    assert captures, "Expected unsafe capture diagnostic for outer object mutation"
    assert any("state" in d.get("message", "") for d in captures)


def test_parallel_for_reports_outer_collection_index_mutation():
    code = """
set values to [1, 2, 3]
set shared to [0, 0, 0]
parallel for each item in values
    set shared[item] to item
end
"""

    captures = _parallel_capture_diagnostics(code)

    assert captures, "Expected unsafe capture diagnostic for outer collection mutation"
    assert any("shared" in d.get("message", "") for d in captures)


def test_parallel_for_allows_loop_local_assignments_without_capture_warning():
    code = """
set values to [1, 2, 3]
parallel for each item in values
    set local_total to item
end
"""

    captures = _parallel_capture_diagnostics(code)

    assert not captures, "Did not expect capture warning for loop-local variable declaration"


def test_parallel_for_does_not_flag_loop_variable_reassignment_as_capture():
    code = """
set values to [1, 2, 3]
parallel for each item in values
    set item to item plus 1
end
"""

    captures = _parallel_capture_diagnostics(code)

    assert not captures, "Did not expect capture warning for loop variable reassignment"
