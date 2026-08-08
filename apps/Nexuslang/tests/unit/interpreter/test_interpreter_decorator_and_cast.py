from types import SimpleNamespace

import pytest

import nexuslang.decorators as decorators_module
from nexuslang.errors import NxlRuntimeError
from nexuslang.interpreter.interpreter import Interpreter, ReturnException
from nexuslang.parser.ast import ArrowKindAnnotation, StarKindAnnotation
from nexuslang.runtime.runtime import Runtime
from nexuslang.stdlib import register_stdlib
from nexuslang.typesystem.hkt import ArrowKind as HKTArrowKind
from nexuslang.typesystem.hkt import STAR


@pytest.fixture
def interpreter():
    runtime = Runtime()
    register_stdlib(runtime)
    interp = Interpreter(runtime)
    if not hasattr(interp.runtime, "_function_attributes"):
        interp.runtime._function_attributes = {}
    return interp


def _decorator_node(name, arguments=None, line=1):
    return SimpleNamespace(name=name, arguments=arguments or {}, line=line)


def test_ast_kind_to_hkt_maps_star_arrow_and_fallback():
    star = StarKindAnnotation()
    arrow = ArrowKindAnnotation(StarKindAnnotation(), StarKindAnnotation())

    assert Interpreter._ast_kind_to_hkt(star) is STAR

    mapped_arrow = Interpreter._ast_kind_to_hkt(arrow)
    assert isinstance(mapped_arrow, HKTArrowKind)
    assert mapped_arrow.param_kind is STAR
    assert mapped_arrow.result_kind is STAR

    # Unknown kind annotations default to STAR.
    assert Interpreter._ast_kind_to_hkt(object()) is STAR


def test_apply_decorator_builtin_simple(interpreter, monkeypatch):
    node = _decorator_node("trace")

    def builtin_decorator(fn):
        return ("decorated", fn)

    monkeypatch.setattr(decorators_module, "get_decorator", lambda _name: builtin_decorator)

    result = interpreter.apply_decorator(node, "function-value", SimpleNamespace(name="f"))

    assert result == ("decorated", "function-value")


def test_apply_decorator_builtin_with_arguments(interpreter, monkeypatch):
    calls = {}

    def decorator_factory(**kwargs):
        calls["kwargs"] = kwargs

        def actual_decorator(fn):
            return f"deprecated:{fn}:{kwargs['reason']}"

        return actual_decorator

    monkeypatch.setattr(decorators_module, "get_decorator", lambda _name: decorator_factory)
    monkeypatch.setattr(interpreter, "execute", lambda expr: expr)

    node = _decorator_node("deprecated", arguments={"reason": "legacy"})
    result = interpreter.apply_decorator(node, "fn", SimpleNamespace(name="target"))

    assert calls["kwargs"] == {"reason": "legacy"}
    assert result == "deprecated:fn:legacy"


def test_apply_decorator_attribute_definition_records_runtime_metadata(interpreter, monkeypatch):
    interpreter.attribute_definitions["Audit"] = {
        "properties": [("level", None), ("tag", None)],
    }
    monkeypatch.setattr(decorators_module, "get_decorator", lambda _name: None)
    monkeypatch.setattr(interpreter, "execute", lambda expr: expr)

    def fn_impl():
        return None

    node = _decorator_node("Audit", arguments={"_args": [3], "tag": "security"})
    fn_def = SimpleNamespace(name="process_data")

    returned = interpreter.apply_decorator(node, fn_impl, fn_def)

    assert returned is fn_impl
    assert fn_impl._applied_attributes["Audit"] == {"level": 3, "tag": "security"}
    assert interpreter.runtime._function_attributes["process_data"]["Audit"] == {
        "level": 3,
        "tag": "security",
    }


def test_apply_decorator_user_defined_function_returns_body_result(interpreter, monkeypatch):
    monkeypatch.setattr(decorators_module, "get_decorator", lambda _name: None)

    decorator_fn = SimpleNamespace(
        parameters=[SimpleNamespace(name="fn")],
        body=["stmt"],
    )
    interpreter.functions["wrap"] = decorator_fn

    monkeypatch.setattr(interpreter, "execute", lambda stmt: "wrapped-result")

    node = _decorator_node("wrap")
    result = interpreter.apply_decorator(node, "function-value", SimpleNamespace(name="f"))

    assert result == "wrapped-result"


def test_apply_decorator_user_defined_function_handles_return_exception(interpreter, monkeypatch):
    monkeypatch.setattr(decorators_module, "get_decorator", lambda _name: None)

    decorator_fn = SimpleNamespace(
        parameters=[SimpleNamespace(name="fn")],
        body=["stmt"],
    )
    interpreter.functions["wrap"] = decorator_fn

    def _execute(_stmt):
        raise ReturnException("return-value")

    monkeypatch.setattr(interpreter, "execute", _execute)

    node = _decorator_node("wrap")
    result = interpreter.apply_decorator(node, "function-value", SimpleNamespace(name="f"))

    assert result == "return-value"


def test_apply_decorator_unknown_raises_runtime_error(interpreter, monkeypatch):
    monkeypatch.setattr(decorators_module, "get_decorator", lambda _name: None)
    monkeypatch.setattr(interpreter, "get_variable", lambda _name: (_ for _ in ()).throw(NameError("missing")))

    node = _decorator_node("not_defined", line=9)

    with pytest.raises(NxlRuntimeError, match="Unknown decorator"):
        interpreter.apply_decorator(node, "function-value", SimpleNamespace(name="f"))


def test_execute_type_cast_expression_paths(interpreter, monkeypatch):
    monkeypatch.setattr(interpreter, "execute", lambda expr: expr)

    int_node = SimpleNamespace(expression="3.9", target_type="integer")
    float_node = SimpleNamespace(expression="2", target_type="float")
    upper_node = SimpleNamespace(expression="abc", target_type="uppercase")
    lower_node = SimpleNamespace(expression="ABC", target_type="lowercase")
    fallback_node = SimpleNamespace(expression={"k": 1}, target_type="custom")

    assert interpreter.execute_type_cast_expression(int_node) == 3
    assert interpreter.execute_type_cast_expression(float_node) == 2.0
    assert interpreter.execute_type_cast_expression(upper_node) == "ABC"
    assert interpreter.execute_type_cast_expression(lower_node) == "abc"
    assert interpreter.execute_type_cast_expression(fallback_node) == {"k": 1}
