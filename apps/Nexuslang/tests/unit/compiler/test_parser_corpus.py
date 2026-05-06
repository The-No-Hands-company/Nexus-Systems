"""
Comprehensive parser test corpus for NexusLang.

Covers every major AST node type and grammar production rule.
Each test parses a canonical snippet and validates:
  1. Parsing does not crash.
  2. The top-level statement is the expected AST node type.
  3. Key attributes (name, operator, fields, …) are correctly set.

This corpus is the basis for parser regression detection: any change to
parser/parser.py that silently alters AST structure will be caught here.
"""

import os
import sys
from pathlib import Path
import pytest

_ROOT = str(Path(__file__).resolve().parents[4])
_SRC = os.path.join(_ROOT, "src")
if _SRC not in sys.path:
    sys.path.insert(0, _SRC)

from nexuslang.parser.lexer import Lexer
from nexuslang.parser.parser import Parser
from nexuslang.parser.ast import (
    Program,
    VariableDeclaration,
    IndexAssignment,
    MemberAssignment,
    FunctionDefinition,
    AsyncFunctionDefinition,
    Parameter,
    ClassDefinition,
    PropertyDeclaration,
    MethodDefinition,
    InterfaceDefinition,
    TraitDefinition,
    StructDefinition,
    StructField,
    UnionDefinition,
    EnumDefinition,
    EnumMember,
    IfStatement,
    WhileLoop,
    ForLoop,
    RepeatNTimesLoop,
    RepeatWhileLoop,
    MatchExpression,
    MatchCase,
    TryCatch,
    RaiseStatement,
    ReturnStatement,
    BreakStatement,
    ContinueStatement,
    PrintStatement,
    ImportStatement,
    SelectiveImport,
    ExportStatement,
    TypeAliasDefinition,
    TestBlock,
    RequireStatement,
    EnsureStatement,
    GuaranteeStatement,
    InvariantStatement,
    MemoryAllocation,
    MemoryDeallocation,
    BinaryOperation,
    UnaryOperation,
    Identifier,
    Literal,
    FunctionCall,
    ListExpression,
    DictExpression,
    FStringExpression,
    ListComprehension,
    LambdaExpression,
    TernaryExpression,
    AddressOfExpression,
    DereferenceExpression,
    SizeofExpression,
    IndexExpression,
    MemberAccess,
    ObjectInstantiation,
    TypeCastExpression,
    ExpectStatement,
    InlineAssembly,
    ExternFunctionDeclaration,
    AwaitExpression,
    YieldExpression,
    GenericTypeInstantiation,
)


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def parse(source: str) -> Program:
    """Lex + parse *source* and return the Program node."""
    lexer = Lexer(source)
    tokens = lexer.tokenize()
    parser = Parser(tokens)
    return parser.parse()


def first(source: str):
    """Return the first statement in *source*."""
    prog = parse(source)
    assert isinstance(prog, Program)
    assert len(prog.statements) >= 1, f"Expected at least one statement in:\n{source!r}"
    return prog.statements[0]


# ---------------------------------------------------------------------------
# 1. Variable declarations
# ---------------------------------------------------------------------------

class TestVariableDeclarations:
    def test_integer_literal(self):
        node = first("set count to 0")
        assert isinstance(node, VariableDeclaration)
        assert node.name == "count"
        assert isinstance(node.value, Literal)
        assert node.value.value == 0

    def test_float_literal(self):
        node = first("set pi to 3.14")
        assert isinstance(node, VariableDeclaration)
        assert node.name == "pi"
        assert abs(node.value.value - 3.14) < 1e-9

    def test_string_literal(self):
        node = first('set greeting to "Hello"')
        assert isinstance(node, VariableDeclaration)
        assert node.name == "greeting"
        assert node.value.value == "Hello"

    def test_boolean_true(self):
        node = first("set flag to true")
        assert isinstance(node, VariableDeclaration)
        assert node.value.value is True

    def test_boolean_false(self):
        node = first("set done to false")
        assert isinstance(node, VariableDeclaration)
        assert node.value.value is False

    def test_typed_declaration(self):
        node = first("set x to 10 as Integer")
        assert isinstance(node, VariableDeclaration)
        assert node.name == "x"

    def test_null_literal(self):
        node = first("set ptr to null")
        assert isinstance(node, VariableDeclaration)
        assert node.name == "ptr"

    def test_negative_integer(self):
        node = first("set debt to -500")
        assert isinstance(node, VariableDeclaration)
        assert node.name == "debt"

    def test_list_literal_value(self):
        node = first("set nums to [1, 2, 3]")
        assert isinstance(node, VariableDeclaration)
        assert node.name == "nums"
        assert isinstance(node.value, ListExpression)

    def test_dict_literal_value(self):
        node = first('set info to {"name": "Alice", "age": 30}')
        assert isinstance(node, VariableDeclaration)
        assert node.name == "info"
        assert isinstance(node.value, DictExpression)


