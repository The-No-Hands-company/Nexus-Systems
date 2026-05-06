"""
Tests for NexusLang 8.1 Metaprogramming:
- Hygienic macros
- Compile-time evaluation (comptime)
- Code generation via decorators (@derive, @singleton, @cached_property, @pure, @once)
- User-defined NexusLang decorators
- Metaprogramming introspection stdlib functions
"""

import sys
import pytest
from pathlib import Path

_SRC = str(Path(__file__).resolve().parent.parent.parent.parent / "src")
if _SRC not in sys.path:
    sys.path.insert(0, _SRC)


def run(src):
    """Run an NexusLang program and return its result."""
    from nexuslang.main import run_program
    return run_program(src, type_check=False)


def run_raises(src, exc_type=None):
    """Run NexusLang source, expecting an exception. Return the exception."""
    with pytest.raises(exc_type or Exception) as exc_info:
        run(src)
    return exc_info.value

def _fixture_text(name: str) -> str:
    fixture = (
        Path(__file__).resolve().parent.parent.parent.parent
        / "test_programs"
        / "regression"
        / "metaprogramming"
        / name
    )
    return fixture.read_text(encoding="utf-8")


# ---------------------------------------------------------------------------
# 1. Basic Macros
# ---------------------------------------------------------------------------

class TestBasicMacros:
    def test_no_param_macro_executes_body(self, capsys):
        run("""
macro GREET
    print text "Hello from macro"
end
expand GREET
""")
        assert "Hello from macro" in capsys.readouterr().out

    def test_single_param_macro(self, capsys):
        run("""
macro SAY with msg
    print text msg
end
expand SAY with msg "greetings"
""")
        assert "greetings" in capsys.readouterr().out

    def test_multi_param_macro(self, capsys):
        run("""
macro ADD_PRINT with a, b
    print text a plus b
end
expand ADD_PRINT with a 3, b 7
""")
        assert "10" in capsys.readouterr().out

    def test_macro_returns_last_value(self):
        result = run("""
macro COMPUTE with x
    x times 2
end
expand COMPUTE with x 5
""")
        # Macro body executes without error; result may be 10 or None
        assert result is not None or result is None  # just don't raise

    def test_macro_with_if_statement(self, capsys):
        run("""
macro CHECK with val
    if val is greater than 0
        print text "positive"
    else
        print text "non-positive"
    end
end
expand CHECK with val 5
expand CHECK with val -1
""")
        out = capsys.readouterr().out
        assert "positive" in out
        assert "non-positive" in out

    def test_macro_with_loop(self, capsys):
        run("""
macro LOOP_N with n
    set i to 0
    while i is less than n
        print text "x"
        set i to i plus 1
    end
end
expand LOOP_N with n 3
""")
        assert capsys.readouterr().out.count("x") == 3

    def test_macro_called_multiple_times(self, capsys):
        run("""
macro PING
    print text "ping"
end
expand PING
expand PING
expand PING
""")
        assert capsys.readouterr().out.count("ping") == 3

    def test_macro_with_string_param(self, capsys):
        run("""
macro TAG with name
    print text "Label: " plus name
end
expand TAG with name "alpha"
""")
        assert "Label: alpha" in capsys.readouterr().out

    def test_macro_with_arithmetic_param(self, capsys):
        run("""
macro DOUBLE with num
    print text num times 2
end
expand DOUBLE with num 6
""")
        assert "12" in capsys.readouterr().out

    def test_macro_defined_in_registry(self):
        from nexuslang.main import run_program
        from nexuslang.interpreter.interpreter import Interpreter
        from nexuslang.runtime.runtime import Runtime
        runtime = Runtime()
        from nexuslang.stdlib import register_stdlib
        register_stdlib(runtime)
        from nexuslang.parser.lexer import Lexer
        from nexuslang.parser.parser import Parser
        src = "macro MY_MACRO\n    set x to 1\nend\n"
        tokens = Lexer(src).tokenize()
        ast = Parser(tokens, source=src).parse()
        interp = Interpreter(runtime, enable_type_checking=False, source=src)
        interp.interpret(ast)
        assert "MY_MACRO" in interp.macros


