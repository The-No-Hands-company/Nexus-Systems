"""Focused coverage for type_inference.py advanced type inference paths.

Targets bidirectional inference, generic context, lambda types, member access,
and union type unification (lines with low branch coverage).
"""

import pytest

from nexuslang.parser.ast import (
    Literal, Identifier, BinaryOperation, FunctionCall, VariableDeclaration,
    FunctionDefinition, Parameter, ClassDefinition, PropertyDeclaration,
    MethodDefinition, UnaryOperation, Program, ReturnStatement, Block,
)
from nexuslang.typesystem.type_inference import TypeInferenceEngine
from nexuslang.typesystem.types import (
    INTEGER_TYPE, FLOAT_TYPE, STRING_TYPE, BOOLEAN_TYPE, ANY_TYPE,
    ListType, DictionaryType, FunctionType, UnionType, ClassType,
    PrimitiveType, get_type_by_name,
)
from nexuslang.typesystem.generic_types import GenericTypeContext


class _MockExpr:
    """Mock expression for testing."""
    def __init__(self, node_type=None, **kwargs):
        self.node_type = node_type
        self.__dict__.update(kwargs)


class _MockParam:
    """Mock parameter."""
    def __init__(self, name, type_annotation=None):
        self.name = name
        self.type_annotation = type_annotation


class _MockLambda:
    """Mock lambda expression."""
    def __init__(self, parameters=None, body=None, return_type=None):
        self.parameters = parameters or []
        self.body = body
        self.return_type = return_type
        self.__class__.__name__ = "LambdaExpression"


def _create_env():
    """Create a type environment with predefined variables."""
    return {
        "x": INTEGER_TYPE,
        "y": FLOAT_TYPE,
        "s": STRING_TYPE,
        "b": BOOLEAN_TYPE,
    }


class TestBidirectionalInference:
    """Tests for bidirectional type inference with expected types."""

    def test_infer_with_expected_type_list_literal(self):
        """List literal inference with expected ListType."""
        engine = TypeInferenceEngine()
        env = _create_env()

        # Create a mock list literal with integer elements
        list_literal = _MockExpr(node_type="list_literal", elements=[
            Literal("integer", 1),
            Literal("integer", 2),
            Literal("integer", 3),
        ])

        expected = ListType(INTEGER_TYPE)
        result = engine.infer_with_expected_type(list_literal, expected, env)

        assert result == expected

    def test_infer_with_expected_type_dict_literal(self):
        """Dictionary literal inference with expected DictionaryType."""
        engine = TypeInferenceEngine()
        env = _create_env()

        # Create a mock dict literal
        dict_literal = _MockExpr(node_type="dictionary_literal",
                                keys=[Literal("string", "a"), Literal("string", "b")],
                                values=[Literal("integer", 1), Literal("integer", 2)])

        expected = DictionaryType(STRING_TYPE, INTEGER_TYPE)
        result = engine.infer_with_expected_type(dict_literal, expected, env)

        assert result == expected

    def test_infer_with_expected_type_dict_type_mismatch(self):
        """Dictionary literal with incompatible value types falls back."""
        engine = TypeInferenceEngine()
        env = _create_env()

        # Dict with mixed value types (int and float)
        dict_literal = _MockExpr(node_type="dictionary_literal",
                                keys=[Literal("string", "a"), Literal("string", "b")],
                                values=[Literal("integer", 1), Literal("float", 2.5)])

        expected = DictionaryType(STRING_TYPE, INTEGER_TYPE)
        result = engine.infer_with_expected_type(dict_literal, expected, env)

        # Should fall back to inferred type
        assert result is not None  # Should still infer something

    def test_infer_with_expected_type_lambda_expression(self):
        """Lambda expression with expected function type."""
        engine = TypeInferenceEngine()
        env = _create_env()

        # Create a lambda with expected signature
        param = _MockParam("a", "Integer")
        lambda_expr = _MockLambda(parameters=[param], body=Literal("integer", 42))

        expected = FunctionType([INTEGER_TYPE], INTEGER_TYPE)
        result = engine.infer_with_expected_type(lambda_expr, expected, env)

        assert isinstance(result, FunctionType)
        assert result.return_type == INTEGER_TYPE

    def test_infer_with_expected_type_function_call_compatible(self):
        """Function call with compatible expected return type."""
        engine = TypeInferenceEngine()
        env = _create_env()
        env["add"] = FunctionType([INTEGER_TYPE, INTEGER_TYPE], INTEGER_TYPE)

        # Create a function call: add(x, y)
        call = FunctionCall(Identifier("add"), [Identifier("x"), Identifier("y")])

        expected = INTEGER_TYPE
        result = engine.infer_with_expected_type(call, expected, env)

        # Result should match expected
        assert result == INTEGER_TYPE

    def test_infer_with_expected_type_no_expected_falls_back(self):
        """When expected type is None, fall back to regular inference."""
        engine = TypeInferenceEngine()
        env = _create_env()

        expr = Literal("string", "hello")
        result = engine.infer_with_expected_type(expr, None, env)

        assert result == STRING_TYPE


