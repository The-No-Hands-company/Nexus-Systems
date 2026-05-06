"""
Assertion library tests: lexer, AST, parser, and interpreter integration.
Split from test_session_features.py.
"""

import sys
import os
import tempfile
import pytest
from pathlib import Path

_SRC = str(Path(__file__).resolve().parent.parent.parent.parent / "src")
if _SRC not in sys.path:
    sys.path.insert(0, _SRC)

class TestAssertionLibraryLexer:
    def _lex(self, src):
        from nexuslang.parser.lexer import Lexer
        return Lexer(src).tokenize()

    def test_require_keyword_token(self):
        from nexuslang.parser.lexer import TokenType
        toks = self._lex("require x")
        assert TokenType.REQUIRE in [t.type for t in toks]

    def test_ensure_keyword_token(self):
        from nexuslang.parser.lexer import TokenType
        toks = self._lex("ensure x")
        assert TokenType.ENSURE in [t.type for t in toks]

    def test_guarantee_keyword_token(self):
        from nexuslang.parser.lexer import TokenType
        toks = self._lex("guarantee x")
        assert TokenType.GUARANTEE in [t.type for t in toks]

    def test_expect_token_with_equal(self):
        from nexuslang.parser.lexer import TokenType
        toks = self._lex("expect x to equal 5")
        types = [t.type for t in toks]
        assert TokenType.EXPECT in types
        assert TokenType.TO in types

    def test_expect_token_with_not(self):
        from nexuslang.parser.lexer import TokenType
        toks = self._lex("expect x to not equal 5")
        types = [t.type for t in toks]
        assert TokenType.NOT in types

class TestAssertionLibraryAST:
    def test_expect_statement_import(self):
        from nexuslang.parser.ast import ExpectStatement
        node = ExpectStatement(actual_expr=None, matcher="equal")
        assert node.node_type == "expect_statement"

    def test_expect_statement_matcher(self):
        from nexuslang.parser.ast import ExpectStatement
        node = ExpectStatement(actual_expr=None, matcher="greater_than")
        assert node.matcher == "greater_than"

    def test_expect_statement_negated_default_false(self):
        from nexuslang.parser.ast import ExpectStatement
        node = ExpectStatement(actual_expr=None, matcher="equal")
        assert node.negated is False

    def test_expect_statement_negated_true(self):
        from nexuslang.parser.ast import ExpectStatement
        node = ExpectStatement(actual_expr=None, matcher="equal", negated=True)
        assert node.negated is True

    def test_require_statement_import(self):
        from nexuslang.parser.ast import RequireStatement
        node = RequireStatement(condition=None)
        assert node.node_type == "require_statement"

    def test_ensure_statement_import(self):
        from nexuslang.parser.ast import EnsureStatement
        node = EnsureStatement(condition=None)
        assert node.node_type == "ensure_statement"

    def test_guarantee_statement_import(self):
        from nexuslang.parser.ast import GuaranteeStatement
        node = GuaranteeStatement(condition=None)
        assert node.node_type == "guarantee_statement"

class TestAssertionLibraryParser:
    def _parse(self, src):
        from nexuslang.parser.parser import Parser
        from nexuslang.parser.lexer import Lexer
        return Parser(Lexer(src).tokenize()).parse()

    def test_parse_expect_equal(self):
        from nexuslang.parser.ast import ExpectStatement
        prog = self._parse("set x to 5\nexpect x to equal 5")
        assert any(isinstance(s, ExpectStatement) for s in prog.statements)

    def test_parse_expect_equal_matcher(self):
        from nexuslang.parser.ast import ExpectStatement
        prog = self._parse("set x to 5\nexpect x to equal 5")
        node = next(s for s in prog.statements if isinstance(s, ExpectStatement))
        assert node.matcher == "equal"

    def test_parse_expect_not_equal(self):
        from nexuslang.parser.ast import ExpectStatement
        prog = self._parse("set x to 5\nexpect x to not equal 3")
        node = next(s for s in prog.statements if isinstance(s, ExpectStatement))
        assert node.negated is True

    def test_parse_expect_greater_than(self):
        from nexuslang.parser.ast import ExpectStatement
        prog = self._parse("set x to 5\nexpect x to be greater than 3")
        node = next(s for s in prog.statements if isinstance(s, ExpectStatement))
        assert node.matcher == "greater_than"

    def test_parse_expect_less_than(self):
        from nexuslang.parser.ast import ExpectStatement
        prog = self._parse("set x to 5\nexpect x to be less than 10")
        node = next(s for s in prog.statements if isinstance(s, ExpectStatement))
        assert node.matcher == "less_than"

    def test_parse_expect_greater_than_or_equal_to(self):
        from nexuslang.parser.ast import ExpectStatement
        prog = self._parse("set x to 5\nexpect x to be greater than or equal to 5")
        node = next(s for s in prog.statements if isinstance(s, ExpectStatement))
        assert node.matcher == "greater_than_or_equal_to"

    def test_parse_expect_less_than_or_equal_to(self):
        from nexuslang.parser.ast import ExpectStatement
        prog = self._parse("set x to 5\nexpect x to be less than or equal to 5")
        node = next(s for s in prog.statements if isinstance(s, ExpectStatement))
        assert node.matcher == "less_than_or_equal_to"

    def test_parse_expect_contain(self):
        from nexuslang.parser.ast import ExpectStatement
        prog = self._parse('set s to "hello"\nexpect s to contain "ell"')
        node = next(s for s in prog.statements if isinstance(s, ExpectStatement))
        assert node.matcher == "contain"

    def test_parse_expect_be_true(self):
        from nexuslang.parser.ast import ExpectStatement
        prog = self._parse("set flag to true\nexpect flag to be true")
        node = next(s for s in prog.statements if isinstance(s, ExpectStatement))
        assert node.matcher == "be_true"

    def test_parse_expect_be_false(self):
        from nexuslang.parser.ast import ExpectStatement
        prog = self._parse("set flag to false\nexpect flag to be false")
        node = next(s for s in prog.statements if isinstance(s, ExpectStatement))
        assert node.matcher == "be_false"

    def test_parse_expect_be_null(self):
        from nexuslang.parser.ast import ExpectStatement
        prog = self._parse("set v to null\nexpect v to be null")
        node = next(s for s in prog.statements if isinstance(s, ExpectStatement))
        assert node.matcher == "be_null"

    def test_parse_require(self):
        from nexuslang.parser.ast import RequireStatement
        prog = self._parse("require 1 equals 1")
        assert any(isinstance(s, RequireStatement) for s in prog.statements)

    def test_parse_ensure(self):
        from nexuslang.parser.ast import EnsureStatement
        prog = self._parse("ensure 1 equals 1")
        assert any(isinstance(s, EnsureStatement) for s in prog.statements)

    def test_parse_guarantee(self):
        from nexuslang.parser.ast import GuaranteeStatement
        prog = self._parse("guarantee 1 equals 1")
        assert any(isinstance(s, GuaranteeStatement) for s in prog.statements)

