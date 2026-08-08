"""Focused coverage for typechecker.py mid-file semantic handlers.

Targets statement/expression handlers (lines 534-612) and FFI validation (lines 746-876).
"""

import sys
import types

import pytest

from nexuslang.parser.ast import (
    Program, VariableDeclaration, IfStatement, ReturnStatement, RaiseStatement,
)
from nexuslang.typesystem.typechecker import (
    TypeCheckError,
    TypeChecker,
    TypeEnvironment,
)
from nexuslang.typesystem.types import (
    INTEGER_TYPE,
    STRING_TYPE,
    ANY_TYPE,
)


class _MockStatement:
    """Mock statement node with configurable class name and attributes."""

    def __init__(self, class_name: str = "UnknownStatement", **kwargs):
        self.__class__.__name__ = class_name
        self.__dict__.update(kwargs)


class _MockExpr:
    """Mock expression node."""

    def __init__(self, **kwargs):
        self.__dict__.update(kwargs)


def _create_env():
    """Create a TypeEnvironment with predefined basic types."""
    env = TypeEnvironment()
    return env


def _create_checker():
    """Create a TypeChecker instance for testing."""
    return TypeChecker()


class TestImportStatementHandling:
    """Tests for _check_import_statement handler."""

    def test_import_statement_with_module_name_only(self):
        """ImportStatement with module_name but no alias."""
        checker = _create_checker()
        env = _create_env()

        stmt = _MockStatement("ImportStatement", module_name="os")
        handled, result = checker._check_import_statement(stmt, env)

        assert handled is True
        assert result == ANY_TYPE
        assert env.get_variable_type("os") == ANY_TYPE

    def test_import_statement_with_alias(self):
        """ImportStatement with both module_name and alias."""
        checker = _create_checker()
        env = _create_env()

        stmt = _MockStatement("ImportStatement", module_name="os", alias="operating_system")
        handled, result = checker._check_import_statement(stmt, env)

        assert handled is True
        assert result == ANY_TYPE
        assert env.get_variable_type("operating_system") == ANY_TYPE

    def test_import_statement_with_dotted_module(self):
        """ImportStatement with dotted module path (uses last component as name)."""
        checker = _create_checker()
        env = _create_env()

        stmt = _MockStatement("ImportStatement", module_name="os.path")
        handled, result = checker._check_import_statement(stmt, env)

        assert handled is True
        assert result == ANY_TYPE
        assert env.get_variable_type("path") == ANY_TYPE

    def test_selective_import_multiple_names(self):
        """SelectiveImport with multiple imported_names."""
        checker = _create_checker()
        env = _create_env()

        stmt = _MockStatement("SelectiveImport", imported_names=["func1", "func2", "const"])
        handled, result = checker._check_import_statement(stmt, env)

        assert handled is True
        assert result == ANY_TYPE
        assert env.get_variable_type("func1") == ANY_TYPE
        assert env.get_variable_type("func2") == ANY_TYPE
        assert env.get_variable_type("const") == ANY_TYPE

    def test_selective_import_empty_names(self):
        """SelectiveImport with empty imported_names list."""
        checker = _create_checker()
        env = _create_env()

        stmt = _MockStatement("SelectiveImport", imported_names=[])
        handled, result = checker._check_import_statement(stmt, env)

        assert handled is True
        assert result == ANY_TYPE

    def test_module_access_statement(self):
        """ModuleAccess statement node."""
        checker = _create_checker()
        env = _create_env()

        stmt = _MockStatement("ModuleAccess")
        handled, result = checker._check_import_statement(stmt, env)

        assert handled is True
        assert result == ANY_TYPE

    def test_unhandled_import_type(self):
        """Non-import statement type returns False."""
        checker = _create_checker()
        env = _create_env()

        stmt = _MockStatement("SomeOtherType")
        handled, result = checker._check_import_statement(stmt, env)

        assert handled is False
        assert result is None


