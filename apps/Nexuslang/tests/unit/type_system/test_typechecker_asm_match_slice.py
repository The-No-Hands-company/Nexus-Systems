"""
Coverage slice: typechecker.py lines 880-1020
Targets: _check_inline_assembly_statement, _normalize_asm_token,
         _is_valid_asm_constraint, _is_valid_asm_clobber,
         _check_match_expression_statement, _bind_pattern_types,
         _unify_result_types
"""
import pytest
from nexuslang.parser.ast import Identifier, Literal, ReturnStatement, IdentifierPattern
from nexuslang.typesystem.typechecker import TypeChecker, TypeEnvironment, TypeCheckError
from nexuslang.typesystem.types import (
    ANY_TYPE, BOOLEAN_TYPE, INTEGER_TYPE, FLOAT_TYPE, STRING_TYPE,
    PrimitiveType,
)


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _checker() -> TypeChecker:
    return TypeChecker()


def _env() -> TypeEnvironment:
    return TypeEnvironment()


def _asm_node(inputs=None, outputs=None, clobbers=None, line_number=1):
    """Build a minimal InlineAssembly mock node."""
    return type("InlineAssembly", (), {
        "inputs": inputs if inputs is not None else [],
        "outputs": outputs if outputs is not None else [],
        "clobbers": clobbers if clobbers is not None else [],
        "line_number": line_number,
    })()


def _match_node(expression, cases):
    """Build a minimal MatchExpression mock node."""
    return type("MatchExpression", (), {
        "expression": expression,
        "cases": cases,
    })()


def _case_node(pattern, body=None, guard=None, line_number=1):
    return type("MatchCase", (), {
        "pattern": pattern,
        "body": body if body is not None else [],
        "guard": guard,
        "line_number": line_number,
    })()


def _pattern_name(name):
    return type("Pattern", (), {"name": name})()


def _pattern_binding(binding):
    return type("Pattern", (), {"binding": binding, "bindings": None})()


def _pattern_bindings(bindings):
    return type("Pattern", (), {"bindings": bindings})()


def _pattern_empty():
    return type("Pattern", (), {})()


def _lit(value=0):
    return Literal("integer", value)


def _bool_lit():
    return Literal("boolean", True)


# ---------------------------------------------------------------------------
# _check_inline_assembly_statement – non-InlineAssembly node returns False
# ---------------------------------------------------------------------------