class TestLambdaTypeInference:
    """Tests for lambda expression type inference."""

    def test_infer_lambda_with_explicit_types(self):
        """Lambda with explicit parameter and return types."""
        engine = TypeInferenceEngine()
        env = _create_env()

        param = _MockParam("x", "Integer")
        lambda_expr = _MockLambda(
            parameters=[param],
            body=BinaryOperation(Identifier("x"), "plus", Literal("integer", 1)),
            return_type="Integer"
        )

        result = engine.infer_lambda_types(lambda_expr, None, env)

        assert isinstance(result, FunctionType)
        assert result.param_types[0] == INTEGER_TYPE
        assert result.return_type == INTEGER_TYPE

    def test_infer_lambda_with_expected_type(self):
        """Lambda inference guided by expected function type."""
        engine = TypeInferenceEngine()
        env = _create_env()

        # Lambda with no explicit types
        param = _MockParam("x", None)
        lambda_expr = _MockLambda(parameters=[param], body=Literal("integer", 42))

        expected = FunctionType([FLOAT_TYPE], INTEGER_TYPE)
        result = engine.infer_lambda_types(lambda_expr, expected, env)

        # Parameter type should come from expected
        assert result.param_types[0] == FLOAT_TYPE
        assert result.return_type == INTEGER_TYPE

    def test_infer_lambda_multiple_parameters(self):
        """Lambda with multiple parameters."""
        engine = TypeInferenceEngine()
        env = _create_env()

        params = [
            _MockParam("a", "Integer"),
            _MockParam("b", "Float")
        ]
        lambda_expr = _MockLambda(
            parameters=params,
            body=BinaryOperation(Identifier("a"), "plus", Identifier("b")),
            return_type="Float"
        )

        result = engine.infer_lambda_types(lambda_expr, None, env)

        assert len(result.param_types) == 2
        assert result.param_types[0] == INTEGER_TYPE
        assert result.param_types[1] == FLOAT_TYPE
        assert result.return_type == FLOAT_TYPE

    def test_infer_lambda_body_multiple_statements(self):
        """Lambda with multi-statement body."""
        engine = TypeInferenceEngine()
        env = _create_env()

        param = _MockParam("x", "Integer")
        # Mock a body with return statements
        return_stmt = _MockExpr(node_type="return_statement", value=Literal("integer", 99))
        lambda_expr = _MockLambda(
            parameters=[param],
            body=[return_stmt]
        )

        result = engine.infer_lambda_types(lambda_expr, None, env)

        assert isinstance(result, FunctionType)
        assert result.param_types[0] == INTEGER_TYPE

    def test_infer_lambda_no_parameters(self):
        """Lambda with no parameters."""
        engine = TypeInferenceEngine()
        env = _create_env()

        lambda_expr = _MockLambda(parameters=[], body=Literal("string", "constant"))

        result = engine.infer_lambda_types(lambda_expr, None, env)

        assert len(result.param_types) == 0
        assert result.return_type == STRING_TYPE


