"""TypeChecker coverage tests for channel syntax."""

import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', '..', 'src'))

from nexuslang.parser.lexer import Lexer
from nexuslang.parser.parser import Parser
from nexuslang.typesystem.typechecker import TypeChecker
from nexuslang.typesystem.types import ChannelType, INTEGER_TYPE


def _parse(code: str):
    lexer = Lexer(code)
    tokens = lexer.scan_tokens()
    parser = Parser(tokens)
    return parser.parse()


def test_typechecker_accepts_channel_send_receive_program():
    code = """
    set ch to create channel
    send 1 to ch
    set x to receive from ch
    """

    ast = _parse(code)
    checker = TypeChecker()
    errors = checker.check_program(ast)
    assert errors == []


def test_channel_payload_type_is_inferred_from_send():
    code = """
    set ch to create channel
    send 1 to ch
    set x to receive from ch
    """

    ast = _parse(code)
    checker = TypeChecker()
    errors = checker.check_program(ast)

    assert errors == []
    ch_type = checker.env.get_variable_type("ch")
    x_type = checker.env.get_variable_type("x")
    assert isinstance(ch_type, ChannelType)
    assert ch_type.payload_type == INTEGER_TYPE
    assert x_type == INTEGER_TYPE


def test_typechecker_rejects_mismatched_channel_payload_send():
    code = """
    set ch to create channel
    send 1 to ch
    send "oops" to ch
    """

    ast = _parse(code)
    checker = TypeChecker()
    errors = checker.check_program(ast)

    assert any("Cannot send value of type" in err for err in errors)


def test_typechecker_enforces_channel_payload_inside_typed_function_param():
    code = """
    function push_bad with ch as Channel<Integer>
        send "oops" to ch
    end
    """

    ast = _parse(code)
    checker = TypeChecker()
    errors = checker.check_program(ast)

    assert any("Cannot send value of type" in err for err in errors)


def test_typechecker_flags_branch_payload_mismatch_for_same_channel():
    code = """
    set ch to create channel
    if true
        send 1 to ch
    else
        send "oops" to ch
    end
    """

    ast = _parse(code)
    checker = TypeChecker()
    errors = checker.check_program(ast)

    assert any("Cannot send value of type" in err for err in errors)


def test_typechecker_accepts_close_on_channel():
    code = """
    set ch to create channel
    close ch
    """

    ast = _parse(code)
    checker = TypeChecker()
    errors = checker.check_program(ast)

    assert errors == []


def test_typechecker_rejects_close_on_non_channel():
    code = """
    set x to 10
    close x
    """

    ast = _parse(code)
    checker = TypeChecker()
    errors = checker.check_program(ast)

    assert any("Close target must be a channel" in err for err in errors)
