"""Tests for allocator syntax and parallel for loop language features."""
import pytest
import sys
import os

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'src'))

from nexuslang.parser.lexer import Lexer, TokenType
from nexuslang.parser.parser import Parser
from nexuslang.parser.ast import AllocatorHint, ParallelForLoop


# ---------------------------------------------------------------------------
# Allocator syntax – lexer
# ---------------------------------------------------------------------------

class TestAllocatorLexer:
    def _tokenize(self, code: str):
        return Lexer(code).tokenize()

    def test_allocator_keyword_tokenizes(self):
        tokens = self._tokenize('allocator')
        types = [t.type for t in tokens]
        assert TokenType.ALLOCATOR in types

    def test_parallel_keyword_tokenizes(self):
        tokens = self._tokenize('parallel')
        types = [t.type for t in tokens]
        assert TokenType.PARALLEL in types

    def test_allocator_not_in_regular_text(self):
        # Using 'allocator' as part of a larger identifier should still work
        tokens = self._tokenize('set my_allocator to 5')
        # 'my_allocator' is an IDENTIFIER, not ALLOCATOR
        ident_tokens = [t for t in tokens if t.type == TokenType.IDENTIFIER]
        names = [t.value for t in ident_tokens]
        assert 'my_allocator' in names


# ---------------------------------------------------------------------------
# AllocatorHint AST node
# ---------------------------------------------------------------------------

class TestAllocatorHintNode:
    def test_node_type(self):
        node = AllocatorHint('List', 'arena', line_number=1)
        assert node.node_type == 'allocator_hint'

    def test_fields(self):
        node = AllocatorHint('List of Integer', 'pool_allocator', line_number=5)
        assert node.base_type == 'List of Integer'
        assert node.allocator_name == 'pool_allocator'
        assert node.line_number == 5


# ---------------------------------------------------------------------------
# AllocatorHint parsing
# ---------------------------------------------------------------------------

class TestAllocatorHintParsing:
    def _parse(self, code: str):
        tokens = Lexer(code).tokenize()
        parser = Parser(tokens, source=code)
        return parser.parse()

    def test_allocator_hint_in_variable_declaration(self):
        code = 'set items to [] as List with allocator arena'
        ast = self._parse(code)
        # Should not raise – parser handles allocator hint
        assert ast is not None

    def test_simple_list_without_allocator(self):
        code = 'set items to []'
        ast = self._parse(code)
        assert ast is not None


# ---------------------------------------------------------------------------
# ParallelForLoop AST node
# ---------------------------------------------------------------------------

class TestParallelForLoopNode:
    def test_node_type(self):
        node = ParallelForLoop('item', None, [], line_number=1)
        assert node.node_type == 'parallel_for_loop'

    def test_fields(self):
        node = ParallelForLoop('x', 'iterable_expr', ['body_stmt'], line_number=3)
        assert node.var_name == 'x'
        assert node.iterable == 'iterable_expr'
        assert node.body == ['body_stmt']
        assert node.line_number == 3


# ---------------------------------------------------------------------------
# Parallel for loop parsing
# ---------------------------------------------------------------------------

class TestParallelForLoopParsing:
    def _parse(self, code: str):
        tokens = Lexer(code).tokenize()
        parser = Parser(tokens, source=code)
        return parser.parse()

    def test_parallel_for_each_parses(self):
        code = '''\
set items to [1, 2, 3]
parallel for each item in items
    print text item
end'''
        ast = self._parse(code)
        assert ast is not None

    def test_parallel_for_body_executes(self):
        """Full integration: parse and execute a parallel for loop."""
        from nexuslang.interpreter.interpreter import Interpreter
        from nexuslang.runtime.runtime import Runtime
        from nexuslang.stdlib import register_stdlib

        code = '''\
set results to []
set items to [10, 20, 30]
parallel for each x in items
    print text x
end'''
        tokens = Lexer(code).tokenize()
        parser_obj = Parser(tokens, source=code)
        ast = parser_obj.parse()
        runtime = Runtime()
        register_stdlib(runtime)
        interp = Interpreter(runtime, enable_type_checking=False)
        # Should execute without error
        interp.interpret(ast)

    def test_parallel_for_empty_list(self):
        """Parallel for over empty list should not raise."""
        from nexuslang.interpreter.interpreter import Interpreter
        from nexuslang.runtime.runtime import Runtime
        from nexuslang.stdlib import register_stdlib

        code = '''\
set items to []
parallel for each x in items
    print text x
end'''
        tokens = Lexer(code).tokenize()
        parser_obj = Parser(tokens, source=code)
        ast = parser_obj.parse()
        runtime = Runtime()
        register_stdlib(runtime)
        interp = Interpreter(runtime, enable_type_checking=False)
        interp.interpret(ast)


# ---------------------------------------------------------------------------
# PARALLEL token
# ---------------------------------------------------------------------------

class TestParallelToken:
    def test_parallel_dispatches_to_parallel_for(self):
        """The parser dispatches 'parallel for each' to parse_parallel_for."""
        code = '''\
set numbers to [1, 2, 3]
parallel for each n in numbers
    print text n
end'''
        tokens = Lexer(code).tokenize()
        parser_obj = Parser(tokens, source=code)
        ast = parser_obj.parse()
        # Find the ParallelForLoop node in the AST
        parallel_nodes = [stmt for stmt in ast.statements
                          if isinstance(stmt, ParallelForLoop)]
        assert len(parallel_nodes) == 1, "Expected exactly one ParallelForLoop in AST"
        assert parallel_nodes[0].var_name == 'n'
