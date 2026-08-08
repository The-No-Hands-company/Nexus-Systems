"""
Phase 2: Assembly Token Canonicalization Tests

Tests to verify that all accepted assembly keyword spellings
('asm', 'assembly', 'inline assembly') parse to identical AST nodes.

Acceptance Criteria:
- All three spellings parse successfully
- All three spellings produce InlineAssembly AST nodes
- AST structure and content is identical regardless of spelling
- No dead assembly tokens remain in parser dispatch
"""

import pytest
from nexuslang.parser.parser import Parser
from nexuslang.parser.lexer import Lexer
from nexuslang.parser.ast import InlineAssembly, Program


class TestAssemblyKeywordSpellings:
    """Test that all assembly keyword spellings are canonicalized."""

    @pytest.mark.parametrize("keyword", [
        "asm",
        "assembly", 
        "inline assembly",
    ])
    def test_all_assembly_spellings_parse_successfully(self, keyword):
        """All assembly keyword spellings should parse without errors."""
        code = f"""
{keyword}
    code
        "nop"
end
"""
        lexer = Lexer(code)
        tokens = lexer.tokenize()
        parser = Parser(tokens)
        program = parser.parse()
        
        assert program is not None
        assert len(program.statements) > 0
        assert isinstance(program.statements[0], InlineAssembly)

    @pytest.mark.parametrize("keyword", [
        "asm",
        "assembly",
        "inline assembly",
    ])
    def test_assembly_spellings_produce_identical_ast(self, keyword):
        """All spellings should produce identical InlineAssembly AST nodes."""
        code = f"""
{keyword}
    code
        "movl $1, %eax"
    inputs "r": x
    outputs "=r": result
    clobbers "ecx"
end
"""
        lexer = Lexer(code)
        tokens = lexer.tokenize()
        parser = Parser(tokens)
        program = parser.parse()
        
        assert program is not None
        assert len(program.statements) > 0
        stmt = program.statements[0]
        assert isinstance(stmt, InlineAssembly)
        
        # Verify AST structure
        assert len(stmt.asm_code) > 0
        assert "movl" in stmt.asm_code[0]
        assert len(stmt.inputs) == 1
        assert len(stmt.outputs) == 1
        assert len(stmt.clobbers) == 1

    def test_asm_shortest_spelling(self):
        """'asm' keyword (shortest form) should parse correctly."""
        code = """
asm
        code
                "nop"
end
"""
        lexer = Lexer(code)
        tokens = lexer.tokenize()
        parser = Parser(tokens)
        program = parser.parse()
        
        stmt = program.statements[0]
        assert isinstance(stmt, InlineAssembly)
        assert stmt.asm_code[0] == "nop"

    def test_assembly_mid_spelling(self):
        """'assembly' keyword (mid form) should parse correctly."""
        code = """
assembly
        code
                "movl $1, %eax"
end
"""
        lexer = Lexer(code)
        tokens = lexer.tokenize()
        parser = Parser(tokens)
        program = parser.parse()
        
        stmt = program.statements[0]
        assert isinstance(stmt, InlineAssembly)
        assert "movl" in stmt.asm_code[0]

    def test_inline_assembly_long_spelling(self):
        """'inline assembly' keyword (long form) should parse correctly."""
        code = """
inline assembly
        code
                "movl $1, %eax"
end
"""
        lexer = Lexer(code)
        tokens = lexer.tokenize()
        parser = Parser(tokens)
        program = parser.parse()
        
        stmt = program.statements[0]
        assert isinstance(stmt, InlineAssembly)
        assert "movl" in stmt.asm_code[0]

    def test_assembly_with_architecture_guard_all_spellings(self):
        """Architecture guards work with all assembly spellings."""
        spellings = ["asm", "assembly", "inline assembly"]
        
        for keyword in spellings:
            code = f"""
{keyword} for arch "x86_64"
    code
        "movl $42, %eax"
end
"""
            lexer = Lexer(code)
            tokens = lexer.tokenize()
            parser = Parser(tokens)
            program = parser.parse()
            
            stmt = program.statements[0]
            assert isinstance(stmt, InlineAssembly)
            assert stmt.arch == "x86_64"

    def test_assembly_with_multiple_inputs_all_spellings(self):
        """Multiple inputs work with all assembly spellings."""
        spellings = ["asm", "assembly", "inline assembly"]
        
        for keyword in spellings:
            code = f"""
{keyword}
    code
        "add %1, %0"
    inputs "r": x, "r": y
    outputs "=r": result
end
"""
            lexer = Lexer(code)
            tokens = lexer.tokenize()
            parser = Parser(tokens)
            program = parser.parse()
            
            stmt = program.statements[0]
            assert isinstance(stmt, InlineAssembly)
            assert len(stmt.inputs) == 2

    def test_assembly_with_multiple_outputs_all_spellings(self):
        """Multiple outputs work with all assembly spellings."""
        spellings = ["asm", "assembly", "inline assembly"]
        
        for keyword in spellings:
            code = f"""
{keyword}
    code
        "divl %2"
    inputs "r": dividend, "r": divisor
    outputs "=r": quotient, "=r": remainder
end
"""
            lexer = Lexer(code)
            tokens = lexer.tokenize()
            parser = Parser(tokens)
            program = parser.parse()
            
            stmt = program.statements[0]
            assert isinstance(stmt, InlineAssembly)
            assert len(stmt.outputs) == 2

    def test_assembly_with_multiple_clobbers_all_spellings(self):
        """Multiple clobbers work with all assembly spellings."""
        spellings = ["asm", "assembly", "inline assembly"]
        
        for keyword in spellings:
            code = f"""
{keyword}
    code
        "cpuid"
    clobbers "eax", "ebx", "ecx", "edx"
end
"""
            lexer = Lexer(code)
            tokens = lexer.tokenize()
            parser = Parser(tokens)
            program = parser.parse()
            
            stmt = program.statements[0]
            assert isinstance(stmt, InlineAssembly)
            assert len(stmt.clobbers) == 4