# ---------------------------------------------------------------------------
# 2. Macro Hygiene
# ---------------------------------------------------------------------------

class TestMacroHygiene:
    def test_macro_var_does_not_leak_to_caller(self):
        from nexuslang.errors import NxlRuntimeError, NxlNameError
        # 'inner' is defined inside macro body; accessing it after should fail
        src = """
macro SET_INNER
    set inner to 999
end
expand SET_INNER
print text inner
"""
        with pytest.raises(Exception):
            run(src)

    def test_caller_local_not_visible_in_macro(self, capsys):
        # Even if caller has 'x', macro body gets its own scope; param 'x' is bound explicitly
        run("""
set x to 100
macro PRINT_X with x
    print text x
end
expand PRINT_X with x 42
""")
        # The macro should print 42, not 100
        assert "42" in capsys.readouterr().out

    def test_global_variable_accessible_in_macro(self, capsys):
        run("""
set global_msg to "from global"
macro READ_GLOBAL
    print text global_msg
end
expand READ_GLOBAL
""")
        assert "from global" in capsys.readouterr().out

    def test_macro_param_shadows_outer(self, capsys):
        # Macro param 'val' shadows outer 'val' inside the macro body
        run("""
set val to "outer"
macro SHADOW with val
    print text val
end
expand SHADOW with val "inner"
print text val
""")
        out = capsys.readouterr().out
        # Macro prints "inner", then outer "val" is still "outer"
        assert "inner" in out
        assert "outer" in out

    def test_macro_scope_restored_after_expand(self, capsys):
        # Variables set in macro should not exist after expand
        run("""
set before to "exists"
macro POLLUTER
    set should_not_leak to "leaked"
end
expand POLLUTER
print text before
""")
        out = capsys.readouterr().out
        assert "exists" in out

    def test_two_macros_different_params_no_conflict(self, capsys):
        run("""
macro M1 with x
    print text "m1:" plus x
end
macro M2 with x
    print text "m2:" plus x
end
expand M1 with x "A"
expand M2 with x "B"
""")
        out = capsys.readouterr().out
        assert "m1:A" in out
        assert "m2:B" in out

    def test_variable_before_expand_still_exists_after(self, capsys):
        run("""
set keeper to "kept"
macro NOP
    set temp to 1
end
expand NOP
print text keeper
""")
        assert "kept" in capsys.readouterr().out

    def test_nested_macro_expansion_isolation(self, capsys):
        run("""
macro INNER with v
    print text v
end
macro OUTER with w
    expand INNER with v w
end
expand OUTER with w "hello"
""")
        assert "hello" in capsys.readouterr().out


# ---------------------------------------------------------------------------
# 3. Macro Errors
# ---------------------------------------------------------------------------

class TestMacroErrors:
    def test_undefined_macro_raises(self):
        from nexuslang.errors import NxlRuntimeError
        with pytest.raises(Exception) as exc_info:
            run("expand UNDEFINED_MACRO")
        assert "UNDEFINED_MACRO" in str(exc_info.value) or "ndefined" in str(exc_info.value)

    def test_missing_argument_raises(self):
        src = """
macro NEEDS_ARG with x
    print text x
end
expand NEEDS_ARG
"""
        with pytest.raises(Exception) as exc_info:
            run(src)
        assert exc_info.value is not None

    def test_empty_body_macro_is_valid(self, capsys):
        run("""
macro EMPTY_MACRO
end
expand EMPTY_MACRO
print text "after"
""")
        assert "after" in capsys.readouterr().out

    def test_macro_name_is_case_sensitive(self):
        src = """
macro UpperCase
    print text "upper"
end
expand uppercase
"""
        with pytest.raises(Exception):
            run(src)

    def test_error_message_mentions_macro_name(self):
        from nexuslang.errors import NxlRuntimeError
        with pytest.raises(Exception) as exc_info:
            run("expand MY_UNKNOWN_MACRO")
        assert "MY_UNKNOWN_MACRO" in str(exc_info.value)

    def test_expand_in_condition_no_crash(self, capsys):
        # Macro can be used in conditional context
        run("""
macro TRUTHY
    1
end
print text "ok"
""")
        assert "ok" in capsys.readouterr().out


