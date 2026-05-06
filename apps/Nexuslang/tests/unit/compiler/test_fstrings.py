"""
F-string tests: lexer tokens, AST nodes, and parser integration.
Merged from test_fstring_lexer.py, test_fstring_ast.py, test_fstring_parser.py.
"""

import sys
import os
from pathlib import Path
import pytest

_REPO_ROOT = str((Path(__file__).resolve().parent.parent.parent.parent))
_SRC = os.path.join(_REPO_ROOT, "src")
if _SRC not in sys.path:
    sys.path.insert(0, _SRC)

# --- from test_fstring_lexer.py ---

sys.path.insert(0, '/run/media/zajferx/Data/dev/The-No-hands-Company/projects/Active/NexusLang/src')

from nexuslang.parser.lexer import Lexer, TokenType

def test_fstring_lexer():
    """Test lexer recognition of f-strings."""
    print("Testing F-String Lexer...")
    
    # Test 1: Simple f-string
    print("\n1. Testing simple f-string:")
    code = 'f"Hello, {name}!"'
    lexer = Lexer(code)
    tokens = lexer.scan_tokens()
    
    # Should have FSTRING_LITERAL token
    fstring_tokens = [t for t in tokens if t.type == TokenType.FSTRING_LITERAL]
    assert len(fstring_tokens) == 1, f"Expected 1 f-string token, got {len(fstring_tokens)}"
    
    parts = fstring_tokens[0].literal
    print(f"   Parts: {parts}")
    assert len(parts) == 3, f"Expected 3 parts, got {len(parts)}"
    assert parts[0] == ('literal', 'Hello, ', None)
    assert parts[1] == ('expr', 'name', None)
    assert parts[2] == ('literal', '!', None)
    print("    Simple f-string lexed correctly")
    
    # Test 2: Expression in f-string
    print("\n2. Testing expression in f-string:")
    code2 = 'f"Result: {x + y}"'
    lexer2 = Lexer(code2)
    tokens2 = lexer2.scan_tokens()
    
    fstring_tokens2 = [t for t in tokens2 if t.type == TokenType.FSTRING_LITERAL]
    parts2 = fstring_tokens2[0].literal
    print(f"   Parts: {parts2}")
    assert parts2[1] == ('expr', 'x + y', None)
    print("    Expression f-string lexed correctly")
    
    # Test 3: Multiple interpolations
    print("\n3. Testing multiple interpolations:")
    code3 = 'f"{a} + {b} = {a + b}"'
    lexer3 = Lexer(code3)
    tokens3 = lexer3.scan_tokens()
    
    fstring_tokens3 = [t for t in tokens3 if t.type == TokenType.FSTRING_LITERAL]
    parts3 = fstring_tokens3[0].literal
    print(f"   Parts: {parts3}")
    assert len(parts3) == 5
    print("    Multiple interpolations work")
    
    print("\n All f-string lexer tests passed!")

if __name__ == "__main__":
    test_fstring_lexer()


# --- from test_fstring_ast.py ---

from nexuslang.parser.ast import FStringExpression, Literal, Identifier
from nexuslang.parser.parser import Parser
from nexuslang.parser.lexer import Lexer, TokenType

def test_fstring_parsing():
    """Test parsing f-strings."""
    print("Testing F-String Parsing...")
    
    # Test 1: Simple interpolation
    print("\n1. Testing simple interpolation:")
    code = 'f"Hello, {name}!"'
    
    lexer = Lexer(code)
    parser = Parser(lexer.scan_tokens())
    
    # For now, just test that we can create the AST node manually
    parts = [
        (True, "Hello, "),
        (False, Identifier("name")),
        (True, "!")
    ]
    fstring = FStringExpression(parts)
    
    assert len(fstring.parts) == 3
    assert fstring.parts[0] == (True, "Hello, ")
    print("    F-string AST node created")
    
    # Test 2: Expression interpolation
    print("\n2. Testing expression interpolation:")
    parts2 = [
        (True, "Result: "),
        (False, Literal(TokenType.INTEGER_LITERAL, 42)),
    ]
    fstring2 = FStringExpression(parts2)
    assert len(fstring2.parts) == 2
    print("    Expression interpolation works")
    
    print("\n F-string AST tests passed!")
    print("Note: Parser integration pending")

if __name__ == "__main__":
    test_fstring_parsing()


# --- from test_fstring_parser.py ---

sys.path.insert(0, '/run/media/zajferx/Data/dev/The-No-hands-Company/projects/Active/NexusLang/src')

from nexuslang.parser.lexer import Lexer
from nexuslang.parser.parser import Parser
from nexuslang.parser.ast import FStringExpression, Identifier

def test_fstring_parser():
    """Test parser conversion of f-strings to AST."""
    print("Testing F-String Parser...")
    
    # Test 1: Simple f-string
    print("\n1. Testing simple f-string parsing:")
    code = 'f"Hello, {name}!"'
    lexer = Lexer(code)
    tokens = lexer.scan_tokens()
    parser = Parser(tokens)
    
    # Parse as expression
    ast = parser.expression()
    
    assert isinstance(ast, FStringExpression), f"Expected FStringExpression, got {type(ast)}"
    assert len(ast.parts) == 3
    assert ast.parts[0] == (True, 'Hello, ', None)
    assert ast.parts[0][0] == True  # is_literal
    assert isinstance(ast.parts[1][1], Identifier)  # expression
    assert ast.parts[1][1].name == 'name'
    assert ast.parts[2] == (True, '!', None)
    print("    Simple f-string parsed correctly")
    
    # Test 2: Expression in f-string
    print("\n2. Testing expression parsing:")
    code2 = 'f"Result: {x + y}"'
    lexer2 = Lexer(code2)
    tokens2 = lexer2.scan_tokens()
    parser2 = Parser(tokens2)
    ast2 = parser2.expression()
    
    assert isinstance(ast2, FStringExpression)
    assert len(ast2.parts) == 2
    print(f"   Parts: {ast2.parts}")
    print("    Expression f-string parsed")
    
    print("\n All f-string parser tests passed!")

if __name__ == "__main__":
    test_fstring_parser()