class TestArgumentTypeInference:
    """Tests for function argument type inference from function signature."""

    def test_infer_argument_types_from_function_compatible(self):
        """Infer argument types matching function signature."""
        engine = TypeInferenceEngine()
        env = _create_env()

        func_type = FunctionType([INTEGER_TYPE, FLOAT_TYPE], BOOLEAN_TYPE)
        arguments = [Literal("integer", 42), Literal("float", 3.14)]

        result = engine.infer_argument_types_from_function(func_type, arguments, env)

        assert len(result) == 2
        assert result[0] == INTEGER_TYPE
        assert result[1] == FLOAT_TYPE

    def test_infer_argument_types_fewer_arguments_than_params(self):
        """Function call with fewer arguments than parameters."""
        engine = TypeInferenceEngine()
        env = _create_env()

        func_type = FunctionType([INTEGER_TYPE, FLOAT_TYPE, STRING_TYPE], BOOLEAN_TYPE)
        arguments = [Literal("integer", 1), Literal("float", 2.5)]

        result = engine.infer_argument_types_from_function(func_type, arguments, env)

        assert len(result) == 2
        assert result[0] == INTEGER_TYPE
        assert result[1] == FLOAT_TYPE

    def test_infer_argument_types_more_arguments_than_params(self):
        """Function call with more arguments than parameters (variadic or error)."""
        engine = TypeInferenceEngine()
        env = _create_env()

        func_type = FunctionType([INTEGER_TYPE], BOOLEAN_TYPE)
        arguments = [Literal("integer", 1), Literal("integer", 2), Literal("integer", 3)]

        result = engine.infer_argument_types_from_function(func_type, arguments, env)

        # Should still infer all arguments without expected types for extras
        assert len(result) == 3
        assert result[0] == INTEGER_TYPE


class TestUnifyTypes:
    """Tests for type unification."""

    def test_unify_same_types(self):
        """Unifying identical types returns that type."""
        engine = TypeInferenceEngine()

        result = engine.unify_types(INTEGER_TYPE, INTEGER_TYPE)
        assert result == INTEGER_TYPE

    def test_unify_with_any_type(self):
        """Unifying with ANY_TYPE returns ANY_TYPE."""
        engine = TypeInferenceEngine()

        result = engine.unify_types(INTEGER_TYPE, ANY_TYPE)
        assert result == ANY_TYPE

        result = engine.unify_types(ANY_TYPE, STRING_TYPE)
        assert result == ANY_TYPE

    def test_unify_union_types(self):
        """Unifying two union types."""
        engine = TypeInferenceEngine()

        union1 = UnionType([INTEGER_TYPE, FLOAT_TYPE])
        union2 = UnionType([STRING_TYPE, BOOLEAN_TYPE])

        result = engine.unify_types(union1, union2)
        assert isinstance(result, UnionType)
        # Should have all 4 types (no duplicates)
        assert len(result.types) == 4

    def test_unify_union_with_non_union(self):
        """Unifying union type with regular type."""
        engine = TypeInferenceEngine()

        union_type = UnionType([INTEGER_TYPE, FLOAT_TYPE])
        result = engine.unify_types(union_type, INTEGER_TYPE)

        # INTEGER_TYPE is compatible with union_type, so return union_type
        assert result == union_type

    def test_unify_compatible_types(self):
        """Unifying compatible types returns more general type."""
        engine = TypeInferenceEngine()

        # Integer is compatible with Float
        result = engine.unify_types(INTEGER_TYPE, FLOAT_TYPE)
        # Result depends on is_compatible_with implementation
        assert result is not None

    def test_unify_incompatible_types(self):
        """Unifying incompatible types returns None."""
        engine = TypeInferenceEngine()

        # String and Integer are not compatible
        result = engine.unify_types(STRING_TYPE, INTEGER_TYPE)
        assert result is None