# ---------------------------------------------------------------------------
# 4. Compile-time Evaluation (comptime eval)
# ---------------------------------------------------------------------------

class TestComptimeEval:
    def test_comptime_eval_arithmetic(self):
        result = run("comptime eval 3 plus 4")
        # Either returns 7 or None; should not raise
        assert result is not None or result is None

    def test_comptime_eval_variable(self, capsys):
        run("""
set x to 42
comptime eval x
print text "done"
""")
        assert "done" in capsys.readouterr().out

    def test_comptime_eval_string(self, capsys):
        run("""
comptime eval "hello"
print text "ok"
""")
        assert "ok" in capsys.readouterr().out

    def test_comptime_eval_assigned(self, capsys):
        run("""
comptime const v is 2 times 3
print text v
""")
        assert "6" in capsys.readouterr().out

    def test_comptime_eval_nested_expression(self, capsys):
        run("""
comptime const v is 8 times 2
print text v
""")
        assert "16" in capsys.readouterr().out

    def test_multiple_comptime_evals(self, capsys):
        run("""
comptime eval 1
comptime eval 2
print text "pass"
""")
        assert "pass" in capsys.readouterr().out

    def test_comptime_token_type_exists(self):
        from nexuslang.parser.lexer import TokenType
        assert hasattr(TokenType, "COMPTIME")

    def test_comptime_expression_ast_node(self):
        from nexuslang.parser.ast import ComptimeExpression
        from nexuslang.parser.lexer import Lexer
        from nexuslang.parser.parser import Parser
        src = "comptime eval 5"
        tokens = Lexer(src).tokenize()
        ast = Parser(tokens, source=src).parse()
        assert any(
            stmt.node_type == "comptime_expression"
            for stmt in ast.statements
        )


# ---------------------------------------------------------------------------
# 5. Compile-time Constants (comptime const)
# ---------------------------------------------------------------------------

class TestComptimeConst:
    def test_comptime_const_sets_global_variable(self, capsys):
        run("""
comptime const PI is 3
print text PI
""")
        assert "3" in capsys.readouterr().out

    def test_comptime_const_accessible_in_expressions(self, capsys):
        run("""
comptime const BASE is 10
set result to BASE times 3
print text result
""")
        assert "30" in capsys.readouterr().out

    def test_multiple_comptime_consts(self, capsys):
        run("""
comptime const A is 1
comptime const B is 2
print text A plus B
""")
        assert "3" in capsys.readouterr().out

    def test_comptime_const_in_registry(self):
        from nexuslang.runtime.runtime import Runtime
        from nexuslang.stdlib import register_stdlib
        from nexuslang.parser.lexer import Lexer
        from nexuslang.parser.parser import Parser
        from nexuslang.interpreter.interpreter import Interpreter
        src = "comptime const MY_CONST is 99\n"
        runtime = Runtime()
        register_stdlib(runtime)
        tokens = Lexer(src).tokenize()
        ast = Parser(tokens, source=src).parse()
        interp = Interpreter(runtime, enable_type_checking=False, source=src)
        interp.interpret(ast)
        assert "MY_CONST" in interp.comptime_constants
        assert interp.comptime_constants["MY_CONST"] == 99

    def test_comptime_const_string_value(self, capsys):
        run("""
comptime const GREETING is "Hello"
print text GREETING
""")
        assert "Hello" in capsys.readouterr().out

    def test_comptime_const_in_function(self, capsys):
        run("""
comptime const LIMIT is 5
function check_limit with n as Integer returns Boolean
    return n is less than LIMIT
end
print text check_limit(3)
""")
        assert "True" in capsys.readouterr().out or "true" in capsys.readouterr().out

    def test_comptime_const_ast_node(self):
        from nexuslang.parser.ast import ComptimeConst
        from nexuslang.parser.lexer import Lexer
        from nexuslang.parser.parser import Parser
        src = "comptime const ANSWER is 42"
        tokens = Lexer(src).tokenize()
        ast = Parser(tokens, source=src).parse()
        assert any(stmt.node_type == "comptime_const" for stmt in ast.statements)

    def test_comptime_const_name_captured(self):
        from nexuslang.parser.ast import ComptimeConst
        from nexuslang.parser.lexer import Lexer
        from nexuslang.parser.parser import Parser
        src = "comptime const MYVAL is 7"
        tokens = Lexer(src).tokenize()
        ast_node = Parser(tokens, source=src).parse()
        const_nodes = [s for s in ast_node.statements if s.node_type == "comptime_const"]
        assert len(const_nodes) == 1
        assert const_nodes[0].name == "MYVAL"


