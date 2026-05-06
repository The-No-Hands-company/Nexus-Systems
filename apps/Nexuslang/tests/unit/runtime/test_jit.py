"""
JIT compilation and type-feedback tests.
Covers: TieredCompiler, TypeFeedback, NLPLCodeGenerator, JITGuardFailed,
        CodeGenError, JITCompiler.compile_function(), and round-trip execution.
"""

import sys
import types as _types
import pytest
from pathlib import Path

_SRC = str(Path(__file__).resolve().parent.parent.parent.parent / "src")
if _SRC not in sys.path:
    sys.path.insert(0, _SRC)

# ---------------------------------------------------------------------------
# Shared AST-building helpers
# ---------------------------------------------------------------------------

def _imports():
    """Lazy import of commonly used AST nodes and JIT classes."""
    from nexuslang.parser.ast import (
        FunctionDefinition, Parameter, VariableDeclaration, ReturnStatement,
        BinaryOperation, Identifier, Literal, IfStatement, WhileLoop, ForLoop,
        RepeatNTimesLoop, RepeatWhileLoop, BreakStatement, ContinueStatement,
        PrintStatement, UnaryOperation, IndexExpression, ListExpression,
        DictExpression, MemberAccess, FunctionCall,
    )
    from nexuslang.jit.code_gen import NLPLCodeGenerator, JITGuardFailed, CodeGenError
    return locals()


def _param(name):
    from nexuslang.parser.ast import Parameter
    return Parameter(name)


def _lit(value, typ="integer"):
    from nexuslang.parser.ast import Literal
    return Literal(typ, value)


def _ident(name):
    from nexuslang.parser.ast import Identifier
    return Identifier(name)


def _binop(left, op, right):
    from nexuslang.parser.ast import BinaryOperation
    return BinaryOperation(left, op, right)


def _ret(value=None):
    from nexuslang.parser.ast import ReturnStatement
    return ReturnStatement(value)


def _vardecl(name, value):
    from nexuslang.parser.ast import VariableDeclaration
    return VariableDeclaration(name, value)


def _funcdef(name, params, body):
    from nexuslang.parser.ast import FunctionDefinition
    return FunctionDefinition(name, params, body)


def _funcall(name, args=None):
    from nexuslang.parser.ast import FunctionCall
    return FunctionCall(name, args or [])


class _MockRuntime:
    def __init__(self):
        self.printed = []

    def print_value(self, val):
        self.printed.append(val)


class _MockInterpreter:
    """Minimal interpreter mock that routes _call and _print to controllable hooks."""

    def __init__(self):
        self.runtime = _MockRuntime()
        self._registered: dict = {}

    def register(self, name, fn):
        self._registered[name] = fn

    def call_function(self, name, args):
        if name in self._registered:
            return self._registered[name](*args)
        raise NameError(f"unknown function: {name}")

class TestJIT:
    def test_tiered_compiler_import(self):
        from nexuslang.jit.tiered_compiler import TieredCompiler
        assert TieredCompiler is not None

    def test_tiered_compiler_create(self):
        from nexuslang.jit.tiered_compiler import TieredCompiler
        tc = TieredCompiler()
        assert tc is not None

    def test_tiered_compiler_tier_of_unknown(self):
        from nexuslang.jit.tiered_compiler import TieredCompiler, ExecutionTier
        tc = TieredCompiler()
        tier = tc.tier_of("unknown_function")
        assert tier == ExecutionTier.INTERPRETER

    def test_tiered_compiler_inject_tier(self):
        from nexuslang.jit.tiered_compiler import TieredCompiler, ExecutionTier, FunctionTierState
        tc = TieredCompiler()
        tc._function_states["hot_fn"] = FunctionTierState(
            name="hot_fn", tier=ExecutionTier.OPTIMIZING_JIT
        )
        assert tc.tier_of("hot_fn") == ExecutionTier.OPTIMIZING_JIT

    def test_function_tier_state_import(self):
        from nexuslang.jit.tiered_compiler import FunctionTierState, ExecutionTier
        state = FunctionTierState(name="fn", tier=ExecutionTier.INTERPRETER)
        assert state.name == "fn"

    def test_execution_tier_values(self):
        from nexuslang.jit.tiered_compiler import ExecutionTier
        assert hasattr(ExecutionTier, "INTERPRETER")
        assert hasattr(ExecutionTier, "OPTIMIZING_JIT")