# ---------------------------------------------------------------------------
# 2. Assignments
# ---------------------------------------------------------------------------

class TestAssignments:
    def test_index_assignment(self):
        # Second statement is the index assignment
        src = "set x to 1\nset x[0] to 99"
        prog = parse(src)
        assert len(prog.statements) == 2
        node = prog.statements[1]
        assert isinstance(node, IndexAssignment)

    def test_member_assignment_dot(self):
        node = first("set obj.field to 42")
        assert isinstance(node, MemberAssignment)


# ---------------------------------------------------------------------------
# 3. Function definitions
# ---------------------------------------------------------------------------

class TestFunctionDefinitions:
    def test_simple_function(self):
        src = "function greet\n    print text \"Hello\"\nend"
        node = first(src)
        assert isinstance(node, FunctionDefinition)
        assert node.name == "greet"

    def test_function_with_params_and_return(self):
        src = (
            "function add with a as Integer and b as Integer returns Integer\n"
            "    return a plus b\n"
            "end"
        )
        node = first(src)
        assert isinstance(node, FunctionDefinition)
        assert node.name == "add"
        assert len(node.parameters) == 2
        assert node.parameters[0].name == "a"
        assert node.parameters[1].name == "b"

    def test_function_with_default_parameter(self):
        src = (
            'function greet with name as String default to "World" returns String\n'
            '    return "Hello " plus name\n'
            "end"
        )
        node = first(src)
        assert isinstance(node, FunctionDefinition)
        assert node.name == "greet"
        assert any(p.default_value is not None for p in node.parameters)

    def test_function_with_variadic_parameter(self):
        src = (
            "function sum_all with *numbers as Integer returns Integer\n"
            "    set total to 0\n"
            "    return total\n"
            "end"
        )
        node = first(src)
        assert isinstance(node, FunctionDefinition)

    def test_async_function(self):
        src = (
            "async function fetch with url as String returns String\n"
            "    return url\n"
            "end"
        )
        node = first(src)
        assert isinstance(node, AsyncFunctionDefinition)
        assert node.name == "fetch"

    def test_function_return_no_value(self):
        src = "function do_nothing\n    return\nend"
        node = first(src)
        assert isinstance(node, FunctionDefinition)

    def test_function_with_yield_statement(self):
        src = "function gen\n    yield 1\nend"
        node = first(src)
        assert isinstance(node, FunctionDefinition)
        assert len(node.body) == 1
        assert isinstance(node.body[0], YieldExpression)
        assert isinstance(node.body[0].value, Literal)
        assert node.body[0].value.value == 1

    def test_nested_function_call_in_body(self):
        src = (
            "function wrapper\n"
            '    print text "start"\n'
            "    return 0\n"
            "end"
        )
        node = first(src)
        assert isinstance(node, FunctionDefinition)
        assert len(node.body) >= 1

    def test_function_with_three_params(self):
        src = (
            "function clamp with value as Integer and low as Integer and high as Integer returns Integer\n"
            "    return value\n"
            "end"
        )
        node = first(src)
        assert isinstance(node, FunctionDefinition)
        assert len(node.parameters) == 3


# ---------------------------------------------------------------------------
# 4. Class definitions
# ---------------------------------------------------------------------------