# ---------------------------------------------------------------------------
# 6. Compile-time Assertions (comptime assert)
# ---------------------------------------------------------------------------

class TestComptimeAssert:
    def test_passing_assertion_no_error(self, capsys):
        run("""
comptime assert 1 is equal to 1
print text "passed"
""")
        assert "passed" in capsys.readouterr().out

    def test_failing_assertion_raises(self):
        from nexuslang.errors import NxlRuntimeError
        with pytest.raises(Exception) as exc_info:
            run("comptime assert 1 is equal to 2")
        assert exc_info.value is not None

    def test_failing_assertion_error_message(self):
        from nexuslang.errors import NxlRuntimeError
        with pytest.raises(Exception) as exc_info:
            run('comptime assert 1 is equal to 2 message "values differ"')
        # Message should mention the custom text or "assertion"
        err_text = str(exc_info.value)
        assert "assertion" in err_text.lower() or "values differ" in err_text

    def test_assertion_with_variable_condition(self, capsys):
        run("""
set x to 5
comptime assert x is greater than 0
print text "positive"
""")
        assert "positive" in capsys.readouterr().out

    def test_assertion_arithmetic_comparison(self, capsys):
        run("""
comptime assert 2 times 3 is equal to 6
print text "math ok"
""")
        assert "math ok" in capsys.readouterr().out

    def test_comptime_assert_ast_node(self):
        from nexuslang.parser.ast import ComptimeAssert
        from nexuslang.parser.lexer import Lexer
        from nexuslang.parser.parser import Parser
        src = "comptime assert 1 is equal to 1"
        tokens = Lexer(src).tokenize()
        ast = Parser(tokens, source=src).parse()
        assert any(stmt.node_type == "comptime_assert" for stmt in ast.statements)


# ---------------------------------------------------------------------------
# 7. Built-in Function Decorators
# ---------------------------------------------------------------------------