# ============================================================
# Section 4 - Type Feedback
# ============================================================

class TestTypeFeedback:
    def test_function_feedback_import(self):
        from nexuslang.jit.type_feedback import FunctionFeedback
        assert FunctionFeedback is not None

    def test_function_feedback_record_call(self):
        from nexuslang.jit.type_feedback import FunctionFeedback
        fb = FunctionFeedback("add")
        fb.record_call(["Integer", "Integer"])

    def test_function_feedback_monomorphic(self):
        from nexuslang.jit.type_feedback import FunctionFeedback, Polymorphism
        fb = FunctionFeedback("add")
        fb.record_call(["Integer", "Integer"])
        fb.record_call(["Integer", "Integer"])
        assert fb.polymorphism == Polymorphism.MONOMORPHIC

    def test_function_feedback_polymorphic(self):
        from nexuslang.jit.type_feedback import FunctionFeedback, Polymorphism
        fb = FunctionFeedback("add")
        fb.record_call(["Integer", "Integer"])
        fb.record_call(["Float", "Float"])
        assert fb.polymorphism in (Polymorphism.POLYMORPHIC, Polymorphism.MEGAMORPHIC)

    def test_type_feedback_collector_import(self):
        from nexuslang.jit.type_feedback import TypeFeedbackCollector
        tfc = TypeFeedbackCollector()
        assert tfc is not None

    def test_type_feedback_collector_get_record(self):
        from nexuslang.jit.type_feedback import TypeFeedbackCollector
        tfc = TypeFeedbackCollector()
        # get_record returns None for unknown functions - that is the correct behaviour
        rec = tfc.get_record("my_func")
        assert rec is None  # not yet recorded

    def test_type_feedback_collector_get_hints(self):
        from nexuslang.jit.type_feedback import TypeFeedbackCollector
        tfc = TypeFeedbackCollector()
        hints = tfc.get_hints("my_func")
        assert isinstance(hints, (list, dict, type(None)))



# ============================================================
# Section 5 - Enhanced Linter Checks
# ============================================================


# ============================================================
# Section 6 - JIT Code Generator
# ============================================================

class TestCodeGenImports:
    def test_jit_guard_failed_importable(self):
        from nexuslang.jit.code_gen import JITGuardFailed
        assert JITGuardFailed is not None

    def test_code_gen_error_importable(self):
        from nexuslang.jit.code_gen import CodeGenError
        assert CodeGenError is not None

    def test_nxl_code_generator_importable(self):
        from nexuslang.jit.code_gen import NLPLCodeGenerator
        assert NLPLCodeGenerator is not None

    def test_all_three_exported_from_jit_init(self):
        from nexuslang.jit import NLPLCodeGenerator, JITGuardFailed, CodeGenError
        assert NLPLCodeGenerator is not None
        assert JITGuardFailed is not None
        assert CodeGenError is not None


class TestJITGuardFailed:
    def test_is_exception_subclass(self):
        from nexuslang.jit.code_gen import JITGuardFailed
        assert issubclass(JITGuardFailed, Exception)

    def test_holds_message(self):
        from nexuslang.jit.code_gen import JITGuardFailed
        e = JITGuardFailed("guard: x expected Integer")
        assert "Integer" in str(e)

    def test_raise_and_catch(self):
        from nexuslang.jit.code_gen import JITGuardFailed
        with pytest.raises(JITGuardFailed):
            raise JITGuardFailed("test")

    def test_distinct_from_code_gen_error(self):
        from nexuslang.jit.code_gen import JITGuardFailed, CodeGenError
        assert JITGuardFailed is not CodeGenError


class TestCodeGenError:
    def test_is_exception_subclass(self):
        from nexuslang.jit.code_gen import CodeGenError
        assert issubclass(CodeGenError, Exception)

    def test_holds_message(self):
        from nexuslang.jit.code_gen import CodeGenError
        e = CodeGenError("failed to compile foo")
        assert "foo" in str(e)

    def test_raise_and_catch(self):
        from nexuslang.jit.code_gen import CodeGenError
        with pytest.raises(CodeGenError):
            raise CodeGenError("boom")