class TestAssertionLibraryInterpreter:
    def _interp(self, src):
        from nexuslang.interpreter.interpreter import Interpreter
        from nexuslang.runtime.runtime import Runtime
        from nexuslang.stdlib import register_stdlib
        from nexuslang.parser.parser import Parser
        from nexuslang.parser.lexer import Lexer
        rt = Runtime()
        register_stdlib(rt)
        prog = Parser(Lexer(src).tokenize()).parse()
        i = Interpreter(runtime=rt)
        i.interpret(prog)
        return i

    def test_expect_equal_passes(self):
        self._interp("set x to 5\nexpect x to equal 5")

    def test_expect_equal_fails(self):
        with pytest.raises(AssertionError):
            self._interp("set x to 1\nexpect x to equal 2")

    def test_expect_not_equal_passes(self):
        self._interp("set x to 1\nexpect x to not equal 2")

    def test_expect_not_equal_fails(self):
        with pytest.raises(AssertionError):
            self._interp("set x to 5\nexpect x to not equal 5")

    def test_expect_greater_than_passes(self):
        self._interp("set x to 5\nexpect x to be greater than 3")

    def test_expect_greater_than_fails(self):
        with pytest.raises(AssertionError):
            self._interp("set x to 2\nexpect x to be greater than 5")

    def test_expect_less_than_passes(self):
        self._interp("set x to 3\nexpect x to be less than 10")

    def test_expect_less_than_fails(self):
        with pytest.raises(AssertionError):
            self._interp("set x to 10\nexpect x to be less than 3")

    def test_expect_contain_passes(self):
        self._interp('set s to "hello world"\nexpect s to contain "world"')

    def test_expect_contain_fails(self):
        with pytest.raises(AssertionError):
            self._interp('set s to "hello"\nexpect s to contain "xyz"')

    def test_expect_be_true_passes(self):
        self._interp("set x to 1\nexpect x to be true")

    def test_expect_be_false_passes(self):
        self._interp("set x to 0\nexpect x to be false")

    def test_expect_be_null_passes(self):
        self._interp("set v to null\nexpect v to be null")

    def test_expect_not_be_null_passes(self):
        self._interp("set v to 42\nexpect v to not be null")

    def test_expect_failure_in_test_block_does_not_propagate(self):
        # Inside a test block, expect failures are recorded, not propagated
        self._interp('test "will fail" do\n  expect 1 to equal 2\nend')


# ============================================================
# Section 17 - Contract programming (require / ensure / guarantee)
# ============================================================

# ============================================================
# New assertion matchers: be_empty, have_length, start_with,
# end_with, be_of_type, raise_error
# ============================================================


