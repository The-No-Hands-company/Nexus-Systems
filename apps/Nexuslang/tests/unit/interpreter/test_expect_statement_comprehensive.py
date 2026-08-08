"""Comprehensive interpreter coverage for execute_expect_statement."""

import pytest

from nexuslang.interpreter.interpreter import Interpreter
from nexuslang.parser.ast import ExpectStatement, Literal
from nexuslang.parser.lexer import Lexer
from nexuslang.parser.parser import Parser
from nexuslang.runtime.runtime import Runtime


def run_program(source: str):
    """Parse and execute source text using the interpreter pipeline."""
    runtime = Runtime()
    interpreter = Interpreter(runtime)
    lexer = Lexer(source)
    tokens = lexer.tokenize()
    parser = Parser(tokens, source=source)
    ast = parser.parse()
    result = interpreter.interpret(ast)
    return interpreter, result


class TestExpectComparisons:
    def test_equal_passes(self):
        run_program("expect 2 plus 2 to equal 4")

    def test_equal_failure_raises_assertion_error(self):
        with pytest.raises(AssertionError, match="to equal"):
            run_program("expect 2 plus 2 to equal 5")

    def test_not_equal_passes(self):
        run_program("expect 7 to not equal 3")

    def test_not_equal_failure_raises_assertion_error(self):
        with pytest.raises(AssertionError, match="not to equal"):
            run_program("expect 7 to not equal 7")

    def test_relational_matchers_pass(self):
        source = """
        expect 10 to be greater than 3
        expect 2 to be less than 5
        expect 5 to be greater than or equal to 5
        expect 5 to be less than or equal to 5
        """
        run_program(source)


class TestExpectUnaryAndType:
    def test_unary_matchers_pass(self):
        source = """
        expect true to be true
        expect false to be false
        expect null to be null
        expect [] to be empty
        """
        run_program(source)

    def test_be_empty_failure_raises_assertion_error(self):
        with pytest.raises(AssertionError, match="to be empty"):
            run_program("expect [1, 2] to be empty")

    def test_be_of_type_passes_with_builtin_names(self):
        source = """
        expect 42 to be of type "integer"
        expect "hello" to be of type "string"
        expect [1, 2] to be of type "list"
        """
        run_program(source)

    def test_be_of_type_failure_raises_assertion_error(self):
        with pytest.raises(AssertionError, match="type"):
            run_program("expect 42 to be of type \"string\"")


class TestExpectApproxAndCollectionMatchers:
    def test_approximately_equal_default_tolerance_passes(self):
        run_program("expect 1.0000000001 to be approximately equal to 1.0")

    def test_approximately_equal_within_passes(self):
        run_program("expect 3.14 to be approximately equal to 3.0 within 0.2")

    def test_approximately_equal_failure_raises_assertion_error(self):
        with pytest.raises(AssertionError, match="approximately equal"):
            run_program("expect 3.14 to be approximately equal to 3.0 within 0.01")

    def test_contain_and_length_matchers_pass(self):
        source = """
        expect "hello" to contain "ell"
        expect [1, 2, 3] to contain 2
        expect "abc" to have length 3
        """
        run_program(source)

    def test_start_with_and_end_with_matchers_pass(self):
        source = """
        expect "nexus" to start with "ne"
        expect "nexus" to end with "us"
        expect [1, 2, 3] to start with 1
        expect [1, 2, 3] to end with 3
        """
        run_program(source)


class TestExpectRaiseError:
    def test_raise_error_matcher_passes(self):
        source = """
        function boom returns Integer
            raise error with message "boom"
            return 0
        end

        expect boom() to raise error
        """
        run_program(source)

    def test_not_raise_error_matcher_passes(self):
        source = """
        function safe returns Integer
            return 7
        end

        expect safe() to not raise error
        """
        run_program(source)

    def test_raise_error_failure_when_nothing_raised(self):
        source = """
        function safe returns Integer
            return 7
        end

        expect safe() to raise error
        """
        with pytest.raises(AssertionError, match="to raise an error"):
            run_program(source)


class TestExpectInternalEdgeCases:
    def test_unknown_matcher_raises_runtime_error(self):
        runtime = Runtime()
        interpreter = Interpreter(runtime)
        node = ExpectStatement(
            actual_expr=Literal("integer", 1),
            matcher="unknown_matcher",
            expected_expr=None,
            negated=False,
            tolerance_expr=None,
        )

        with pytest.raises(RuntimeError, match="Unknown expect matcher"):
            interpreter.execute_expect_statement(node)