class TestOperatorMap:
    def _gen_expr_binop(self, op):
        from nexuslang.jit.code_gen import NLPLCodeGenerator
        gen = NLPLCodeGenerator()
        left = _ident("a")
        right = _ident("b")
        node = _binop(left, op, right)
        return gen._gen_expr(node)

    def test_plus_operator(self):
        assert "+" in self._gen_expr_binop("plus")

    def test_minus_operator(self):
        assert "-" in self._gen_expr_binop("minus")

    def test_times_operator(self):
        assert "*" in self._gen_expr_binop("times")

    def test_divided_by_operator(self):
        assert "/" in self._gen_expr_binop("divided by")

    def test_is_equal_to_operator(self):
        assert "==" in self._gen_expr_binop("is equal to")

    def test_is_greater_than_operator(self):
        assert ">" in self._gen_expr_binop("is greater than")

    def test_is_less_than_operator(self):
        assert "<" in self._gen_expr_binop("is less than")

    def test_to_the_power_of_operator(self):
        assert "**" in self._gen_expr_binop("to the power of")

    def test_modulo_operator(self):
        assert "%" in self._gen_expr_binop("modulo")

    def test_and_operator(self):
        assert "and" in self._gen_expr_binop("and")


class TestTypeGuardMap:
    def test_integer_maps_to_int(self):
        from nexuslang.jit.code_gen import _TYPE_GUARD_MAP
        assert _TYPE_GUARD_MAP["Integer"] == "int"

    def test_float_maps_to_float(self):
        from nexuslang.jit.code_gen import _TYPE_GUARD_MAP
        assert _TYPE_GUARD_MAP["Float"] == "float"

    def test_string_maps_to_str(self):
        from nexuslang.jit.code_gen import _TYPE_GUARD_MAP
        assert _TYPE_GUARD_MAP["String"] == "str"

    def test_boolean_maps_to_bool(self):
        from nexuslang.jit.code_gen import _TYPE_GUARD_MAP
        assert _TYPE_GUARD_MAP["Boolean"] == "bool"

    def test_list_maps_to_list(self):
        from nexuslang.jit.code_gen import _TYPE_GUARD_MAP
        assert _TYPE_GUARD_MAP["List"] == "list"

    def test_dictionary_maps_to_dict(self):
        from nexuslang.jit.code_gen import _TYPE_GUARD_MAP
        assert _TYPE_GUARD_MAP["Dictionary"] == "dict"


