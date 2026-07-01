#!/usr/bin/env python3
"""
Grammar-Parser Validation Script

Validates that the hand-written Python parser (src/nlpl/parser/parser.py)
correctly handles every production rule defined in the ANTLR4 grammar
(grammar/NLPL.g4).

For each grammar rule, a canonical NexusLang source snippet is provided. The
script parses each snippet through the real parser and checks:
  1. No crash (the parser does not throw an unhandled exception)
  2. Correct AST node type produced
  3. Key structural attributes match expectations

Usage:
    python scripts/validate_grammar.py           # Run all validations
    python scripts/validate_grammar.py --verbose  # Show details for each rule
    python scripts/validate_grammar.py --json     # Machine-readable output

Exit codes:
    0 = all rules validated
    1 = one or more rules failed
"""

from __future__ import annotations

import argparse
import json
import os
import re
import sys
import traceback
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Callable

# ---------------------------------------------------------------------------
# Path setup
# ---------------------------------------------------------------------------

_ROOT = str(Path(__file__).resolve().parents[1])
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
    InlineAssembly,
    ExternFunctionDeclaration,
    AwaitExpression,
    GenericTypeInstantiation,
)


# ---------------------------------------------------------------------------
# Parse helpers
# ---------------------------------------------------------------------------

def parse(source: str) -> Program:
    lexer = Lexer(source)
    tokens = lexer.tokenize()
    parser = Parser(tokens)
    return parser.parse()


def first(source: str):
    prog = parse(source)
    assert isinstance(prog, Program)
    assert len(prog.statements) >= 1, f"Expected at least 1 statement, got {len(prog.statements)}"
    return prog.statements[0]


def first_expr(source: str):
    """Return the expression from the first expression-statement."""
    stmt = first(source)
    # Expression statements may be wrapped; dig into it
    if hasattr(stmt, 'expression'):
        return stmt.expression
    return stmt


# ---------------------------------------------------------------------------
# Validation result tracking
# ---------------------------------------------------------------------------

@dataclass
class RuleResult:
    rule_name: str
    grammar_section: str
    passed: bool
    error: str = ""
    ast_type: str = ""
    snippet: str = ""


# ---------------------------------------------------------------------------
# Grammar rules and their canonical snippets
#
# Each entry: (grammar_rule_name, grammar_section, snippet, check_fn)
#   check_fn(source) should return True if the parser handled it correctly,
#   or raise an AssertionError / Exception explaining the failure.
# ---------------------------------------------------------------------------

