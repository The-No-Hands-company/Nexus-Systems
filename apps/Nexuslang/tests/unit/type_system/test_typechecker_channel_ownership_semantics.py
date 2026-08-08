"""Focused semantic coverage for channel and ownership helpers in typechecker."""

import pytest

from nexuslang.parser.ast import Identifier
from nexuslang.typesystem.typechecker import TypeChecker, TypeEnvironment
from nexuslang.typesystem.types import (
    ANY_TYPE,
    INTEGER_TYPE,
    STRING_TYPE,
    ChannelType,
    AnyType,
)


class _Node:
    def __init__(self, class_name: str, **attrs):
        self.__class__ = type(class_name, (), {})
        for key, value in attrs.items():
            setattr(self, key, value)


def _checker() -> TypeChecker:
    return TypeChecker(enable_ownership_passes=False)


def _env() -> TypeEnvironment:
    return TypeEnvironment()


class TestChannelHelpers:
    def test_send_to_any_channel_is_noop(self):
        checker = _checker()
        env = _env()
        stmt = _Node("SendStatement", channel="ch", value="v", line_number=1)

        checker.check_expression = lambda expr, e: AnyType() if expr == "ch" else INTEGER_TYPE
        checker._check_send_statement(stmt, env)

        assert checker.errors == []

    def test_send_to_non_channel_reports_error(self):
        checker = _checker()
        env = _env()
        stmt = _Node("SendStatement", channel="not_ch", value="v", line_number=2)

        checker.check_expression = lambda expr, e: INTEGER_TYPE
        checker._check_send_statement(stmt, env)

        assert any("Send target must be a channel" in err for err in checker.errors)

    def test_send_refines_channel_identifier_payload(self):
        checker = _checker()
        env = _env()
        env.define_variable("ch", ChannelType(ANY_TYPE))

        stmt = _Node("SendStatement", channel=Identifier("ch"), value="payload", line_number=3)
        checker.check_expression = lambda expr, e: ChannelType(ANY_TYPE) if expr is stmt.channel else STRING_TYPE

        checker._check_send_statement(stmt, env)

        refined = env.get_variable_type("ch")
        assert isinstance(refined, ChannelType)
        assert refined.payload_type == STRING_TYPE

    def test_send_type_mismatch_reports_error(self):
        checker = _checker()
        env = _env()

        stmt = _Node("SendStatement", channel="ch", value="payload", line_number=4)
        checker.check_expression = lambda expr, e: ChannelType(STRING_TYPE) if expr == "ch" else INTEGER_TYPE

        checker._check_send_statement(stmt, env)

        assert any("Cannot send value of type" in err for err in checker.errors)

    def test_receive_from_any_channel_returns_any(self):
        checker = _checker()
        env = _env()
        stmt = _Node("ReceiveExpression", channel="ch", line_number=5)
        checker.check_expression = lambda expr, e: AnyType()

        result = checker._check_receive_expression(stmt, env)

        assert result == ANY_TYPE

    def test_receive_from_non_channel_reports_and_returns_any(self):
        checker = _checker()
        env = _env()
        stmt = _Node("ReceiveExpression", channel="x", line_number=6)
        checker.check_expression = lambda expr, e: INTEGER_TYPE

        result = checker._check_receive_expression(stmt, env)

        assert result == ANY_TYPE
        assert any("Receive target must be a channel" in err for err in checker.errors)

    def test_receive_from_channel_returns_payload_type(self):
        checker = _checker()
        env = _env()
        stmt = _Node("ReceiveExpression", channel="ch", line_number=7)
        checker.check_expression = lambda expr, e: ChannelType(STRING_TYPE)

        result = checker._check_receive_expression(stmt, env)

        assert result == STRING_TYPE

    def test_close_non_channel_reports_error(self):
        checker = _checker()
        env = _env()
        stmt = _Node("CloseStatement", channel="x", line_number=8)
        checker.check_expression = lambda expr, e: INTEGER_TYPE

        checker._check_close_statement(stmt, env)

        assert any("Close target must be a channel" in err for err in checker.errors)

    def test_close_any_channel_no_error(self):
        checker = _checker()
        env = _env()
        stmt = _Node("CloseStatement", channel="x", line_number=9)
        checker.check_expression = lambda expr, e: AnyType()

        checker._check_close_statement(stmt, env)

        assert checker.errors == []


class TestChannelRefinement:
    def test_refine_ignores_non_identifier(self):
        checker = _checker()
        env = _env()
        checker._refine_channel_identifier_type("not-identifier", INTEGER_TYPE, env)
        assert True

    def test_refine_ignores_missing_variable(self):
        checker = _checker()
        env = _env()
        checker._refine_channel_identifier_type(Identifier("missing"), INTEGER_TYPE, env)
        assert True

    def test_refine_ignores_non_channel_variable(self):
        checker = _checker()
        env = _env()
        env.define_variable("x", INTEGER_TYPE)

        checker._refine_channel_identifier_type(Identifier("x"), STRING_TYPE, env)

        assert env.get_variable_type("x") == INTEGER_TYPE

    def test_refine_ignores_already_typed_channel(self):
        checker = _checker()
        env = _env()
        env.define_variable("ch", ChannelType(INTEGER_TYPE))

        checker._refine_channel_identifier_type(Identifier("ch"), STRING_TYPE, env)

        typed = env.get_variable_type("ch")
        assert isinstance(typed, ChannelType)
        assert typed.payload_type == INTEGER_TYPE


class TestOwnershipStatementHelper:
    def test_move_expression_returns_existing_var_type(self):
        checker = _checker()
        env = _env()
        env.define_variable("value", STRING_TYPE)

        handled, result = checker._check_ownership_statement(_Node("MoveExpression", var_name="value"), env)

        assert handled is True
        assert result == STRING_TYPE

    def test_move_expression_missing_var_returns_any(self):
        checker = _checker()
        env = _env()

        handled, result = checker._check_ownership_statement(_Node("MoveExpression", var_name="missing"), env)

        assert handled is True
        assert result == ANY_TYPE

    def test_borrow_expression_returns_existing_var_type(self):
        checker = _checker()
        env = _env()
        env.define_variable("buf", INTEGER_TYPE)

        handled, result = checker._check_ownership_statement(_Node("BorrowExpression", var_name="buf"), env)

        assert handled is True
        assert result == INTEGER_TYPE

    def test_lifetime_annotation_requires_non_empty_string(self):
        checker = _checker()
        env = _env()

        with pytest.raises(TypeError, match="statement.label must be a non-empty string"):
            checker._check_ownership_statement(_Node("LifetimeAnnotation", label=""), env)

    def test_rc_and_drop_paths_are_handled(self):
        checker = _checker()
        env = _env()
        checker.check_statement = lambda value, e: ANY_TYPE

        handled_rc, result_rc = checker._check_ownership_statement(_Node("RcCreation", value=_Node("Literal")), env)
        handled_drop, result_drop = checker._check_ownership_statement(_Node("DropBorrowStatement", var_name="x"), env)

        assert handled_rc is True and result_rc == ANY_TYPE
        assert handled_drop is True and result_drop == ANY_TYPE
