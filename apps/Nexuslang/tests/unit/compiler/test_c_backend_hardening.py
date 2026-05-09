"""Regression coverage for C backend hardening changes."""

import os
import sys

import pytest


sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', '..', 'src'))

from nexuslang.compiler.backends.c_generator import CCodeGenerator
from nexuslang.parser.lexer import TokenType
from nexuslang.parser.ast import (
    BinaryOperation,
    ForLoop,
    FunctionDefinition,
    GeneratorExpression,
    Identifier,
    ListExpression,
    Literal,
    ObjectInstantiation,
    Parameter,
    PrintStatement,
    Program,
    ReturnStatement,
    VariableDeclaration,
)


def test_c_foreach_over_list_literal_emits_concrete_bounds():
    ast = Program([
        ForLoop(
            "item",
            ListExpression([Literal("integer", 1), Literal("integer", 2)]),
            [PrintStatement(Identifier("item"))],
        )
    ])

    generator = CCodeGenerator(target="c")
    c_code = generator.generate(ast)

    assert "for (int _i = 0; _i < 2; _i++)" in c_code
    assert "/* length */" not in c_code


def test_c_foreach_over_non_array_variable_fails_fast():
    ast = Program([
        VariableDeclaration("value", Literal("integer", 5)),
        ForLoop("item", Identifier("value"), [PrintStatement(Identifier("item"))]),
    ])

    generator = CCodeGenerator(target="c")

    with pytest.raises(RuntimeError, match="cannot lower for-each"):
        generator.generate(ast)


def test_c_object_instantiation_forwards_constructor_arguments():
    ast = Program([
        VariableDeclaration(
            "point",
            ObjectInstantiation("Point", [Literal("integer", 10), Literal("integer", 20)]),
        )
    ])

    generator = CCodeGenerator(target="c")
    c_code = generator.generate(ast)

    assert "Point_new(10, 20)" in c_code


def test_c_foreach_over_generator_expression_materializes_values():
    ast = Program([
        ForLoop(
            "item",
            GeneratorExpression(
                Identifier("x"),
                Identifier("x"),
                ListExpression([Literal("integer", 1), Literal("integer", 2), Literal("integer", 3)]),
                None,
            ),
            [PrintStatement(Identifier("item"))],
        )
    ])

    generator = CCodeGenerator(target="c")
    c_code = generator.generate(ast)

    assert "For each loop over generator expression (materialized)" in c_code
    assert "Unhandled expression: GeneratorExpression" not in c_code
    assert "__nxl_gen_values_" in c_code
    assert "for (int _i = 0; _i < __nxl_gen_count_" in c_code


def test_c_foreach_over_filtered_mapped_generator_expression_materializes_conditionally():
    ast = Program([
        ForLoop(
            "item",
            GeneratorExpression(
                BinaryOperation(Identifier("x"), TokenType.PLUS, Literal("integer", 10)),
                Identifier("x"),
                ListExpression([Literal("integer", 1), Literal("integer", 2), Literal("integer", 3)]),
                BinaryOperation(Identifier("x"), TokenType.GREATER_THAN, Literal("integer", 1)),
            ),
            [PrintStatement(Identifier("item"))],
        )
    ])

    generator = CCodeGenerator(target="c")
    c_code = generator.generate(ast)

    assert "if (" in c_code
    assert "if ((x > 1))" in c_code
    assert "__nxl_gen_values_" in c_code


def test_c_foreach_over_generator_identifier_source_local_array_materializes():
    ast = Program([
        VariableDeclaration(
            "numbers",
            ListExpression([Literal("integer", 1), Literal("integer", 2), Literal("integer", 3)]),
        ),
        ForLoop(
            "item",
            GeneratorExpression(
                BinaryOperation(Identifier("x"), TokenType.PLUS, Literal("integer", 5)),
                Identifier("x"),
                Identifier("numbers"),
                None,
            ),
            [PrintStatement(Identifier("item"))],
        ),
    ])

    generator = CCodeGenerator(target="c")
    c_code = generator.generate(ast)

    assert "int numbers[] = {1, 2, 3};" in c_code
    assert "For each loop over generator expression (materialized)" in c_code
    assert "__nxl_gen_values_" in c_code