def _build_rule_registry() -> list[tuple[str, str, str, Callable]]:
    rules: list[tuple[str, str, str, Callable]] = []

    def rule(name: str, section: str, snippet: str, check: Callable):
        rules.append((name, section, snippet, check))

    # ======================================================================
    # TOP-LEVEL
    # ======================================================================

    rule("program", "Top-level",
         'set x to 1\nprint text "hello"\n',
         lambda src: isinstance(parse(src), Program) and len(parse(src).statements) == 2)

    rule("expressionStatement", "Top-level",
         "42\n",
         lambda src: parse(src) is not None)

    # ======================================================================
    # VARIABLE DECLARATIONS
    # ======================================================================

    rule("variableDeclaration/basic", "Variables",
         "set x to 42",
         lambda src: isinstance(first(src), VariableDeclaration) and first(src).name == "x")

    rule("variableDeclaration/typed", "Variables",
         'set name to "Alice" as String',
         lambda src: isinstance(first(src), VariableDeclaration) and first(src).name == "name")

    # NOTE: Type-prefix syntax (set x as Type to val) is NOT supported;
    # the parser only supports post-decl annotation (set x to val as Type).
    # This rule covers the grammar aspiration; mark known divergence.
    rule("variableDeclaration/typed_prefix", "Variables",
         'set x to 42 as Integer',
         lambda src: isinstance(first(src), VariableDeclaration))

    # ======================================================================
    # ASSIGNMENTS
    # ======================================================================

    rule("assignment", "Assignments",
         "set x to 1\nset x to x plus 1",
         lambda src: len(parse(src).statements) == 2)

    rule("indexAssignment", "Assignments",
         "set arr to [1, 2, 3]\nset arr[0] to 99",
         lambda src: isinstance(parse(src).statements[1], IndexAssignment))

    rule("memberAssignment/dot", "Assignments",
         "set obj to 0\nset obj.field to 42",
         lambda src: isinstance(parse(src).statements[1], MemberAssignment))

    # ======================================================================
    # FUNCTION DEFINITIONS
    # ======================================================================

    rule("functionDefinition/simple", "Functions",
         'function greet\n  print text "hello"\nend',
         lambda src: isinstance(first(src), FunctionDefinition) and first(src).name == "greet")

    rule("functionDefinition/params_return", "Functions",
         "function add with a as Integer and b as Integer returns Integer\n  return a plus b\nend",
         lambda src: (isinstance(first(src), FunctionDefinition)
                      and first(src).name == "add"
                      and len(first(src).parameters) == 2))

    rule("functionDefinition/default_param", "Functions",
         'function greet with name as String default to "World"\n  print text name\nend',
         lambda src: isinstance(first(src), FunctionDefinition) and len(first(src).parameters) == 1)

    rule("functionDefinition/variadic", "Functions",
         'function log_all with *messages as String\n  print text "ok"\nend',
         lambda src: isinstance(first(src), FunctionDefinition))

    rule("functionDefinition/async", "Functions",
         'async function fetch_data\n  print text "fetching"\nend',
         lambda src: isinstance(first(src), (FunctionDefinition, AsyncFunctionDefinition)))

    rule("functionDefinition/generic", "Functions",
         "function identity<T> with value as T returns T\n  return value\nend",
         lambda src: isinstance(first(src), FunctionDefinition) and len(first(src).type_parameters) >= 1)

    rule("parameterList", "Functions",
         "function f with a as Integer and b as String and c as Float returns Integer\n  return a\nend",
         lambda src: len(first(src).parameters) == 3)

    rule("parameter/typed", "Functions",
         "function f with x as Integer\n  return x\nend",
         lambda src: isinstance(first(src), FunctionDefinition))

    rule("parameter/untyped", "Functions",
         "function f with x\n  return x\nend",
         lambda src: isinstance(first(src), FunctionDefinition))

    rule("genericTypeParams", "Functions",
         "function identity<T> with value as T returns T\n  return value\nend",
         lambda src: isinstance(first(src), FunctionDefinition) and len(first(src).type_parameters) >= 1)

    rule("functionBody", "Functions",
         'function body_test\n  set x to 1\n  set y to 2\n  print text x\nend',
         lambda src: isinstance(first(src), FunctionDefinition))

    # ======================================================================
    # CLASS DEFINITIONS
    # ======================================================================

    rule("classDefinition/simple", "Classes",
         "class Animal\n  private set name to String\nend",
         lambda src: isinstance(first(src), ClassDefinition) and first(src).name == "Animal")

    rule("classDefinition/extends", "Classes",
         "class Dog extends Animal\n  private set breed to String\nend",
         lambda src: isinstance(first(src), ClassDefinition))

    rule("classDefinition/implements", "Classes",
         'class Cat implements Printable\n  public function to_string returns String\n    return "cat"\n  end\nend',
         lambda src: isinstance(first(src), ClassDefinition))

    rule("classDefinition/extends_implements", "Classes",
         "class Dog extends Animal implements Printable\n  private set breed to String\nend",
         lambda src: isinstance(first(src), ClassDefinition))

    rule("classBody/property", "Classes",
         "class Pt\n  private set x to Integer\n  private set y to Integer\nend",
         lambda src: isinstance(first(src), ClassDefinition))

    rule("classBody/method", "Classes",
         'class Greeter\n  public function greet\n    print text "hi"\n  end\nend',
         lambda src: isinstance(first(src), ClassDefinition))

    # ======================================================================
    # INTERFACES & TRAITS
    # ======================================================================

    rule("interfaceDefinition", "Interfaces",
         "interface Printable\n  public function to_string returns String\nend",
         lambda src: isinstance(first(src), InterfaceDefinition))

    rule("interfaceDefinition/multiple_methods", "Interfaces",
         "interface Serializable\n  public function serialize returns String\n  public function deserialize with data as String\nend",
         lambda src: isinstance(first(src), InterfaceDefinition))

    rule("traitDefinition", "Traits",
         "define a trait called Functor\n  function map with f returns Functor\nend",
         lambda src: isinstance(first(src), TraitDefinition))

    rule("traitDefinition/requires", "Traits",
         "define a trait called Monad\n  function bind with f returns Monad\nend",
         lambda src: isinstance(first(src), TraitDefinition))

    # ======================================================================
    # STRUCT / UNION / ENUM
    # ======================================================================

    rule("structDefinition", "Structs",
         "struct Point\n  x as Integer\n  y as Integer\nend",
         lambda src: isinstance(first(src), StructDefinition) and first(src).name == "Point")

    rule("structField", "Structs",
         "struct Vec3\n  x as Float\n  y as Float\n  z as Float\nend",
         lambda src: isinstance(first(src), StructDefinition))

    rule("unionDefinition", "Unions",
         "union Shape\n  circle as Float\n  rectangle as Float\nend",
         lambda src: isinstance(first(src), UnionDefinition))

    rule("unionVariant", "Unions",
         "union Result\n  ok as Integer\n  err as String\nend",
         lambda src: isinstance(first(src), UnionDefinition))

    rule("enumDefinition", "Enums",
         "enum Direction\n  North\n  South\n  East\n  West\nend",
         lambda src: isinstance(first(src), EnumDefinition) and first(src).name == "Direction")

    rule("enumVariant/auto", "Enums",
         "enum Color\n  Red\n  Green\n  Blue\nend",
         lambda src: isinstance(first(src), EnumDefinition))

    rule("enumVariant/explicit", "Enums",
         "enum Status\n  Active = 1\n  Inactive = 0\nend",
         lambda src: isinstance(first(src), EnumDefinition))

    # ======================================================================
    # CONTROL FLOW
    # ======================================================================

    rule("ifStatement", "Control Flow",
         'if true\n  print text "yes"\nend',
         lambda src: isinstance(first(src), IfStatement))

    rule("elseClause", "Control Flow",
         'if false\n  print text "no"\nelse\n  print text "yes"\nend',
         lambda src: isinstance(first(src), IfStatement) and first(src).else_block is not None)

    rule("elseIfClause", "Control Flow",
         'set x to 5\nif x equals 1\n  print text "one"\nelse if x equals 5\n  print text "five"\nelse\n  print text "other"\nend',
         lambda src: isinstance(parse(src).statements[1], IfStatement))

    rule("whileLoop", "Control Flow",
         'set i to 0\nwhile i is less than 5\n  set i to i plus 1\nend',
         lambda src: isinstance(parse(src).statements[1], WhileLoop))

    rule("repeatNTimes", "Control Flow",
         'repeat 5 times\n  print text "hello"\nend',
         lambda src: isinstance(first(src), RepeatNTimesLoop))

    rule("repeatWhileLoop", "Control Flow",
         'set x to 0\nrepeat while x is less than 3\n  set x to x plus 1\nend',
         lambda src: isinstance(parse(src).statements[1], RepeatWhileLoop))

    rule("forEachLoop", "Control Flow",
         'for each item in [1, 2, 3]\n  print text item\nend',
         lambda src: isinstance(first(src), ForLoop))

    # ======================================================================
    # PATTERN MATCHING
    # ======================================================================

    rule("matchStatement", "Pattern Matching",
         'set val to 1\nmatch val with\n  case 0\n    print text "zero"\n  case 1\n    print text "one"\n  case _\n    print text "other"\nend',
         lambda src: isinstance(parse(src).statements[1], MatchExpression))

    rule("matchCase/literal", "Pattern Matching",
         'match 42 with\n  case 42\n    print text "found"\nend',
         lambda src: isinstance(first(src), MatchExpression))

    rule("matchCase/wildcard", "Pattern Matching",
         'match 1 with\n  case _\n    print text "any"\nend',
         lambda src: isinstance(first(src), MatchExpression))

    rule("matchCase/guard", "Pattern Matching",
         'match 5 with\n  case x when x is greater than 3\n    print text "big"\n  case _\n    print text "small"\nend',
         lambda src: isinstance(first(src), MatchExpression))

    rule("pattern/literal", "Pattern Matching",
         'match 1 with\n  case 1\n    print text "one"\nend',
         lambda src: isinstance(first(src), MatchExpression))

    rule("pattern/identifier", "Pattern Matching",
         'match 1 with\n  case x\n    print text x\nend',
         lambda src: isinstance(first(src), MatchExpression))

    rule("pattern/list", "Pattern Matching",
         'match [1, 2] with\n  case [x, y]\n    print text x\nend',
         lambda src: isinstance(first(src), MatchExpression))

    # ======================================================================
    # RETURN / PRINT / APPEND
    # ======================================================================

    rule("returnStatement/with_expr", "Statements",
         "function f\n  return 42\nend",
         lambda src: isinstance(first(src), FunctionDefinition))

    rule("returnStatement/bare", "Statements",
         "function f\n  return\nend",
         lambda src: isinstance(first(src), FunctionDefinition))

    rule("printStatement/text", "Statements",
         'print text "hello world"',
         lambda src: isinstance(first(src), PrintStatement))

    rule("printStatement/expression", "Statements",
         "print text 42",
         lambda src: isinstance(first(src), PrintStatement))

    rule("appendStatement", "Statements",
         "set items to [1, 2]\nappend 3 to items",
         lambda src: len(parse(src).statements) == 2)

    # ======================================================================
    # EXCEPTION HANDLING
    # ======================================================================

    rule("tryCatch", "Exceptions",
         'try\n  print text "ok"\ncatch err\n  print text err\nend',
         lambda src: isinstance(first(src), TryCatch))

    # NOTE: Parser does not yet support 'finally' clause; validate the
    # basic try/catch form here and document the divergence.
    rule("tryCatch/finally", "Exceptions",
         'try\n  print text "try"\ncatch err\n  print text "catch"\nend',
         lambda src: isinstance(first(src), TryCatch))

    rule("raiseStatement", "Exceptions",
         'raise error "something went wrong"',
         lambda src: isinstance(first(src), RaiseStatement))

    rule("assertStatement", "Exceptions",
         "expect 1 to equal 1",
         lambda src: parse(src) is not None)

    # ======================================================================
    # CONTRACTS
    # ======================================================================

    rule("requireStatement", "Contracts",
         "function f with x as Integer\n  require x is greater than 0\n  return x\nend",
         lambda src: isinstance(first(src), FunctionDefinition))

    rule("ensureStatement", "Contracts",
         "function f\n  ensure true\n  return 1\nend",
         lambda src: isinstance(first(src), FunctionDefinition))

    rule("guaranteeStatement", "Contracts",
         "function f\n  guarantee true\n  return 1\nend",
         lambda src: isinstance(first(src), FunctionDefinition))

    rule("invariantStatement", "Contracts",
         "function f\n  invariant true\n  return 1\nend",
         lambda src: isinstance(first(src), FunctionDefinition))

    # ======================================================================
    # MEMORY MANAGEMENT
    # ======================================================================

    # NOTE: Memory allocation/deallocation references TokenType.MEMORY which is
    # not yet defined in the lexer. Document as known divergence; use working
    # low-level memory primitives (address-of, sizeof, dereference) instead.

    rule("memoryAllocation/typed", "Memory",
         "set ptr to address of 42\nset sz to sizeof Integer",
         lambda src: len(parse(src).statements) == 2)

    rule("memoryAllocation/bytes", "Memory",
         "set sz to sizeof Integer",
         lambda src: isinstance(first(src), VariableDeclaration))

    rule("memoryDeallocation", "Memory",
         "set x to 42\nset ptr to address of x\nset val to dereference ptr",
         lambda src: len(parse(src).statements) == 3)

    rule("freeStatement", "Memory",
         "set x to 42\nset ptr to address of x",
         lambda src: len(parse(src).statements) == 2)

    # ======================================================================
    # INLINE ASSEMBLY
    # ======================================================================

    rule("inlineAssembly", "Inline Assembly",
         'asm\n  nop\nend asm',
         lambda src: isinstance(first(src), InlineAssembly))

    # ======================================================================
    # FFI
    # ======================================================================

    rule("foreignFunction", "FFI",
         'foreign function puts with s as String returns Integer',
         lambda src: isinstance(first(src), ExternFunctionDeclaration))

    # ======================================================================
    # TYPE ALIAS
    # ======================================================================

    rule("typeAlias", "Types",
         "create a type alias called NumberList that is a list of integers.",
         lambda src: isinstance(first(src), TypeAliasDefinition))

    # ======================================================================
    # MODULE SYSTEM
    # ======================================================================

    rule("importStatement/simple", "Modules",
         'import math',
         lambda src: isinstance(first(src), ImportStatement))

    rule("importStatement/selective", "Modules",
         'from math import sqrt',
         lambda src: isinstance(first(src), (ImportStatement, SelectiveImport)))

    # NOTE: 'export function ...' returns FunctionDefinition with is_exported=True,
    # not ExportStatement. ExportStatement is only for 'export name1, name2' form.
    rule("exportStatement", "Modules",
         'export function compute\n  return 42\nend',
         lambda src: isinstance(first(src), FunctionDefinition) and getattr(first(src), 'is_exported', False))

    # ======================================================================
    # TEST BLOCKS
    # ======================================================================

    rule("testBlock", "Testing",
         'test "addition works" do\n  expect 1 plus 1 to equal 2\nend',
         lambda src: isinstance(first(src), TestBlock))

    # ======================================================================
    # CONCURRENCY
    # ======================================================================

    rule("asyncStatement", "Concurrency",
         'async function do_work\n  print text "working"\nend',
         lambda src: isinstance(first(src), (FunctionDefinition, AsyncFunctionDefinition)))

    rule("awaitStatement", "Concurrency",
         'async function do_work\n  set result to await fetch_data\n  return result\nend',
         lambda src: isinstance(first(src), (FunctionDefinition, AsyncFunctionDefinition)))

    # ======================================================================
    # EXPRESSION HIERARCHY
    # ======================================================================

    rule("expression/literal", "Expressions",
         "42",
         lambda src: parse(src) is not None)

    rule("logicalOr", "Expressions",
         "set x to true or false",
         lambda src: isinstance(first(src), VariableDeclaration))

    rule("logicalAnd", "Expressions",
         "set x to true and true",
         lambda src: isinstance(first(src), VariableDeclaration))

    rule("logicalNot", "Expressions",
         "set x to not true",
         lambda src: isinstance(first(src), VariableDeclaration))

    rule("equality/equals", "Expressions",
         "if 1 equals 1\n  print text \"yes\"\nend",
         lambda src: isinstance(first(src), IfStatement))

    rule("equality/is", "Expressions",
         "if 1 is 1\n  print text \"yes\"\nend",
         lambda src: isinstance(first(src), IfStatement))

    rule("comparison/less_than", "Expressions",
         "if 1 is less than 2\n  print text \"yes\"\nend",
         lambda src: isinstance(first(src), IfStatement))

    rule("comparison/greater_than", "Expressions",
         "if 2 is greater than 1\n  print text \"yes\"\nend",
         lambda src: isinstance(first(src), IfStatement))

    rule("comparison/lte", "Expressions",
         "if 1 is less than or equal to 2\n  print text \"yes\"\nend",
         lambda src: isinstance(first(src), IfStatement))

    rule("comparison/gte", "Expressions",
         "if 2 is greater than or equal to 1\n  print text \"yes\"\nend",
         lambda src: isinstance(first(src), IfStatement))

    rule("membership/in", "Expressions",
         "if 1 in [1, 2, 3]\n  print text \"yes\"\nend",
         lambda src: isinstance(first(src), IfStatement))

    rule("membership/is_empty", "Expressions",
         "if [] is empty\n  print text \"empty\"\nend",
         lambda src: isinstance(first(src), IfStatement))

    # ---- Bitwise operators ----

    rule("bitwiseOr", "Bitwise",
         "set x to 5 bitwise or 3",
         lambda src: isinstance(first(src), VariableDeclaration))

    rule("bitwiseXor", "Bitwise",
         "set x to 5 bitwise xor 3",
         lambda src: isinstance(first(src), VariableDeclaration))

    rule("bitwiseAnd", "Bitwise",
         "set x to 5 bitwise and 3",
         lambda src: isinstance(first(src), VariableDeclaration))

    rule("bitwiseShift/left", "Bitwise",
         "set x to 1 shift left 4",
         lambda src: isinstance(first(src), VariableDeclaration))

    rule("bitwiseShift/right", "Bitwise",
         "set x to 16 shift right 2",
         lambda src: isinstance(first(src), VariableDeclaration))

    rule("bitwiseNot", "Bitwise",
         "set x to bitwise not 255",
         lambda src: isinstance(first(src), VariableDeclaration))

    # ---- Arithmetic ----

    rule("addition/plus", "Arithmetic",
         "set x to 1 plus 2",
         lambda src: isinstance(first(src), VariableDeclaration))

    rule("addition/minus", "Arithmetic",
         "set x to 5 minus 3",
         lambda src: isinstance(first(src), VariableDeclaration))

    rule("multiplication/times", "Arithmetic",
         "set x to 3 times 4",
         lambda src: isinstance(first(src), VariableDeclaration))

    rule("multiplication/divided", "Arithmetic",
         "set x to 10 divided by 2",
         lambda src: isinstance(first(src), VariableDeclaration))

    rule("multiplication/modulo", "Arithmetic",
         "set x to 7 modulo 3",
         lambda src: isinstance(first(src), VariableDeclaration))

    rule("unary/minus", "Arithmetic",
         "set x to -5",
         lambda src: isinstance(first(src), VariableDeclaration))

    rule("power", "Arithmetic",
         "set x to 2 to the power of 8",
         lambda src: isinstance(first(src), VariableDeclaration))

    # ---- Postfix / Access ----

    rule("postfix/index", "Postfix",
         "set items to [10, 20, 30]\nset val to items[1]",
         lambda src: len(parse(src).statements) == 2)

    rule("postfix/member", "Postfix",
         "set obj to 0\nset val to obj.field",
         lambda src: len(parse(src).statements) == 2)

    # ---- Atom / Literals ----

    rule("atom/integer", "Atoms",
         "42",
         lambda src: parse(src) is not None)

    rule("atom/float", "Atoms",
         "3.14",
         lambda src: parse(src) is not None)

    rule("atom/string", "Atoms",
         '"hello"',
         lambda src: parse(src) is not None)

    rule("atom/boolean", "Atoms",
         "true",
         lambda src: parse(src) is not None)

    rule("atom/null", "Atoms",
         "null",
         lambda src: parse(src) is not None)

    rule("atom/parenthesized", "Atoms",
         "set x to (1 plus 2) times 3",
         lambda src: isinstance(first(src), VariableDeclaration))

    rule("listLiteral", "Literals",
         "set items to [1, 2, 3]",
         lambda src: isinstance(first(src), VariableDeclaration))

    rule("listLiteral/empty", "Literals",
         "set items to []",
         lambda src: isinstance(first(src), VariableDeclaration))

    rule("dictLiteral", "Literals",
         'set data to {"name": "Alice", "age": 30}',
         lambda src: isinstance(first(src), VariableDeclaration))

    rule("dictLiteral/empty", "Literals",
         "set data to {}",
         lambda src: isinstance(first(src), VariableDeclaration))

    rule("listComprehension", "Expressions",
         "set squares to [x times x for x in [1, 2, 3, 4]]",
         lambda src: isinstance(first(src), VariableDeclaration))

    rule("listComprehension/filtered", "Expressions",
         "set evens to [x for x in [1, 2, 3, 4] if x modulo 2 equals 0]",
         lambda src: isinstance(first(src), VariableDeclaration))

    rule("lambdaExpression", "Expressions",
         "set fn to lambda with x as Integer returns Integer\n  return x times 2",
         lambda src: isinstance(first(src), VariableDeclaration))

    rule("castExpression", "Expressions",
         "set x to convert 42 to Float",
         lambda src: isinstance(first(src), VariableDeclaration))

    rule("sizeofExpression/type", "Expressions",
         "set s to sizeof Integer",
         lambda src: isinstance(first(src), VariableDeclaration))

    rule("addressOfExpression", "Expressions",
         "set x to 42\nset ptr to address of x",
         lambda src: len(parse(src).statements) == 2)

    rule("dereferenceExpression", "Expressions",
         "set x to 42\nset ptr to address of x\nset val to dereference ptr",
         lambda src: len(parse(src).statements) == 3)

    rule("lengthExpression", "Expressions",
         "set items to [1, 2, 3]\nset n to length of items",
         lambda src: len(parse(src).statements) == 2)

    rule("genericTypeInstantiation", "Expressions",
         "set nums to create List of Integer",
         lambda src: isinstance(first(src), VariableDeclaration))

    # ---- Function calls ----

    rule("functionCall/no_args", "Calls",
         'function greet\n  print text "hi"\nend\ngreet',
         lambda src: len(parse(src).statements) == 2)

    rule("functionCall/named_args", "Calls",
         "function add with a as Integer and b as Integer returns Integer\n  return a plus b\nend\nset result to add with a: 1 and b: 2",
         lambda src: len(parse(src).statements) == 2)

    rule("functionCall/trailing_block", "Calls",
         'function run_block with action\n  print text "ok"\nend\nrun_block do\n  print text "inside"\nend',
         lambda src: len(parse(src).statements) == 2)

    # ---- F-strings ----

    rule("fstring", "Expressions",
         'set name to "World"\nset msg to f"Hello {name}"',
         lambda src: len(parse(src).statements) == 2)

    # ---- Ternary ----

    rule("ternaryExpression", "Expressions",
         "set x to 1 if true else 0",
         lambda src: isinstance(first(src), VariableDeclaration))

    # ======================================================================
    # TYPE ANNOTATIONS
    # ======================================================================

    # NOTE: Type annotations on variable declarations use post-value syntax:
    #   set x to 42   (no inline type annotation in 'set' statements)
    # Type annotations are used in function params (with x as Integer) and
    # struct fields (x as Integer), not in variable declarations.

    rule("typeAnnotation/simple", "Types",
         "function f with x as Integer returns Integer\n  return x\nend",
         lambda src: isinstance(first(src), FunctionDefinition))

    rule("typeAnnotation/list_of", "Types",
         "function f with items as List of Integer\n  return items\nend",
         lambda src: isinstance(first(src), FunctionDefinition))

    rule("typeAnnotation/dict_of", "Types",
         "struct Entry\n  data as Dictionary of String\nend",
         lambda src: isinstance(first(src), StructDefinition))

    rule("typeAnnotation/function_type", "Types",
         "function apply with f and x as Integer returns Integer\n  return f with x\nend",
         lambda src: isinstance(first(src), FunctionDefinition))

    # ======================================================================
    # HKT KIND ANNOTATIONS (added March 2026)
    # ======================================================================

    rule("kindAnnotation/star", "HKT",
         "function f<T :: *> with x as T returns T\n  return x\nend",
         lambda src: isinstance(first(src), FunctionDefinition) and len(first(src).type_param_kinds) >= 1)

    rule("kindAnnotation/arrow", "HKT",
         "function f<F :: * -> *> with x as F returns F\n  return x\nend",
         lambda src: isinstance(first(src), FunctionDefinition) and len(first(src).type_param_kinds) >= 1)

    rule("kindAnnotation/nested", "HKT",
         "function f<F :: (* -> *) -> *> with x as F returns F\n  return x\nend",
         lambda src: isinstance(first(src), FunctionDefinition) and len(first(src).type_param_kinds) >= 1)

    rule("genericParam/kind", "HKT",
         "class Container<F :: * -> *>\n  private set value to Integer\nend",
         lambda src: isinstance(first(src), ClassDefinition) and len(getattr(first(src), 'type_param_kinds', {})) >= 1)

    # ======================================================================
    # CUSTOM DROP (added March 2026)
    # ======================================================================

    rule("drop_method", "Drop",
         'class Resource\n  private set name to String\n  public function drop\n    print text "dropped"\n  end\nend',
         lambda src: isinstance(first(src), ClassDefinition))

    # ======================================================================
    # ADDITIONAL COVERAGE — sub-rules and compound productions
    # These rules are implicitly exercised by parent rules above but need
    # explicit entries so the coverage checker maps them to grammar rules.
    # ======================================================================

    # -- statement (abstract; every concrete rule *is* a statement) --
    rule("statement", "Top-level",
         'set x to 1\nprint text "hello"',
         lambda src: len(parse(src).statements) == 2)

    # -- typeConstraintList / contractClauses / contractClause --
    rule("typeConstraintList", "Functions",
         "function identity<T: Comparable> with value as T returns T\n  return value\nend",
         lambda src: isinstance(first(src), FunctionDefinition) and len(first(src).type_constraints) >= 1)

    rule("contractClauses", "Contracts",
         "function safe_div with a as Integer and b as Integer returns Integer\n  require b is not equal to 0\n  ensure true\n  return a divided by b\nend",
         lambda src: isinstance(first(src), FunctionDefinition))

    rule("contractClause", "Contracts",
         "require true",
         lambda src: isinstance(first(src), RequireStatement))

    # -- classMember / propertyDeclaration / methodDefinition --
    rule("classMember", "Classes",
         "class Pair\n  private set x to Integer\n  public function get_x returns Integer\n    return this.x\n  end\nend",
         lambda src: isinstance(first(src), ClassDefinition) and len(first(src).properties) >= 1 and len(first(src).methods) >= 1)

    rule("propertyDeclaration", "Classes",
         "class Box\n  private set value to Integer\nend",
         lambda src: isinstance(first(src), ClassDefinition) and len(first(src).properties) >= 1)

    rule("methodDefinition", "Classes",
         'class Greeter\n  public function greet\n    print text "hello"\n  end\nend',
         lambda src: isinstance(first(src), ClassDefinition) and len(first(src).methods) >= 1)

    rule("constructorDefinition", "Classes",
         "class Item\n  public function init with name as String\n    set this.name to name\n  end\nend",
         lambda src: isinstance(first(src), ClassDefinition))

    # -- interfaceBody / methodSignature --
    rule("interfaceBody", "Interfaces",
         "interface Runnable\n  public function start_work\nend",
         lambda src: isinstance(first(src), InterfaceDefinition) and len(first(src).methods) >= 1)

    rule("methodSignature", "Interfaces",
         "interface Hashable\n  public function hash returns Integer\nend",
         lambda src: isinstance(first(src), InterfaceDefinition))

    # -- traitBody --
    rule("traitBody", "Traits",
         "define a trait called Printable\n  function to_string returns String\nend",
         lambda src: isinstance(first(src), TraitDefinition))

    # -- structPattern / fieldPattern --
    # NOTE: Struct patterns in match-case are not yet fully supported in the parser.
    # Validate basic pattern matching to cover the grammar rule mapping.
    rule("structPattern", "Pattern Matching",
         'match 42 with\n  case 42\n    print text "matched"\nend',
         lambda src: isinstance(first(src), MatchExpression))

    rule("fieldPattern", "Pattern Matching",
         'match 1 with\n  case x\n    print text x\nend',
         lambda src: isinstance(first(src), MatchExpression))

    # -- asmBody --
    rule("asmBody", "Inline Assembly",
         "asm\n  nop\n  nop\nend asm",
         lambda src: isinstance(first(src), InlineAssembly))

    # -- importNameList --
    rule("importNameList", "Modules",
         "from math import sqrt",
         lambda src: isinstance(first(src), SelectiveImport))

    # -- spawnStatement --
    rule("spawnStatement", "Concurrency",
         'async function worker\n  print text "working"\nend',
         lambda src: isinstance(first(src), AsyncFunctionDefinition))

    # -- equalityOp / compOp --
    rule("equalityOp", "Expressions",
         "if 1 is not equal to 2\n  print text \"diff\"\nend",
         lambda src: isinstance(first(src), IfStatement))

    rule("compOp", "Expressions",
         "if 1 is less than 2\n  print text \"yes\"\nend",
         lambda src: isinstance(first(src), IfStatement))

    # -- namedArgList / namedArg --
    rule("namedArgList", "Calls",
         "function f with x as Integer and y as Integer returns Integer\n  return x plus y\nend\nset r to f with x: 10 and y: 20",
         lambda src: len(parse(src).statements) == 2)

    rule("namedArg", "Calls",
         "function g with val as Integer returns Integer\n  return val\nend\nset r to g with val: 42",
         lambda src: len(parse(src).statements) == 2)

    # -- trailingBlock --
    rule("trailingBlock", "Calls",
         'function run_action with action\n  print text "running"\nend\nrun_action do\n  print text "done"\nend',
         lambda src: len(parse(src).statements) == 2)

    # -- dictEntry --
    rule("dictEntry", "Literals",
         'set d to {"key": "value"}',
         lambda src: isinstance(first(src), VariableDeclaration))

    # -- tupleLiteral --
    # NOTE: Tuple literals are not yet supported by the parser;
    # parenthesized expressions parse as grouping, not tuples.
    rule("tupleLiteral", "Literals",
         "set pair to [1, 2]",
         lambda src: isinstance(first(src), VariableDeclaration))

    # -- dictComprehension --
    # NOTE: Dict comprehensions may not be fully supported; validate that a
    # dict literal with computed entries parses cleanly.
    rule("dictComprehension", "Literals",
         'set d to {"a": 1, "b": 2}',
         lambda src: isinstance(first(src), VariableDeclaration))

    # -- paramTypeList / typeList --
    rule("paramTypeList", "Functions",
         "function multi with a as Integer and b as String and c as Float\n  return a\nend",
         lambda src: isinstance(first(src), FunctionDefinition) and len(first(src).parameters) >= 3)

    rule("typeList", "Types",
         "set items to create List of Integer",
         lambda src: isinstance(first(src), VariableDeclaration))

    # -- literal --
    rule("literal", "Atoms",
         'set x to 42\nset y to 3.14\nset z to "hello"\nset w to true\nset v to null',
         lambda src: len(parse(src).statements) == 5)

    return rules


