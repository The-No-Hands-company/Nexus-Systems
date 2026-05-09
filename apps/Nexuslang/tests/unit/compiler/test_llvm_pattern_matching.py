"""
Test LLVM compiler support for comprehensive pattern matching.

These tests verify that pattern matching (Option, Result, Tuple, List)
compiles correctly to LLVM IR with proper variant checking and data extraction.
"""

import pytest
import sys
import os

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../../src'))

from nexuslang.parser.lexer import Lexer
from nexuslang.parser.parser import Parser
from nexuslang.parser.ast import MatchExpression, OptionPattern, ResultPattern
from nexuslang.compiler.backends.llvm_ir_generator import LLVMIRGenerator


def _find_first_match(ast):
    for stmt in ast.statements:
        body = getattr(stmt, "body", None) or []
        for body_stmt in body:
            if isinstance(body_stmt, MatchExpression):
                return body_stmt
    raise AssertionError("Expected at least one match expression in parsed AST")


class TestOptionPatternMatching:
    """Test Option<T> pattern matching compilation."""
    
    def test_option_some_pattern(self):
        """Test that LLVM generator has Option pattern support methods."""
        generator = LLVMIRGenerator()
        
        # Verify Option pattern methods exist
        assert hasattr(generator, '_generate_option_pattern_match')
        assert hasattr(generator, '_generate_option_pattern_binding')
    
    def test_option_none_pattern(self):
        """Test that None pattern can be compiled."""
        code = """function is_none that takes opt as Option returns Boolean
    match opt with
        case None
            return true
        case _
            return false
end
"""
        
        lexer = Lexer(code)
        tokens = lexer.scan_tokens()
        parser = Parser(tokens)
        ast = parser.parse()
        
        generator = LLVMIRGenerator()
        llvm_ir = generator.generate(ast)
        
        # Should compile successfully
        assert 'define' in llvm_ir

    def test_option_pattern_runtime_value_extraction_binding(self):
        """OptionPattern(Some value) should call runtime value extractor and bind local."""
        code = """function unwrap_opt that takes opt as Option returns Integer
    match opt with
        case Some payload
            return payload
        case None
            return 0
end
"""

        lexer = Lexer(code)
        tokens = lexer.scan_tokens()
        parser = Parser(tokens)
        ast = parser.parse()

        match_stmt = _find_first_match(ast)
        match_stmt.cases[0].pattern = OptionPattern("Some", "payload")
        match_stmt.cases[1].pattern = OptionPattern("None", None)

        llvm_ir = LLVMIRGenerator().generate(ast)

        assert '@NLPL_Optional_has_value' in llvm_ir
        assert '@NLPL_Optional_get_value_ptr' in llvm_ir

    def test_option_pointer_pattern_uses_runtime_helpers(self):
        """Pointer-backed Option patterns should use runtime helper calls."""
        generator = LLVMIRGenerator()
        pattern = OptionPattern("Some", "payload")

        cond_reg = generator._generate_option_pattern_match(pattern, "%opt", "i8*", "  ")
        generator._generate_option_pattern_binding(pattern, "%opt", "i8*", "  ")

        ir = "\n".join(generator.ir_lines)
        assert cond_reg.startswith("%")
        assert "call i1 @NLPL_Optional_has_value(i8* %opt)" in ir
        assert "call i32 @NLPL_Optional_get_value_kind(i8* %opt)" in ir
        assert "call i64 @NLPL_Optional_get_value_i64(i8* %opt)" in ir
        assert "call double @NLPL_Optional_get_value_f64(i8* %opt)" in ir
        assert "call i8* @NLPL_Optional_get_value_ptr(i8* %opt)" in ir

    def test_option_typed_integer_binding_uses_fast_path(self):
        """Typed Some payload bindings should use direct i64 extractor path."""
        generator = LLVMIRGenerator()
        pattern = OptionPattern("Some", "payload", binding_type_annotation="Integer")

        generator._generate_option_pattern_binding(pattern, "%opt", "i8*", "  ")
        ir = "\n".join(generator.ir_lines)

        assert "call i64 @NLPL_Optional_get_value_i64(i8* %opt)" in ir
        assert "call i32 @NLPL_Optional_get_value_kind(i8* %opt)" not in ir
        assert generator.local_vars["payload"][0] == "i64"

    def test_option_pattern_parses_binding_type_annotation(self):
        """Parser should preserve optional payload binding type metadata."""
        code = """function main returns Integer
    set opt to 0
    match opt with
        case Some with payload as Integer
            return 1
        case None
            return 0
end
"""

        lexer = Lexer(code)
        tokens = lexer.scan_tokens()
        parser = Parser(tokens)
        ast = parser.parse()

        match_stmt = _find_first_match(ast)
        assert isinstance(match_stmt.cases[0].pattern, OptionPattern)
        assert match_stmt.cases[0].pattern.binding_type_annotation == "Integer"