def test_c_foreach_over_generator_identifier_source_uses_length_metadata_bound():
    ast = Program([
        FunctionDefinition(
            name="emit_mapped",
            parameters=[
                Parameter("numbers", "Integer[]"),
                Parameter("numbers_length", "Integer"),
            ],
            body=[
                ForLoop(
                    "item",
                    GeneratorExpression(
                        BinaryOperation(Identifier("x"), TokenType.PLUS, Literal("integer", 5)),
                        Identifier("x"),
                        Identifier("numbers"),
                        None,
                    ),
                    [PrintStatement(Identifier("item"))],
                ),
                ReturnStatement(Literal("integer", 0)),
            ],
            return_type="Integer",
        )
    ])

    generator = CCodeGenerator(target="c")
    c_code = generator.generate(ast)

    assert "int __nxl_gen_src_bound_" in c_code
    assert "numbers_length" in c_code
    assert "((int*)numbers)[__nxl_gi]" in c_code
    assert "malloc(sizeof(int) * (size_t)__nxl_gen_src_bound_" in c_code


def test_c_foreach_over_generator_identifier_source_uses_len_alias():
    """Generator source should accept _len alias for metadata bound."""
    ast = Program([
        FunctionDefinition(
            name="process_with_len",
            parameters=[
                Parameter("items", "Integer[]"),
                Parameter("items_len", "Integer"),
            ],
            body=[
                ForLoop(
                    "x",
                    GeneratorExpression(
                        BinaryOperation(Identifier("x"), TokenType.PLUS, Literal("integer", 1)),
                        Identifier("x"),
                        Identifier("items"),
                        None,
                    ),
                    [PrintStatement(Identifier("x"))],
                ),
                ReturnStatement(Literal("integer", 0)),
            ],
            return_type="Integer",
        )
    ])

    generator = CCodeGenerator(target="c")
    c_code = generator.generate(ast)

    assert "items_len" in c_code
    assert "int __nxl_gen_src_bound_" in c_code


def test_c_foreach_over_generator_identifier_source_uses_size_alias():
    """Generator source should accept _size alias for metadata bound."""
    ast = Program([
        FunctionDefinition(
            name="process_with_size",
            parameters=[
                Parameter("values", "Integer[]"),
                Parameter("values_size", "Integer"),
            ],
            body=[
                ForLoop(
                    "v",
                    GeneratorExpression(
                        Identifier("v"),
                        Identifier("v"),
                        Identifier("values"),
                        None,
                    ),
                    [PrintStatement(Identifier("v"))],
                ),
                ReturnStatement(Literal("integer", 0)),
            ],
            return_type="Integer",
        )
    ])

    generator = CCodeGenerator(target="c")
    c_code = generator.generate(ast)

    assert "values_size" in c_code
    assert "int __nxl_gen_src_bound_" in c_code


def test_c_foreach_over_generator_identifier_source_uses_count_alias():
    """Generator source should accept _count alias for metadata bound."""
    ast = Program([
        FunctionDefinition(
            name="process_with_count",
            parameters=[
                Parameter("elements", "Integer[]"),
                Parameter("elements_count", "Integer"),
            ],
            body=[
                ForLoop(
                    "e",
                    GeneratorExpression(
                        BinaryOperation(Identifier("e"), TokenType.PLUS, Literal("integer", 2)),
                        Identifier("e"),
                        Identifier("elements"),
                        None,
                    ),
                    [PrintStatement(Identifier("e"))],
                ),
                ReturnStatement(Literal("integer", 0)),
            ],
            return_type="Integer",
        )
    ])

    generator = CCodeGenerator(target="c")
    c_code = generator.generate(ast)

    assert "elements_count" in c_code
    assert "int __nxl_gen_src_bound_" in c_code


def test_c_foreach_over_generator_identifier_source_missing_metadata_fails():
    """Generator source without metadata should fail with helpful error."""
    ast = Program([
        FunctionDefinition(
            name="process_no_metadata",
            parameters=[Parameter("unknown_array", "Integer[]")],
            body=[
                ForLoop(
                    "item",
                    GeneratorExpression(
                        Identifier("item"),
                        Identifier("item"),
                        Identifier("unknown_array"),
                        None,
                    ),
                    [PrintStatement(Identifier("item"))],
                ),
                ReturnStatement(Literal("integer", 0)),
            ],
            return_type="Integer",
        )
    ])

    generator = CCodeGenerator(target="c")
    with pytest.raises(RuntimeError) as exc_info:
        generator.generate(ast)

    error_msg = str(exc_info.value)
    assert "without known array size" in error_msg
    assert "unknown_array_length" in error_msg or "unknown_array_len" in error_msg
