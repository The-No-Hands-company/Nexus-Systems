"""C backend coverage for basic pattern matching lowering."""

import os
import sys


sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', '..', 'src'))

from nexuslang.compiler.backends.c_generator import CCodeGenerator
from nexuslang.parser.ast import MatchExpression, OptionPattern, ResultPattern
from nexuslang.parser.lexer import Lexer
from nexuslang.parser.parser import Parser


def _parse(code: str):
    lexer = Lexer(code)
    tokens = lexer.scan_tokens()
    parser = Parser(tokens)
    return parser.parse()


def _find_first_match(ast):
    for stmt in ast.statements:
        body = getattr(stmt, "body", None) or []
        for body_stmt in body:
            if isinstance(body_stmt, MatchExpression):
                return body_stmt
    raise AssertionError("Expected at least one match expression in parsed AST")


def test_c_match_expression_lowers_integer_cases_and_wildcard():
    ast = _parse(
        """
function main returns Integer
    set x to 2
    match x with
        case 1
            print text "one"
        case 2
            print text "two"
        case _
            print text "other"
    end
    return 0
end
"""
    )

    c_code = CCodeGenerator(target="c").generate(ast)

    assert "bool __nxl_match_done_" in c_code
    assert "== 1" in c_code
    assert "== 2" in c_code
    assert "other" in c_code


def test_c_match_expression_lowers_string_literal_case_with_strcmp():
    ast = _parse(
        """
function main returns Integer
    set status to "ready"
    match status with
        case "ready"
            print text "go"
        case _
            print text "stop"
    end
    return 0
end
"""
    )

    c_code = CCodeGenerator(target="c").generate(ast)

    assert "strcmp(" in c_code
    assert "ready" in c_code


def test_c_match_expression_lowers_identifier_binding_and_guard():
    ast = _parse(
        """
function main returns Integer
    set x to 5
    match x with
        case n if n is greater than 3
            print text n
        case _
            print text 0
    end
    return 0
end
"""
    )

    c_code = CCodeGenerator(target="c").generate(ast)

    assert "__match_bind_n_" in c_code
    assert "> 3" in c_code
    assert "printf(\"%d\\n\"," in c_code


def test_c_match_expression_lowers_option_patterns_and_binding():
    ast = _parse(
        """
function main returns Integer
    set opt to 0
    match opt with
        case Some payload
            print text payload
        case None
            print text 0
    end
    return 0
end
"""
    )

    match_stmt = _find_first_match(ast)
    match_stmt.cases[0].pattern = OptionPattern("Some", "payload")
    match_stmt.cases[1].pattern = OptionPattern("None", None)

    c_code = CCodeGenerator(target="c").generate(ast)

    assert (
        "NLPL_Optional_has_value" in c_code
        or "((intptr_t)(opt)) != 0" in c_code
    )
    assert (
        "NLPL_Optional_get_value" in c_code
        or "= (intptr_t)(opt);" in c_code
    )
    assert "__match_bind_payload_" in c_code


def test_c_match_expression_lowers_result_patterns_and_binding():
    ast = _parse(
        """
function main returns Integer
    set res to 0
    match res with
        case Ok value
            print text value
        case Err message
            print text message
    end
    return 0
end
"""
    )

    match_stmt = _find_first_match(ast)
    match_stmt.cases[0].pattern = ResultPattern("Ok", "value")
    match_stmt.cases[1].pattern = ResultPattern("Err", "message")

    c_code = CCodeGenerator(target="c").generate(ast)

    assert (
        "NLPL_Result_is_ok" in c_code
        or "((intptr_t)(res)) >= 0" in c_code
    )
    assert (
        "NLPL_Result_get_value" in c_code
        or "= (intptr_t)(res);" in c_code
    )
    assert (
        "NLPL_Result_get_error" in c_code
        or '"error"' in c_code
    )
    assert "__match_bind_value_" in c_code
    assert "__match_bind_message_" in c_code


def test_c_match_expression_uses_scalar_option_fallback_without_runtime_helpers():
    ast = _parse(
        """
function main returns Integer
    set opt to 42
    match opt with
        case payload
            print text payload
        case _
            print text 0
    end
    return 0
end
"""
    )

    match_stmt = _find_first_match(ast)
    match_stmt.cases[0].pattern = OptionPattern("Some", "payload")
    match_stmt.cases[1].pattern = OptionPattern("None", None)

    c_code = CCodeGenerator(target="c").generate(ast)

    assert "((intptr_t)(opt)) != 0" in c_code
    assert "NLPL_Optional_has_value" not in c_code
    assert "NLPL_Optional_get_value" not in c_code
    assert "__match_bind_payload_" in c_code
    assert "= (intptr_t)(opt);" in c_code


def test_c_match_expression_uses_scalar_result_fallback_without_runtime_helpers():
    ast = _parse(
        """
function main returns Integer
    set res to -1
    match res with
        case e
            print text e
        case v
            print text v
    end
    return 0
end
"""
    )

    match_stmt = _find_first_match(ast)
    match_stmt.cases[0].pattern = ResultPattern("Err", "message")
    match_stmt.cases[1].pattern = ResultPattern("Ok", "value")

    c_code = CCodeGenerator(target="c").generate(ast)

    assert "((intptr_t)(res)) >= 0" in c_code
    assert "NLPL_Result_is_ok" not in c_code
    assert "NLPL_Result_get_value" not in c_code
    assert "NLPL_Result_get_error" not in c_code
    assert '__match_bind_message_' in c_code
    assert '"error"' in c_code
    assert "__match_bind_value_" in c_code


def test_c_match_expression_lowers_variant_patterns_and_binding():
    ast = _parse(
        """
function main returns Integer
    set res to 0
    match res with
        case Ok v
            print text v
        case Err e
            print text e
    end
    return 0
end
"""
    )

    c_code = CCodeGenerator(target="c").generate(ast)

    assert (
        "NLPL_Result_is_ok" in c_code
        or "((intptr_t)(res)) >= 0" in c_code
    )
    assert "__match_bind_v_" in c_code
    assert "__match_bind_e_" in c_code


def test_c_match_expression_lowers_tuple_pattern_bindings_for_arrays():
    ast = _parse(
        """
function main returns Integer
    set pair to [2, 3]
    match pair with
        case (x, y)
            print text x
            print text y
        case _
            print text 0
    end
    return 0
end
"""
    )

    c_code = CCodeGenerator(target="c").generate(ast)

    assert "pair[0]" in c_code
    assert "pair[1]" in c_code
    assert "__match_bind_x_" in c_code
    assert "__match_bind_y_" in c_code


def test_c_match_expression_lowers_list_pattern_with_rest_binding():
    ast = _parse(
        """
function main returns Integer
    set nums to [1, 2, 3, 4]
    match nums with
        case [head, ...tail]
            print text head
        case _
            print text 0
    end
    return 0
end
"""
    )

    c_code = CCodeGenerator(target="c").generate(ast)

    assert "nums[0]" in c_code
    assert "__match_bind_head_" in c_code
    assert "__match_bind_tail_" in c_code