class TestClassDefinitions:
    def test_simple_class(self):
        src = (
            "class Animal\n"
            "    private set name to String\n"
            "end"
        )
        node = first(src)
        assert isinstance(node, ClassDefinition)
        assert node.name == "Animal"

    def test_class_with_method(self):
        src = (
            "class Counter\n"
            "    public function increment\n"
            "        set this.count to this.count plus 1\n"
            "    end\n"
            "end"
        )
        node = first(src)
        assert isinstance(node, ClassDefinition)
        assert node.name == "Counter"

    def test_class_extends(self):
        src = (
            "class Dog extends Animal\n"
            "    public function speak\n"
            '        print text "Woof"\n'
            "    end\n"
            "end"
        )
        node = first(src)
        assert isinstance(node, ClassDefinition)
        assert node.name == "Dog"
        assert "Animal" in node.parent_classes

    def test_class_implements_interface(self):
        src = (
            "class Book implements Printable\n"
            "    public function to_string returns String\n"
            '        return "Book"\n'
            "    end\n"
            "end"
        )
        node = first(src)
        assert isinstance(node, ClassDefinition)
        assert "Printable" in node.implemented_interfaces

    def test_class_extends_and_implements(self):
        src = (
            "class GradStudent extends Student implements Printable\n"
            "    public function to_string returns String\n"
            '        return "GradStudent"\n'
            "    end\n"
            "end"
        )
        node = first(src)
        assert isinstance(node, ClassDefinition)
        assert "Student" in node.parent_classes
        assert "Printable" in node.implemented_interfaces

    def test_class_with_typed_property(self):
        src = (
            "class Point\n"
            "    private set x to Integer\n"
            "    private set y to Integer\n"
            "end"
        )
        node = first(src)
        assert isinstance(node, ClassDefinition)
        assert node.name == "Point"


# ---------------------------------------------------------------------------
# 5. Interfaces and Traits
# ---------------------------------------------------------------------------

class TestInterfacesAndTraits:
    def test_interface_definition(self):
        src = (
            "interface Printable\n"
            "    public function to_string returns String\n"
            "end"
        )
        node = first(src)
        assert isinstance(node, InterfaceDefinition)
        assert node.name == "Printable"

    def test_interface_multiple_methods(self):
        src = (
            "interface Comparable\n"
            "    public function compare_to with other returns Integer\n"
            "    public function check with other returns Boolean\n"
            "end"
        )
        node = first(src)
        assert isinstance(node, InterfaceDefinition)
        assert len(node.methods) >= 2

    def test_trait_definition(self):
        src = (
            "define a trait called Greetable\n"
            "    function greet returns String\n"
            "end"
        )
        node = first(src)
        assert isinstance(node, TraitDefinition)
        assert node.name == "Greetable"

    def test_abstract_class(self):
        from nexuslang.parser.ast import AbstractClassDefinition
        src = (
            "abstract class Shape\n"
            "    abstract function area returns Float\n"
            "end"
        )
        node = first(src)
        # Abstract class is either AbstractClassDefinition or ClassDefinition
        assert isinstance(node, (AbstractClassDefinition, ClassDefinition))


# ---------------------------------------------------------------------------
# 6. Structs, Unions, Enums
# ---------------------------------------------------------------------------

class TestStructUnionEnum:
    def test_struct_basic(self):
        src = (
            "struct Point\n"
            "    x as Integer\n"
            "    y as Integer\n"
            "end"
        )
        node = first(src)
        assert isinstance(node, StructDefinition)
        assert node.name == "Point"
        assert len(node.fields) == 2

    def test_struct_field_names(self):
        src = (
            "struct RGB\n"
            "    r as Integer\n"
            "    g as Integer\n"
            "    b as Integer\n"
            "end"
        )
        node = first(src)
        assert isinstance(node, StructDefinition)
        field_names = [f.name for f in node.fields]
        assert "r" in field_names
        assert "g" in field_names
        assert "b" in field_names

    def test_union_basic(self):
        src = (
            "union Number\n"
            "    as_integer as Integer\n"
            "    as_float as Float\n"
            "end"
        )
        node = first(src)
        assert isinstance(node, UnionDefinition)
        assert node.name == "Number"
        assert len(node.fields) == 2

    def test_enum_auto_values(self):
        src = (
            "enum Direction\n"
            "    North\n"
            "    South\n"
            "    East\n"
            "    West\n"
            "end"
        )
        node = first(src)
        assert isinstance(node, EnumDefinition)
        assert node.name == "Direction"
        assert len(node.members) == 4

    def test_enum_explicit_values(self):
        src = (
            "enum Status\n"
            "    Success = 0\n"
            "    Error = 1\n"
            "    Pending = 2\n"
            "end"
        )
        node = first(src)
        assert isinstance(node, EnumDefinition)
        assert node.name == "Status"
        assert len(node.members) == 3

    def test_enum_member_names(self):
        src = (
            "enum LogLevel\n"
            "    DEBUG\n"
            "    INFO\n"
            "    ERROR\n"
            "end"
        )
        node = first(src)
        assert isinstance(node, EnumDefinition)
        names = [m.name for m in node.members]
        assert "DEBUG" in names
        assert "INFO" in names
        assert "ERROR" in names


