"""TypeChecker coverage tests for parallel for loops."""

import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', '..', 'src'))

from nexuslang.parser.lexer import Lexer
from nexuslang.parser.parser import Parser
from nexuslang.typesystem.typechecker import TypeChecker


def _parse(code: str):
    lexer = Lexer(code)
    tokens = lexer.scan_tokens()
    parser = Parser(tokens)
    return parser.parse()


def test_typechecker_accepts_parallel_for_over_list():
    code = """
    set items to [1, 2, 3]
    parallel for each x in items
        set y to x plus 1
    end
    """

    ast = _parse(code)
    checker = TypeChecker()
    errors = checker.check_program(ast)

    assert errors == []


def test_typechecker_rejects_parallel_for_over_non_list():
    code = """
    set items to 42
    parallel for each x in items
        set y to x
    end
    """

    ast = _parse(code)
    checker = TypeChecker()
    errors = checker.check_program(ast)

    assert any("Parallel for iterable must be a list" in err for err in errors)


def test_typechecker_flags_parallel_for_outer_variable_write():
    code = """
    set items to [1, 2, 3]
    set total to 0
    parallel for each x in items
        set total to total plus x
    end
    """

    ast = _parse(code)
    checker = TypeChecker()
    errors = checker.check_program(ast)

    assert any("loop-carried dependency" in err for err in errors)


def test_typechecker_flags_parallel_for_reduction_variable_update():
    code = """
    set items to [1, 2, 3]
    set total to 0
    parallel for each x in items
        set total to total plus x
    end
    """

    ast = _parse(code)
    checker = TypeChecker()
    errors = checker.check_program(ast)

    assert any("reduction variable" in err for err in errors)
    assert any("parallel_reduce" in err for err in errors)


def test_typechecker_flags_parallel_for_outer_member_mutation():
    code = """
    set items to [1, 2, 3]
    set state to create dictionary
    parallel for each x in items
        set state.count to x
    end
    """

    ast = _parse(code)
    checker = TypeChecker()
    errors = checker.check_program(ast)

    assert any("mutates outer object 'state'" in err for err in errors)


def test_typechecker_flags_parallel_for_outer_index_mutation():
    code = """
    set items to [1, 2, 3]
    set shared to [0, 0, 0]
    parallel for each x in items
        set shared[x] to x
    end
    """

    ast = _parse(code)
    checker = TypeChecker()
    errors = checker.check_program(ast)

    assert any("mutates outer collection 'shared'" in err for err in errors)


def test_typechecker_allows_parallel_for_loop_variable_shadowing_outer_name():
    code = """
    set x to 100
    set items to [1, 2, 3]
    parallel for each x in items
        set x to x plus 1
    end
    """

    ast = _parse(code)
    checker = TypeChecker()
    errors = checker.check_program(ast)

    assert not any("loop-carried dependency" in err for err in errors)