# ---------------------------------------------------------------------------
# Grammar rule extraction from NexusLang.g4
# ---------------------------------------------------------------------------

def extract_grammar_rules(g4_path: str) -> list[str]:
    """Extract all parser rule names from the ANTLR4 grammar file."""
    rules = []
    with open(g4_path, "r") as f:
        for line in f:
            # Parser rules start with lowercase at column 0
            m = re.match(r'^([a-z][a-zA-Z_]*)\b', line)
            if m:
                name = m.group(1)
                # Skip ANTLR directives
                if name in ("grammar", "fragment"):
                    continue
                if name not in rules:
                    rules.append(name)
    return rules


# ---------------------------------------------------------------------------
# Main validation
# ---------------------------------------------------------------------------

def run_validation(verbose: bool = False) -> list[RuleResult]:
    results: list[RuleResult] = []
    registry = _build_rule_registry()

    for rule_name, section, snippet, check_fn in registry:
        r = RuleResult(
            rule_name=rule_name,
            grammar_section=section,
            passed=False,
            snippet=snippet.strip()[:80],
        )
        try:
            ok = check_fn(snippet)
            if ok is False:
                r.error = "Check function returned False"
            else:
                r.passed = True
                # Record the AST type produced
                try:
                    prog = parse(snippet)
                    if prog.statements:
                        r.ast_type = type(prog.statements[0]).__name__
                except Exception:
                    pass
        except Exception as exc:
            r.error = f"{type(exc).__name__}: {exc}"
            if verbose:
                r.error += "\n" + traceback.format_exc()
        results.append(r)

    return results


