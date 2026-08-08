"""Advanced branch coverage tests for lifetime checker internals."""

from nexuslang.parser.ast import (
    AsyncFunctionDefinition,
    BorrowExpression,
    BorrowExpressionWithLifetime,
    FunctionDefinition,
    LifetimeAnnotation,
    Parameter,
    Program,
    ReturnStatement,
    ReturnTypeWithLifetime,
    Literal,
)
from nexuslang.typesystem.lifetime_checker import LifetimeChecker


def _check(program: Program):
    return LifetimeChecker().check(program)


def _borrow_with_lifetime(var_name: str, label: str):
    return BorrowExpressionWithLifetime(var_name, False, LifetimeAnnotation(label))


def _borrow_param(name: str, label: str):
    return Parameter(name, ReturnTypeWithLifetime("String", lifetime=LifetimeAnnotation(label)))


def _fn(name: str, body, params=None, return_label=None):
    return_type = "String"
    if return_label is not None:
        return_type = ReturnTypeWithLifetime("String", lifetime=LifetimeAnnotation(return_label))
    return FunctionDefinition(name, params or [], body, return_type)


def _named_node(class_name: str, **attrs):
    cls = type(class_name, (), {})
    node = cls()
    for key, value in attrs.items():
        setattr(node, key, value)
    return node


class TestLifetimeCheckerAdvanced:
    def test_label_of_none_returns_none(self):
        checker = LifetimeChecker()
        assert checker._label_of(None) is None

    def test_line_uses_line_fallback(self):
        checker = LifetimeChecker()
        node = _named_node("AnyNode", line=77)
        assert checker._line(node) == 77

    def test_check_node_none_is_noop(self):
        checker = LifetimeChecker()
        checker._check_node(None)
        assert checker._errors == []

    def test_generic_fallback_recurses_into_expression(self):
        nested = _borrow_with_lifetime("x", "inner")
        unknown = _named_node("UnknownNode", expression=nested)
        fn = _fn("f", [unknown], params=[_borrow_param("x", "outer")], return_label="outer")

        errors = _check(Program([fn]))

        assert any("inner" in str(error) for error in errors)

    def test_collects_param_label_from_param_lifetime_attribute(self):
        param = _named_node(
            "ParameterLike",
            name="x",
            type_annotation=None,
            lifetime=LifetimeAnnotation("a"),
        )
        fn = _fn(
            "echo",
            [ReturnStatement(_borrow_with_lifetime("x", "a"))],
            params=[param],
            return_label="a",
        )

        errors = _check(Program([fn]))

        assert errors == []

    def test_async_function_uses_function_definition_handler(self):
        async_fn = AsyncFunctionDefinition(
            "af",
            [_borrow_param("x", "outer")],
            [ReturnStatement(_borrow_with_lifetime("x", "inner"))],
            ReturnTypeWithLifetime("String", lifetime=LifetimeAnnotation("outer")),
        )

        errors = _check(Program([async_fn]))

        assert any("inner" in str(error) for error in errors)

    def test_return_borrow_without_lifetime_recorded_as_error(self):
        fn = _fn(
            "plain_borrow_return",
            [ReturnStatement(BorrowExpression("x"))],
            params=[_borrow_param("x", "a")],
            return_label="a",
        )

        errors = _check(Program([fn]))

        assert any("without a lifetime annotation" in str(error).lower() for error in errors)

    def test_return_statement_without_value_is_accepted(self):
        fn = _fn("empty_return", [ReturnStatement(None)], params=[_borrow_param("x", "a")], return_label="a")

        errors = _check(Program([fn]))

        assert errors == []
        assert not any("without a lifetime annotation" in str(error).lower() for error in errors)

    def test_unused_param_lifetime_emits_warning(self):
        fn = _fn("unused", [Literal("string", "ok")], params=[_borrow_param("x", "a")], return_label=None)

        errors = _check(Program([fn]))

        assert any(error.is_warning for error in errors)

    def test_if_statement_with_non_list_branches_recurses(self):
        then_node = _borrow_with_lifetime("x", "inner")
        else_node = _borrow_with_lifetime("x", "outer")
        if_node = _named_node(
            "IfStatement",
            condition=Literal("boolean", True),
            then_block=then_node,
            else_block=else_node,
        )
        fn = _fn("if_wrap", [if_node], params=[_borrow_param("x", "outer")], return_label="outer")

        errors = _check(Program([fn]))

        assert any("inner" in str(error) for error in errors)

    def test_while_and_for_loop_else_paths_recurse(self):
        while_node = _named_node(
            "WhileLoop",
            condition=Literal("boolean", True),
            body=[_borrow_with_lifetime("x", "inner")],
            else_body=[_borrow_with_lifetime("x", "outer")],
        )
        for_node = _named_node(
            "ForLoop",
            iterable=Literal("integer", 1),
            start=None,
            end=None,
            step=None,
            body=[_borrow_with_lifetime("x", "inner2")],
            else_body=[_borrow_with_lifetime("x", "outer")],
        )
        fn = _fn("loop_wrap", [while_node, for_node], params=[_borrow_param("x", "outer")], return_label="outer")

        errors = _check(Program([fn]))

        assert any("inner" in str(error) for error in errors)
        assert any("inner2" in str(error) for error in errors)

    def test_match_expression_and_guard_are_checked(self):
        case = _named_node(
            "MatchCase",
            guard=_borrow_with_lifetime("x", "inner_guard"),
            body=[_borrow_with_lifetime("x", "inner_body")],
        )
        match_node = _named_node(
            "MatchExpression",
            expression=Literal("integer", 1),
            cases=[case],
        )
        fn = _fn("match_wrap", [match_node], params=[_borrow_param("x", "outer")], return_label="outer")

        errors = _check(Program([fn]))

        assert any("inner_guard" in str(error) for error in errors)
        assert any("inner_body" in str(error) for error in errors)

    def test_trycatch_alias_handlers_delegate(self):
        tcb = _named_node(
            "TryCatchBlock",
            try_block=[_borrow_with_lifetime("x", "inner_t")],
            catch_block=[_borrow_with_lifetime("x", "inner_c")],
            finally_block=[_borrow_with_lifetime("x", "outer")],
        )
        tcs = _named_node(
            "TryCatchStatement",
            try_block=[_borrow_with_lifetime("x", "inner_t2")],
            catch_block=[],
            finally_block=[],
        )
        fn = _fn("try_wrap", [tcb, tcs], params=[_borrow_param("x", "outer")], return_label="outer")

        errors = _check(Program([fn]))

        assert any("inner_t" in str(error) for error in errors)
        assert any("inner_c" in str(error) for error in errors)
        assert any("inner_t2" in str(error) for error in errors)

    def test_class_definition_recurses_methods(self):
        method = _fn(
            "m",
            [ReturnStatement(_borrow_with_lifetime("x", "inner"))],
            params=[_borrow_param("x", "outer")],
            return_label="outer",
        )
        class_node = _named_node("ClassDefinition", methods=[method])

        errors = _check(Program([class_node]))

        assert any("inner" in str(error) for error in errors)

    def test_pass_handlers_for_borrow_move_drop_do_not_error(self):
        checker = LifetimeChecker()
        checker._check_node(_named_node("BorrowExpression"))
        checker._check_node(_named_node("MoveExpression"))
        checker._check_node(_named_node("DropBorrowStatement"))

        assert checker._errors == []
