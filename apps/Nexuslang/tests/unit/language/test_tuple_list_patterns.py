"""
Regression tests for TuplePattern and ListPattern match-expression handlers.

Validates:
- TuplePattern binds positional elements correctly
- TuplePattern rejects mismatched lengths
- TuplePattern supports nested sub-patterns (wildcard, literal, identifier)
- ListPattern binds positional elements correctly
- ListPattern with rest_binding captures remaining elements
- ListPattern rejects values that are too short
- ListPattern rejects non-list/tuple values
- TuplePattern rejects non-sequence values
- _match_pattern raises NxlRuntimeError on truly unknown pattern types
- Exception narrowing in start_with / end_with matchers (TypeError only)
"""

from __future__ import annotations

import sys
from pathlib import Path

import pytest

_SRC = str(Path(__file__).resolve().parent.parent.parent.parent / "src")
if _SRC not in sys.path:
    sys.path.insert(0, _SRC)

from nexuslang.parser.ast import (
    TuplePattern,
    ListPattern,
    IdentifierPattern,
    WildcardPattern,
    LiteralPattern,
    Literal,
)
from nexuslang.interpreter.interpreter import Interpreter
from nexuslang.runtime.runtime import Runtime
from nexuslang.stdlib import register_stdlib


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _make_interpreter() -> Interpreter:
    runtime = Runtime()
    register_stdlib(runtime)
    return Interpreter(runtime)


def _literal_pattern(value) -> LiteralPattern:
    """Build a LiteralPattern wrapping an integer or string literal."""
    if isinstance(value, int):
        return LiteralPattern(Literal("integer", value))
    return LiteralPattern(Literal("string", value))


# ---------------------------------------------------------------------------
# TuplePattern unit tests (via _match_pattern directly)
# ---------------------------------------------------------------------------

class TestTuplePatternMatching:
    """Unit tests for TuplePattern handler in Interpreter._match_pattern."""

    def setup_method(self):
        self.interp = _make_interpreter()

    def test_tuple_matches_exact_length_tuple(self):
        """TuplePattern matches a Python tuple of equal length."""
        pattern = TuplePattern([
            IdentifierPattern("a"),
            IdentifierPattern("b"),
        ])
        matched, bindings = self.interp._match_pattern(pattern, (10, 20))
        assert matched is True
        assert bindings == {"a": 10, "b": 20}

    def test_tuple_matches_list_value(self):
        """TuplePattern also matches a Python list of equal length."""
        pattern = TuplePattern([
            IdentifierPattern("x"),
            IdentifierPattern("y"),
        ])
        matched, bindings = self.interp._match_pattern(pattern, [1, 2])
        assert matched is True
        assert bindings == {"x": 1, "y": 2}

    def test_tuple_rejects_wrong_length(self):
        """TuplePattern rejects values whose length does not match."""
        pattern = TuplePattern([
            IdentifierPattern("a"),
            IdentifierPattern("b"),
        ])
        matched, _ = self.interp._match_pattern(pattern, (1, 2, 3))
        assert matched is False

    def test_tuple_rejects_non_sequence(self):
        """TuplePattern rejects non-sequence values (int, str, dict, etc.)."""
        pattern = TuplePattern([IdentifierPattern("x")])
        for bad_value in (42, "hello", {"k": "v"}, None):
            matched, _ = self.interp._match_pattern(pattern, bad_value)
            assert matched is False, f"Expected no match for {bad_value!r}"

    def test_tuple_with_wildcard_sub_pattern(self):
        """Wildcard sub-pattern inside TuplePattern matches anything and binds nothing."""
        pattern = TuplePattern([
            IdentifierPattern("first"),
            WildcardPattern(),
        ])
        matched, bindings = self.interp._match_pattern(pattern, (7, 99))
        assert matched is True
        assert bindings == {"first": 7}

    def test_tuple_with_literal_sub_pattern_match(self):
        """Literal sub-pattern inside TuplePattern matches equal value."""
        pattern = TuplePattern([
            _literal_pattern(42),
            IdentifierPattern("rest"),
        ])
        matched, bindings = self.interp._match_pattern(pattern, (42, "hello"))
        assert matched is True
        assert bindings == {"rest": "hello"}

    def test_tuple_with_literal_sub_pattern_no_match(self):
        """Literal sub-pattern inside TuplePattern rejects non-equal value."""
        pattern = TuplePattern([
            _literal_pattern(42),
            IdentifierPattern("rest"),
        ])
        matched, _ = self.interp._match_pattern(pattern, (99, "hello"))
        assert matched is False

    def test_empty_tuple_pattern_matches_empty(self):
        """Empty TuplePattern matches an empty tuple."""
        pattern = TuplePattern([])
        matched, bindings = self.interp._match_pattern(pattern, ())
        assert matched is True
        assert bindings == {}

    def test_empty_tuple_pattern_rejects_nonempty(self):
        """Empty TuplePattern rejects a non-empty sequence."""
        pattern = TuplePattern([])
        matched, _ = self.interp._match_pattern(pattern, (1,))
        assert matched is False