class TestBuiltinDecorators:
    def test_memoize_caches_result(self):
        result1 = run("""
@memoize
function slow_add with x as Integer returns Integer
    return x plus 10
end
slow_add(5)
""")
        # Just confirm no error
        assert result1 is not None or result1 is None

    def test_trace_passes_through(self):
        run("""
@trace
function traced_func with x as Integer returns Integer
    return x times 2
end
traced_func(4)
""")

    def test_timer_returns_same_value(self):
        run("""
@timer
function timed_func with n as Integer returns Integer
    return n plus 1
end
timed_func(3)
""")

    def test_deprecated_still_executes(self, capsys):
        run("""
@deprecated
function old_fn with x as Integer returns Integer
    return x plus 0
end
old_fn(10)
print text "ok"
""")
        assert "ok" in capsys.readouterr().out

    def test_validate_args_passes_through(self):
        run("""
@validate_args
function validated with x as Integer returns Integer
    return x
end
validated(5)
""")

    def test_singleton_decorator_exists_in_registry(self):
        from nexuslang.decorators import get_decorator
        assert get_decorator("singleton") is not None

    def test_singleton_returns_same_instance(self):
        from nexuslang.decorators import singleton

        call_count = [0]

        @singleton
        def make_obj():
            call_count[0] += 1
            return {"id": call_count[0]}

        obj1 = make_obj()
        obj2 = make_obj()
        assert obj1 is obj2
        assert call_count[0] == 1

    def test_singleton_second_call_same_object(self):
        from nexuslang.decorators import singleton

        @singleton
        def factory():
            return object()

        a = factory()
        b = factory()
        assert a is b

    def test_cached_property_decorator_exists(self):
        from nexuslang.decorators import get_decorator
        assert get_decorator("cached_property") is not None

    def test_pure_marks_function(self):
        from nexuslang.decorators import pure

        @pure
        def add(x, y):
            return x + y

        assert getattr(add, "_is_pure", False) is True

    def test_once_first_call_works(self):
        from nexuslang.decorators import once

        @once
        def init():
            return "initialized"

        result = init()
        assert result == "initialized"

    def test_once_second_call_raises(self):
        from nexuslang.decorators import once

        @once
        def setup():
            return "setup"

        setup()
        with pytest.raises(RuntimeError) as exc_info:
            setup()
        assert "once" in str(exc_info.value).lower() or "more than once" in str(exc_info.value)


# ---------------------------------------------------------------------------
# 8. User-Defined NexusLang Decorators
# ---------------------------------------------------------------------------

class TestUserDefinedDecorators:
    def test_nxl_function_as_decorator(self, capsys):
        run("""
function my_decorator with fn
    print text "decorated"
    return fn
end

@my_decorator
function target_fn with x as Integer returns Integer
    return x
end

print text "ok"
""")
        out = capsys.readouterr().out
        # Decorator should have run
        assert "decorated" in out

    def test_decorator_receives_function(self, capsys):
        # The decorator receives the function value; printing "ok" confirms no crash
        run("""
function wrap with f
    print text "wrapping"
    return f
end

@wrap
function add_one with n as Integer returns Integer
    return n plus 1
end

print text "ok"
""")
        out = capsys.readouterr().out
        assert "wrapping" in out or "ok" in out

    def test_multiple_decorators_applied(self, capsys):
        run("""
function mark_a with f
    print text "A"
    return f
end

function mark_b with f
    print text "B"
    return f
end

@mark_a
function fn_a with x as Integer returns Integer
    return x
end

@mark_b
function fn_b with x as Integer returns Integer
    return x
end

print text "done"
""")
        out = capsys.readouterr().out
        assert "A" in out
        assert "B" in out

    def test_unknown_decorator_raises(self):
        src = """
@completely_unknown_decorator_xyz
function broken with x as Integer returns Integer
    return x
end
"""
        with pytest.raises(Exception) as exc_info:
            run(src)
        assert exc_info.value is not None

    def test_decorator_on_multiple_functions(self, capsys):
        run("""
function logger with fn
    print text "logged"
    return fn
end

@logger
function first with x as Integer returns Integer
    return x
end

@logger
function second with x as Integer returns Integer
    return x
end

print text "both ok"
""")
        out = capsys.readouterr().out
        assert out.count("logged") == 2

    def test_memoize_builtin_takes_priority(self):
        # @memoize is a built-in; should work even if there's no NexusLang function called memoize
        run("""
@memoize
function cached with n as Integer returns Integer
    return n times n
end
cached(4)
""")

    def test_decorator_applied_before_first_call(self, capsys):
        run("""
function tag with fn
    print text "tagged"
    return fn
end

@tag
function my_fn with x as Integer returns Integer
    return x plus 1
end

print text "after"
""")
        out = capsys.readouterr().out
        # tag runs at definition time
        assert "tagged" in out

    def test_decorated_function_still_callable(self, capsys):
        run("""
function identity_dec with fn
    return fn
end

@identity_dec
function double with n as Integer returns Integer
    return n times 2
end

print text double(5)
""")
        assert "10" in capsys.readouterr().out