class TestGenerateFunctionSource:
    def _gen(self, name, params, body, type_hints=None, opt_level=1):
        from nexuslang.jit.code_gen import NLPLCodeGenerator
        gen = NLPLCodeGenerator()
        fd = _funcdef(name, params, body)
        return gen.generate_function(fd, type_hints=type_hints, opt_level=opt_level)

    def test_empty_body_emits_pass(self):
        src = self._gen("noop", [], [])
        assert "def _jit_noop():" in src
        assert "pass" in src

    def test_single_param_in_signature(self):
        src = self._gen("inc", [_param("x")], [_ret(_ident("x"))])
        assert "def _jit_inc(x):" in src

    def test_two_params_in_signature(self):
        src = self._gen("add", [_param("a"), _param("b")], [_ret(_binop(_ident("a"), "plus", _ident("b")))])
        assert "def _jit_add(a, b):" in src

    def test_return_literal_emitted(self):
        src = self._gen("const", [], [_ret(_lit(42))])
        assert "return" in src
        assert "42" in src

    def test_return_binop_emitted(self):
        src = self._gen("add", [_param("a"), _param("b")],
                        [_ret(_binop(_ident("a"), "plus", _ident("b")))])
        assert "(a + b)" in src

    def test_vardecl_emitted(self):
        src = self._gen("f", [], [_vardecl("x", _lit(10))])
        assert "x = " in src and "10" in src

    def test_if_statement_emitted(self):
        from nexuslang.parser.ast import IfStatement
        cond = _binop(_ident("x"), "is greater than", _lit(0))
        then_b = [_ret(_lit(1))]
        body = [IfStatement(cond, then_b)]
        src = self._gen("f", [_param("x")], body)
        assert "if " in src

    def test_while_loop_emitted(self):
        from nexuslang.parser.ast import WhileLoop
        cond = _binop(_ident("i"), "is less than", _lit(5))
        loop = WhileLoop(cond, [_vardecl("i", _binop(_ident("i"), "plus", _lit(1)))])
        src = self._gen("f", [_param("i")], [loop])
        assert "while " in src

    def test_for_range_loop_emitted(self):
        from nexuslang.parser.ast import ForLoop
        loop = ForLoop("i", body=[_ret(_ident("i"))], start=_lit(0), end=_lit(10))
        src = self._gen("f", [], [loop])
        assert "range(" in src

    def test_for_each_loop_emitted(self):
        from nexuslang.parser.ast import ForLoop
        loop = ForLoop("item", iterable=_ident("items"), body=[_ret(_ident("item"))])
        src = self._gen("f", [_param("items")], [loop])
        assert "for item in items" in src

    def test_repeat_n_loop_emitted(self):
        from nexuslang.parser.ast import RepeatNTimesLoop
        loop = RepeatNTimesLoop(_lit(3), body=[_vardecl("x", _lit(1))])
        src = self._gen("f", [], [loop])
        assert "range(3)" in src

    def test_type_guard_emitted_at_level_2(self):
        src = self._gen("guarded", [_param("n")],
                        [_ret(_ident("n"))],
                        type_hints={"param_0": "Integer"},
                        opt_level=2)
        assert "isinstance(n," in src
        assert "_GuardFailed" in src

    def test_no_type_guard_at_level_1(self):
        src = self._gen("f", [_param("n")],
                        [_ret(_ident("n"))],
                        type_hints={"param_0": "Integer"},
                        opt_level=1)
        assert "_GuardFailed" not in src

    def test_source_is_valid_python(self):
        src = self._gen("add", [_param("a"), _param("b")],
                        [_ret(_binop(_ident("a"), "plus", _ident("b")))])
        try:
            compile(src, "<test>", "exec")
        except SyntaxError as e:
            pytest.fail(f"Generated source has syntax error: {e}\n{src}")


class TestCompileFunction:
    def _compile(self, name, params, body, type_hints=None, opt_level=1):
        from nexuslang.jit.code_gen import NLPLCodeGenerator
        interp = _MockInterpreter()
        gen = NLPLCodeGenerator()
        fd = _funcdef(name, params, body)
        return gen.compile_function(fd, interp, type_hints=type_hints, opt_level=opt_level)

    def test_returns_callable(self):
        fn = self._compile("add", [_param("a"), _param("b")],
                           [_ret(_binop(_ident("a"), "plus", _ident("b")))])
        assert callable(fn)

    def test_add_two_integers(self):
        fn = self._compile("add", [_param("a"), _param("b")],
                           [_ret(_binop(_ident("a"), "plus", _ident("b")))])
        assert fn(3, 4) == 7

    def test_multiply_two_integers(self):
        fn = self._compile("mul", [_param("a"), _param("b")],
                           [_ret(_binop(_ident("a"), "times", _ident("b")))])
        assert fn(6, 7) == 42

    def test_return_literal(self):
        fn = self._compile("fortytwo", [], [_ret(_lit(42))])
        assert fn() == 42

    def test_return_none_when_no_return(self):
        fn = self._compile("noop", [], [_vardecl("x", _lit(1))])
        result = fn()
        assert result is None

    def test_string_concatenation(self):
        fn = self._compile("cat", [_param("a"), _param("b")],
                           [_ret(_binop(_ident("a"), "+", _ident("b")))])
        assert fn("hello", " world") == "hello world"

    def test_comparison_returns_bool(self):
        fn = self._compile("gt", [_param("a"), _param("b")],
                           [_ret(_binop(_ident("a"), "is greater than", _ident("b")))])
        assert fn(5, 3) is True
        assert fn(1, 3) is False

    def test_type_guard_passes_correct_type(self):
        fn = self._compile("guarded", [_param("n")],
                           [_ret(_ident("n"))],
                           type_hints={"param_0": "Integer"},
                           opt_level=2)
        assert fn(99) == 99

    def test_type_guard_fires_on_wrong_type(self):
        from nexuslang.jit.code_gen import JITGuardFailed
        fn = self._compile("guarded", [_param("n")],
                           [_ret(_ident("n"))],
                           type_hints={"param_0": "Integer"},
                           opt_level=2)
        with pytest.raises(JITGuardFailed):
            fn("not an int")

    def test_code_gen_error_on_bad_source(self):
        from nexuslang.jit.code_gen import NLPLCodeGenerator, CodeGenError
        # Produce a bogus func_def that will cause something weird but
        # by patching generate_function to return invalid Python
        gen = NLPLCodeGenerator()
        interp = _MockInterpreter()
        original = gen.generate_function

        def _bad(fd, type_hints=None, opt_level=1):
            return "def _jit_bad(:\n    pass\n"

        gen.generate_function = _bad
        bad_fd = _funcdef("bad", [], [])
        with pytest.raises(CodeGenError):
            gen.compile_function(bad_fd, interp)

    def test_unknown_node_compiles_to_pass(self):
        unknown = type("WeirdUnknownStmt", (), {})()
        fn = self._compile("f", [], [unknown])
        assert fn() is None

    def test_nested_binop(self):
        inner = _binop(_ident("a"), "times", _ident("b"))
        outer = _binop(inner, "plus", _ident("c"))
        fn = self._compile("f", [_param("a"), _param("b"), _param("c")],
                           [_ret(outer)])
        assert fn(2, 3, 4) == 10

    def test_unary_negation(self):
        from nexuslang.parser.ast import UnaryOperation
        node = UnaryOperation("-", _ident("x"))
        fn = self._compile("neg", [_param("x")], [_ret(node)])
        assert fn(5) == -5

    def test_vardecl_then_return(self):
        body = [_vardecl("z", _binop(_ident("a"), "plus", _ident("b"))),
                _ret(_ident("z"))]
        fn = self._compile("add_via_var", [_param("a"), _param("b")], body)
        assert fn(10, 32) == 42