# ---------------------------------------------------------------------------
# 7. Control flow
# ---------------------------------------------------------------------------

class TestControlFlow:
    def test_if_statement(self):
        src = "if x is greater than 0\n    print text \"positive\"\nend"
        node = first(src)
        assert isinstance(node, IfStatement)
        assert node.condition is not None

    def test_if_else(self):
        src = (
            "if x is greater than 0\n"
            '    print text "positive"\n'
            "else\n"
            '    print text "non-positive"\n'
            "end"
        )
        node = first(src)
        assert isinstance(node, IfStatement)
        assert node.else_block is not None

    def test_if_else_if_else(self):
        src = (
            "if score is greater than or equal to 90\n"
            '    print text "A"\n'
            "else if score is greater than or equal to 80\n"
            '    print text "B"\n'
            "else\n"
            '    print text "C"\n'
            "end"
        )
        node = first(src)
        assert isinstance(node, IfStatement)
        # elif blocks are chained as nested IfStatements inside else_block
        assert node.else_block is not None

    def test_while_loop(self):
        src = (
            "while count is less than 10\n"
            "    set count to count plus 1\n"
            "end"
        )
        node = first(src)
        assert isinstance(node, WhileLoop)
        assert node.condition is not None

    def test_for_each_loop(self):
        src = (
            "for each item in collection\n"
            "    print text item\n"
            "end"
        )
        node = first(src)
        assert isinstance(node, ForLoop)

    def test_repeat_n_times(self):
        src = (
            "repeat 5 times\n"
            '    print text "hello"\n'
            "end"
        )
        node = first(src)
        assert isinstance(node, RepeatNTimesLoop)
        assert node.count is not None

    def test_repeat_while(self):
        src = (
            "repeat while x is greater than 0\n"
            "    set x to x minus 1\n"
            "end"
        )
        node = first(src)
        assert isinstance(node, RepeatWhileLoop)

    def test_break_inside_loop(self):
        src = (
            "while true\n"
            "    break\n"
            "end"
        )
        prog = parse(src)
        while_node = prog.statements[0]
        assert isinstance(while_node, WhileLoop)
        assert any(isinstance(s, BreakStatement) for s in while_node.body)

    def test_continue_inside_loop(self):
        src = (
            "for each x in nums\n"
            "    continue\n"
            "end"
        )
        prog = parse(src)
        loop = prog.statements[0]
        assert isinstance(loop, ForLoop)
        assert any(isinstance(s, ContinueStatement) for s in loop.body)

    def test_nested_if(self):
        src = (
            "if a is greater than 0\n"
            "    if b is greater than 0\n"
            '        print text "both positive"\n'
            "    end\n"
            "end"
        )
        node = first(src)
        assert isinstance(node, IfStatement)
        assert len(node.then_block) >= 1


# ---------------------------------------------------------------------------
# 8. Pattern matching
# ---------------------------------------------------------------------------