class TestCheckInlineAssemblyDispatch:
    def test_non_asm_node_returns_false_none(self):
        tc = _checker()
        node = type("OtherNode", (), {})()
        handled, typ = tc._check_inline_assembly_statement(node, _env())
        assert handled is False
        assert typ is None

    def test_empty_asm_node_returns_true_integer(self):
        tc = _checker()
        node = _asm_node()
        handled, typ = tc._check_inline_assembly_statement(node, _env())
        assert handled is True
        assert typ == INTEGER_TYPE

    def test_valid_input_constraint_r_no_errors(self):
        tc = _checker()
        node = _asm_node(inputs=[("r", _lit(5))])
        tc._check_inline_assembly_statement(node, _env())
        assert not any("input constraint" in e for e in tc.errors)

    def test_valid_quoted_input_constraint_no_errors(self):
        tc = _checker()
        node = _asm_node(inputs=['"r"', _lit(5)])
        # quoted constraint passed as tuple
        node.inputs = [('"r"', _lit(5))]
        tc._check_inline_assembly_statement(node, _env())
        assert not any("Invalid inline assembly input constraint" in e for e in tc.errors)

    def test_empty_input_constraint_appends_error(self):
        tc = _checker()
        node = _asm_node(inputs=[("", _lit(0))])
        tc._check_inline_assembly_statement(node, _env())
        assert any("input constraint" in e for e in tc.errors)

    def test_whitespace_only_input_constraint_appends_error(self):
        tc = _checker()
        node = _asm_node(inputs=[("  ", _lit(0))])
        tc._check_inline_assembly_statement(node, _env())
        assert any("input constraint" in e for e in tc.errors)

    def test_valid_output_constraint_with_identifier_no_error(self):
        tc = _checker()
        env = _env()
        env.define_variable("out_var", INTEGER_TYPE)
        target = Identifier("out_var")
        node = _asm_node(outputs=[("=r", target)])
        tc._check_inline_assembly_statement(node, env)
        assert not any("output operand" in e for e in tc.errors)
        assert not any("Undefined output" in e for e in tc.errors)

    def test_output_constraint_non_identifier_appends_error(self):
        tc = _checker()
        node = _asm_node(outputs=[("=r", _lit(0))])
        tc._check_inline_assembly_statement(node, _env())
        assert any("output operand must be an identifier" in e for e in tc.errors)

    def test_output_constraint_undefined_identifier_appends_error(self):
        tc = _checker()
        target = Identifier("ghost_var")
        node = _asm_node(outputs=[("=r", target)])
        tc._check_inline_assembly_statement(node, _env())
        assert any("Undefined output variable" in e for e in tc.errors)

    def test_invalid_output_constraint_no_write_marker_appends_error(self):
        tc = _checker()
        env = _env()
        env.define_variable("x", INTEGER_TYPE)
        target = Identifier("x")
        node = _asm_node(outputs=[("r", target)])  # no '=' or '+'
        tc._check_inline_assembly_statement(node, env)
        assert any("output constraint" in e for e in tc.errors)

    def test_output_constraint_plus_modifier_accepted(self):
        tc = _checker()
        env = _env()
        env.define_variable("x", INTEGER_TYPE)
        target = Identifier("x")
        node = _asm_node(outputs=[("+r", target)])
        tc._check_inline_assembly_statement(node, env)
        assert not any("output constraint" in e for e in tc.errors)

    def test_valid_memory_clobber_no_error(self):
        tc = _checker()
        node = _asm_node(clobbers=["memory"])
        tc._check_inline_assembly_statement(node, _env())
        assert not any("clobber" in e for e in tc.errors)

    def test_valid_cc_clobber_no_error(self):
        tc = _checker()
        node = _asm_node(clobbers=["cc"])
        tc._check_inline_assembly_statement(node, _env())
        assert not any("clobber" in e for e in tc.errors)

    def test_valid_register_clobber_rax_no_error(self):
        tc = _checker()
        node = _asm_node(clobbers=["rax"])
        tc._check_inline_assembly_statement(node, _env())
        assert not any("clobber" in e for e in tc.errors)

    def test_valid_register_clobber_percent_prefix(self):
        tc = _checker()
        node = _asm_node(clobbers=["%rax"])
        tc._check_inline_assembly_statement(node, _env())
        assert not any("clobber" in e for e in tc.errors)

    def test_invalid_clobber_with_space_appends_error(self):
        tc = _checker()
        node = _asm_node(clobbers=["bad reg"])
        tc._check_inline_assembly_statement(node, _env())
        assert any("Invalid inline assembly clobber" in e for e in tc.errors)

    def test_empty_clobber_appends_error(self):
        tc = _checker()
        node = _asm_node(clobbers=[""])
        tc._check_inline_assembly_statement(node, _env())
        assert any("Invalid inline assembly clobber" in e for e in tc.errors)

    def test_duplicate_clobber_appends_duplicate_error(self):
        tc = _checker()
        node = _asm_node(clobbers=["rax", "rax"])
        tc._check_inline_assembly_statement(node, _env())
        assert any("Duplicate inline assembly clobber" in e for e in tc.errors)

    def test_quoted_clobber_normalized_and_accepted(self):
        tc = _checker()
        node = _asm_node(clobbers=['"memory"'])
        tc._check_inline_assembly_statement(node, _env())
        assert not any("Invalid inline assembly clobber" in e for e in tc.errors)


# ---------------------------------------------------------------------------
# _normalize_asm_token
# ---------------------------------------------------------------------------

class TestNormalizeAsmToken:
    def test_unquoted_plain_string(self):
        tc = _checker()
        assert tc._normalize_asm_token("r") == "r"

    def test_quoted_string_strips_quotes(self):
        tc = _checker()
        assert tc._normalize_asm_token('"memory"') == "memory"

    def test_whitespace_padded_unquoted(self):
        tc = _checker()
        assert tc._normalize_asm_token("  rax  ") == "rax"

    def test_whitespace_padded_quoted(self):
        tc = _checker()
        assert tc._normalize_asm_token('  "cc"  ') == "cc"

    def test_single_char_constraint(self):
        tc = _checker()
        assert tc._normalize_asm_token("m") == "m"


# ---------------------------------------------------------------------------
# _is_valid_asm_constraint
# ---------------------------------------------------------------------------