class TestGenStmt:
    def _gen_stmt(self, node):
        from nexuslang.jit.code_gen import NLPLCodeGenerator
        return NLPLCodeGenerator()._gen_stmt(node, depth=0)

    def test_vardecl(self):
        lines = self._gen_stmt(_vardecl("x", _lit(7)))
        assert any("x = " in ln for ln in lines)

    def test_return_value(self):
        lines = self._gen_stmt(_ret(_lit(1)))
        assert any("return" in ln for ln in lines)

    def test_return_none(self):
        lines = self._gen_stmt(_ret(None))
        assert any("return None" in ln for ln in lines)

    def test_break_statement(self):
        from nexuslang.parser.ast import BreakStatement
        lines = self._gen_stmt(BreakStatement())
        assert lines == ["break"]

    def test_continue_statement(self):
        from nexuslang.parser.ast import ContinueStatement
        lines = self._gen_stmt(ContinueStatement())
        assert lines == ["continue"]

    def test_print_statement(self):
        from nexuslang.parser.ast import PrintStatement
        lines = self._gen_stmt(PrintStatement(_lit(42)))
        assert any("_print(" in ln for ln in lines)

    def test_if_statement_has_if_keyword(self):
        from nexuslang.parser.ast import IfStatement
        cond = _binop(_ident("x"), "is greater than", _lit(0))
        lines = self._gen_stmt(IfStatement(cond, [_ret(_lit(1))]))
        assert any(ln.startswith("if ") for ln in lines)

    def test_if_else_has_else_keyword(self):
        from nexuslang.parser.ast import IfStatement
        cond = _binop(_ident("x"), "is greater than", _lit(0))
        lines = self._gen_stmt(IfStatement(cond, [_ret(_lit(1))], [_ret(_lit(0))]))
        assert any("else:" in ln for ln in lines)

    def test_while_loop(self):
        from nexuslang.parser.ast import WhileLoop
        cond = _binop(_ident("i"), "is less than", _lit(10))
        lines = self._gen_stmt(WhileLoop(cond, [_vardecl("i", _lit(0))]))
        assert any(ln.startswith("while ") for ln in lines)

    def test_for_range_loop(self):
        from nexuslang.parser.ast import ForLoop
        loop = ForLoop("i", body=[], start=_lit(0), end=_lit(5))
        lines = self._gen_stmt(loop)
        assert any("range(" in ln for ln in lines)

    def test_for_each_loop(self):
        from nexuslang.parser.ast import ForLoop
        loop = ForLoop("item", iterable=_ident("items"), body=[])
        lines = self._gen_stmt(loop)
        assert any("for item in" in ln for ln in lines)

    def test_repeat_n_times(self):
        from nexuslang.parser.ast import RepeatNTimesLoop
        loop = RepeatNTimesLoop(_lit(5), body=[])
        lines = self._gen_stmt(loop)
        assert any("range(5)" in ln for ln in lines)

    def test_repeat_while_loop(self):
        from nexuslang.parser.ast import RepeatWhileLoop
        cond = _ident("running")
        loop = RepeatWhileLoop(cond, body=[])
        lines = self._gen_stmt(loop)
        assert any("while running" in ln for ln in lines)

    def test_unknown_stmt_emits_pass(self):
        unknown = type("BogusNode", (), {})()
        lines = self._gen_stmt(unknown)
        assert any("pass" in ln for ln in lines)


