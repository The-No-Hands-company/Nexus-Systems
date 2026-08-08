"""
Comprehensive interpreter tests for try/catch execution paths.

Targets `execute_try_catch` behaviors:
- Successful try path with no catch execution
- Runtime errors handled by catch blocks
- Exception variable/property binding
- Typed catch matching and mismatch behavior
- Re-raise behavior
- Legacy "try to ... but if it fails" syntax
"""

import pytest

from nexuslang.interpreter.interpreter import Interpreter
from nexuslang.interpreter.interpreter import NLPLUserException
from nexuslang.parser.lexer import Lexer
from nexuslang.parser.parser import Parser
from nexuslang.runtime.runtime import Runtime


def run_program(source: str) -> Interpreter:
    """Parse and execute source, returning the interpreter for state assertions."""
    runtime = Runtime()
    interpreter = Interpreter(runtime)
    lexer = Lexer(source)
    tokens = lexer.tokenize()
    parser = Parser(tokens, source=source)
    ast = parser.parse()
    interpreter.interpret(ast)
    return interpreter


class TestTryCatchComprehensive:
    """Coverage-driven tests for interpreter try/catch behavior."""

    def test_try_catch_no_error_skips_catch_block(self):
        source = """
        set result to "none"
        try
            set result to "success"
        catch error
            set result to "failed"
        end
        """

        interpreter = run_program(source)
        assert interpreter.get_variable("result") == "success"

    def test_runtime_error_is_caught_and_named_variable_is_bound(self):
        source = """
        set handled to false
        set err_text to ""

        try
            set x to 10 divided by 0
        catch err
            set handled to true
            set err_text to err
        end
        """

        interpreter = run_program(source)
        assert interpreter.get_variable("handled") is True
        assert isinstance(interpreter.get_variable("err_text"), str)
        assert interpreter.get_variable("err_text") != ""

    def test_catch_with_message_and_type_properties_binds_fields(self):
        source = """
        set got_message to ""
        set got_type to ""

        try
            set y to unknown_name
        catch err with message, type
            set got_message to message
            set got_type to type
        end
        """

        interpreter = run_program(source)
        assert isinstance(interpreter.get_variable("got_message"), str)
        assert interpreter.get_variable("got_message") != ""
        assert isinstance(interpreter.get_variable("got_type"), str)
        assert interpreter.get_variable("got_type") != ""

    def test_typed_catch_matches_user_exception_type(self):
        source = """
        set matched to false

        try
            raise PaymentError with message "declined"
        catch err as PaymentError
            set matched to true
        end
        """

        interpreter = run_program(source)
        assert interpreter.get_variable("matched") is True

    def test_typed_catch_mismatch_re_raises_exception(self):
        source = """
        try
            raise PaymentError with message "declined"
        catch err as NetworkError
            set should_not_run to true
        end
        """

        with pytest.raises(NLPLUserException):
            run_program(source)

    def test_reraise_uses_last_exception_and_outer_try_catches(self):
        source = """
        set handled_outer to false
        set captured to ""

        try
            try
                raise InnerError with message "inner boom"
            catch err
                raise error
            end
        catch outer with message
            set handled_outer to true
            set captured to message
        end
        """

        interpreter = run_program(source)
        assert interpreter.get_variable("handled_outer") is True
        assert "inner boom" in interpreter.get_variable("captured")

    def test_catch_assignments_update_outer_scope(self):
        source = """
        set status to "before"

        try
            raise ValidationError with message "bad input"
        catch err
            set status to "handled"
        end
        """

        interpreter = run_program(source)
        assert interpreter.get_variable("status") == "handled"

    def test_raise_error_without_active_exception_fails(self):
        source = """
        raise error
        """

        with pytest.raises(Exception):
            run_program(source)

    def test_try_to_but_if_it_fails_syntax_executes_catch_block(self):
        source = """
        set handled to false

        try to set z to 1 divided by 0 but if it fails,
            set handled to true
        end
        """

        interpreter = run_program(source)
        assert interpreter.get_variable("handled") is True

    def test_return_inside_try_propagates_control_flow(self):
        source = """
        function compute returns Integer
            try
                return 7
            catch error
                return 0
            end
        end

        set result to compute()
        """

        interpreter = run_program(source)
        assert interpreter.get_variable("result") == 7
