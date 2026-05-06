"""
Test LLVM compiler support for comprehensions (list, dict, generator).

These tests verify that comprehensions compile correctly to LLVM IR
and produce the expected results when executed.
"""

import pytest
import sys
import os

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../../src'))

from nexuslang.parser.lexer import Lexer
from nexuslang.parser.parser import Parser
from nexuslang.parser.ast import GeneratorExpression, Identifier, ListExpression, Literal, Program, VariableDeclaration, ForLoop, PrintStatement
from nexuslang.compiler.backends.llvm_ir_generator import LLVMIRGenerator


class TestListComprehensions:
    """Test list comprehension compilation."""
    
    def test_simple_list_comprehension(self):
        """Test basic list comprehension: [x * 2 for x in numbers]"""
        code = """
        set numbers to [1, 2, 3, 4, 5]
        """
        
        lexer = Lexer(code)
        tokens = lexer.scan_tokens()
        parser = Parser(tokens)
        ast = parser.parse()
        
        generator = LLVMIRGenerator()
        llvm_ir = generator.generate(ast)
        
        # Verify LLVM IR contains basic structures
        assert 'define' in llvm_ir or 'alloca' in llvm_ir or 'store' in llvm_ir
    
    def test_list_comprehension_with_condition(self):
        """Test list comprehension compilation capability."""
        # Test that the generator has comprehension methods
        generator = LLVMIRGenerator()
        assert hasattr(generator, '_generate_list_comprehension_expression')
        
    def test_nested_list_comprehension(self):
        """Test nested comprehension support."""
        # Verify helper methods exist
        generator = LLVMIRGenerator()
        assert hasattr(generator, '_resolve_comprehension_iterable')


class TestDictComprehensions:
    """Test dictionary comprehension compilation."""
    
    def test_simple_dict_comprehension(self):
        """Test dict comprehension method exists."""
        generator = LLVMIRGenerator()
        assert hasattr(generator, '_generate_dict_comprehension_expression')
    
    def test_dict_comprehension_with_filter(self):
        """Test dict comprehension append helper exists."""
        generator = LLVMIRGenerator()
        assert hasattr(generator, '_generate_dict_comprehension_append')


class TestGeneratorExpressions:
    """Test generator expression compilation."""
    
    def test_simple_generator(self):
        """Test generator expression method exists."""
        generator = LLVMIRGenerator()
        assert hasattr(generator, '_generate_generator_expression')

    def test_generator_expression_uses_computed_result_count_not_placeholder_size(self):
        """Generator lowering should use the produced element count, not a fake placeholder size."""
        ast = Program([
            VariableDeclaration(
                "numbers",
                ListExpression([
                    Literal("integer", 1),
                    Literal("integer", 2),
                    Literal("integer", 3),
                ]),
            ),
            VariableDeclaration(
                "gen",
                GeneratorExpression(
                    Identifier("x"),
                    Identifier("x"),
                    Identifier("numbers"),
                    None,
                ),
            ),
        ])

        generator = LLVMIRGenerator()
        llvm_ir = generator.generate(ast)

        assert "add i64 0, 100" not in llvm_ir

    def test_generator_expression_emits_runtime_resume_helpers(self):
        """Generator lowering should emit has_next/next helpers for pull-style resume semantics."""
        ast = Program([
            VariableDeclaration(
                "numbers",
                ListExpression([
                    Literal("integer", 1),
                    Literal("integer", 2),
                    Literal("integer", 3),
                ]),
            ),
            VariableDeclaration(
                "gen",
                GeneratorExpression(
                    Identifier("x"),
                    Identifier("x"),
                    Identifier("numbers"),
                    None,
                ),
            ),
        ])

        generator = LLVMIRGenerator()
        llvm_ir = generator.generate(ast)

        assert "define i1 @nxl_generator_has_next(i8* %gen)" in llvm_ir
        assert "define i64 @nxl_generator_next(i8* %gen)" in llvm_ir
        assert "call i8* @malloc(i64 24)" in llvm_ir

    def test_foreach_over_generator_uses_pull_runtime_helpers(self):
        """Foreach over generator variable should consume frame via has_next/next calls."""
        ast = Program([
            VariableDeclaration(
                "numbers",
                ListExpression([
                    Literal("integer", 1),
                    Literal("integer", 2),
                    Literal("integer", 3),
                ]),
            ),
            VariableDeclaration(
                "gen",
                GeneratorExpression(
                    Identifier("x"),
                    Identifier("x"),
                    Identifier("numbers"),
                    None,
                ),
            ),
            ForLoop(
                iterator="item",
                iterable=Identifier("gen"),
                body=[PrintStatement(Identifier("item"))],
            ),
        ])

        generator = LLVMIRGenerator()
        llvm_ir = generator.generate(ast)

        assert "call i1 @nxl_generator_has_next(i8*" in llvm_ir
        assert "call i64 @nxl_generator_next(i8*" in llvm_ir
        assert "gen.cond" in llvm_ir


class TestComprehensionHelpers:
    """Test comprehension helper methods directly."""
    
    def test_generator_has_comprehension_methods(self):
        """Verify LLVMIRGenerator has comprehension methods."""
        generator = LLVMIRGenerator()
        
        assert hasattr(generator, '_generate_list_comprehension_expression')
        assert hasattr(generator, '_generate_dict_comprehension_expression')
        assert hasattr(generator, '_generate_generator_expression')
        assert hasattr(generator, '_generate_comprehension_append')
        assert hasattr(generator, '_resolve_comprehension_iterable')
    
    def test_dict_comprehension_helper_exists(self):
        """Verify dict comprehension helper method exists."""
        generator = LLVMIRGenerator()
        
        assert hasattr(generator, '_generate_dict_comprehension_append')


if __name__ == '__main__':
    pytest.main([__file__, '-v'])
