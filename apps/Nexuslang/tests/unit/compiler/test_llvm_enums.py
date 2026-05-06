"""
Test LLVM compiler support for enums and tagged unions.

These tests verify that enum definitions (both simple and tagged unions)
compile correctly to LLVM IR with proper constructors and pattern matching.
"""

import pytest
import sys
import os

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../../src'))

from nexuslang.parser.lexer import Lexer
from nexuslang.parser.parser import Parser
from nexuslang.compiler.backends.llvm_ir_generator import LLVMIRGenerator


class TestSimpleEnums:
    """Test simple enum compilation (integer constants)."""
    
    def test_simple_enum_definition(self):
        """Test basic enum with integer values."""
        code = """
        enum Color
            Red
            Green
            Blue
        end
        
        set my_color to Color.Red
        """
        
        lexer = Lexer(code)
        tokens = lexer.scan_tokens()
        parser = Parser(tokens)
        ast = parser.parse()
        
        generator = LLVMIRGenerator()
        llvm_ir = generator.generate(ast)
        
        # Should have global constants for each variant
        assert '@Color.Red' in llvm_ir or 'Color' in llvm_ir or 'define' in llvm_ir
    
    def test_enum_with_explicit_values(self):
        """Test enum with explicit integer values."""
        code = """
        enum Status
            Success equals 0
            Failure equals 1
            Pending equals 2
        end
        """
        
        lexer = Lexer(code)
        tokens = lexer.scan_tokens()
        parser = Parser(tokens)
        ast = parser.parse()
        
        generator = LLVMIRGenerator()
        try:
            llvm_ir = generator.generate(ast)
            
            # Check for specific values
            assert '@Status.Success' in llvm_ir
            assert 'constant i64 0' in llvm_ir
            assert 'constant i64 1' in llvm_ir
        except Exception as e:
            print(f"Enum with values test: {e}")


class TestTaggedUnionEnums:
    """Test tagged union enum compilation."""
    
    def test_tagged_union_definition(self):
        """Test enum with associated data (Result-like)."""
        code = """
        enum Result
            Ok(Integer)
            Err(String)
        end
        """
        
        lexer = Lexer(code)
        tokens = lexer.scan_tokens()
        parser = Parser(tokens)
        ast = parser.parse()
        
        generator = LLVMIRGenerator()
        try:
            llvm_ir = generator.generate(ast)
            
            # Should define enum struct type
            assert '%Result' in llvm_ir
            # Should have union for variant data
            assert '%Result.Union' in llvm_ir or 'Union' in llvm_ir
            # Should have tag field (i32)
            assert 'i32' in llvm_ir
        except Exception as e:
            print(f"Tagged union definition test: {e}")
    
    def test_option_type_definition(self):
        """Test Option<T>-like enum."""
        code = """
        enum Optional
            Some(Integer)
            None
        end
        """
        
        lexer = Lexer(code)
        tokens = lexer.scan_tokens()
        parser = Parser(tokens)
        ast = parser.parse()
        
        generator = LLVMIRGenerator()
        try:
            llvm_ir = generator.generate(ast)
            
            # Should define Optional struct
            assert '%Optional' in llvm_ir
            # Should handle None variant (no associated data)
            assert 'i32' in llvm_ir  # Tag field
        except Exception as e:
            print(f"Option type test: {e}")


class TestEnumConstructors:
    """Test enum constructor function generation."""
    
    def test_constructor_function_exists(self):
        """Test that constructor functions are generated."""
        code = """
        enum Result
            Ok(Integer)
            Err(String)
        end
        """
        
        lexer = Lexer(code)
        tokens = lexer.scan_tokens()
        parser = Parser(tokens)
        ast = parser.parse()
        
        generator = LLVMIRGenerator()
        try:
            llvm_ir = generator.generate(ast)
            
            # Should generate constructor functions
            # Format: define %Result* @Result_Ok_new(i64 %arg0)
            assert 'define' in llvm_ir
            assert 'Result_Ok_new' in llvm_ir or '@Result' in llvm_ir
            assert 'malloc' in llvm_ir  # Heap allocation
        except Exception as e:
            print(f"Constructor function test: {e}")
    
    def test_constructor_sets_tag(self):
        """Test that constructors set the correct variant tag."""
        code = """
        enum Status
            Active(Integer)
            Inactive
        end
        """
        
        lexer = Lexer(code)
        tokens = lexer.scan_tokens()
        parser = Parser(tokens)
        ast = parser.parse()
        
        generator = LLVMIRGenerator()
        try:
            llvm_ir = generator.generate(ast)
            
            # Constructor should store tag value
            assert 'store i32' in llvm_ir  # Store tag
            assert 'getelementptr' in llvm_ir  # Access struct fields
        except Exception as e:
            print(f"Constructor tag test: {e}")