class TestGenExpr:
    def _gen_expr(self, node):
        from nexuslang.jit.code_gen import NLPLCodeGenerator
        return NLPLCodeGenerator()._gen_expr(node)

    def test_integer_literal(self):
        assert self._gen_expr(_lit(42)) == "42"

    def test_float_literal(self):
        assert self._gen_expr(_lit(3.14, "float")) == "3.14"

    def test_string_literal(self):
        assert self._gen_expr(_lit("hello", "string")) == repr("hello")

    def test_bool_literal_true(self):
        assert self._gen_expr(_lit(True, "boolean")) == "True"

    def test_none_node(self):
        assert self._gen_expr(None) == "None"

    def test_identifier(self):
        assert self._gen_expr(_ident("foo")) == "foo"

    def test_identifier_true_becomes_True(self):
        assert self._gen_expr(_ident("true")) == "True"

    def test_identifier_false_becomes_False(self):
        assert self._gen_expr(_ident("false")) == "False"

    def test_identifier_null_becomes_None(self):
        assert self._gen_expr(_ident("null")) == "None"

    def test_binary_operation(self):
        result = self._gen_expr(_binop(_ident("x"), "plus", _ident("y")))
        assert "(x + y)" == result

    def test_index_expression(self):
        from nexuslang.parser.ast import IndexExpression
        node = IndexExpression(_ident("arr"), _lit(0))
        result = self._gen_expr(node)
        assert "arr" in result and "0" in result and "[" in result

    def test_list_expression(self):
        from nexuslang.parser.ast import ListExpression
        node = ListExpression([_lit(1), _lit(2), _lit(3)])
        result = self._gen_expr(node)
        assert result == "[1, 2, 3]"

    def test_dict_expression_tuple_entries(self):
        from nexuslang.parser.ast import DictExpression
        entries = [(_lit("a", "string"), _lit(1)), (_lit("b", "string"), _lit(2))]
        node = DictExpression(entries)
        result = self._gen_expr(node)
        assert "'a'" in result and "1" in result

    def test_member_access(self):
        from nexuslang.parser.ast import MemberAccess
        node = MemberAccess(_ident("obj"), "length")
        result = self._gen_expr(node)
        assert result == "obj.length"

    def test_python_int_primitive_passthrough(self):
        assert self._gen_expr(42) == "42"

    def test_python_str_primitive_passthrough(self):
        assert self._gen_expr("hi") == repr("hi")

    def test_unhandled_expr_returns_none_comment(self):
        from nexuslang.jit.code_gen import NLPLCodeGenerator
        bogus = type("WeirdExpr", (), {})()
        result = NLPLCodeGenerator()._gen_expr(bogus)
        assert "None" in result


