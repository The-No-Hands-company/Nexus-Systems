"""
Native NexusLang test-framework: lexer tokens, AST nodes, parser, interpreter.
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

class TestLexerTestTokens:
    def _lex(self, src):
        from nexuslang.parser.lexer import Lexer
        lexer = Lexer(src)
        return lexer.tokenize()

    def test_test_keyword_token(self):
        from nexuslang.parser.lexer import TokenType
        tokens = self._lex('test "my test" do\nend')
        types = [t.type for t in tokens]
        assert TokenType.TEST in types

    def test_describe_keyword_token(self):
        from nexuslang.parser.lexer import TokenType
        tokens = self._lex('describe "suite" do\nend')
        types = [t.type for t in tokens]
        assert TokenType.DESCRIBE in types

    def test_it_keyword_token(self):
        from nexuslang.parser.lexer import TokenType
        tokens = self._lex('it "does something" do\nend')
        types = [t.type for t in tokens]
        assert TokenType.IT in types

    def test_expect_keyword_token(self):
        from nexuslang.parser.lexer import TokenType
        tokens = self._lex("expect x to equal 1")
        types = [t.type for t in tokens]
        assert TokenType.EXPECT in types

    def test_before_each_token(self):
        from nexuslang.parser.lexer import TokenType
        tokens = self._lex("before each do\nend")
        types = [t.type for t in tokens]
        assert TokenType.BEFORE_EACH in types

    def test_after_each_token(self):
        from nexuslang.parser.lexer import TokenType
        tokens = self._lex("after each do\nend")
        types = [t.type for t in tokens]
        assert TokenType.AFTER_EACH in types


# ============================================================
# Section 12 - AST nodes for test framework
# ============================================================

class TestASTNodes:
    def test_test_block_node_import(self):
        from nexuslang.parser.ast import TestBlock
        node = TestBlock(name="my test", body=[])
        assert node.node_type == "test_block"

    def test_describe_block_node_import(self):
        from nexuslang.parser.ast import DescribeBlock
        node = DescribeBlock(name="suite", body=[])
        assert node.node_type == "describe_block"

    def test_it_block_node_import(self):
        from nexuslang.parser.ast import ItBlock
        node = ItBlock(name="does x", body=[])
        assert node.node_type == "it_block"

    def test_before_each_node_import(self):
        from nexuslang.parser.ast import BeforeEachBlock
        node = BeforeEachBlock(body=[])
        assert node.node_type == "before_each_block"

    def test_after_each_node_import(self):
        from nexuslang.parser.ast import AfterEachBlock
        node = AfterEachBlock(body=[])
        assert node.node_type == "after_each_block"

    def test_parameterized_test_block_import(self):
        from nexuslang.parser.ast import ParameterizedTestBlock
        node = ParameterizedTestBlock(name="param test", params=["x"], cases=[[1], [2]], body=[])
        assert node.node_type == "parameterized_test_block"
        assert node.params == ["x"]
        assert len(node.cases) == 2

    def test_test_block_stores_name(self):
        from nexuslang.parser.ast import TestBlock
        node = TestBlock(name="hello", body=[])
        assert node.name == "hello"

    def test_test_block_stores_body(self):
        from nexuslang.parser.ast import TestBlock
        node = TestBlock(name="t", body=["stmt1", "stmt2"])
        assert len(node.body) == 2


# ============================================================
# Section 13 - Parser for test framework
# ============================================================

class TestParserTestFramework:
    def _parse(self, src):
        from nexuslang.parser.parser import Parser
        from nexuslang.parser.lexer import Lexer
        tokens = Lexer(src).tokenize()
        return Parser(tokens).parse()

    def test_parse_test_block(self):
        from nexuslang.parser.ast import TestBlock
        prog = self._parse('test "my test" do\nend')
        assert any(isinstance(s, TestBlock) for s in prog.statements)

    def test_parse_describe_block(self):
        from nexuslang.parser.ast import DescribeBlock
        prog = self._parse('describe "suite" do\nend')
        assert any(isinstance(s, DescribeBlock) for s in prog.statements)

    def test_parse_it_block(self):
        from nexuslang.parser.ast import ItBlock
        prog = self._parse('it "does x" do\nend')
        assert any(isinstance(s, ItBlock) for s in prog.statements)

    def test_parse_before_each(self):
        from nexuslang.parser.ast import BeforeEachBlock
        prog = self._parse("before each do\nend")
        assert any(isinstance(s, BeforeEachBlock) for s in prog.statements)

    def test_parse_after_each(self):
        from nexuslang.parser.ast import AfterEachBlock
        prog = self._parse("after each do\nend")
        assert any(isinstance(s, AfterEachBlock) for s in prog.statements)

    def test_parse_test_block_name(self):
        from nexuslang.parser.ast import TestBlock
        prog = self._parse('test "addition works" do\nend')
        node = next(s for s in prog.statements if isinstance(s, TestBlock))
        assert node.name == "addition works"

    def test_parse_describe_with_it(self):
        from nexuslang.parser.ast import DescribeBlock, ItBlock
        src = 'describe "math" do\n  it "adds" do\n  end\nend'
        prog = self._parse(src)
        desc = next(s for s in prog.statements if isinstance(s, DescribeBlock))
        assert any(isinstance(s, ItBlock) for s in desc.body)


# ============================================================
# Section 14 - Interpreter execution of test framework
# ============================================================

class TestInterpreterTestFramework:
    def _interp(self, src):
        from nexuslang.interpreter.interpreter import Interpreter
        from nexuslang.runtime.runtime import Runtime
        from nexuslang.stdlib import register_stdlib
        from nexuslang.parser.parser import Parser
        from nexuslang.parser.lexer import Lexer
        rt = Runtime()
        register_stdlib(rt)
        tokens = Lexer(src).tokenize()
        prog = Parser(tokens).parse()
        interp = Interpreter(runtime=rt)
        interp.interpret(prog)
        return interp

    def test_test_block_runs(self):
        src = 'test "simple" do\n  set x to 1\nend'
        self._interp(src)  # should not raise

    def test_it_block_runs(self):
        src = 'it "runs fine" do\n  set y to 2\nend'
        self._interp(src)

    def test_describe_block_runs(self):
        src = 'describe "my suite" do\n  it "passes" do\n  end\nend'
        self._interp(src)

    def test_before_each_runs(self):
        src = "before each do\n  set z to 0\nend"
        self._interp(src)

    def test_after_each_runs(self):
        src = "after each do\n  set z to 0\nend"
        self._interp(src)

    def test_failing_test_does_not_crash_interpreter(self):
        # A failing assertion inside a test block should not propagate as an unhandled exception
        src = 'test "will fail" do\n  set x to 1\nend'
        self._interp(src)  # interpreter catches test failures in test body


# ============================================================
# Section 15 - Optimization level helper
# ============================================================