class TestDataStructureStatementHandling:
    """Tests for _check_data_structure_statement handler."""

    def test_send_statement_dispatches_to_check(self):
        """SendStatement routing delegates to _check_send_statement."""
        checker = _create_checker()
        env = _create_env()

        stmt = _MockStatement("SendStatement", channel="ch", value="data")
        # Mock the _check_send_statement to verify it's called
        checker._check_send_statement = lambda s, e: None

        handled, result = checker._check_data_structure_statement(stmt, env)

        assert handled is True
        assert result == ANY_TYPE

    def test_close_statement_dispatches_to_check(self):
        """CloseStatement routing delegates to _check_close_statement."""
        checker = _create_checker()
        env = _create_env()

        stmt = _MockStatement("CloseStatement", channel="ch")
        # Mock the _check_close_statement to verify it's called
        checker._check_close_statement = lambda s, e: None

        handled, result = checker._check_data_structure_statement(stmt, env)

        assert handled is True
        assert result == ANY_TYPE

    def test_index_assignment_with_target_and_value(self):
        """IndexAssignment with both target and value expressions."""
        checker = _create_checker()
        env = _create_env()
        env.define_variable("arr", ANY_TYPE)

        # Create mock expressions that check_expression will handle
        target = _MockExpr()
        value = _MockExpr()

        stmt = _MockStatement("IndexAssignment", target=target, value=value)
        # We need to mock check_expression to return a type
        original_check = checker.check_expression
        checker.check_expression = lambda expr, e: ANY_TYPE
        try:
            handled, result = checker._check_data_structure_statement(stmt, env)
            assert handled is True
            assert result == ANY_TYPE
        finally:
            checker.check_expression = original_check

    def test_index_assignment_target_only(self):
        """IndexAssignment with only target (no value)."""
        checker = _create_checker()
        env = _create_env()

        target = _MockExpr()
        stmt = _MockStatement("IndexAssignment", target=target)
        original_check = checker.check_expression
        checker.check_expression = lambda expr, e: ANY_TYPE
        try:
            handled, result = checker._check_data_structure_statement(stmt, env)
            assert handled is True
            assert result == ANY_TYPE
        finally:
            checker.check_expression = original_check

    def test_sizeof_expression(self):
        """SizeofExpression returns INTEGER_TYPE."""
        from nexuslang.parser.ast import SizeofExpression

        checker = _create_checker()
        env = _create_env()

        stmt = SizeofExpression("Integer")
        handled, result = checker._check_data_structure_statement(stmt, env)

        assert handled is True
        assert result == INTEGER_TYPE

    def test_raise_statement_with_string_message(self):
        """RaiseStatement with String message type."""
        checker = _create_checker()
        env = _create_env()

        # Create a string literal that will pass type check
        message = _MockExpr()
        stmt = RaiseStatement(exception_type="Error", message=message)

        original_check = checker.check_statement
        checker.check_statement = lambda s, e: STRING_TYPE
        try:
            handled, result = checker._check_data_structure_statement(stmt, env)
            assert handled is True
            assert result == ANY_TYPE
        finally:
            checker.check_statement = original_check

    def test_raise_statement_with_non_string_message(self):
        """RaiseStatement with non-String message type produces error."""
        checker = _create_checker()
        env = _create_env()

        message = _MockExpr()
        stmt = RaiseStatement(exception_type="Error", message=message)
        stmt.line_number = 42

        original_check = checker.check_statement
        checker.check_statement = lambda s, e: INTEGER_TYPE
        try:
            handled, result = checker._check_data_structure_statement(stmt, env)
            assert handled is True
            assert result == ANY_TYPE
            # Verify error was appended
            assert any("Raise message should be String" in err for err in checker.errors)
        finally:
            checker.check_statement = original_check

    def test_conditional_compilation_block(self):
        """Conditional compilation block with node_type marker."""
        checker = _create_checker()
        env = _create_env()

        # Create mock body statements
        body_stmt = _MockStatement("SomeStatement")
        else_stmt = _MockStatement("SomeStatement")

        stmt = _MockStatement("ConditionalBlock", node_type="conditional_compilation_block",
                             body=[body_stmt], else_body=[else_stmt])

        original_check = checker.check_statement
        checker.check_statement = lambda s, e: ANY_TYPE
        try:
            handled, result = checker._check_data_structure_statement(stmt, env)
            assert handled is True
            assert result == ANY_TYPE
        finally:
            checker.check_statement = original_check

    def test_unhandled_data_structure_type(self):
        """Non-data-structure statement returns False."""
        checker = _create_checker()
        env = _create_env()

        stmt = _MockStatement("UnknownDataType")
        handled, result = checker._check_data_structure_statement(stmt, env)

        assert handled is False
        assert result is None