class TestNewAssertionMatchers:
    """
    Tests for the expanded assertion library added to the interpreter.
    Covers parser integration AND interpreter execution.
    """

    def _interp(self, src):
        from nexuslang.interpreter.interpreter import Interpreter
        from nexuslang.runtime.runtime import Runtime
        from nexuslang.stdlib import register_stdlib
        from nexuslang.parser.parser import Parser
        from nexuslang.parser.lexer import Lexer
        rt = Runtime()
        register_stdlib(rt)
        prog = Parser(Lexer(src).tokenize()).parse()
        i = Interpreter(runtime=rt)
        i.interpret(prog)
        return i

    def _parse_matcher(self, src):
        from nexuslang.parser.ast import ExpectStatement
        from nexuslang.parser.parser import Parser
        from nexuslang.parser.lexer import Lexer
        prog = Parser(Lexer(src).tokenize()).parse()
        return next(s for s in prog.statements if isinstance(s, ExpectStatement))

    # -- be_empty -----------------------------------------------------------

    def test_parse_be_empty(self):
        node = self._parse_matcher("set x to []\nexpect x to be empty")
        assert node.matcher == "be_empty"

    def test_be_empty_passes_on_empty_list(self):
        self._interp("set x to []\nexpect x to be empty")

    def test_be_empty_passes_on_empty_string(self):
        self._interp('set s to ""\nexpect s to be empty')

    def test_be_empty_fails_on_non_empty_list(self):
        with pytest.raises(AssertionError):
            self._interp("set x to [1, 2]\nexpect x to be empty")

    def test_be_empty_fails_on_non_empty_string(self):
        with pytest.raises(AssertionError):
            self._interp('set s to "hello"\nexpect s to be empty')

    def test_not_be_empty_passes_on_non_empty_string(self):
        self._interp('set s to "hi"\nexpect s to not be empty')

    # -- have_length --------------------------------------------------------

    def test_parse_have_length(self):
        node = self._parse_matcher("set x to \"abc\"\nexpect x to have length 3")
        assert node.matcher == "have_length"

    def test_have_length_passes_on_string(self):
        self._interp('set s to "hello"\nexpect s to have length 5')

    def test_have_length_fails_wrong_count(self):
        with pytest.raises(AssertionError):
            self._interp('set s to "hello"\nexpect s to have length 3')

    def test_have_size_is_alias(self):
        node = self._parse_matcher("set x to \"abc\"\nexpect x to have size 3")
        assert node.matcher == "have_length"

    def test_have_length_negated(self):
        self._interp('set s to "hello"\nexpect s to not have length 99')

    # -- start_with ---------------------------------------------------------

    def test_parse_start_with(self):
        node = self._parse_matcher('set s to "hello"\nexpect s to start with "hel"')
        assert node.matcher == "start_with"

    def test_start_with_passes_string(self):
        self._interp('set s to "hello world"\nexpect s to start with "hello"')

    def test_start_with_fails_string(self):
        with pytest.raises(AssertionError):
            self._interp('set s to "hello world"\nexpect s to start with "world"')

    def test_start_with_negated(self):
        self._interp('set s to "hello"\nexpect s to not start with "world"')

    # -- end_with -----------------------------------------------------------

    def test_parse_end_with(self):
        node = self._parse_matcher('set s to "hello"\nexpect s to end with "lo"')
        assert node.matcher == "end_with"

    def test_end_with_passes_string(self):
        self._interp('set s to "hello world"\nexpect s to end with "world"')

    def test_end_with_fails_string(self):
        with pytest.raises(AssertionError):
            self._interp('set s to "hello world"\nexpect s to end with "hello"')

    def test_end_with_negated(self):
        self._interp('set s to "hello"\nexpect s to not end with "xyz"')

    # -- be_of_type ---------------------------------------------------------

    def test_parse_be_of_type(self):
        node = self._parse_matcher('set x to 5\nexpect x to be of type "Integer"')
        assert node.matcher == "be_of_type"

    def test_be_of_type_integer_passes(self):
        self._interp('set x to 42\nexpect x to be of type "int"')

    def test_be_of_type_string_passes(self):
        self._interp('set s to "hello"\nexpect s to be of type "string"')

    def test_be_of_type_fails_on_wrong_type(self):
        with pytest.raises(AssertionError):
            self._interp('set x to 42\nexpect x to be of type "string"')

    def test_be_of_type_negated(self):
        self._interp('set x to 42\nexpect x to not be of type "string"')

    def test_parse_be_a_type(self):
        node = self._parse_matcher('set x to 5\nexpect x to be a "int"')
        assert node.matcher == "be_of_type"

    # -- raise_error --------------------------------------------------------

    def test_parse_raise_error(self):
        node = self._parse_matcher(
            "function boom returns Integer\n  stop now\n  return 0\nend\n"
            "expect boom() to raise error"
        )
        assert node.matcher == "raise_error"

    def test_raise_error_passes_when_exception_raised(self):
        # Access undefined variable inside a function — guaranteed NxlNameError
        self._interp(
            "function boom returns Integer\n"
            "  set x to 1\n"
            "  set y to 0\n"
            "  return x divided by y\n"
            "end\n"
            "expect boom() to raise error"
        )

    def test_raise_error_passes_on_not_raise_when_no_exception(self):
        self._interp(
            "function safe returns Integer\n"
            "  return 42\n"
            "end\n"
            "expect safe() to not raise error"
        )

    def test_raise_error_fails_when_no_exception_raised(self):
        with pytest.raises(AssertionError):
            self._interp(
                "function safe returns Integer\n"
                "  return 42\n"
                "end\n"
                "expect safe() to raise error"
            )