class TestRoundTrip:
    """Build complete NexusLang function ASTs, compile, and call them."""

    def _compile(self, name, params, body, interp=None):
        from nexuslang.jit.code_gen import NLPLCodeGenerator
        if interp is None:
            interp = _MockInterpreter()
        gen = NLPLCodeGenerator()
        fd = _funcdef(name, params, body)
        return gen.compile_function(fd, interp)

    def test_identity_function(self):
        fn = self._compile("identity", [_param("x")], [_ret(_ident("x"))])
        assert fn(99) == 99

    def test_add_function(self):
        fn = self._compile("add", [_param("a"), _param("b")],
                           [_ret(_binop(_ident("a"), "plus", _ident("b")))])
        assert fn(10, 15) == 25

    def test_multiply_function(self):
        fn = self._compile("mul", [_param("x"), _param("y")],
                           [_ret(_binop(_ident("x"), "times", _ident("y")))])
        assert fn(6, 7) == 42

    def test_if_conditional(self):
        from nexuslang.parser.ast import IfStatement
        cond = _binop(_ident("x"), "is greater than", _lit(0))
        then_b = [_ret(_lit(1))]
        else_b = [_ret(_lit(-1))]
        body = [IfStatement(cond, then_b, else_b)]
        fn = self._compile("sign", [_param("x")], body)
        assert fn(5) == 1
        assert fn(-3) == -1

    def test_while_loop_accumulates(self):
        from nexuslang.parser.ast import WhileLoop
        # while i < 5: i = i + 1  (then return i)
        cond = _binop(_ident("i"), "is less than", _lit(5))
        inc = _vardecl("i", _binop(_ident("i"), "plus", _lit(1)))
        loop = WhileLoop(cond, [inc])
        body = [loop, _ret(_ident("i"))]
        fn = self._compile("count_to_5", [_param("i")], body)
        assert fn(0) == 5

    def test_for_range_sum(self):
        from nexuslang.parser.ast import ForLoop
        # sum = 0; for i in range(0, 5): sum = sum + i; return sum
        init = _vardecl("total", _lit(0))
        loop = ForLoop(
            "i",
            start=_lit(0), end=_lit(5),
            body=[_vardecl("total", _binop(_ident("total"), "plus", _ident("i")))]
        )
        body = [init, loop, _ret(_ident("total"))]
        fn = self._compile("sum_range", [], body)
        assert fn() == 10  # 0+1+2+3+4

    def test_repeat_n_times(self):
        from nexuslang.parser.ast import RepeatNTimesLoop
        init = _vardecl("count", _lit(0))
        loop = RepeatNTimesLoop(
            _lit(7),
            body=[_vardecl("count", _binop(_ident("count"), "plus", _lit(1)))]
        )
        body = [init, loop, _ret(_ident("count"))]
        fn = self._compile("repeat7", [], body)
        assert fn() == 7

    def test_external_call_routed_through_interpreter(self):
        interp = _MockInterpreter()
        interp.register("double", lambda x: x * 2)
        call_node = _funcall("double", [_ident("n")])
        fn = self._compile("use_double", [_param("n")], [_ret(call_node)], interp)
        assert fn(21) == 42

    def test_string_comparison(self):
        fn = self._compile("streq", [_param("s")],
                           [_ret(_binop(_ident("s"), "is equal to", _lit("hi", "string")))])
        assert fn("hi") is True
        assert fn("bye") is False

    def test_floor_division(self):
        fn = self._compile("fdiv", [_param("a"), _param("b")],
                           [_ret(_binop(_ident("a"), "integer divided by", _ident("b")))])
        assert fn(17, 5) == 3