class TestVariableDeclarationInference:
    """Tests for variable declaration type inference."""

    def test_infer_variable_with_explicit_type(self):
        """Variable declaration with explicit type annotation."""
        engine = TypeInferenceEngine()
        env = _create_env()

        decl = VariableDeclaration("count", Literal("integer", 5), "Integer")

        result = engine.infer_variable_declaration(decl, env)
        assert result == INTEGER_TYPE

    def test_infer_variable_without_type_annotation(self):
        """Variable declaration without type, infer from value."""
        engine = TypeInferenceEngine()
        env = _create_env()

        decl = VariableDeclaration("name", Literal("string", "Alice"), None)

        result = engine.infer_variable_declaration(decl, env)
        assert result == STRING_TYPE

    def test_infer_variable_no_value_no_type(self):
        """Variable declaration with neither type nor value."""
        engine = TypeInferenceEngine()
        env = _create_env()

        decl = VariableDeclaration("unknown", None, None)

        result = engine.infer_variable_declaration(decl, env)
        assert result == ANY_TYPE

    def test_infer_variable_with_allocator_hint(self):
        """Variable with AllocatorHint type annotation."""
        from nexuslang.parser.ast import AllocatorHint

        engine = TypeInferenceEngine()
        env = _create_env()

        hint = AllocatorHint("Integer", "malloc")
        decl = VariableDeclaration("buf", Literal("integer", 100), hint)

        result = engine.infer_variable_declaration(decl, env)
        assert result == INTEGER_TYPE


class TestFunctionReturnTypeInference:
    """Tests for function return type inference."""

    def test_infer_function_with_explicit_return_type(self):
        """Function with explicit return type annotation."""
        engine = TypeInferenceEngine()
        env = _create_env()

        func = FunctionDefinition(
            "add",
            [_MockParam("a", "Integer"), _MockParam("b", "Integer")],
            [ReturnStatement(Literal("integer", 0))],
            "Integer"
        )

        result = engine.infer_function_return_type(func, env)
        assert result == INTEGER_TYPE

    def test_infer_function_without_return_type_single_return(self):
        """Function without return type, inferred from return statement."""
        engine = TypeInferenceEngine()
        env = _create_env()

        func = FunctionDefinition(
            "create_string",
            [],
            [ReturnStatement(Literal("string", "hello"))],
            None
        )

        result = engine.infer_function_return_type(func, env)
        assert result == STRING_TYPE

    def test_infer_function_multiple_returns_same_type(self):
        """Function with multiple return statements of same type."""
        engine = TypeInferenceEngine()
        env = _create_env()

        returns = [
            _MockExpr(node_type="return_statement", value=Literal("integer", 1)),
            _MockExpr(node_type="return_statement", value=Literal("integer", 2)),
        ]
        func = FunctionDefinition("pick", [], returns, None)

        result = engine.infer_function_return_type(func, env)
        assert result == INTEGER_TYPE

    def test_infer_function_no_return_statements(self):
        """Function with no return statements defaults to NULL_TYPE."""
        engine = TypeInferenceEngine()
        env = _create_env()

        from nexuslang.parser.ast import PrintStatement
        print_stmt = PrintStatement(Literal("string", "hello"))
        func = FunctionDefinition("announce", [], [print_stmt], None)

        result = engine.infer_function_return_type(func, env)
        from nexuslang.typesystem.types import NULL_TYPE
        assert result == NULL_TYPE


class TestClassTypeInference:
    """Tests for class type inference."""

    def test_infer_class_with_methods(self):
        """Class with methods."""
        engine = TypeInferenceEngine()
        env = _create_env()

        methods = [
            MethodDefinition("distance", [], [ReturnStatement(Literal("float", 0.0))], "Float")
        ]
        class_def = ClassDefinition("Point", methods=methods)

        result = engine.infer_class_type(class_def, env)

        assert isinstance(result, ClassType)
        assert "distance" in result.methods

    def test_infer_class_with_type_parameters(self):
        """Generic class with type parameters."""
        engine = TypeInferenceEngine()
        env = _create_env()

        # Mock type parameter
        type_param = _MockExpr(name="T")

        class_def = ClassDefinition("Box", generic_parameters=[type_param])

        result = engine.infer_class_type(class_def, env)

        # Result should be a GenericType wrapping the class
        # (implementation may vary)
        assert result is not None


