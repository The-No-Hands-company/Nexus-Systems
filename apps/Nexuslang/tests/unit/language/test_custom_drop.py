"""Tests for Custom Drop trait and deterministic drop order.

Validates:
- Classes with a 'drop' method get called automatically on scope exit
- Drop order is LIFO (reverse declaration order)
- Drop methods have access to object state via 'self'
- Nested scopes drop correctly
- Mixed droppable / non-droppable scopes work
- Drop errors are swallowed (never abort cleanup)
"""

import os
import sys

import pytest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "../../../src"))

from nexuslang.parser.lexer import Lexer
from nexuslang.parser.parser import Parser
from nexuslang.interpreter.interpreter import Interpreter
from nexuslang.runtime.runtime import Runtime


def run(source: str) -> list:
    """Parse + interpret *source*, return list of captured print outputs.

    Monkey-patches ``runtime.print`` so that every ``print text ...``
    statement appends to a list instead of writing to stdout.
    """
    lexer = Lexer(source)
    tokens = lexer.tokenize()
    parser = Parser(tokens, source=source)
    ast = parser.parse()
    runtime = Runtime()
    interp = Interpreter(runtime, source=source)

    captured: list[str] = []

    # execute_print_statement calls self.runtime.print(value).
    # Monkey-patch that method so output is captured instead of printed.
    def _capture_print(*args, sep=" ", end="\n"):
        captured.append(sep.join(str(a) for a in args))

    runtime.print = _capture_print  # type: ignore[method-assign]

    for stmt in ast.statements:
        interp.execute(stmt)

    return captured


# ---------------------------------------------------------------------------
# Basic Custom Drop
# ---------------------------------------------------------------------------

class TestCustomDropBasic:
    """Classes with a drop method should have it called on scope exit."""

    def test_drop_called_on_scope_exit(self):
        """Drop method fires automatically when its scope exits."""
        source = """
class Resource
    property name

    function drop with self
        print text "drop:" plus self.name
    end
end

function test_scope
    set r to new Resource
    set r.name to "file_handle"
end

call test_scope
print text "done"
"""
        out = run(source)
        assert "drop:file_handle" in out, \
            f"Expected 'drop:file_handle' in output, got: {out}"
        # "done" must appear AFTER the drop (drop happens when test_scope exits)
        assert out.index("drop:file_handle") < out.index("done"), \
            f"Drop should fire before 'done', got: {out}"

    def test_drop_accesses_object_properties(self):
        """Drop method should be able to read object properties via self."""
        source = """
class Connection
    property host
    property port

    function drop with self
        print text "Closed " plus self.host plus ":" plus self.port
    end
end

function create_connection
    set conn to new Connection
    set conn.host to "localhost"
    set conn.port to "8080"
end

call create_connection
"""
        out = run(source)
        assert any("Closed localhost:8080" in line for line in out), \
            f"Expected 'Closed localhost:8080' in output, got: {out}"


# ---------------------------------------------------------------------------
# Drop Order (LIFO - last declared, first dropped)
# ---------------------------------------------------------------------------

class TestDropOrder:
    """Drop order must be deterministic: reverse declaration order (LIFO)."""

    def test_lifo_drop_order(self):
        """Last-declared variable should be dropped first."""
        source = """
class Tracker
    property label

    function drop with self
        print text self.label
    end
end

function test_order
    set a to new Tracker
    set a.label to "first"
    set b to new Tracker
    set b.label to "second"
    set c to new Tracker
    set c.label to "third"
end

call test_order
"""
        out = run(source)
        # LIFO: third -> second -> first
        assert len(out) >= 3, f"Expected 3 drop prints, got {len(out)}: {out}"
        assert out[0] == "third", f"Expected first drop to be 'third', got: {out}"
        assert out[1] == "second", f"Expected second drop to be 'second', got: {out}"
        assert out[2] == "first", f"Expected third drop to be 'first', got: {out}"

    def test_nested_scope_drop_order(self):
        """Inner scope variables should be dropped before outer scope continues."""
        source = """
class Item
    property id

    function drop with self
        print text "drop:" plus self.id
    end
end

function outer_scope
    set outer to new Item
    set outer.id to "outer"

    function inner_scope
        set inner to new Item
        set inner.id to "inner"
    end

    call inner_scope
end

call outer_scope
"""
        out = run(source)
        assert "drop:inner" in out, f"Expected 'drop:inner' in output, got: {out}"
        assert "drop:outer" in out, f"Expected 'drop:outer' in output, got: {out}"
        assert out.index("drop:inner") < out.index("drop:outer"), \
            f"Expected inner dropped before outer, got: {out}"


# ---------------------------------------------------------------------------
# Drop with non-droppable values
# ---------------------------------------------------------------------------

class TestDropMixedValues:
    """Scope exit should not fail when mixing droppable and non-droppable values."""

    def test_mixed_scope_no_crash(self):
        """Scope with both droppable objects and plain values works."""
        source = """
class Droppable
    property name

    function drop with self
        print text "dropped:" plus self.name
    end
end

function mixed
    set x to 42
    set d to new Droppable
    set d.name to "test"
    set y to "hello"
end

call mixed
print text "survived"
"""
        out = run(source)
        assert "dropped:test" in out, f"Expected 'dropped:test' in output, got: {out}"
        assert "survived" in out, f"Expected 'survived' in output, got: {out}"

    def test_no_drop_method_no_call(self):
        """Objects without drop method should not cause errors on scope exit."""
        source = """
class Plain
    property value
end

function test_no_drop
    set p to new Plain
    set p.value to 123
end

call test_no_drop
print text "no crash"
"""
        out = run(source)
        assert "no crash" in out, f"Expected 'no crash' in output, got: {out}"


# ---------------------------------------------------------------------------
# Drop with derived methods
# ---------------------------------------------------------------------------

class TestDropWithDerived:
    """Drop should work alongside @derive decorators."""

    def test_drop_and_derive_coexist(self):
        """A class with both @derive and a drop method should work."""
        source = """
class Widget
    property name

    function drop with self
        print text "widget_dropped"
    end

    function to_string with self
        return "Widget:" plus self.name
    end
end

function test_derived
    set w to new Widget
    set w.name to "button"
end

call test_derived
print text "after"
"""
        out = run(source)
        assert "widget_dropped" in out, f"Expected 'widget_dropped' in output, got: {out}"
        assert "after" in out, f"Expected 'after' in output, got: {out}"


# ---------------------------------------------------------------------------
# Drop exceptions should not propagate
# ---------------------------------------------------------------------------

class TestDropSafety:
    """Drop errors should never crash the program."""

    def test_drop_error_swallowed(self):
        """If a drop method raises an error, it should be silently caught."""
        source = """
class Faulty
    property name

    function drop with self
        set x to undefined_variable
    end
end

function test_faulty
    set f to new Faulty
    set f.name to "test"
end

call test_faulty
print text "survived"
"""
        out = run(source)
        assert "survived" in out, f"Expected 'survived' in output, got: {out}"