class TestIsValidAsmConstraint:
    def test_valid_input_r(self):
        tc = _checker()
        assert tc._is_valid_asm_constraint("r", is_output=False) is True

    def test_valid_input_m(self):
        tc = _checker()
        assert tc._is_valid_asm_constraint("m", is_output=False) is True

    def test_valid_output_equals_r(self):
        tc = _checker()
        assert tc._is_valid_asm_constraint("=r", is_output=True) is True

    def test_valid_output_plus_r(self):
        tc = _checker()
        assert tc._is_valid_asm_constraint("+r", is_output=True) is True

    def test_empty_constraint_invalid(self):
        tc = _checker()
        assert tc._is_valid_asm_constraint("", is_output=False) is False

    def test_whitespace_constraint_invalid(self):
        tc = _checker()
        assert tc._is_valid_asm_constraint("r m", is_output=False) is False

    def test_output_without_write_marker_invalid(self):
        tc = _checker()
        assert tc._is_valid_asm_constraint("r", is_output=True) is False

    def test_complex_input_constraint_valid(self):
        tc = _checker()
        assert tc._is_valid_asm_constraint("rm", is_output=False) is True

    def test_constraint_with_special_chars_valid(self):
        tc = _checker()
        assert tc._is_valid_asm_constraint("=&r", is_output=True) is True


# ---------------------------------------------------------------------------
# _is_valid_asm_clobber
# ---------------------------------------------------------------------------

class TestIsValidAsmClobber:
    def test_memory_valid(self):
        tc = _checker()
        assert tc._is_valid_asm_clobber("memory") is True

    def test_cc_valid(self):
        tc = _checker()
        assert tc._is_valid_asm_clobber("cc") is True

    def test_register_name_valid(self):
        tc = _checker()
        assert tc._is_valid_asm_clobber("rax") is True

    def test_register_with_percent_valid(self):
        tc = _checker()
        assert tc._is_valid_asm_clobber("%rbx") is True

    def test_empty_clobber_invalid(self):
        tc = _checker()
        assert tc._is_valid_asm_clobber("") is False

    def test_clobber_with_space_invalid(self):
        tc = _checker()
        assert tc._is_valid_asm_clobber("bad reg") is False

    def test_numeric_only_invalid(self):
        tc = _checker()
        # Does not start with letter so regex fails
        assert tc._is_valid_asm_clobber("123") is False


# ---------------------------------------------------------------------------
# _check_match_expression_statement – dispatch
# ---------------------------------------------------------------------------

class TestCheckMatchExpressionDispatch:
    def test_non_match_node_returns_false_none(self):
        tc = _checker()
        node = type("OtherNode", (), {})()
        handled, typ = tc._check_match_expression_statement(node, _env())
        assert handled is False
        assert typ is None

    def test_empty_cases_returns_any_type(self):
        tc = _checker()
        node = _match_node(expression=_lit(1), cases=[])
        handled, typ = tc._check_match_expression_statement(node, _env())
        assert handled is True
        assert typ == ANY_TYPE

    def test_single_case_no_guard_returns_handled(self):
        tc = _checker()
        case = _case_node(pattern=_pattern_name("x"), body=[])
        node = _match_node(expression=_lit(42), cases=[case])
        handled, typ = tc._check_match_expression_statement(node, _env())
        assert handled is True

    def test_guard_boolean_type_no_error(self):
        tc = _checker()
        case = _case_node(pattern=_pattern_name("x"), body=[], guard=_bool_lit())
        node = _match_node(expression=_lit(1), cases=[case])
        tc._check_match_expression_statement(node, _env())
        assert not any("Guard" in e for e in tc.errors)

    def test_guard_non_boolean_appends_error(self):
        tc = _checker()
        # Integer literal as guard – check_expression returns INTEGER_TYPE
        case = _case_node(pattern=_pattern_name("x"), body=[], guard=_lit(99))
        node = _match_node(expression=_lit(1), cases=[case])
        tc._check_match_expression_statement(node, _env())
        assert any("Guard condition must be boolean" in e for e in tc.errors)

    def test_uniform_case_body_types_unified(self):
        tc = _checker()
        ret1 = ReturnStatement(_lit(1))
        ret2 = ReturnStatement(_lit(2))
        case1 = _case_node(pattern=_pattern_name("a"), body=[ret1])
        case2 = _case_node(pattern=_pattern_name("b"), body=[ret2])
        node = _match_node(expression=_lit(0), cases=[case1, case2])
        handled, typ = tc._check_match_expression_statement(node, _env())
        assert handled is True
        # Both bodies produce INTEGER_TYPE -> unified result is INTEGER_TYPE
        assert typ == INTEGER_TYPE

    def test_mixed_case_body_types_return_any(self):
        tc = _checker()
        ret_int = ReturnStatement(_lit(1))
        ret_str = ReturnStatement(Literal("string", "hello"))
        case1 = _case_node(pattern=_pattern_name("a"), body=[ret_int])
        case2 = _case_node(pattern=_pattern_name("b"), body=[ret_str])
        node = _match_node(expression=_lit(0), cases=[case1, case2])
        handled, typ = tc._check_match_expression_statement(node, _env())
        assert handled is True
        assert typ == ANY_TYPE