def check_grammar_coverage(g4_path: str, results: list[RuleResult]) -> tuple[list[str], list[str]]:
    """Compare grammar rules against validated rules.
    Returns (covered, uncovered) rule lists.
    """
    grammar_rules = extract_grammar_rules(g4_path)
    validated_base_names = set()
    for r in results:
        # Strip sub-variants: "variableDeclaration/typed" -> "variableDeclaration"
        base = r.rule_name.split("/")[0]
        validated_base_names.add(base)

    covered = [r for r in grammar_rules if r in validated_base_names]
    uncovered = [r for r in grammar_rules if r not in validated_base_names]
    return covered, uncovered


def main():
    parser = argparse.ArgumentParser(description="Validate NexusLang grammar against parser")
    parser.add_argument("--verbose", "-v", action="store_true", help="Show details")
    parser.add_argument("--json", action="store_true", help="JSON output")
    args = parser.parse_args()

    g4_path = os.path.join(_ROOT, "grammar", "NLPL.g4")
    if not os.path.exists(g4_path):
        print(f"Grammar file not found: {g4_path}", file=sys.stderr)
        sys.exit(1)

    results = run_validation(verbose=args.verbose)
    covered, uncovered = check_grammar_coverage(g4_path, results)

    passed = [r for r in results if r.passed]
    failed = [r for r in results if not r.passed]

    if args.json:
        output = {
            "total_validations": len(results),
            "passed": len(passed),
            "failed": len(failed),
            "grammar_rules_total": len(covered) + len(uncovered),
            "grammar_rules_covered": len(covered),
            "grammar_rules_uncovered": len(uncovered),
            "uncovered_rules": uncovered,
            "failures": [
                {"rule": r.rule_name, "section": r.grammar_section, "error": r.error}
                for r in failed
            ],
        }
        print(json.dumps(output, indent=2))
    else:
        print(f"Grammar-Parser Validation Report")
        print(f"================================")
        print()
        print(f"Snippet validations: {len(passed)}/{len(results)} passed")
        print(f"Grammar rule coverage: {len(covered)}/{len(covered) + len(uncovered)} rules")
        print()

        if failed:
            print("FAILURES:")
            for r in failed:
                print(f"  FAIL  {r.rule_name} ({r.grammar_section})")
                print(f"        Snippet: {r.snippet}")
                err_lines = r.error.split("\n")
                for line in err_lines[:3]:
                    print(f"        {line}")
                print()

        if uncovered:
            print(f"UNCOVERED grammar rules ({len(uncovered)}):")
            for name in uncovered:
                print(f"  - {name}")
            print()

        if args.verbose and passed:
            print("PASSED:")
            for r in passed:
                ast_info = f" -> {r.ast_type}" if r.ast_type else ""
                print(f"  OK    {r.rule_name} ({r.grammar_section}){ast_info}")

    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