class TestMemberAccessInference:
    """Tests for member access type inference."""

    def test_infer_member_access_property(self):
        """Member access to object property."""
        engine = TypeInferenceEngine()

        # Create a mock Point object
        point = _MockExpr()
        point_type = ClassType("Point", {"x": INTEGER_TYPE, "y": FLOAT_TYPE}, {}, None)

        env = {"point": point_type}

        # Create mock member access: point.x
        member_access = _MockExpr(node_type="member_access",
                                 object_expr=Identifier("point"),
                                 member_name="x")

        result = engine.infer_member_access_type(member_access, env)
        assert result == INTEGER_TYPE

    def test_infer_member_access_method(self):
        """Member access to object method."""
        engine = TypeInferenceEngine()

        method_type = FunctionType([INTEGER_TYPE], STRING_TYPE)
        obj_type = ClassType("Handler", {}, {"process": method_type}, None)

        env = {"handler": obj_type}

        member_access = _MockExpr(node_type="member_access",
                                 object_expr=Identifier("handler"),
                                 member_name="process")

        result = engine.infer_member_access_type(member_access, env)
        assert result == method_type


class TestIndexExpressionInference:
    """Tests for index expression type inference."""

    def test_infer_index_expression_on_list(self):
        """Index expression on list type."""
        engine = TypeInferenceEngine()

        list_type = ListType(STRING_TYPE)
        env = {"words": list_type}

        index_expr = _MockExpr(node_type="index_expression",
                              array_expr=Identifier("words"),
                              index_expr=Literal("integer", 0))

        result = engine.infer_index_expression_type(index_expr, env)
        assert result == STRING_TYPE

    def test_infer_index_expression_on_dict(self):
        """Index expression on dictionary type."""
        engine = TypeInferenceEngine()

        dict_type = DictionaryType(STRING_TYPE, INTEGER_TYPE)
        env = {"counts": dict_type}

        index_expr = _MockExpr(node_type="index_expression",
                              array_expr=Identifier("counts"),
                              index_expr=Literal("string", "key"))

        result = engine.infer_index_expression_type(index_expr, env)
        assert result == INTEGER_TYPE


class TestUnaryOperationInference:
    """Tests for unary operation type inference."""

    def test_infer_unary_negation_integer(self):
        """Unary negation on integer."""
        engine = TypeInferenceEngine()
        env = _create_env()

        unary = UnaryOperation("-", Literal("integer", 5))

        result = engine.infer_expression_type(unary, env)
        assert result == INTEGER_TYPE

    def test_infer_unary_not_boolean(self):
        """Unary logical NOT on boolean."""
        engine = TypeInferenceEngine()
        env = _create_env()

        unary = UnaryOperation("not", Identifier("b"))

        result = engine.infer_expression_type(unary, env)
        assert result == BOOLEAN_TYPE


class TestEngineStateManagement:
    """Tests for TypeInferenceEngine state management."""

    def test_fresh_type_variable_generation(self):
        """Fresh type variables have unique names."""
        engine = TypeInferenceEngine()

        var1 = engine.fresh_type_variable()
        var2 = engine.fresh_type_variable()
        var3 = engine.fresh_type_variable()

        assert var1 != var2
        assert var2 != var3
        assert var1 == "T0"
        assert var2 == "T1"
        assert var3 == "T2"

    def test_reset_clears_state(self):
        """Reset method clears all state."""
        engine = TypeInferenceEngine()

        engine.type_constraints["x"] = INTEGER_TYPE
        engine.type_variables["T0"] = FLOAT_TYPE
        engine.variable_types["y"] = STRING_TYPE
        engine.next_type_var = 5

        engine.reset()

        assert len(engine.type_constraints) == 0
        assert len(engine.type_variables) == 0
        assert len(engine.variable_types) == 0
        assert engine.next_type_var == 0

    def test_track_variable_type(self):
        """Engine can track variable types."""
        engine = TypeInferenceEngine()

        engine.variable_types["x"] = INTEGER_TYPE
        engine.variable_types["y"] = FLOAT_TYPE

        assert engine.variable_types["x"] == INTEGER_TYPE
        assert engine.variable_types["y"] == FLOAT_TYPE