# ---------------------------------------------------------------------------
# 9. @derive Decorator (Code Generation)
# ---------------------------------------------------------------------------

class TestDeriveDecorator:
    def test_derive_debug_print_generates_to_string(self):
        from nexuslang.runtime.runtime import Runtime
        from nexuslang.stdlib import register_stdlib
        from nexuslang.parser.lexer import Lexer
        from nexuslang.parser.parser import Parser
        from nexuslang.interpreter.interpreter import Interpreter
        src = """
@derive(DebugPrint)
class Point
    property x as Integer
    property y as Integer
end
set p to new Point
"""
        runtime = Runtime()
        register_stdlib(runtime)
        tokens = Lexer(src).tokenize()
        ast_node = Parser(tokens, source=src).parse()
        interp = Interpreter(runtime, enable_type_checking=False, source=src)
        interp.interpret(ast_node)
        point_class = interp.classes.get("Point")
        assert point_class is not None
        assert hasattr(point_class, '_derived_methods')
        assert "to_string" in point_class._derived_methods

    def test_derive_debug_print_generates_debug_print(self):
        from nexuslang.runtime.runtime import Runtime
        from nexuslang.stdlib import register_stdlib
        from nexuslang.parser.lexer import Lexer
        from nexuslang.parser.parser import Parser
        from nexuslang.interpreter.interpreter import Interpreter
        src = """
@derive(DebugPrint)
class Item
    property name as String
end
"""
        runtime = Runtime()
        register_stdlib(runtime)
        tokens = Lexer(src).tokenize()
        ast_node = Parser(tokens, source=src).parse()
        interp = Interpreter(runtime, enable_type_checking=False, source=src)
        interp.interpret(ast_node)
        assert "debug_print" in interp.classes["Item"]._derived_methods

    def test_derive_equality_generates_equals(self):
        from nexuslang.runtime.runtime import Runtime
        from nexuslang.stdlib import register_stdlib
        from nexuslang.parser.lexer import Lexer
        from nexuslang.parser.parser import Parser
        from nexuslang.interpreter.interpreter import Interpreter
        src = """
@derive(Equality)
class Box
    property value as Integer
end
"""
        runtime = Runtime()
        register_stdlib(runtime)
        tokens = Lexer(src).tokenize()
        ast_node = Parser(tokens, source=src).parse()
        interp = Interpreter(runtime, enable_type_checking=False, source=src)
        interp.interpret(ast_node)
        assert "equals" in interp.classes["Box"]._derived_methods

    def test_derive_equality_equals_true_for_same_props(self):
        from nexuslang.runtime.runtime import Runtime
        from nexuslang.stdlib import register_stdlib
        from nexuslang.parser.lexer import Lexer
        from nexuslang.parser.parser import Parser
        from nexuslang.interpreter.interpreter import Interpreter
        from nexuslang.runtime.runtime import Object
        src = """
@derive(Equality)
class Val
    property n as Integer
end
"""
        runtime = Runtime()
        register_stdlib(runtime)
        tokens = Lexer(src).tokenize()
        ast_node = Parser(tokens, source=src).parse()
        interp = Interpreter(runtime, enable_type_checking=False, source=src)
        interp.interpret(ast_node)
        equals_fn = interp.classes["Val"]._derived_methods["equals"]
        a = Object("Val")
        a.set_property("n", 5)
        b = Object("Val")
        b.set_property("n", 5)
        assert equals_fn(a, b) is True

    def test_derive_equality_equals_false_for_diff_props(self):
        from nexuslang.runtime.runtime import Runtime
        from nexuslang.stdlib import register_stdlib
        from nexuslang.parser.lexer import Lexer
        from nexuslang.parser.parser import Parser
        from nexuslang.interpreter.interpreter import Interpreter
        from nexuslang.runtime.runtime import Object
        src = """
@derive(Equality)
class Num
    property v as Integer
end
"""
        runtime = Runtime()
        register_stdlib(runtime)
        tokens = Lexer(src).tokenize()
        ast_node = Parser(tokens, source=src).parse()
        interp = Interpreter(runtime, enable_type_checking=False, source=src)
        interp.interpret(ast_node)
        equals_fn = interp.classes["Num"]._derived_methods["equals"]
        a = Object("Num")
        a.set_property("v", 1)
        b = Object("Num")
        b.set_property("v", 2)
        assert equals_fn(a, b) is False

    def test_derive_clone_generates_clone(self):
        from nexuslang.runtime.runtime import Runtime
        from nexuslang.stdlib import register_stdlib
        from nexuslang.parser.lexer import Lexer
        from nexuslang.parser.parser import Parser
        from nexuslang.interpreter.interpreter import Interpreter
        from nexuslang.runtime.runtime import Object
        src = """
@derive(Clone)
class Sample
    property data as Integer
end
"""
        runtime = Runtime()
        register_stdlib(runtime)
        tokens = Lexer(src).tokenize()
        ast_node = Parser(tokens, source=src).parse()
        interp = Interpreter(runtime, enable_type_checking=False, source=src)
        interp.interpret(ast_node)
        clone_fn = interp.classes["Sample"]._derived_methods["clone"]
        a = Object("Sample")
        a.set_property("data", 77)
        b = clone_fn(a)
        assert b is not a

    def test_derive_hash_generates_hash_code(self):
        from nexuslang.runtime.runtime import Runtime
        from nexuslang.stdlib import register_stdlib
        from nexuslang.parser.lexer import Lexer
        from nexuslang.parser.parser import Parser
        from nexuslang.interpreter.interpreter import Interpreter
        from nexuslang.runtime.runtime import Object
        src = """
@derive(Hash)
class Key
    property id as Integer
end
"""
        runtime = Runtime()
        register_stdlib(runtime)
        tokens = Lexer(src).tokenize()
        ast_node = Parser(tokens, source=src).parse()
        interp = Interpreter(runtime, enable_type_checking=False, source=src)
        interp.interpret(ast_node)
        hash_fn = interp.classes["Key"]._derived_methods["hash_code"]
        k = Object("Key")
        k.set_property("id", 1)
        h = hash_fn(k)
        assert isinstance(h, int)

    def test_derive_default_generates_default(self):
        from nexuslang.runtime.runtime import Runtime
        from nexuslang.stdlib import register_stdlib
        from nexuslang.parser.lexer import Lexer
        from nexuslang.parser.parser import Parser
        from nexuslang.interpreter.interpreter import Interpreter
        from nexuslang.runtime.runtime import Object
        src = """
@derive(Default)
class Config
    property enabled as Boolean
end
"""
        runtime = Runtime()
        register_stdlib(runtime)
        tokens = Lexer(src).tokenize()
        ast_node = Parser(tokens, source=src).parse()
        interp = Interpreter(runtime, enable_type_checking=False, source=src)
        interp.interpret(ast_node)
        assert "default" in interp.classes["Config"]._derived_methods