class TestPatternMatching:
    def test_basic_match(self):
        src = (
            "match n with\n"
            "    case 0\n"
            "        return \"zero\"\n"
            "    case 1\n"
            "        return \"one\"\n"
            "    case n\n"
            "        return \"other\"\n"
            "end"
        )
        node = first(src)
        assert isinstance(node, MatchExpression)
        assert node.expression is not None
        assert len(node.cases) >= 3

    def test_match_with_guard(self):
        src = (
            "match n with\n"
            "    case 0\n"
            "        return \"zero\"\n"
            "    case n if n is less than 0\n"
            "        return \"negative\"\n"
            "    case n\n"
            "        return \"positive\"\n"
            "end"
        )
        node = first(src)
        assert isinstance(node, MatchExpression)
        # At least one case has a guard
        guards = [c for c in node.cases if c.guard is not None]
        assert len(guards) >= 1

    def test_match_wildcard(self):
        src = (
            "match value with\n"
            '    case "hello"\n'
            '        print text "greeting"\n'
            '    case _\n'
            '        print text "unknown"\n'
            "end"
        )
        node = first(src)
        assert isinstance(node, MatchExpression)

    def test_match_string_cases(self):
        src = (
            "match cmd with\n"
            '    case "start"\n'
            '        print text "starting"\n'
            '    case "stop"\n'
            '        print text "stopping"\n'
            '    case _\n'
            '        print text "unknown command"\n'
            "end"
        )
        node = first(src)
        assert isinstance(node, MatchExpression)
        assert len(node.cases) == 3


# ---------------------------------------------------------------------------
# 9. Error handling
# ---------------------------------------------------------------------------

class TestErrorHandling:
    def test_try_catch(self):
        src = (
            "try\n"
            "    set x to 1\n"
            "catch err\n"
            '    print text "caught"\n'
            "end"
        )
        node = first(src)
        assert isinstance(node, TryCatch)
        assert node.try_block is not None
        assert node.catch_block is not None

    def test_try_catch_exception_var(self):
        src = (
            "try\n"
            "    risky_operation\n"
            "catch my_error\n"
            "    print text my_error\n"
            "end"
        )
        node = first(src)
        assert isinstance(node, TryCatch)
        assert node.exception_var == "my_error"

    def test_raise_string_error(self):
        src = 'raise error "something went wrong"'
        node = first(src)
        assert isinstance(node, RaiseStatement)

    def test_raise_exception_object(self):
        src = 'raise ValueError "invalid value"'
        node = first(src)
        assert isinstance(node, RaiseStatement)
        assert node.exception_type == "ValueError"

    def test_assert_statement(self):
        # NexusLang assertion syntax uses 'expect ... to equal ...'
        src = "expect x to equal 0"
        node = first(src)
        assert isinstance(node, ExpectStatement)


# ---------------------------------------------------------------------------
# 10. Import / export / type alias
# ---------------------------------------------------------------------------

class TestImportsExports:
    def test_import_module(self):
        src = "import math"
        node = first(src)
        assert isinstance(node, ImportStatement)
        assert node.module_name == "math"

    def test_import_with_alias(self):
        src = "import math as m"
        node = first(src)
        assert isinstance(node, ImportStatement)
        assert node.module_name == "math"
        assert node.alias == "m"

    def test_selective_import(self):
        # Selective import uses 'from module import name' syntax
        src = "from math import sqrt"
        node = first(src)
        assert isinstance(node, SelectiveImport)

    def test_export_statement(self):
        # export wraps a statement, producing an ExportStatement node
        src = 'export set msg to "hello"'
        node = first(src)
        assert isinstance(node, ExportStatement)

    def test_type_alias(self):
        # Verbose natural-language type alias syntax
        src = "create a type alias called NumberList that is a list of integers."
        node = first(src)
        assert isinstance(node, TypeAliasDefinition)
        assert node.name == "NumberList"


# ---------------------------------------------------------------------------
# 11. Expressions
# ---------------------------------------------------------------------------

