"""Regression tests for typechecker edge cases."""

from nexuslang.parser.lexer import Lexer
from nexuslang.parser.parser import Parser
from nexuslang.typesystem.typechecker import TypeChecker


def _typecheck(source: str) -> list[str]:
    tokens = Lexer(source).tokenize()
    program = Parser(tokens, source).parse()
    checker = TypeChecker()
    checker.check_program(program)
    return checker.errors


def test_unary_minus_keyword_is_supported():
    """Natural-language unary minus should be accepted by typechecker."""
    errors = _typecheck("set y to 2\nset x to minus y\n")
    assert "Unsupported unary operator: minus" not in errors


def test_floor_divide_phrase_is_supported():
    """Natural-language floor division should be type-checked as a valid operator."""
    errors = _typecheck("set x to 9 integer divided by 2\n")
    assert "Unsupported binary operator: integer divided by" not in errors


def test_concatenate_operator_is_supported_for_strings():
    """String concatenation keyword should be accepted by typechecker."""
    errors = _typecheck('set x to "a" concatenate "b"\n')
    assert "Unsupported binary operator: concatenate" not in errors


def test_memory_allocation_statement_is_typechecked():
    """Memory allocation should no longer be rejected as unsupported."""
    errors = _typecheck("allocate a new Integer in memory and name it buffer\n")
    assert errors == []


def test_memory_allocation_type_mismatch_reports_error():
    """Allocated type should validate the optional initial value type."""
    errors = _typecheck('allocate a new Integer in memory with value "oops" and name it buffer\n')
    assert any("Cannot initialize allocated 'buffer'" in e for e in errors)


def test_memory_deallocation_existing_variable_is_allowed():
    """Freeing an existing identifier should not produce typechecker errors."""
    errors = _typecheck("set buffer to 1\nfree memory at buffer\n")
    assert errors == []


def test_memory_deallocation_undefined_identifier_reports_error():
    """Freeing unknown memory should produce an actionable diagnostic."""
    errors = _typecheck("free memory at missing_buffer\n")
    assert any("Cannot free undefined memory identifier 'missing_buffer'" in e for e in errors)


def test_arithmetic_type_error_includes_line_context():
    """Type mismatch diagnostics should include source line information."""
    errors = _typecheck("set t to true\nset x to t plus 1\n")
    assert any("Line 2:" in e and "Left operand of 'plus'" in e for e in errors)


def test_logical_type_error_includes_line_context():
    """Logical type mismatch diagnostics should include source line information."""
    errors = _typecheck("set x to 1 and 2\n")
    assert any("Line 1:" in e and "Left operand of 'and'" in e for e in errors)


def test_generator_function_requires_list_like_return_annotation():
    """Generator functions should reject non-list explicit return annotations."""
    errors = _typecheck(
        "function bad_gen returns Integer\n"
        "    yield 1\n"
        "end\n"
    )
    assert any("generator function" in e.lower() and "list-like return type" in e.lower() for e in errors)


def test_generator_function_rejects_return_value_statement():
    """Generator functions should not allow `return value`; yield should be used instead."""
    errors = _typecheck(
        "function bad_return returns List<Integer>\n"
        "    yield 1\n"
        "    return 2\n"
        "end\n"
    )
    assert any("generator function" in e.lower() and "return a value" in e.lower() for e in errors)


def test_generator_function_reports_incompatible_yield_types():
    """Mixed incompatible yield types should produce a dedicated generator diagnostic."""
    errors = _typecheck(
        "function mixed_gen\n"
        "    yield 1\n"
        "    yield \"oops\"\n"
        "end\n"
    )
    assert any("incompatible yield types" in e.lower() for e in errors)


def test_statement_contains_yield_handles_token_enum_cycles_without_recursion():
    """Yield scan should not recurse infinitely through Token/Enum metadata internals."""
    source = (
        "set tasks to []\n"
        "function mark_done with task_id as Integer\n"
        "    for each task in tasks\n"
        "        if task[\"id\"] is equal to task_id\n"
        "            set task[\"status\"] to \"done\"\n"
        "        end\n"
        "    end\n"
        "end\n"
    )

    tokens = Lexer(source).tokenize()
    program = Parser(tokens, source).parse()
    checker = TypeChecker()

    function_def = next(stmt for stmt in program.statements if type(stmt).__name__ == "FunctionDefinition")

    # This call previously raised RecursionError due to metadata cycles.
    assert checker._statement_contains_yield(function_def.body[0]) is False


def test_typechecker_accepts_function_heavy_business_flow_without_recursion_crash():
    """Function-heavy business logic should typecheck without recursion-depth failures."""
    source = (
        "set tasks to []\n"
        "function add_task with title as String and owner as String\n"
        "    set task to {\"id\": 1, \"title\": title, \"owner\": owner, \"status\": \"todo\"}\n"
        "    append task to tasks\n"
        "end\n"
        "function mark_done with task_id as Integer\n"
        "    for each task in tasks\n"
        "        if task[\"id\"] is equal to task_id\n"
        "            set task[\"status\"] to \"done\"\n"
        "        end\n"
        "    end\n"
        "end\n"
        "add_task with \"A\" and \"Ana\"\n"
        "mark_done with 1\n"
    )

    errors = _typecheck(source)
    assert isinstance(errors, list)