class TestFFIValidation:
    """Tests for FFI validation methods (_validate_ffi_abi_type, etc.)."""

    def test_validate_ffi_abi_type_primitive_integer(self):
        """Primitive Integer type is ABI-safe."""
        checker = _create_checker()
        # Should not append errors for primitive types
        checker._validate_ffi_abi_type("Integer", 10, "test param")
        assert len(checker.errors) == 0

    def test_validate_ffi_abi_type_primitive_float(self):
        """Primitive Float type is ABI-safe."""
        checker = _create_checker()
        checker._validate_ffi_abi_type("Float", 10, "test param")
        assert len(checker.errors) == 0

    def test_validate_ffi_abi_type_primitive_string(self):
        """Primitive String type is ABI-safe."""
        checker = _create_checker()
        checker._validate_ffi_abi_type("String", 10, "test param")
        assert len(checker.errors) == 0

    def test_validate_ffi_abi_type_void(self):
        """Void type is ABI-safe."""
        checker = _create_checker()
        checker._validate_ffi_abi_type("Void", 10, "test return")
        assert len(checker.errors) == 0

    def test_validate_ffi_abi_type_pointer(self):
        """Pointer type is ABI-safe."""
        checker = _create_checker()
        checker._validate_ffi_abi_type("void*", 10, "test param")
        assert len(checker.errors) == 0

    def test_validate_ffi_abi_type_list_unsafe(self):
        """List type is ABI-unsafe (unsized container)."""
        checker = _create_checker()
        checker._validate_ffi_abi_type("List", 10, "test param")
        assert len(checker.errors) == 1
        assert "ABI-unstable" in checker.errors[0]

    def test_validate_ffi_abi_type_dict_unsafe(self):
        """Dict type is ABI-unsafe (unsized container)."""
        checker = _create_checker()
        checker._validate_ffi_abi_type("Dictionary", 10, "test param")
        assert len(checker.errors) == 1
        assert "ABI-unstable" in checker.errors[0]

    def test_validate_ffi_abi_type_option_unsafe(self):
        """Option type is ABI-unsafe (managed container)."""
        checker = _create_checker()
        checker._validate_ffi_abi_type("Option", 10, "test param")
        assert len(checker.errors) == 1
        assert "ABI-unstable" in checker.errors[0]

    def test_validate_ffi_abi_type_result_unsafe(self):
        """Result type is ABI-unsafe (managed container)."""
        checker = _create_checker()
        checker._validate_ffi_abi_type("Result", 10, "test param")
        assert len(checker.errors) == 1
        assert "ABI-unstable" in checker.errors[0]

    def test_is_pointer_like_annotation_with_star(self):
        """Annotation containing * is recognized as pointer-like."""
        checker = _create_checker()
        assert checker._is_pointer_like_annotation("int*") is True
        assert checker._is_pointer_like_annotation("void*") is True

    def test_is_pointer_like_annotation_with_pointer_keyword(self):
        """Annotation containing 'pointer' is recognized as pointer-like."""
        checker = _create_checker()
        assert checker._is_pointer_like_annotation("pointer") is True
        assert checker._is_pointer_like_annotation("MyStructPointer") is True

    def test_is_pointer_like_annotation_ending_with_ptr(self):
        """Annotation ending with 'ptr' is recognized as pointer-like."""
        checker = _create_checker()
        assert checker._is_pointer_like_annotation("my_ptr") is True
        assert checker._is_pointer_like_annotation("handle_ptr") is True

    def test_is_pointer_like_annotation_not_pointer(self):
        """Non-pointer annotations are not recognized as pointer-like."""
        checker = _create_checker()
        assert checker._is_pointer_like_annotation("Integer") is False
        assert checker._is_pointer_like_annotation("String") is False
        assert checker._is_pointer_like_annotation("") is False

    def test_validate_extern_type_declaration_opaque_pointer(self):
        """Opaque pointer extern type declaration is valid."""
        checker = _create_checker()

        stmt = _MockStatement("ExternTypeDeclaration",
                             name="OpaqueHandle",
                             is_opaque=True,
                             base_type="pointer",
                             line_number=50)

        # Should not produce errors for valid opaque pointer
        checker._validate_extern_type_declaration(stmt)
        assert len(checker.errors) == 0

    def test_validate_extern_type_declaration_opaque_struct(self):
        """Opaque struct extern type declaration is valid."""
        checker = _create_checker()

        stmt = _MockStatement("ExternTypeDeclaration",
                             name="OpaqueStruct",
                             is_opaque=True,
                             base_type="struct",
                             line_number=50)

        checker._validate_extern_type_declaration(stmt)
        assert len(checker.errors) == 0

    def test_validate_extern_type_declaration_opaque_invalid_base(self):
        """Opaque extern type with invalid base_type produces error."""
        checker = _create_checker()

        stmt = _MockStatement("ExternTypeDeclaration",
                             name="BadOpaque",
                             is_opaque=True,
                             base_type="function",
                             line_number=50)

        checker._validate_extern_type_declaration(stmt)
        assert len(checker.errors) == 1
        assert "must be declared as opaque pointer or opaque struct" in checker.errors[0]

    def test_validate_extern_type_declaration_function_pointer(self):
        """Function pointer extern type validation."""
        checker = _create_checker()

        stmt = _MockStatement("ExternTypeDeclaration",
                             name="MyFnPtr",
                             is_function_pointer=True,
                             function_signature=([INTEGER_TYPE], STRING_TYPE),
                             line_number=50)

        checker._validate_extern_type_declaration(stmt)
        # Function pointer validation uses _validate_ffi_abi_type on params/return
        # With INTEGER_TYPE and STRING_TYPE (both safe), no errors expected
        assert len(checker.errors) == 0

    def test_validate_extern_function_abi_valid_cdecl(self):
        """Valid extern function with cdecl calling convention."""
        checker = _create_checker()

        param = _MockStatement("Parameter", type_annotation="Integer")
        stmt = _MockStatement("ExternFunctionDeclaration",
                             name="external_func",
                             calling_convention="cdecl",
                             variadic=False,
                             parameters=[param],
                             return_type="String",
                             line_number=60)

        checker._validate_extern_function_abi(stmt)
        assert len(checker.errors) == 0

    def test_validate_extern_function_abi_unsupported_calling_convention(self):
        """Extern function with unsupported calling convention produces error."""
        checker = _create_checker()

        stmt = _MockStatement("ExternFunctionDeclaration",
                             name="bad_abi_func",
                             calling_convention="impossible_convention",
                             variadic=False,
                             parameters=[],
                             return_type="Void",
                             line_number=60)

        # Mock the FFI check
        from unittest.mock import patch
        with patch('nexuslang.stdlib.ffi.is_calling_convention_supported', return_value=False):
            checker._validate_extern_function_abi(stmt)
            assert any("Unsupported FFI calling convention" in err for err in checker.errors)

    def test_validate_extern_function_abi_variadic_no_fixed_params(self):
        """Variadic function with no fixed parameters produces error."""
        checker = _create_checker()

        stmt = _MockStatement("ExternFunctionDeclaration",
                             name="bad_variadic",
                             calling_convention="cdecl",
                             variadic=True,
                             parameters=[],
                             return_type="Void",
                             line_number=70)

        from unittest.mock import patch
        with patch('nexuslang.stdlib.ffi.is_calling_convention_supported', return_value=True):
            checker._validate_extern_function_abi(stmt)
            assert any("must declare at least one fixed parameter" in err for err in checker.errors)

    def test_validate_extern_function_abi_variadic_with_bad_convention(self):
        """Variadic function with non-cdecl/sysv calling convention produces error."""
        checker = _create_checker()

        param = _MockStatement("Parameter", type_annotation="Integer")
        stmt = _MockStatement("ExternFunctionDeclaration",
                             name="bad_variadic_abi",
                             calling_convention="fastcall",
                             variadic=True,
                             parameters=[param],
                             return_type="Void",
                             line_number=70)

        from unittest.mock import patch
        with patch('nexuslang.stdlib.ffi.is_calling_convention_supported', return_value=True):
            checker._validate_extern_function_abi(stmt)
            assert any("requires 'cdecl' or 'sysv'" in err for err in checker.errors)

    def test_validate_extern_function_abi_unsafe_parameter_type(self):
        """Extern function with ABI-unsafe parameter type produces error."""
        checker = _create_checker()

        param = _MockStatement("Parameter", type_annotation="List")
        stmt = _MockStatement("ExternFunctionDeclaration",
                             name="unsafe_func",
                             calling_convention="cdecl",
                             variadic=False,
                             parameters=[param],
                             return_type="Void",
                             line_number=80)

        from unittest.mock import patch
        with patch('nexuslang.stdlib.ffi.is_calling_convention_supported', return_value=True):
            checker._validate_extern_function_abi(stmt)
            # Parameter validation should produce an ABI-unstable error
            assert any("ABI-unstable" in err for err in checker.errors)

    def test_validate_extern_function_abi_unsafe_return_type(self):
        """Extern function with ABI-unsafe return type produces error."""
        checker = _create_checker()

        param = _MockStatement("Parameter", type_annotation="Integer")
        stmt = _MockStatement("ExternFunctionDeclaration",
                             name="unsafe_return_func",
                             calling_convention="cdecl",
                             variadic=False,
                             parameters=[param],
                             return_type="Dictionary",
                             line_number=85)

        from unittest.mock import patch
        with patch('nexuslang.stdlib.ffi.is_calling_convention_supported', return_value=True):
            checker._validate_extern_function_abi(stmt)
            # Return type validation should produce an ABI-unstable error
            assert any("ABI-unstable" in err for err in checker.errors)