# ---------------------------------------------------------------------------
# _bind_pattern_types
# ---------------------------------------------------------------------------

class TestBindPatternTypes:
    def test_identifier_pattern_binds_to_match_expr_type(self):
        """Uses real IdentifierPattern so type_inference_engine path handles it."""
        tc = _checker()
        pattern = IdentifierPattern("val")
        case_env = TypeEnvironment(parent=_env())
        tc._bind_pattern_types(pattern, STRING_TYPE, case_env)
        assert case_env.get_variable_type("val") == STRING_TYPE

    def test_fallback_pattern_name_binds_to_match_expr_type(self):
        """Fallback branch: pattern.name attribute (no type_inference_engine)."""
        tc = _checker()
        del tc.type_inference_engine
        pattern = _pattern_name("val")
        case_env = TypeEnvironment(parent=_env())
        tc._bind_pattern_types(pattern, STRING_TYPE, case_env)
        assert case_env.get_variable_type("val") == STRING_TYPE

    def test_fallback_pattern_binding_binds_inner_type(self):
        """Fallback branch: pattern.binding, no type_parameters -> ANY_TYPE."""
        tc = _checker()
        del tc.type_inference_engine
        pattern = _pattern_binding("inner")
        case_env = TypeEnvironment(parent=_env())
        tc._bind_pattern_types(pattern, INTEGER_TYPE, case_env)
        assert case_env.get_variable_type("inner") == ANY_TYPE

    def test_fallback_pattern_binding_with_generic_type_params(self):
        """Fallback branch: pattern.binding, type with type_parameters[0] -> inner type."""
        tc = _checker()
        del tc.type_inference_engine
        # Create a mock type that has type_parameters (as the fallback branch checks)
        mock_type = type("MockGenericType", (), {"type_parameters": [FLOAT_TYPE]})()
        pattern = _pattern_binding("item")
        case_env = TypeEnvironment(parent=_env())
        tc._bind_pattern_types(pattern, mock_type, case_env)
        assert case_env.get_variable_type("item") == FLOAT_TYPE

    def test_fallback_pattern_bindings_list_each_bound_to_any(self):
        """Fallback branch: pattern.bindings list -> each bound to ANY_TYPE."""
        tc = _checker()
        del tc.type_inference_engine
        pattern = _pattern_bindings(["a", "b", "c"])
        case_env = TypeEnvironment(parent=_env())
        tc._bind_pattern_types(pattern, INTEGER_TYPE, case_env)
        for name in ("a", "b", "c"):
            assert case_env.get_variable_type(name) == ANY_TYPE

    def test_empty_pattern_no_bindings_no_error(self):
        tc = _checker()
        pattern = _pattern_empty()
        case_env = TypeEnvironment(parent=_env())
        # Should not raise
        tc._bind_pattern_types(pattern, INTEGER_TYPE, case_env)


# ---------------------------------------------------------------------------
# _unify_result_types
# ---------------------------------------------------------------------------

class TestUnifyResultTypes:
    def test_empty_list_returns_any_type(self):
        tc = _checker()
        assert tc._unify_result_types([]) == ANY_TYPE

    def test_single_type_returns_that_type(self):
        tc = _checker()
        assert tc._unify_result_types([INTEGER_TYPE]) == INTEGER_TYPE

    def test_uniform_types_return_that_type(self):
        tc = _checker()
        assert tc._unify_result_types([FLOAT_TYPE, FLOAT_TYPE, FLOAT_TYPE]) == FLOAT_TYPE

    def test_mixed_types_return_any(self):
        tc = _checker()
        assert tc._unify_result_types([INTEGER_TYPE, STRING_TYPE]) == ANY_TYPE

    def test_any_type_in_list_mixed_returns_any(self):
        tc = _checker()
        assert tc._unify_result_types([ANY_TYPE, INTEGER_TYPE]) == ANY_TYPE