class TestExpressions:
    def test_addition(self):
        node = first("set result to a plus b")
        assert isinstance(node, VariableDeclaration)
        assert isinstance(node.value, BinaryOperation)
        # operator is a Token object; check its lexeme
        assert node.value.operator.lexeme == "plus"

    def test_subtraction(self):
        node = first("set result to a minus b")
        assert isinstance(node.value, BinaryOperation)
        assert node.value.operator.lexeme == "minus"

    def test_multiplication(self):
        node = first("set result to a times b")
        assert isinstance(node.value, BinaryOperation)
        assert node.value.operator.lexeme == "times"

    def test_division(self):
        node = first("set result to a divided by b")
        assert isinstance(node.value, BinaryOperation)
        assert "divided" in node.value.operator.lexeme

    def test_modulo(self):
        node = first("set result to a modulo b")
        assert isinstance(node.value, BinaryOperation)

    def test_logical_and(self):
        node = first("set ok to a and b")
        assert isinstance(node.value, BinaryOperation)

    def test_logical_or(self):
        node = first("set ok to a or b")
        assert isinstance(node.value, BinaryOperation)

    def test_unary_not(self):
        node = first("set inverted to not flag")
        assert isinstance(node.value, UnaryOperation)

    def test_comparison_greater(self):
        node = first("set big to x is greater than 10")
        assert isinstance(node.value, BinaryOperation)

    def test_comparison_equal(self):
        node = first("set same to x equals y")
        assert isinstance(node.value, BinaryOperation)

    def test_function_call_no_args(self):
        node = first("set result to do_work")
        # Either Identifier or FunctionCall
        assert isinstance(node.value, (Identifier, FunctionCall))

    def test_function_call_with_args(self):
        node = first("set result to add with a: 1 and b: 2")
        assert isinstance(node.value, FunctionCall)

    def test_fstring_expression(self):
        node = first('set msg to f"Hello {name}!"')
        assert isinstance(node, VariableDeclaration)
        assert isinstance(node.value, FStringExpression)

    def test_list_comprehension(self):
        node = first("set squares to [x times x for x in numbers]")
        assert isinstance(node, VariableDeclaration)
        assert isinstance(node.value, ListComprehension)

    def test_index_expression(self):
        node = first("set first to arr[0]")
        assert isinstance(node, VariableDeclaration)
        assert isinstance(node.value, IndexExpression)

    def test_member_access(self):
        node = first("set name to person.name")
        assert isinstance(node, VariableDeclaration)
        assert isinstance(node.value, MemberAccess)

    def test_object_instantiation(self):
        node = first("set dog to new Dog")
        assert isinstance(node, VariableDeclaration)
        assert isinstance(node.value, ObjectInstantiation)

    def test_type_cast(self):
        node = first("set s to convert x to String")
        assert isinstance(node, VariableDeclaration)
        assert isinstance(node.value, TypeCastExpression)

    @pytest.mark.xfail(
        reason="Ternary '?' operator is not supported by the NexusLang lexer — known coverage gap",
        strict=True,
    )
    def test_ternary_expression(self):
        node = first("set val to x > 0 ? x : 0")
        assert isinstance(node, VariableDeclaration)
        from nexuslang.parser.ast import TernaryExpression
        assert isinstance(node.value, TernaryExpression)

    def test_lambda_expression(self):
        # Lambda body is terminated by dedent, not 'end'
        src = (
            "set doubler to lambda with x as Integer returns Integer\n"
            "    return x times 2"
        )
        node = first(src)
        assert isinstance(node, VariableDeclaration)
        assert isinstance(node.value, LambdaExpression)


# ---------------------------------------------------------------------------
# 12. Memory and low-level
# ---------------------------------------------------------------------------