class TestResultPatternMatching:
    """Test Result<T, E> pattern matching compilation."""
    
    def test_result_ok_pattern(self):
        """Test that LLVM generator has Result pattern support methods."""
        generator = LLVMIRGenerator()
        
        # Verify Result pattern methods exist
        assert hasattr(generator, '_generate_result_pattern_match')
        assert hasattr(generator, '_generate_result_pattern_binding')
    
    def test_result_err_pattern(self):
        """Test that wildcard pattern works in Result context."""
        code = """function is_error that takes res as Result returns Boolean
    match res with
        case _
            return true
end
"""
        
        lexer = Lexer(code)
        tokens = lexer.scan_tokens()
        parser = Parser(tokens)
        ast = parser.parse()
        
        generator = LLVMIRGenerator()
        llvm_ir = generator.generate(ast)
        
        # Should compile successfully
        assert 'define' in llvm_ir

    def test_result_pattern_runtime_error_extraction_binding(self):
        """ResultPattern(Err e) should call runtime error extractor and bind local."""
        code = """function read_error that takes res as Result returns Integer
    match res with
        case Err message
            print text message
            return 1
        case Ok value
            return 0
end
"""

        lexer = Lexer(code)
        tokens = lexer.scan_tokens()
        parser = Parser(tokens)
        ast = parser.parse()

        match_stmt = _find_first_match(ast)
        match_stmt.cases[0].pattern = ResultPattern("Err", "message")
        match_stmt.cases[1].pattern = ResultPattern("Ok", "value")

        llvm_ir = LLVMIRGenerator().generate(ast)

        assert '@NLPL_Result_is_ok' in llvm_ir
        assert '@NLPL_Result_get_error_ptr' in llvm_ir
        assert '@NLPL_Result_get_value_ptr' in llvm_ir

    def test_result_pointer_err_pattern_uses_runtime_helpers(self):
        """Pointer-backed Result Err patterns should use runtime matcher and error extractor."""
        generator = LLVMIRGenerator()
        pattern = ResultPattern("Err", "message")

        cond_reg = generator._generate_result_pattern_match(pattern, "%res", "i8*", "  ")
        generator._generate_result_pattern_binding(pattern, "%res", "i8*", "  ")

        ir = "\n".join(generator.ir_lines)
        assert cond_reg.startswith("%")
        assert "call i1 @NLPL_Result_is_ok(i8* %res)" in ir
        assert "call i32 @NLPL_Result_get_error_kind(i8* %res)" in ir
        assert "call i64 @NLPL_Result_get_error_i64(i8* %res)" in ir
        assert "call double @NLPL_Result_get_error_f64(i8* %res)" in ir
        assert "call i8* @NLPL_Result_get_error_ptr(i8* %res)" in ir

    def test_result_pointer_ok_pattern_uses_runtime_value_helper(self):
        """Pointer-backed Result Ok bindings should use runtime value extractor."""
        generator = LLVMIRGenerator()
        pattern = ResultPattern("Ok", "value")

        generator._generate_result_pattern_binding(pattern, "%res", "i8*", "  ")
        ir = "\n".join(generator.ir_lines)

        assert "call i32 @NLPL_Result_get_value_kind(i8* %res)" in ir
        assert "call i64 @NLPL_Result_get_value_i64(i8* %res)" in ir
        assert "call double @NLPL_Result_get_value_f64(i8* %res)" in ir
        assert "call i8* @NLPL_Result_get_value_ptr(i8* %res)" in ir

    def test_result_typed_float_err_binding_uses_fast_path(self):
        """Typed Err payload bindings should use direct f64 extractor path."""
        generator = LLVMIRGenerator()
        pattern = ResultPattern("Err", "error_payload", binding_type_annotation="Float")

        generator._generate_result_pattern_binding(pattern, "%res", "i8*", "  ")
        ir = "\n".join(generator.ir_lines)

        assert "call double @NLPL_Result_get_error_f64(i8* %res)" in ir
        assert "call i32 @NLPL_Result_get_error_kind(i8* %res)" not in ir
        assert generator.local_vars["error_payload"][0] == "double"

    def test_result_pattern_parses_binding_type_annotation(self):
        """Parser should preserve optional Result binding type metadata."""
        code = """function main returns Integer
    set res to 0
    match res with
        case Ok with value as Float
            return 1
        case Err with error
            return 0
end
"""

        lexer = Lexer(code)
        tokens = lexer.scan_tokens()
        parser = Parser(tokens)
        ast = parser.parse()

        match_stmt = _find_first_match(ast)
        assert isinstance(match_stmt.cases[0].pattern, ResultPattern)
        assert match_stmt.cases[0].pattern.binding_type_annotation == "Float"

    def test_option_scalar_some_pattern_binding_fails_fast(self):
        """Scalar Option Some binding should fail fast without heuristic coercion."""
        generator = LLVMIRGenerator()
        pattern = OptionPattern("Some", "payload")

        with pytest.raises(
            RuntimeError,
            match="OptionPattern binding requires runtime-backed pointer representation",
        ):
            generator._generate_option_pattern_binding(pattern, "%opt", "i64", "  ")

    def test_result_scalar_err_pattern_binding_fails_fast(self):
        """Scalar Result Err binding should fail fast without heuristic coercion."""
        generator = LLVMIRGenerator()
        pattern = ResultPattern("Err", "error_payload")

        with pytest.raises(
            RuntimeError,
            match="ResultPattern binding requires runtime-backed pointer representation",
        ):
            generator._generate_result_pattern_binding(pattern, "%res", "i64", "  ")


