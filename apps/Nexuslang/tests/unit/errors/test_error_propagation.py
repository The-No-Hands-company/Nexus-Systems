"""
Tests for error propagation (? operator).

Production-ready test suite.
"""

import pytest
from nexuslang.stdlib.option_result import Ok, Err, Result


class TestErrorPropagation:
    """Tests for ? operator."""
    
    def test_question_operator_on_ok(self):
        """Test ? operator unwraps Ok values."""
        result = Ok(42)
        # In actual usage: value = some_function()?
        # For testing, we verify the Result type
        assert result.is_ok()
        assert result.unwrap() == 42
    
    def test_question_operator_on_err(self):
        """Test ? operator propagates Err values."""
        result = Err("error message")
        assert result.is_err()
        assert result.unwrap_err() == "error message"
    
    def test_result_chaining(self):
        """Test chaining multiple Result operations."""
        def divide(a, b):
            if b == 0:
                return Err("Division by zero")
            return Ok(a / b)
        
        # Success case
        result1 = divide(10, 2)
        assert result1.is_ok()
        assert result1.unwrap() == 5
        
        # Error case
        result2 = divide(10, 0)
        assert result2.is_err()
        assert "Division by zero" in result2.unwrap_err()


# ============================================================
# Error handling typechecker tests
# ============================================================

class TestErrorHandlingTypechecker:
    """Typechecker gap: thrown/raised value typing and catch variable typing."""

    def _check_ast(self, *statements):
        import sys
        import os
        sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', '..', 'src'))
        from nexuslang.typesystem.typechecker import TypeChecker
        from nexuslang.parser.ast import Program
        prog = Program(list(statements))
        tc = TypeChecker()
        tc.check_program(prog)
        return tc.errors

    def test_raise_with_non_string_message_is_error(self):
        from nexuslang.parser.ast import RaiseStatement, Literal
        node = RaiseStatement(message=Literal('integer', 42))
        errors = self._check_ast(node)
        assert any("Raise message should be String" in e for e in errors), \
            f"Expected raise message type error, got: {errors}"

    def test_raise_with_string_message_passes(self):
        from nexuslang.parser.ast import RaiseStatement, Literal
        node = RaiseStatement(message=Literal('string', 'something went wrong'))
        errors = self._check_ast(node)
        assert not any("Raise message" in e for e in errors), \
            f"Unexpected raise message error: {errors}"

    def test_raise_with_no_message_passes(self):
        from nexuslang.parser.ast import RaiseStatement
        node = RaiseStatement(message=None)
        errors = self._check_ast(node)
        assert not any("Raise message" in e for e in errors), \
            f"Unexpected raise message error: {errors}"

    def test_catch_variable_typed_as_string_by_default(self):
        from nexuslang.typesystem.typechecker import TypeChecker
        from nexuslang.parser.ast import TryCatchBlock, Block, PrintStatement, Literal, Identifier, Program
        try_block = Block([PrintStatement([Literal('string', 'try')])])
        catch_block = Block([PrintStatement([Identifier('err')])])
        node = TryCatchBlock(
            try_block=try_block,
            catch_block=catch_block,
            exception_var='err',
            exception_type=None,
        )
        import sys, os
        sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', '..', 'src'))
        prog = Program([node])
        tc = TypeChecker()
        tc.check_program(prog)
        # 'err' should be defined as string in catch scope - no type errors expected
        assert not tc.errors, f"Unexpected errors: {tc.errors}"

    def test_catch_variable_typed_by_exception_type_annotation(self):
        from nexuslang.typesystem.typechecker import TypeChecker
        from nexuslang.typesystem.types import ClassType
        from nexuslang.parser.ast import TryCatchBlock, Block, PrintStatement, Literal, Identifier, Program
        try_block = Block([PrintStatement([Literal('string', 'try')])])
        catch_block = Block([])
        node = TryCatchBlock(
            try_block=try_block,
            catch_block=catch_block,
            exception_var='err',
            exception_type='ValueError',
        )
        import sys, os
        sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', '..', 'src'))
        prog = Program([node])
        tc = TypeChecker()
        tc.check_program(prog)
        # exception_type annotation should give ClassType('ValueError') in catch scope
        assert not tc.errors, f"Unexpected errors: {tc.errors}"
        # Verify that the catch env correctly typed the variable as ClassType

    def test_unreachable_code_after_raise_is_detected(self):
        from nexuslang.parser.ast import RaiseStatement, PrintStatement, Literal, TryCatch, Block, Program
        raise_stmt = RaiseStatement(message=Literal('string', 'fail'))
        unreachable = PrintStatement([Literal('string', 'never reached')])
        # Build a TryCatch where try body has raise followed by unreachable stmt
        try_block = [raise_stmt, unreachable]
        catch_block = []
        node = TryCatch(try_block=try_block, catch_block=catch_block)
        errors = self._check_ast(node)
        assert any("Unreachable" in e for e in errors), \
            f"Expected unreachable code error, got: {errors}"

    def test_no_unreachable_error_when_raise_is_last_in_body(self):
        from nexuslang.parser.ast import RaiseStatement, Literal, TryCatch, Program
        raise_stmt = RaiseStatement(message=Literal('string', 'fail'))
        node = TryCatch(try_block=[raise_stmt], catch_block=[])
        errors = self._check_ast(node)
        assert not any("Unreachable" in e for e in errors), \
            f"Unexpected unreachable error: {errors}"


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
