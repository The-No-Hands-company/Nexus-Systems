"""Focused coverage for control-flow/error-handling and type-resolution slices."""

from types import SimpleNamespace

import pytest

from nexuslang.parser.ast import (
    Block,
    ConcurrentBlock,
    Identifier,
    Literal,
    ReturnStatement,
    TryCatch,
    TryCatchBlock,
    VariableDeclaration,
)
from nexuslang.typesystem.typechecker import TypeCheckError, TypeChecker, TypeEnvironment
from nexuslang.typesystem.types import (
    ANY_TYPE,
    INTEGER_TYPE,
    ListType,
    NULL_TYPE,
    STRING_TYPE,
    ClassType,
    GenericType,
    FunctionType,
    UnionType,
)


def _checker() -> TypeChecker:
    return TypeChecker(enable_ownership_passes=False)


def _env() -> TypeEnvironment:
    return TypeEnvironment()


class TestControlFlowAndErrorHandlingSlice:
    def test_return_statement_reports_mismatch_against_expected_return_type(self):
        checker = _checker()
        env = _env()
        env.set_return_type(INTEGER_TYPE)

        result = checker.check_return_statement(ReturnStatement(Literal("string", "oops")), env)

        assert result == STRING_TYPE
        assert any("Return value of type" in error for error in checker.errors)

    def test_generator_return_with_value_reports_error_and_returns_null(self):
        checker = _checker()
        env = _env()
        env.is_generator_function = True

        result = checker.check_return_statement(ReturnStatement(Literal("integer", 1)), env)

        assert result == NULL_TYPE
        assert any("Generator function cannot return a value" in error for error in checker.errors)

    def test_check_statements_for_unreachable_stops_after_terminal_statement(self):
        checker = _checker()
        env = _env()

        unreachable = Identifier("missing_name")
        unreachable.line_number = 123
        result = checker._check_statements_for_unreachable(
            [ReturnStatement(Literal("integer", 42)), unreachable],
            env,
        )

        assert result == INTEGER_TYPE
        assert any("Unreachable code after 'ReturnStatement'" in error for error in checker.errors)
        # The unreachable identifier must not be type-checked after the break.
        assert not any("Undefined variable: missing_name" in error for error in checker.errors)

    def test_check_block_uses_child_scope(self):
        checker = _checker()
        env = _env()

        result = checker.check_block(
            Block([VariableDeclaration("local_only", Literal("integer", 9))]),
            env,
        )

        assert result == INTEGER_TYPE
        with pytest.raises(TypeCheckError, match="Undefined variable: local_only"):
            env.get_variable_type("local_only")

    def test_check_concurrent_block_returns_list_of_union_types(self):
        checker = _checker()

        result = checker.check_concurrent_block(
            ConcurrentBlock([Literal("integer", 1), Literal("string", "x")]),
            _env(),
        )

        assert isinstance(result, ListType)
        assert isinstance(result.element_type, UnionType)
        assert INTEGER_TYPE in result.element_type.types
        assert STRING_TYPE in result.element_type.types

    def test_check_try_catch_block_exception_var_typed_from_annotation(self):
        checker = _checker()

        block = TryCatchBlock(
            try_block=Block([Literal("integer", 1)]),
            catch_block=Block([Identifier("err")]),
            exception_var="err",
            exception_type="ValueError",
        )
        result = checker.check_try_catch_block(block, _env())

        assert isinstance(result, UnionType)
        assert any(isinstance(t, ClassType) and t.name == "ValueError" for t in result.types)

    def test_check_try_catch_with_none_bodies_returns_any_union(self):
        checker = _checker()

        node = TryCatch(try_block=None, catch_block=None)
        result = checker.check_try_catch(node, _env())

        assert isinstance(result, UnionType)
        assert result.types == [ANY_TYPE, ANY_TYPE]


class TestTypeResolutionSlice:
    def test_resolve_type_returns_registered_type(self):
        checker = _checker()
        expected = ClassType("UserType", {}, {})
        checker.type_registry["UserType"] = expected

        resolved = checker.resolve_type("UserType")

        assert resolved is expected

    def test_resolve_type_instantiates_registered_generic_type(self, monkeypatch):
        checker = _checker()
        checker.type_registry["Integer"] = INTEGER_TYPE

        generic_base = GenericType("Box", ["T"], ANY_TYPE)
        checker.type_registry["Box"] = generic_base

        instantiated = ClassType("BoxOfInt", {}, {})

        def _instantiate(base_type, type_args):
            assert base_type is generic_base
            assert type_args == [INTEGER_TYPE]
            return instantiated

        checker.generic_registry = SimpleNamespace(instantiate_type=_instantiate)

        resolved = checker.resolve_type("Box<Integer>")

        assert resolved is instantiated

    def test_resolve_type_raises_for_unknown_type(self):
        checker = _checker()

        with pytest.raises(TypeError, match="Type MissingType not found"):
            checker.resolve_type("MissingType")

    def test_types_compatible_handles_generic_and_class_inheritance_paths(self):
        checker = _checker()

        generic_left = GenericType("Seq", ["T"], ANY_TYPE)
        generic_right = GenericType("Seq", ["U"], ANY_TYPE)
        generic_other = GenericType("Map", ["T"], ANY_TYPE)

        # The compatibility helper currently recurses into type_parameters directly,
        # so use type objects there to exercise that branch safely.
        generic_left.type_parameters = [INTEGER_TYPE]
        generic_right.type_parameters = [INTEGER_TYPE]
        generic_other.type_parameters = [INTEGER_TYPE]

        assert checker.types_compatible(generic_left, generic_right) is True
        assert checker.types_compatible(generic_left, generic_other) is False

        base = ClassType("Base", {}, {})
        derived = ClassType("Derived", {}, {}, parent_classes=["Base"])
        assert checker.types_compatible(base, derived) is True

    def test_types_compatible_falls_back_to_type_level_compatibility(self):
        checker = _checker()

        assert checker.types_compatible(INTEGER_TYPE, ANY_TYPE) is True

    def test_check_print_statement_accepts_expression_and_returns_any(self):
        checker = _checker()
        stmt = SimpleNamespace(expression=Literal("integer", 5), value=None)

        result = checker.check_print_statement(stmt, _env())

        assert result == ANY_TYPE

    def test_check_type_alias_definition_registers_resolved_target(self):
        checker = _checker()
        checker.type_registry["Integer"] = INTEGER_TYPE

        alias_def = SimpleNamespace(name="MyInt", target_type="Integer")
        result = checker.check_type_alias_definition(alias_def)

        assert result == INTEGER_TYPE
        assert checker.type_registry["MyInt"] == INTEGER_TYPE

    def test_check_lambda_expression_delegates_to_inference_engine(self, monkeypatch):
        checker = _checker()

        expected = FunctionType([INTEGER_TYPE], STRING_TYPE)

        def _infer(lambda_expr, expected_type, variables):
            assert expected_type is None
            return expected

        monkeypatch.setattr(checker.type_inference, "infer_lambda_types", _infer)

        result = checker.check_lambda_expression(SimpleNamespace(), _env())

        assert result is expected