class TestTuplePatternMatching:
    """Test tuple pattern matching compilation."""
    
    def test_simple_tuple_pattern(self):
        """Test matching simple tuple pattern (a, b)."""
        code = """function get_first that takes pair as Tuple returns Integer
    match pair with
        case (x, y)
            return x
end
"""
        
        lexer = Lexer(code)
        tokens = lexer.scan_tokens()
        parser = Parser(tokens)
        ast = parser.parse()
        
        generator = LLVMIRGenerator()
        llvm_ir = generator.generate(ast)
        
        # Should use extractvalue for tuple elements
        assert 'define' in llvm_ir
    
    def test_nested_tuple_pattern(self):
        """Test matching nested tuple pattern ((a, b), c)."""
        code = """function extract_nested that takes data as Tuple returns Integer
    match data with
        case ((x, y), z)
            return x plus z
end
"""
        
        lexer = Lexer(code)
        tokens = lexer.scan_tokens()
        parser = Parser(tokens)
        ast = parser.parse()
        
        generator = LLVMIRGenerator()
        llvm_ir = generator.generate(ast)
        # Nested extraction
        assert 'define' in llvm_ir


class TestListPatternMatching:
    """Test list pattern matching compilation."""
    
    def test_list_head_tail_pattern(self):
        """Test matching [head, ...tail] pattern."""
        code = """function get_head that takes lst as List returns Integer
    match lst with
        case [head, rest]
            return head
        case []
            return 0
end
"""
        
        lexer = Lexer(code)
        tokens = lexer.scan_tokens()
        parser = Parser(tokens)
        ast = parser.parse()
        
        generator = LLVMIRGenerator()
        llvm_ir = generator.generate(ast)
        
        # Should compile list pattern
        assert 'define' in llvm_ir
    
    def test_list_fixed_length_pattern(self):
        """Test matching [x, y, z] fixed-length pattern."""
        code = """function sum_triple that takes lst as List returns Integer
    match lst with
        case [x, y, z]
            return x plus y plus z
        case _
            return 0
end
"""
        
        lexer = Lexer(code)
        tokens = lexer.scan_tokens()
        parser = Parser(tokens)
        ast = parser.parse()
        
        generator = LLVMIRGenerator()
        llvm_ir = generator.generate(ast)
        
        # Should compile fixed-length list pattern
        assert 'define' in llvm_ir

    def test_list_pattern_rest_binding_emits_tail_copy_path(self):
        """List pattern with rest binding should allocate/copy remaining tail values."""
        code = """function split_head that takes lst as List returns Integer
    match lst with
        case [head, ...tail]
            return head
        case _
            return 0
end
"""

        lexer = Lexer(code)
        tokens = lexer.scan_tokens()
        parser = Parser(tokens)
        ast = parser.parse()

        llvm_ir = LLVMIRGenerator().generate(ast)

        assert '@malloc' in llvm_ir
        assert '@llvm.memcpy.p0i8.p0i8.i64' in llvm_ir


class TestPatternMatchingHelpers:
    """Test pattern matching helper methods."""
    
    def test_pattern_matching_methods_exist(self):
        """Verify LLVMIRGenerator has pattern matching methods."""
        generator = LLVMIRGenerator()
        
        # Check for new methods
        assert hasattr(generator, '_generate_pattern_match')
        assert hasattr(generator, '_generate_pattern_bindings')
        assert hasattr(generator, '_generate_option_pattern_match')
        assert hasattr(generator, '_generate_result_pattern_match')
        assert hasattr(generator, '_generate_tuple_pattern_match')
        assert hasattr(generator, '_generate_list_pattern_match')
    
    def test_binding_methods_exist(self):
        """Verify pattern binding helper methods exist."""
        generator = LLVMIRGenerator()
        
        assert hasattr(generator, '_generate_option_pattern_binding')
        assert hasattr(generator, '_generate_result_pattern_binding')


class TestVariantPatternMatching:
    """Test generic variant pattern matching."""
    
    def test_variant_pattern_dispatch(self):
        """Test that variant patterns dispatch correctly."""
        generator = LLVMIRGenerator()
        
        # Verify variant pattern helper exists
        assert hasattr(generator, '_generate_variant_pattern_match')
        assert hasattr(generator, '_generate_variant_pattern_binding')


if __name__ == '__main__':
    pytest.main([__file__, '-v'])