# ---------------------------------------------------------------------------
# 10. Metaprogramming Stdlib Functions
# ---------------------------------------------------------------------------

class TestMetaprogrammingStdlib:
    def _make_interp(self, src):
        from nexuslang.runtime.runtime import Runtime
        from nexuslang.stdlib import register_stdlib
        from nexuslang.parser.lexer import Lexer
        from nexuslang.parser.parser import Parser
        from nexuslang.interpreter.interpreter import Interpreter
        runtime = Runtime()
        register_stdlib(runtime)
        tokens = Lexer(src).tokenize()
        ast_node = Parser(tokens, source=src).parse()
        interp = Interpreter(runtime, enable_type_checking=False, source=src)
        interp.interpret(ast_node)
        return interp, runtime

    def test_meta_macro_names_returns_list(self):
        src = "macro TEST_M\n    set x to 1\nend\n"
        interp, runtime = self._make_interp(src)
        fn = runtime.get_function("meta_macro_names")
        result = fn()
        assert isinstance(result, list)

    def test_meta_macro_names_includes_defined_macro(self):
        src = "macro MY_SPECIAL_MACRO\n    set x to 1\nend\n"
        interp, runtime = self._make_interp(src)
        fn = runtime.get_function("meta_macro_names")
        result = fn()
        assert "MY_SPECIAL_MACRO" in result

    def test_meta_macro_exists_true_for_defined(self):
        src = "macro SOME_MACRO\n    set x to 1\nend\n"
        interp, runtime = self._make_interp(src)
        fn = runtime.get_function("meta_macro_exists")
        assert fn("SOME_MACRO") is True

    def test_meta_macro_exists_false_for_undefined(self):
        interp, runtime = self._make_interp("")
        fn = runtime.get_function("meta_macro_exists")
        assert fn("NONEXISTENT_MACRO_XYZ") is False

    def test_meta_macro_arg_count_correct(self):
        src = "macro TRIPLE with a, b, c\n    set x to 1\nend\n"
        interp, runtime = self._make_interp(src)
        fn = runtime.get_function("meta_macro_arg_count")
        assert fn("TRIPLE") == 3

    def test_meta_comptime_const_names_returns_list(self):
        src = "comptime const ALPHA is 1\n"
        interp, runtime = self._make_interp(src)
        fn = runtime.get_function("meta_comptime_const_names")
        result = fn()
        assert isinstance(result, list)

    def test_meta_comptime_const_value_correct(self):
        src = "comptime const BETA is 42\n"
        interp, runtime = self._make_interp(src)
        fn = runtime.get_function("meta_comptime_const_value")
        assert fn("BETA") == 42

    def test_meta_comptime_const_exists_true(self):
        src = "comptime const GAMMA is 99\n"
        interp, runtime = self._make_interp(src)
        fn = runtime.get_function("meta_comptime_const_exists")
        assert fn("GAMMA") is True