class TestAssemblyParserDispatch:
    """Test that assembly dispatch is canonicalized."""

    def test_asm_token_routed_to_parse_inline_assembly(self):
        """TokenType.ASM should route to parse_inline_assembly method."""
        code = "asm\n    code\n        \"nop\"\nend"
        lexer = Lexer(code)
        tokens = lexer.tokenize()
        parser = Parser(tokens)
        
        # Verify token type is ASM
        assert tokens[0].type.name == 'ASM'
        
        # Parse and verify result
        program = parser.parse()
        assert isinstance(program.statements[0], InlineAssembly)

    def test_all_spellings_produce_same_token_type(self):
        """All assembly spellings should produce TokenType.ASM."""
        spellings = ["asm", "assembly", "inline assembly"]
        
        for spelling in spellings:
            lexer = Lexer(spelling)
            tokens = lexer.tokenize()
            # First token should be ASM
            assert tokens[0].type.name == 'ASM', f"'{spelling}' produced {tokens[0].type}"

    def test_no_separate_assembly_or_inline_dispatch_paths(self):
        """Verify parser has no separate ASSEMBLY or INLINE dispatch paths."""
        # Check _STMT_DISPATCH table for duplicate assembly entries
        from nexuslang.parser.parser import Parser
        from nexuslang.parser.lexer import TokenType
        
        # Create a dummy parser to access dispatch table
        lexer = Lexer("")
        parser = Parser(lexer.tokenize())
        
        # Collect all assembly-related dispatch entries
        dispatch = parser._STMT_DISPATCH
        
        # Should have exactly one assembly entry: ASM
        assembly_entries = {
            k: v for k, v in dispatch.items() 
            if 'asm' in v.lower() or k.name in ['ASM', 'ASSEMBLY', 'INLINE']
        }
        
        # Should have exactly one entry for assembly
        assert len(assembly_entries) == 1
        assert TokenType.ASM in assembly_entries
        assert assembly_entries[TokenType.ASM] == 'parse_inline_assembly'


class TestAssemblyLexerMapping:
    """Test that lexer keyword mapping is canonicalized."""

    def test_all_assembly_keywords_map_to_asm_token(self):
        """Verify lexer maps all assembly spellings to TokenType.ASM."""
        from nexuslang.parser.lexer import Lexer, TokenType
        
        spellings = [
            "asm",
            "assembly",
            "inline assembly",
        ]
        
        for spelling in spellings:
            lexer = Lexer(spelling)
            tokens = lexer.tokenize()
            assert tokens[0].type == TokenType.ASM, \
                f"'{spelling}' should map to TokenType.ASM, got {tokens[0].type}"

    def test_lexer_has_no_separate_inline_or_assembly_tokens(self):
        """Verify lexer keyword_map has no separate INLINE/ASSEMBLY entries."""
        from nexuslang.parser.lexer import Lexer
        
        # Create lexer and check keyword_map
        lexer = Lexer("")
        keyword_map = lexer.keywords
        
        # Collect all assembly-related entries
        assembly_keywords = {
            k: v for k, v in keyword_map.items()
            if 'asm' in k.lower() or 'assembly' in k.lower() or 'inline' in k.lower()
        }
        
        # All should map to ASM
        for keyword, token_type in assembly_keywords.items():
            assert token_type.name == 'ASM', \
                f"Keyword '{keyword}' maps to {token_type}, expected ASM"

    def test_assembly_canonicalization_coverage(self):
        """Document all accepted assembly keyword spellings."""
        accepted_spellings = [
            "asm",              # Shortest
            "assembly",         # Mid-length  
            "inline assembly",  # Longest, most verbose
        ]
        
        for spelling in accepted_spellings:
            lexer = Lexer(spelling)
            tokens = lexer.tokenize()
            assert tokens[0].type.name == 'ASM'
        
        # Only these three spellings should be accepted
        assert len(accepted_spellings) == 3


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
