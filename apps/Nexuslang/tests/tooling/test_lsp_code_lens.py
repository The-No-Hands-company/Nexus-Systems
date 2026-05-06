"""
Tests: Code Lens Provider
=========================

Validates CodeLensProvider against NexusLang source patterns.
"""

import sys
from pathlib import Path

# Make sure the src tree is importable
sys.path.insert(0, str(Path(__file__).parent.parent.parent / "src"))

import pytest
from nexuslang.lsp.code_lens import CodeLensProvider


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
    return CodeLensProvider(MockServer())


FUNCTION_DOC = """\
function greet with name as String returns String
    return "Hello, " plus name
end

function compute_sum with a as Integer and b as Integer returns Integer
    return a plus b
end

set result to greet with "Alice"
set total to greet with "Bob"
set value to compute_sum with 1 and 2
"""

CLASS_DOC = """\
class Rectangle
    property width as Float
    property height as Float
end

set r to new Rectangle
set area to r.width
"""

STRUCT_DOC = """\
struct Point
    x as Integer
    y as Integer
end

set p to new Point
set q to new Point
"""

TEST_DOC = """\
describe "MathSuite"
    it "adds numbers"
        require 1 plus 1 equals 2
    end
    it "subtracts numbers"
        require 5 minus 3 equals 2
    end
    test "multiplies numbers"
        require 2 times 3 equals 6
    end
end

test "standalone case"
    require true
end
"""

EMPTY_DOC = "# just a comment\n"


# ---------------------------------------------------------------------------
# Function reference lenses
# ---------------------------------------------------------------------------


def test_function_lens_count(provider):
    lenses = provider.get_code_lenses("file:///test.nxl", FUNCTION_DOC)
    func_lenses = [
        l for l in lenses if l["command"]["command"] == "nexuslang.findReferences"
    ]
    # Two functions defined
    assert len(func_lenses) == 2


def test_function_greet_has_two_references(provider):
    lenses = provider.get_code_lenses("file:///test.nxl", FUNCTION_DOC)
    greet_lens = next(
        (
            l
            for l in lenses
            if l["command"]["command"] == "nexuslang.findReferences"
            and "greet" in l["command"]["title"].lower()
            # Title is "N reference(s)", not function name — check by line
        ),
        None,
    )
    # Greet is defined on line 0, referenced on lines 8 and 9
    greet_lenses = [
        l for l in lenses if l["range"]["start"]["line"] == 0
    ]
    assert greet_lenses, "Should have a lens on the greet definition line"
    title = greet_lenses[0]["command"]["title"]
    # greet appears twice in calls ("Alice" and "Bob")
    assert "2" in title


def test_function_lens_range_is_on_definition_line(provider):
    lenses = provider.get_code_lenses("file:///test.nxl", FUNCTION_DOC)
    for lens in lenses:
        if lens["command"]["command"] == "nexuslang.findReferences":
            # Range start and end must be on the same line
            assert (
                lens["range"]["start"]["line"] == lens["range"]["end"]["line"]
            )


# ---------------------------------------------------------------------------
# Class / struct reference lenses
# ---------------------------------------------------------------------------


def test_class_lens_present(provider):
    lenses = provider.get_code_lenses("file:///test.nxl", CLASS_DOC)
    ref_lenses = [
        l for l in lenses if l["command"]["command"] == "nexuslang.findReferences"
    ]
    assert len(ref_lenses) >= 1


def test_struct_lens_present(provider):
    lenses = provider.get_code_lenses("file:///test.nxl", STRUCT_DOC)
    ref_lenses = [
        l for l in lenses if l["command"]["command"] == "nexuslang.findReferences"
    ]
    assert len(ref_lenses) >= 1


def test_struct_two_references(provider):
    lenses = provider.get_code_lenses("file:///test.nxl", STRUCT_DOC)
    # Point is referenced twice via 'new Point'
    point_lenses = [
        l
        for l in lenses
        if l["range"]["start"]["line"] == 0  # struct Point on line 0
    ]
    assert point_lenses
    assert "2" in point_lenses[0]["command"]["title"]


# ---------------------------------------------------------------------------
# Test block lenses
# ---------------------------------------------------------------------------


def test_describe_block_lens(provider):
    lenses = provider.get_code_lenses("file:///test.nxl", TEST_DOC)
    suite_lenses = [
        l for l in lenses if l["command"]["command"] == "nexuslang.runTestSuite"
    ]
    assert len(suite_lenses) == 1


def test_describe_block_counts_three_tests(provider):
    lenses = provider.get_code_lenses("file:///test.nxl", TEST_DOC)
    suite_lens = next(
        l for l in lenses if l["command"]["command"] == "nexuslang.runTestSuite"
    )
    assert "3" in suite_lens["command"]["title"]


def test_standalone_test_lens(provider):
    lenses = provider.get_code_lenses("file:///test.nxl", TEST_DOC)
    run_lenses = [
        l for l in lenses if l["command"]["command"] == "nexuslang.runTest"
    ]
    # One standalone 'test "standalone case"'
    assert len(run_lenses) >= 1


def test_run_test_arguments_include_uri(provider):
    uri = "file:///suite.nxl"
    lenses = provider.get_code_lenses(uri, TEST_DOC)
    for lens in lenses:
        if lens["command"]["command"] in ("nexuslang.runTest", "nexuslang.runTestSuite"):
            assert lens["command"]["arguments"][0] == uri


# ---------------------------------------------------------------------------
# Empty / trivial documents
# ---------------------------------------------------------------------------


def test_empty_document_returns_empty_list(provider):
    assert provider.get_code_lenses("file:///empty.nxl", "") == []


def test_comment_only_document_returns_empty_list(provider):
    assert provider.get_code_lenses("file:///c.nxl", EMPTY_DOC) == []


# ---------------------------------------------------------------------------
# resolve_code_lens pass-through
# ---------------------------------------------------------------------------


def test_resolve_code_lens_returns_lens_unchanged(provider):
    lens = {
        "range": {"start": {"line": 0, "character": 0}, "end": {"line": 0, "character": 8}},
        "command": {"title": "1 reference", "command": "nexuslang.findReferences", "arguments": []},
    }
    result = provider.resolve_code_lens(lens)
    assert result == lens


# ---------------------------------------------------------------------------
# Lens shape validation (LSP protocol compliance)
# ---------------------------------------------------------------------------


def test_all_lenses_have_required_fields(provider):
    lenses = provider.get_code_lenses("file:///test.nxl", FUNCTION_DOC)
    for lens in lenses:
        assert "range" in lens
        assert "start" in lens["range"]
        assert "end" in lens["range"]
        assert "line" in lens["range"]["start"]
        assert "character" in lens["range"]["start"]
        assert "command" in lens
        assert "title" in lens["command"]
        assert "command" in lens["command"]
        assert "arguments" in lens["command"]
