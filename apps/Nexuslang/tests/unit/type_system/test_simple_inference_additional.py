"""Focused branch coverage for the lightweight type inference engine."""

from types import SimpleNamespace

from nexuslang.parser.ast import (
    BinaryOperation,
    FunctionCall,
    FunctionDefinition,
    Identifier,
    Literal,
    Parameter,
    Program,
    ReturnStatement,
    UnaryOperation,
    VariableDeclaration,
)
from nexuslang.parser.lexer import Token, TokenType
from nexuslang.typesystem import simple_inference as simple_inference_module
from nexuslang.typesystem.simple_inference import SimpleTypeInference
from nexuslang.typesystem.types import (
    ANY_TYPE,
    BOOLEAN_TYPE,
    DictionaryType,
    FLOAT_TYPE,
    FunctionType,
    INTEGER_TYPE,
    ListType,
    NULL_TYPE,
    STRING_TYPE,
)


class TestSimpleInferenceAdditional:
    def setup_method(self):
        self.inference = SimpleTypeInference()

    def test_get_operator_type_accepts_token_and_token_type(self):
        token = Token(TokenType.FLOOR_DIVIDE, "//", None, 1, 1)

        assert self.inference._get_operator_type(token) == TokenType.FLOOR_DIVIDE
        assert self.inference._get_operator_type(TokenType.IN) == TokenType.IN
        assert self.inference._get_operator_type("plus") is None

    def test_identifier_lookup_prefers_env_then_cache_then_any(self):
        self.inference.variable_types["cached"] = FLOAT_TYPE

        assert self.inference.infer_expression_type(Identifier("scoped"), {"scoped": STRING_TYPE}) == STRING_TYPE
        assert self.inference.infer_expression_type(Identifier("cached")) == FLOAT_TYPE
        assert self.inference.infer_expression_type(Identifier("missing")) == ANY_TYPE

    def test_binary_operations_cover_floor_division_membership_and_unknown_operator(self):
        floor_divide = BinaryOperation(
            Literal("float", 9.0),
            Token(TokenType.FLOOR_DIVIDE, "//", None, 1, 1),
            Literal("integer", 2),
        )
        membership = BinaryOperation(
            Literal("string", "x"),
            TokenType.IN,
            Literal("string", "xyz"),
        )
        unknown = BinaryOperation(Literal("integer", 1), "??", Literal("integer", 2))

        assert self.inference.infer_expression_type(floor_divide) == INTEGER_TYPE
        assert self.inference.infer_expression_type(membership) == BOOLEAN_TYPE
        assert self.inference.infer_expression_type(unknown) == ANY_TYPE

    def test_binary_numeric_operators_fall_back_to_any_for_non_numeric_operands(self):
        plus_non_numeric = BinaryOperation(
            Literal("boolean", True),
            TokenType.PLUS,
            Literal("boolean", False),
        )
        division_non_numeric = BinaryOperation(
            Literal("string", "x"),
            TokenType.DIVIDED_BY,
            Literal("integer", 2),
        )
        power_non_numeric = BinaryOperation(
            Literal("string", "x"),
            TokenType.POWER,
            Literal("integer", 2),
        )
        floor_divide_non_numeric = BinaryOperation(
            Literal("string", "x"),
            TokenType.FLOOR_DIVIDE,
            Literal("integer", 2),
        )

        assert self.inference.infer_expression_type(plus_non_numeric) == ANY_TYPE
        assert self.inference.infer_expression_type(division_non_numeric) == ANY_TYPE
        assert self.inference.infer_expression_type(power_non_numeric) == ANY_TYPE
        assert self.inference.infer_expression_type(floor_divide_non_numeric) == ANY_TYPE

    def test_unary_operations_cover_numeric_boolean_and_fallback_paths(self):
        negative_float = UnaryOperation(TokenType.MINUS, Literal("float", 3.5))
        negate_bool = UnaryOperation(Token(TokenType.NOT, "not", None, 1, 1), Literal("boolean", True))
        invalid = UnaryOperation("not-a-token", Literal("string", "value"))

        assert self.inference.infer_expression_type(negative_float) == FLOAT_TYPE
        assert self.inference.infer_expression_type(negate_bool) == BOOLEAN_TYPE
        assert self.inference.infer_expression_type(invalid) == ANY_TYPE

    def test_function_call_uses_cached_return_type_then_env_function_type(self):
        self.inference.function_return_types["cached_fn"] = STRING_TYPE
        env = {
            "typed_fn": FunctionType([INTEGER_TYPE], BOOLEAN_TYPE),
            "not_callable": INTEGER_TYPE,
        }

        assert self.inference.infer_expression_type(FunctionCall("cached_fn"), env) == STRING_TYPE
        assert self.inference.infer_expression_type(FunctionCall("typed_fn"), env) == BOOLEAN_TYPE
        assert self.inference.infer_expression_type(FunctionCall("not_callable"), env) == ANY_TYPE
        assert self.inference.infer_expression_type(FunctionCall("unknown"), env) == ANY_TYPE

    def test_duck_typed_collection_expressions_infer_element_types_and_empty_defaults(self):
        list_expr = SimpleNamespace(
            node_type="list_expression",
            elements=[Literal("integer", 1), Literal("integer", 2)],
        )
        empty_list_expr = SimpleNamespace(node_type="list_literal", elements=[])
        dict_expr = SimpleNamespace(
            node_type="dict_expression",
            entries=[(Literal("string", "name"), Literal("integer", 7))],
        )
        empty_dict_expr = SimpleNamespace(node_type="dictionary_expression", entries=[])

        list_type = self.inference.infer_expression_type(list_expr)
        empty_list_type = self.inference.infer_expression_type(empty_list_expr)
        dict_type = self.inference.infer_expression_type(dict_expr)
        empty_dict_type = self.inference.infer_expression_type(empty_dict_expr)

        assert list_type == ListType(INTEGER_TYPE)
        assert empty_list_type == ListType(ANY_TYPE)
        assert dict_type == DictionaryType(STRING_TYPE, INTEGER_TYPE)
        assert empty_dict_type == DictionaryType(ANY_TYPE, ANY_TYPE)

    def test_infer_variable_type_covers_annotation_missing_value_and_cached_inference(self):
        annotated = VariableDeclaration("value", Literal("integer", 1), type_annotation="String")
        missing_value = VariableDeclaration("unset", None)
        inferred = VariableDeclaration("count", Literal("integer", 10))

        assert self.inference.infer_variable_type(annotated) == STRING_TYPE
        assert self.inference.infer_variable_type(missing_value) == ANY_TYPE
        assert self.inference.infer_variable_type(inferred) == INTEGER_TYPE
        assert self.inference.variable_types["count"] == INTEGER_TYPE

    def test_infer_function_return_type_covers_null_and_any_paths(self):
        no_value_return = FunctionDefinition(
            "maybe_done",
            [Parameter("item")],
            [ReturnStatement(None)],
        )
        any_return = FunctionDefinition(
            "identity",
            [Parameter("value")],
            [ReturnStatement(Identifier("value"))],
        )

        assert self.inference.infer_function_return_type(no_value_return) == NULL_TYPE
        assert self.inference.infer_function_return_type(any_return) == ANY_TYPE
        assert self.inference.function_return_types["identity"] == ANY_TYPE

    def test_infer_program_types_builds_function_types_with_any_for_untyped_parameters(self):
        program = Program(
            [
                VariableDeclaration("flag", Literal("boolean", True)),
                FunctionDefinition(
                    "echo",
                    [Parameter("value"), Parameter("suffix", "String")],
                    [ReturnStatement(Literal("string", "ok"))],
                ),
            ]
        )

        inferred = self.inference.infer_program_types(program)

        assert inferred["flag"] == BOOLEAN_TYPE
        assert inferred["echo"] == FunctionType([ANY_TYPE, STRING_TYPE], STRING_TYPE)

    def test_expression_and_program_fallback_paths(self):
        assert self.inference.infer_expression_type(object()) == ANY_TYPE

        program = Program(
            [
                ReturnStatement(Literal("integer", 1)),
                VariableDeclaration("x", Literal("integer", 2)),
            ]
        )
        inferred = self.inference.infer_program_types(program)
        assert inferred["x"] == INTEGER_TYPE


class TestSimpleInferenceConvenienceWrappers:
    def setup_method(self):
        simple_inference_module._type_inference.reset()

    def teardown_method(self):
        simple_inference_module._type_inference.reset()

    def test_module_level_wrappers_delegate_to_global_engine(self):
        declaration = VariableDeclaration("x", Literal("integer", 5))
        function = FunctionDefinition(
            "wrap",
            [Parameter("value", "Integer")],
            [ReturnStatement(Identifier("value"))],
        )
        program = Program([declaration, function])

        assert simple_inference_module.infer_variable_type(declaration) == INTEGER_TYPE
        assert simple_inference_module.infer_expression_type(Identifier("x")) == INTEGER_TYPE
        assert simple_inference_module.infer_function_return_type(function) == INTEGER_TYPE
        assert simple_inference_module.infer_program_types(program)["wrap"] == FunctionType([INTEGER_TYPE], INTEGER_TYPE)
