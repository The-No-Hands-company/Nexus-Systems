"""Focused coverage for expect-statement and FFI handler branches."""

from nexuslang.parser.ast import ExpectStatement, Identifier, Literal
from nexuslang.typesystem.typechecker import TypeChecker, TypeEnvironment
from nexuslang.typesystem.types import (
    ANY_TYPE,
    BOOLEAN_TYPE,
    ChannelType,
    DictionaryType,
    INTEGER_TYPE,
    ListType,
    STRING_TYPE,
)


def _checker() -> TypeChecker:
    return TypeChecker(enable_ownership_passes=False)


def _env() -> TypeEnvironment:
    return TypeEnvironment()


def _mock_statement(class_name: str, **attrs):
    cls = type(class_name, (), {})
    obj = cls()
    for key, value in attrs.items():
        setattr(obj, key, value)
    return obj


class TestExpectStatementBranches:
    def test_approximately_equal_reports_non_numeric_expected_and_tolerance(self):
        checker = _checker()
        env = _env()

        node = ExpectStatement(
            Literal("integer", 10),
            "approximately_equal",
            Literal("string", "bad"),
            tolerance_expr=Literal("string", "tiny"),
        )

        result = checker.check_expect_statement(node, env)

        assert result == BOOLEAN_TYPE
        assert any("approximately_equal requires numeric operands" in err for err in checker.errors)
        assert any("tolerance must be numeric" in err for err in checker.errors)

    def test_contain_on_list_reports_element_type_mismatch(self):
        checker = _checker()
        env = _env()
        env.define_variable("nums", ListType(INTEGER_TYPE))

        node = ExpectStatement(
            Identifier("nums"),
            "contain",
            Literal("string", "bad"),
        )

        checker.check_expect_statement(node, env)

        assert any("expect contain on list expects element type" in err for err in checker.errors)

    def test_contain_on_dictionary_reports_key_type_mismatch(self):
        checker = _checker()
        env = _env()
        env.define_variable("mapping", DictionaryType(INTEGER_TYPE, STRING_TYPE))

        node = ExpectStatement(
            Identifier("mapping"),
            "contain",
            Literal("string", "wrong-key"),
        )

        checker.check_expect_statement(node, env)

        assert any("expect contain on dictionary expects key type" in err for err in checker.errors)


class TestFfiStatementBranches:
    def test_check_ffi_statement_registers_extern_function_and_variadic_flag(self):
        checker = _checker()
        env = _env()

        p1 = _mock_statement("Parameter", type_annotation="Integer")
        p2 = _mock_statement("Parameter", type_annotation="String")
        stmt = _mock_statement(
            "ExternFunctionDeclaration",
            name="c_log",
            parameters=[p1, p2],
            return_type="Boolean",
            variadic=True,
            line_number=7,
        )

        handled, result_type = checker._check_ffi_statement(stmt, env)
        fn_type = env.get_function_type("c_log")

        assert handled is True
        assert result_type == ANY_TYPE
        assert fn_type.return_type == BOOLEAN_TYPE
        assert fn_type.variadic is True

    def test_check_ffi_statement_handles_extern_variable_and_type_and_unknown(self):
        checker = _checker()
        env = _env()

        var_stmt = _mock_statement(
            "ExternVariableDeclaration",
            name="errno",
            type_annotation="Integer",
            line_number=11,
        )
        type_stmt = _mock_statement(
            "ExternTypeDeclaration",
            name="Opaque",
            is_opaque=False,
            base_type="struct",
            line_number=12,
        )
        unknown_stmt = _mock_statement("NotAnFfiStatement")

        var_handled, _ = checker._check_ffi_statement(var_stmt, env)
        type_handled, _ = checker._check_ffi_statement(type_stmt, env)
        unknown_handled, unknown_value = checker._check_ffi_statement(unknown_stmt, env)

        assert var_handled is True
        assert type_handled is True
        assert env.get_variable_type("errno") == INTEGER_TYPE
        assert checker.ffi_extern_types.get("Opaque") is type_stmt
        assert unknown_handled is False
        assert unknown_value is None

    def test_validate_extern_type_declaration_function_pointer_signature_checks(self):
        checker = _checker()

        captured = []

        def _capture(annotation, line, context):
            captured.append((annotation, line, context))

        checker._validate_ffi_abi_type = _capture

        stmt = _mock_statement(
            "ExternTypeDeclaration",
            name="FnPtr",
            is_function_pointer=True,
            function_signature=(["Integer", "String"], "Boolean"),
            line_number=44,
        )

        checker._validate_extern_type_declaration(stmt)

        assert len(captured) == 3
        assert "param 1" in captured[0][2]
        assert "param 2" in captured[1][2]
        assert "return" in captured[2][2]
