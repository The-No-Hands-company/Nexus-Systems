"""Focused coverage for TypeEnvironment, TypeRegistry, and ownership pass flow."""

import builtins
import sys
import types

import pytest

from nexuslang.parser.ast import Program
from nexuslang.typesystem.generics_system import TypeParameterInfo
from nexuslang.typesystem.typechecker import (
    TypeCheckError,
    TypeChecker,
    TypeEnvironment,
    TypeRegistry,
)
from nexuslang.typesystem.types import (
    BOOLEAN_TYPE,
    FLOAT_TYPE,
    FunctionType,
    GenericParameter,
    INTEGER_TYPE,
)


class _Stmt:
    pass


def test_type_environment_scope_and_lookup_paths():
    parent = TypeEnvironment()
    child = TypeEnvironment(parent=parent)

    parent.define_variable("a", INTEGER_TYPE)
    assert child.get_variable_type("a") == INTEGER_TYPE

    assert child.assign_variable_type("a", FLOAT_TYPE) is True
    assert parent.get_variable_type("a") == FLOAT_TYPE
    assert child.assign_variable_type("missing", BOOLEAN_TYPE) is False

    with pytest.raises(TypeCheckError, match="Undefined variable"):
        child.get_variable_type("missing")

    fn = FunctionType([INTEGER_TYPE], FLOAT_TYPE)
    parent.define_function("f", fn)
    assert child.get_function_type("f") == fn

    with pytest.raises(TypeCheckError, match="Undefined function"):
        child.get_function_type("g")


def test_type_environment_generic_scope_return_and_generator_paths():
    root = TypeEnvironment()
    nested = TypeEnvironment(parent=root)

    root.set_return_type(INTEGER_TYPE)
    assert nested.get_return_type() == INTEGER_TYPE

    p = TypeParameterInfo(name="T", constraints=[], variance=None)
    root.enter_generic_scope([p])
    assert root.is_type_parameter("T") is True
    assert root.resolve_type(GenericParameter("T")) == GenericParameter("T")

    # Exit with no parent generic context.
    root.exit_generic_scope()
    assert root.generic_context is None

    # Exit with parent generic context available.
    root.enter_generic_scope([p])
    first = root.generic_context
    root.enter_generic_scope([TypeParameterInfo(name="U", constraints=[], variance=None)])
    root.exit_generic_scope()
    assert root.generic_context is first

    assert nested.get_generator_context() is None
    root.is_generator_function = True
    assert nested.get_generator_context() is root


def test_type_registry_class_creation_and_interface_checks():
    reg = TypeRegistry()

    created = reg.create_class_type(
        "Widget",
        {"id": INTEGER_TYPE},
        {"render": FunctionType([], INTEGER_TYPE)},
        ["Base"],
    )
    assert reg["Widget"] == created

    reg.register_interface("Renderable", ["render", "resize"])
    missing = reg.check_interface_implementation("Widget", "Renderable")
    assert missing == ["resize"]

    # Unknown interface is treated as forward reference and should pass.
    assert reg.check_interface_implementation("Widget", "UnknownInterface") == []


def test_check_program_early_return_when_ownership_fails(monkeypatch):
    checker = TypeChecker(enable_ownership_passes=True, stop_on_ownership_errors=True)

    called = {"statement": 0}

    def fake_run(program):
        checker.errors.append("ownership failed")
        return False

    def fake_check_statement(statement, env):
        called["statement"] += 1

    monkeypatch.setattr(checker, "_run_ownership_lifetime_passes", fake_run)
    monkeypatch.setattr(checker, "check_statement", fake_check_statement)

    out = checker.check_program(Program([_Stmt()]))

    assert out == ["ownership failed"]
    assert called["statement"] == 0


def test_check_program_continues_when_stop_flag_is_disabled(monkeypatch):
    checker = TypeChecker(enable_ownership_passes=True, stop_on_ownership_errors=False)

    called = {"statement": 0}

    def fake_run(program):
        return False

    def fake_check_statement(statement, env):
        called["statement"] += 1

    monkeypatch.setattr(checker, "_run_ownership_lifetime_passes", fake_run)
    monkeypatch.setattr(checker, "check_statement", fake_check_statement)

    checker.check_program(Program([_Stmt(), _Stmt()]))

    assert called["statement"] == 2


def test_ownership_passes_import_fallback_returns_true(monkeypatch):
    checker = TypeChecker(enable_ownership_passes=True)
    original_import = builtins.__import__

    def fake_import(name, globals=None, locals=None, fromlist=(), level=0):
        if name.endswith(".borrow_checker") or name.endswith(".lifetime_checker"):
            raise ImportError("missing ownership module")
        return original_import(name, globals, locals, fromlist, level)

    monkeypatch.setattr(builtins, "__import__", fake_import)

    assert checker._run_ownership_lifetime_passes(Program([])) is True


def test_ownership_passes_stop_after_borrow_errors(monkeypatch):
    checker = TypeChecker(enable_ownership_passes=True, stop_on_ownership_errors=True)

    class FakeBorrowChecker:
        def check(self, program):
            return ["borrow issue"]

    class FakeLifetimeChecker:
        called = False

        def check(self, program):
            FakeLifetimeChecker.called = True
            return []

    monkeypatch.setitem(sys.modules, "nexuslang.typesystem.borrow_checker", types.SimpleNamespace(BorrowChecker=FakeBorrowChecker))
    monkeypatch.setitem(sys.modules, "nexuslang.typesystem.lifetime_checker", types.SimpleNamespace(LifetimeChecker=FakeLifetimeChecker))

    ok = checker._run_ownership_lifetime_passes(Program([]))

    assert ok is False
    assert FakeLifetimeChecker.called is False
    assert any("ownership.borrow" in err for err in checker.errors)


def test_ownership_passes_collect_lifetime_errors_and_warnings(monkeypatch):
    checker = TypeChecker(enable_ownership_passes=True, stop_on_ownership_errors=False)

    class FakeBorrowChecker:
        def check(self, program):
            return []

    class _LifetimeItem:
        def __init__(self, text, is_warning):
            self.text = text
            self.is_warning = is_warning

        def __str__(self):
            return self.text

    class FakeLifetimeChecker:
        def check(self, program):
            return [
                _LifetimeItem("hard lifetime", False),
                _LifetimeItem("warn lifetime", True),
            ]

    monkeypatch.setitem(sys.modules, "nexuslang.typesystem.borrow_checker", types.SimpleNamespace(BorrowChecker=FakeBorrowChecker))
    monkeypatch.setitem(sys.modules, "nexuslang.typesystem.lifetime_checker", types.SimpleNamespace(LifetimeChecker=FakeLifetimeChecker))

    ok = checker._run_ownership_lifetime_passes(Program([]))

    assert ok is False
    assert any("ownership.lifetime] hard lifetime" in err for err in checker.errors)
    assert any("ownership.lifetime.warning" in warn for warn in checker.warnings)