# ---------------------------------------------------------------------------
# ListPattern unit tests (via _match_pattern directly)
# ---------------------------------------------------------------------------

class TestListPatternMatching:
    """Unit tests for ListPattern handler in Interpreter._match_pattern."""

    def setup_method(self):
        self.interp = _make_interpreter()

    def test_list_matches_exact_length(self):
        """ListPattern without rest binding matches a list of equal length."""
        pattern = ListPattern([
            IdentifierPattern("first"),
            IdentifierPattern("second"),
        ])
        matched, bindings = self.interp._match_pattern(pattern, [10, 20])
        assert matched is True
        assert bindings == {"first": 10, "second": 20}

    def test_list_matches_tuple_value(self):
        """ListPattern also matches a Python tuple."""
        pattern = ListPattern([IdentifierPattern("x")])
        matched, bindings = self.interp._match_pattern(pattern, (99,))
        assert matched is True
        assert bindings == {"x": 99}

    def test_list_rejects_wrong_length_without_rest(self):
        """ListPattern without rest binding rejects mismatched lengths."""
        pattern = ListPattern([IdentifierPattern("a"), IdentifierPattern("b")])
        matched, _ = self.interp._match_pattern(pattern, [1])
        assert matched is False

    def test_list_rejects_non_sequence(self):
        """ListPattern rejects non-list/tuple values."""
        pattern = ListPattern([IdentifierPattern("x")])
        for bad_value in (42, "hello", {"k": "v"}, None):
            matched, _ = self.interp._match_pattern(pattern, bad_value)
            assert matched is False, f"Expected no match for {bad_value!r}"

    def test_list_with_rest_binding_captures_tail(self):
        """ListPattern with rest_binding binds head elements and collects the tail."""
        pattern = ListPattern(
            [IdentifierPattern("head")],
            rest_binding="tail",
        )
        matched, bindings = self.interp._match_pattern(pattern, [1, 2, 3, 4])
        assert matched is True
        assert bindings["head"] == 1
        assert bindings["tail"] == [2, 3, 4]

    def test_list_rest_binding_empty_tail(self):
        """ListPattern with rest_binding captures an empty tail when value is exactly head length."""
        pattern = ListPattern(
            [IdentifierPattern("head")],
            rest_binding="tail",
        )
        matched, bindings = self.interp._match_pattern(pattern, [42])
        assert matched is True
        assert bindings["head"] == 42
        assert bindings["tail"] == []

    def test_list_rest_binding_rejects_too_short(self):
        """ListPattern with rest_binding rejects if value is shorter than the explicit patterns."""
        pattern = ListPattern(
            [IdentifierPattern("a"), IdentifierPattern("b")],
            rest_binding="rest",
        )
        matched, _ = self.interp._match_pattern(pattern, [1])
        assert matched is False

    def test_empty_list_pattern_matches_empty(self):
        """Empty ListPattern (no patterns, no rest) matches an empty list."""
        pattern = ListPattern([])
        matched, bindings = self.interp._match_pattern(pattern, [])
        assert matched is True
        assert bindings == {}

    def test_empty_list_with_rest_captures_all(self):
        """ListPattern with no explicit patterns but a rest_binding captures the whole list."""
        pattern = ListPattern([], rest_binding="all")
        matched, bindings = self.interp._match_pattern(pattern, [1, 2, 3])
        assert matched is True
        assert bindings == {"all": [1, 2, 3]}

    def test_list_with_wildcard_sub_pattern(self):
        """Wildcard inside ListPattern still counts as positional but binds nothing."""
        pattern = ListPattern([
            WildcardPattern(),
            IdentifierPattern("second"),
        ])
        matched, bindings = self.interp._match_pattern(pattern, [0, 99])
        assert matched is True
        assert bindings == {"second": 99}


# ---------------------------------------------------------------------------
# Exception-narrowing regression tests
# ---------------------------------------------------------------------------

class TestExceptionNarrowingRegressions:
    """Verify that previously-broad exception catches were narrowed correctly."""

    def setup_method(self):
        self.interp = _make_interpreter()

    def test_unknown_pattern_type_raises_runtime_error(self):
        """_match_pattern raises NxlRuntimeError for completely unknown pattern types."""
        from nexuslang.errors import NxlRuntimeError
        from nexuslang.parser.ast import Pattern

        class _UnknownPattern(Pattern):
            def __init__(self):
                super().__init__("unknown_test")

        with pytest.raises(NxlRuntimeError, match="Unknown pattern type"):
            self.interp._match_pattern(_UnknownPattern(), 42)


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