class TestMemoryAndLowLevel:
    def test_address_of(self):
        node = first("set ptr to address of x")
        assert isinstance(node, VariableDeclaration)
        assert isinstance(node.value, AddressOfExpression)

    def test_dereference(self):
        node = first("set val to dereference ptr")
        assert isinstance(node, VariableDeclaration)
        assert isinstance(node.value, DereferenceExpression)

    def test_sizeof_type(self):
        node = first("set sz to sizeof Integer")
        assert isinstance(node, VariableDeclaration)
        assert isinstance(node.value, SizeofExpression)

    def test_sizeof_expression(self):
        node = first("set sz to sizeof numbers")
        assert isinstance(node, VariableDeclaration)
        assert isinstance(node.value, SizeofExpression)

    def test_memory_allocation(self):
        # Regression: this syntax must parse into a MemoryAllocation node.
        src = "allocate a new Integer in memory and name it buffer"
        node = first(src)
        assert isinstance(node, MemoryAllocation)

    def test_memory_deallocation(self):
        # Regression: this syntax must parse into a MemoryDeallocation node.
        src = "free the memory at buffer"
        node = first(src)
        assert isinstance(node, MemoryDeallocation)

    def test_memory_allocation_with_literal_initial_value(self):
        src = "allocate a new Integer in memory with value 5 and name it buffer"
        node = first(src)
        assert isinstance(node, MemoryAllocation)
        assert node.identifier == "buffer"
        assert isinstance(node.initial_value, Literal)

    def test_memory_allocation_with_expression_initial_value(self):
        src = "allocate a new Integer in memory with value 2 plus 3 and name it buffer"
        node = first(src)
        assert isinstance(node, MemoryAllocation)
        assert isinstance(node.initial_value, BinaryOperation)

    def test_memory_deallocation_without_the(self):
        node = first("free memory at buffer")
        assert isinstance(node, MemoryDeallocation)
        assert node.identifier == "buffer"

    def test_memory_deallocation_keyword_identifier_message(self):
        node = first("free memory at message")
        assert isinstance(node, MemoryDeallocation)
        assert node.identifier == "message"

    def test_memory_deallocation_keyword_identifier_value(self):
        node = first("free memory at value")
        assert isinstance(node, MemoryDeallocation)
        assert node.identifier == "value"

    def test_symbolic_address_of(self):
        node = first("set p to &x")
        assert isinstance(node, VariableDeclaration)
        assert isinstance(node.value, AddressOfExpression)

    def test_symbolic_address_of_parenthesized(self):
        node = first("set p to &(x)")
        assert isinstance(node, VariableDeclaration)
        assert isinstance(node.value, AddressOfExpression)

    def test_symbolic_dereference(self):
        node = first("set value to *ptr")
        assert isinstance(node, VariableDeclaration)
        assert isinstance(node.value, DereferenceExpression)

    def test_inline_assembly(self):
        src = "asm\n    nop\nend asm"
        node = first(src)
        assert isinstance(node, InlineAssembly)

    def test_extern_function(self):
        # 'foreign function' does not require an 'end' terminator
        src = "foreign function puts with s as String returns Integer"
        node = first(src)
        assert isinstance(node, ExternFunctionDeclaration)


# ---------------------------------------------------------------------------
# 13. Design-by-contract
# ---------------------------------------------------------------------------

class TestDesignByContract:
    def test_require(self):
        node = first("require x is greater than 0")
        assert isinstance(node, RequireStatement)

    def test_ensure(self):
        node = first("ensure result is greater than or equal to 0")
        assert isinstance(node, EnsureStatement)

    def test_guarantee(self):
        node = first("guarantee n equals 3")
        assert isinstance(node, GuaranteeStatement)

    def test_invariant(self):
        node = first("invariant count is greater than or equal to 0")
        assert isinstance(node, InvariantStatement)

    def test_require_inside_function(self):
        src = (
            "function divide with a as Integer and b as Integer returns Integer\n"
            "    require b is not equal to 0\n"
            "    return a divided by b\n"
            "end"
        )
        fn = first(src)
        assert isinstance(fn, FunctionDefinition)
        # Require should be inside the body
        assert any(isinstance(s, RequireStatement) for s in fn.body)


# ---------------------------------------------------------------------------
# 14. Concurrency / async
# ---------------------------------------------------------------------------

class TestConcurrency:
    def test_async_function_definition(self):
        src = (
            "async function fetch with url as String returns String\n"
            "    return url\n"
            "end"
        )
        node = first(src)
        assert isinstance(node, AsyncFunctionDefinition)

    def test_await_expression(self):
        src = (
            "async function process\n"
            "    set data to await fetch_data\n"
            "end"
        )
        fn = first(src)
        assert isinstance(fn, AsyncFunctionDefinition)
        # Body contains VariableDeclaration whose value is AwaitExpression
        found = any(
            isinstance(s, VariableDeclaration) and isinstance(s.value, AwaitExpression)
            for s in fn.body
        )
        assert found, "Expected AwaitExpression inside async function body"

    def test_async_try_catch(self):
        src = (
            "async function safe_fetch with url as String returns String\n"
            "    try\n"
            "        set data to await fetch_data\n"
            "        return data\n"
            "    catch err\n"
            '        return "error"\n'
            "    end\n"
            "end"
        )
        fn = first(src)
        assert isinstance(fn, AsyncFunctionDefinition)
        assert any(isinstance(s, TryCatch) for s in fn.body)


