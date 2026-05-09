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
        "NLPL_Optional_get_value_ptr" in c_code
        or "= (void*)(opt);" in c_code
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
        "NLPL_Result_get_value_ptr" in c_code
        or "= (void*)(res);" in c_code
    )
    assert (
        "NLPL_Result_get_error_ptr" in c_code
        or "= (void*)(res);" in c_code
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
    assert "NLPL_Optional_get_value_ptr" not in c_code
    assert "__match_bind_payload_" in c_code
    assert "= (void*)(opt);" in c_code


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
    assert "NLPL_Result_get_value_ptr" not in c_code
    assert "NLPL_Result_get_error_ptr" not in c_code
    assert '__match_bind_message_' in c_code
    assert "= (void*)(res);" in c_code
    assert "__match_bind_value_" in c_code


def test_c_option_typed_integer_binding_uses_fast_path():
    generator = CCodeGenerator(target="c")
    pattern = OptionPattern("Some", "payload", binding_type_annotation="Integer")

    generator._generate_pattern_bindings(pattern, "opt", "void*", None)

    c_code = "\n".join(generator.output_buffer)
    assert "NLPL_Optional_get_value_i64((void*)(opt))" in c_code
    assert "NLPL_Optional_get_value_kind((void*)(opt))" not in c_code
    assert generator.symbol_table["payload"] == "int"


def test_c_result_typed_float_err_binding_uses_fast_path():
    generator = CCodeGenerator(target="c")
    pattern = ResultPattern("Err", "error_payload", binding_type_annotation="Float")

    generator._generate_pattern_bindings(pattern, "res", "void*", None)

    c_code = "\n".join(generator.output_buffer)
    assert "NLPL_Result_get_error_f64((void*)(res))" in c_code
    assert "NLPL_Result_get_error_kind((void*)(res))" not in c_code
    assert generator.symbol_table["error_payload"] == "double"


def test_c_option_untyped_pointer_binding_uses_kind_dispatch_fallback():
    generator = CCodeGenerator(target="c")
    pattern = OptionPattern("Some", "payload")

    generator._generate_pattern_bindings(pattern, "opt", "void*", None)

    c_code = "\n".join(generator.output_buffer)
    assert "NLPL_Optional_get_value_kind((void*)(opt))" in c_code
    assert "NLPL_Optional_get_value_i64((void*)(opt))" in c_code
    assert "NLPL_Optional_get_value_f64((void*)(opt))" in c_code
    assert "NLPL_Optional_get_value_ptr((void*)(opt))" in c_code
    # Declarations must be zero-initialized (lifetime-safe hoisting) and must appear
    # in the output; they must NOT be inlined with a function call on the same line.
    lines = c_code.split("\n")
    i64_decl_lines = [l for l in lines if "int64_t __nxl_opt_i64_" in l]
    f64_decl_lines = [l for l in lines if "double __nxl_opt_f64_" in l]
    assert i64_decl_lines, "int64_t spill variable must be declared"
    assert f64_decl_lines, "double spill variable must be declared"
    for decl in i64_decl_lines:
        assert "NLPL_Optional_get_value_i64" not in decl, (
            f"int64_t declaration must be zero-init, not inlined with a call: {decl!r}"
        )
    for decl in f64_decl_lines:
        assert "NLPL_Optional_get_value_f64" not in decl, (
            f"double declaration must be zero-init, not inlined with a call: {decl!r}"
        )
    # The actual runtime calls must appear as bare assignments inside the if branches.
    assert any("= NLPL_Optional_get_value_i64" in l for l in lines), (
        "Assignment to i64 spill inside if-branch must be present"
    )
    assert any("= NLPL_Optional_get_value_f64" in l for l in lines), (
        "Assignment to f64 spill inside if-branch must be present"
    )
    assert generator.symbol_table["payload"] == "void*"


def test_c_result_untyped_pointer_err_binding_uses_kind_dispatch_fallback():
    generator = CCodeGenerator(target="c")
    pattern = ResultPattern("Err", "error_payload")

    generator._generate_pattern_bindings(pattern, "res", "void*", None)

    c_code = "\n".join(generator.output_buffer)
    assert "NLPL_Result_get_error_kind((void*)(res))" in c_code
    assert "NLPL_Result_get_error_i64((void*)(res))" in c_code
    assert "NLPL_Result_get_error_f64((void*)(res))" in c_code
    assert "NLPL_Result_get_error_ptr((void*)(res))" in c_code
    lines = c_code.split("\n")
    i64_decl_lines = [l for l in lines if "int64_t __nxl_res_err_i64_" in l]
    f64_decl_lines = [l for l in lines if "double __nxl_res_err_f64_" in l]
    assert i64_decl_lines, "int64_t err spill variable must be declared"
    assert f64_decl_lines, "double err spill variable must be declared"
    for decl in i64_decl_lines:
        assert "NLPL_Result_get_error_i64" not in decl, (
            f"int64_t declaration must be zero-init, not inlined with a call: {decl!r}"
        )
    for decl in f64_decl_lines:
        assert "NLPL_Result_get_error_f64" not in decl, (
            f"double declaration must be zero-init, not inlined with a call: {decl!r}"
        )
    assert any("= NLPL_Result_get_error_i64" in l for l in lines), (
        "Assignment to i64 err spill inside if-branch must be present"
    )
    assert any("= NLPL_Result_get_error_f64" in l for l in lines), (
        "Assignment to f64 err spill inside if-branch must be present"
    )
    assert generator.symbol_table["error_payload"] == "void*"


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


def test_c_match_expression_lowers_non_identifier_list_expression():
    ast = _parse(
        """
function main returns Integer
    match [2, 3] with
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

    assert "__nxl_match_value_" in c_code
    assert "[] = {2, 3}" in c_code
    assert "[0]" in c_code
    assert "[1]" in c_code