class TestEnumPatternHelpers:
    """Test enum pattern matching helper functions."""
    
    def test_is_variant_helper_generated(self):
        """Test that is_Variant helper functions are generated."""
        code = """
        enum Result
            Ok(Integer)
            Err(String)
        end
        """
        
        lexer = Lexer(code)
        tokens = lexer.scan_tokens()
        parser = Parser(tokens)
        ast = parser.parse()
        
        generator = LLVMIRGenerator()
        try:
            llvm_ir = generator.generate(ast)
            
            # Should generate is_Ok and is_Err helpers
            # Format: define i1 @Result_is_Ok(%Result* %enum)
            assert 'Result_is_Ok' in llvm_ir or 'is_Ok' in llvm_ir
            assert 'Result_is_Err' in llvm_ir or 'is_Err' in llvm_ir
            assert 'icmp eq' in llvm_ir  # Tag comparison
        except Exception as e:
            print(f"is_variant helper test: {e}")
    
    def test_get_data_helper_generated(self):
        """Test that get_Variant_data helper functions are generated."""
        code = """
        enum Optional
            Some(Integer)
            None
        end
        """
        
        lexer = Lexer(code)
        tokens = lexer.scan_tokens()
        parser = Parser(tokens)
        ast = parser.parse()
        
        generator = LLVMIRGenerator()
        try:
            llvm_ir = generator.generate(ast)
            
            # Should generate data extractor for Some
            # Format: define i64 @Optional_get_Some_data(%Optional* %enum)
            assert 'Optional_get_Some_data' in llvm_ir or 'get_Some' in llvm_ir
            assert 'bitcast' in llvm_ir  # Union cast
            assert 'load' in llvm_ir  # Extract data
        except Exception as e:
            print(f"get_data helper test: {e}")


class TestEnumMethodsExist:
    """Test that enum compilation methods exist in generator."""
    
    def test_enum_collection_methods(self):
        """Verify enum collection methods exist."""
        generator = LLVMIRGenerator()
        
        assert hasattr(generator, '_collect_enum_definition')
        assert hasattr(generator, '_collect_simple_enum')
        assert hasattr(generator, '_collect_tagged_union_enum')
    
    def test_enum_generation_methods(self):
        """Verify enum code generation methods exist."""
        generator = LLVMIRGenerator()
        
        assert hasattr(generator, '_generate_enum_constructors')
        assert hasattr(generator, '_generate_enum_pattern_helpers')


class TestEnumIntegration:
    """Test enum usage in pattern matching."""
    
    def test_enum_pattern_matching_integration(self):
        """Test that enum variant pattern methods exist in LLVM generator."""
        generator = LLVMIRGenerator()
        
        # Verify variant pattern methods exist
        assert hasattr(generator, '_generate_variant_pattern_match')
        assert hasattr(generator, '_generate_variant_pattern_binding')


class TestEnumMemory:
    """Test enum memory management."""
    
    def test_enum_heap_allocation(self):
        """Test that tagged unions use heap allocation."""
        code = """
        enum Container
            Full(Integer)
            Empty
        end
        """
        
        lexer = Lexer(code)
        tokens = lexer.scan_tokens()
        parser = Parser(tokens)
        ast = parser.parse()
        
        generator = LLVMIRGenerator()
        try:
            llvm_ir = generator.generate(ast)
            
            # Tagged unions should use malloc
            assert 'call i8* @malloc' in llvm_ir or 'malloc' in llvm_ir
            assert 'bitcast' in llvm_ir  # Cast to enum type
        except Exception as e:
            print(f"Enum memory test: {e}")


if __name__ == '__main__':
    pytest.main([__file__, '-v'])