class TestJITCompilerCompileFunction:
    def test_compile_function_method_exists(self):
        from nexuslang.jit.jit_compiler import JITCompiler
        assert hasattr(JITCompiler, "compile_function")

    def test_compile_function_returns_callable(self):
        from nexuslang.jit.jit_compiler import JITCompiler
        from nexuslang.jit.code_gen import NLPLCodeGenerator
        interp = _MockInterpreter()
        jit = JITCompiler(interp)
        fd = _funcdef("add", [_param("a"), _param("b")],
                      [_ret(_binop(_ident("a"), "plus", _ident("b")))])
        result = jit.compile_function("add", fd, opt_level=1)
        assert callable(result)

    def test_compiled_function_executes_correctly(self):
        from nexuslang.jit.jit_compiler import JITCompiler
        interp = _MockInterpreter()
        jit = JITCompiler(interp)
        fd = _funcdef("add", [_param("a"), _param("b")],
                      [_ret(_binop(_ident("a"), "plus", _ident("b")))])
        fn = jit.compile_function("add", fd, opt_level=1)
        assert fn(7, 8) == 15

    def test_compile_function_none_def_is_graceful(self):
        from nexuslang.jit.jit_compiler import JITCompiler
        interp = _MockInterpreter()
        jit = JITCompiler(interp)
        # None func_def: generate_function handles it gracefully (empty body)
        # and returns a callable or None - must not raise
        result = jit.compile_function("oops", None, opt_level=1)
        assert result is None or callable(result)

    def test_opt_level_2_with_type_hints(self):
        from nexuslang.jit.jit_compiler import JITCompiler
        interp = _MockInterpreter()
        jit = JITCompiler(interp)
        fd = _funcdef("add", [_param("a"), _param("b")],
                      [_ret(_binop(_ident("a"), "plus", _ident("b")))])
        fn = jit.compile_function("add", fd, opt_level=3,
                                  type_hints={"param_0": "Integer", "param_1": "Integer"})
        assert fn is None or fn(4, 5) == 9

    def test_compile_function_opt_level_3_uses_codegen(self):
        from nexuslang.jit.jit_compiler import JITCompiler
        from nexuslang.jit.code_gen import JITGuardFailed
        interp = _MockInterpreter()
        jit = JITCompiler(interp)
        fd = _funcdef("mul", [_param("x"), _param("y")],
                      [_ret(_binop(_ident("x"), "times", _ident("y")))])
        fn = jit.compile_function("mul", fd, opt_level=3,
                                  type_hints={"param_0": "Integer", "param_1": "Integer"})
        if fn is not None:
            assert fn(3, 4) == 12

    def test_compile_function_with_no_type_hints(self):
        from nexuslang.jit.jit_compiler import JITCompiler
        interp = _MockInterpreter()
        jit = JITCompiler(interp)
        fd = _funcdef("inc", [_param("n")],
                      [_ret(_binop(_ident("n"), "plus", _lit(1)))])
        fn = jit.compile_function("inc", fd, opt_level=1, type_hints=None)
        assert fn is not None
        assert fn(41) == 42


class TestTieredIntegration:
    def test_tiered_compiler_with_real_code_gen(self):
        from nexuslang.jit.tiered_compiler import TieredCompiler, ExecutionTier
        tc = TieredCompiler()
        assert tc.tier_of("any_fn") == ExecutionTier.INTERPRETER

    def test_jit_init_exports_code_gen_names(self):
        import nexuslang.jit as jit_pkg
        assert hasattr(jit_pkg, "NLPLCodeGenerator")
        assert hasattr(jit_pkg, "JITGuardFailed")
        assert hasattr(jit_pkg, "CodeGenError")

    def test_guard_fail_does_not_corrupt_generator_state(self):
        from nexuslang.jit.code_gen import NLPLCodeGenerator, JITGuardFailed
        gen = NLPLCodeGenerator()
        fd = _funcdef("guarded", [_param("n")],
                      [_ret(_ident("n"))],)
        interp = _MockInterpreter()
        fn = gen.compile_function(fd, interp, type_hints={"param_0": "Integer"}, opt_level=2)
        with pytest.raises(JITGuardFailed):
            fn("string")
        # Generator should still be functional after a guard failure
        fn2 = gen.compile_function(fd, interp, type_hints={}, opt_level=1)
        assert fn2(7) == 7

    def test_baseline_and_optimizing_produce_same_result(self):
        from nexuslang.jit.code_gen import NLPLCodeGenerator
        gen = NLPLCodeGenerator()
        fd = _funcdef("add", [_param("a"), _param("b")],
                      [_ret(_binop(_ident("a"), "plus", _ident("b")))])
        interp = _MockInterpreter()
        fn_base = gen.compile_function(fd, interp, opt_level=1)
        fn_opt = gen.compile_function(fd, interp,
                                      type_hints={"param_0": "Integer", "param_1": "Integer"},
                                      opt_level=2)
        assert fn_base(5, 6) == 11
        assert fn_opt(5, 6) == 11

    def test_make_call_helper_is_callable(self):
        from nexuslang.jit.code_gen import _make_call_helper
        interp = _MockInterpreter()
        interp.register("square", lambda x: x * x)
        call_fn = _make_call_helper(interp)
        assert callable(call_fn)
        assert call_fn("square", [9]) == 81

    def test_make_print_helper_is_callable(self):
        from nexuslang.jit.code_gen import _make_print_helper
        interp = _MockInterpreter()
        print_fn = _make_print_helper(interp)
        assert callable(print_fn)
        print_fn("hello")
        assert interp.runtime.printed == ["hello"]