# ---------------------------------------------------------------------------
# 15. Test blocks
# ---------------------------------------------------------------------------

class TestBlocks:
    def test_simple_test_block(self):
        src = 'test "addition works" do\n    expect 1 plus 1 to equal 2\nend'
        node = first(src)
        assert isinstance(node, TestBlock)
        assert node.name == "addition works"

    def test_test_block_with_body(self):
        src = (
            'test "variable declaration" do\n'
            "    set x to 42\n"
            "    expect x to equal 42\n"
            "end"
        )
        node = first(src)
        assert isinstance(node, TestBlock)
        assert len(node.body) >= 2

    def test_multiple_test_blocks(self):
        src = (
            'test "first" do\n    expect 1 to equal 1\nend\n'
            'test "second" do\n    expect 1 to equal 1\nend'
        )
        prog = parse(src)
        blocks = [s for s in prog.statements if isinstance(s, TestBlock)]
        assert len(blocks) == 2
        assert blocks[0].name == "first"
        assert blocks[1].name == "second"


# ---------------------------------------------------------------------------
# 16. Generic type instantiation
# ---------------------------------------------------------------------------

class TestGenericTypes:
    def test_create_list_of_type(self):
        src = "set items to create List of Integer"
        node = first(src)
        assert isinstance(node, VariableDeclaration)
        assert isinstance(node.value, GenericTypeInstantiation)

    def test_create_dict_of_type(self):
        src = "set mapping to create Dictionary of String to Integer"
        node = first(src)
        assert isinstance(node, VariableDeclaration)

    def test_create_list_generic(self):
        # 'create List of Integer' produces a GenericTypeInstantiation
        src = "set items to create List of Integer"
        node = first(src)
        assert isinstance(node, VariableDeclaration)
        assert isinstance(node.value, GenericTypeInstantiation)
        assert node.value.generic_name == "list"


# ---------------------------------------------------------------------------
# 17. Return statements
# ---------------------------------------------------------------------------

class TestReturnStatements:
    def test_return_with_expression(self):
        src = "function get_one returns Integer\n    return 1\nend"
        fn = first(src)
        assert isinstance(fn, FunctionDefinition)
        ret = fn.body[-1]
        assert isinstance(ret, ReturnStatement)
        assert ret.value is not None

    def test_return_without_expression(self):
        src = "function early_exit\n    return\nend"
        fn = first(src)
        assert isinstance(fn, FunctionDefinition)
        ret = fn.body[-1]
        assert isinstance(ret, ReturnStatement)

    def test_return_complex_expression(self):
        src = "function calc with a as Integer and b as Integer returns Integer\n    return a times b plus 1\nend"
        fn = first(src)
        ret = fn.body[-1]
        assert isinstance(ret, ReturnStatement)
        assert isinstance(ret.value, BinaryOperation)


# ---------------------------------------------------------------------------
# 18. Edge cases — multiple statements parse correctly
# ---------------------------------------------------------------------------

class TestMultipleStatements:
    def test_three_statements(self):
        src = "set a to 1\nset b to 2\nset c to a plus b"
        prog = parse(src)
        assert len(prog.statements) == 3
        for s in prog.statements:
            assert isinstance(s, VariableDeclaration)

    def test_empty_program(self):
        prog = parse("")
        assert isinstance(prog, Program)
        assert prog.statements == []

    def test_comment_only(self):
        prog = parse("# just a comment")
        assert isinstance(prog, Program)
        assert prog.statements == []

    def test_function_then_call(self):
        src = (
            "function double with x as Integer returns Integer\n"
            "    return x times 2\n"
            "end\n"
            "set result to double with x: 5"
        )
        prog = parse(src)
        assert len(prog.statements) == 2
        assert isinstance(prog.statements[0], FunctionDefinition)
        assert isinstance(prog.statements[1], VariableDeclaration)

    def test_class_then_instantiation(self):
        src = (
            "class Foo\n"
            "    public function bar\n"
            '        print text "bar"\n'
            "    end\n"
            "end\n"
            "set obj to new Foo"
        )
        prog = parse(src)
        assert len(prog.statements) == 2
        assert isinstance(prog.statements[0], ClassDefinition)
        assert isinstance(prog.statements[1], VariableDeclaration)