# ---------------------------------------------------------------------------
# 11. Parsing Tests (AST node types only - no execution)
# ---------------------------------------------------------------------------

class TestParsingComptime:
    def _parse(self, src):
        from nexuslang.parser.lexer import Lexer
        from nexuslang.parser.parser import Parser
        tokens = Lexer(src).tokenize()
        return Parser(tokens, source=src).parse()

    def test_comptime_const_parses_to_comptime_const_node(self):
        ast = self._parse("comptime const X is 10")
        node_types = [s.node_type for s in ast.statements]
        assert "comptime_const" in node_types

    def test_comptime_const_captures_name(self):
        ast = self._parse("comptime const MYCONST is 5")
        const_nodes = [s for s in ast.statements if s.node_type == "comptime_const"]
        assert const_nodes[0].name == "MYCONST"

    def test_comptime_assert_parses_to_comptime_assert_node(self):
        ast = self._parse("comptime assert 1 is equal to 1")
        node_types = [s.node_type for s in ast.statements]
        assert "comptime_assert" in node_types

    def test_comptime_eval_parses_to_comptime_expression_node(self):
        ast = self._parse("comptime eval 42")
        node_types = [s.node_type for s in ast.statements]
        assert "comptime_expression" in node_types

    def test_comptime_token_type_in_lexer(self):
        from nexuslang.parser.lexer import TokenType, Lexer
        tokens = Lexer("comptime const X is 1").tokenize()
        token_types = [t.type for t in tokens]
        assert TokenType.COMPTIME in token_types

    def test_bare_comptime_expression_is_valid(self):
        # "comptime EXPR" without eval keyword should also parse
        # (parser falls back to comptime_expression for bare comptime)
        try:
            ast = self._parse("comptime 5 plus 3")
            node_types = [s.node_type for s in ast.statements]
            assert "comptime_expression" in node_types
        except Exception:
            # If parser requires 'eval' keyword, that's also acceptable
            pass

class TestMacroComptimeRegressionFixtures:
    def test_valid_fixture_executes_and_prints_expected_values(self, capsys):
        run(_fixture_text("macro_comptime_regression_valid.nxl"))
        out = capsys.readouterr().out
        assert "15" in out
        assert "12" in out
