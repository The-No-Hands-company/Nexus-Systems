import sys
sys.path.insert(0, 'src')

from nexuslang.parser.lexer import Lexer
from nexuslang.parser.parser import Parser

code = '''
set name to "Alice"
print text f"Hello, {name}!"
'''

lexer = Lexer(code)
tokens = lexer.scan_tokens()

print("=== TOKENS ===")
for token in tokens:
    print(f"{token.type}: {token.lexeme} (literal: {token.literal if hasattr(token, 'literal') else 'N/A'})")

parser = Parser(tokens)
ast = parser.parse()

print("\n=== AST ===")
for stmt in ast.statements:
    print(f"Statement type: {type(stmt).__name__}")
    if hasattr(stmt, 'value'):
        print(f"  Value type: {type(stmt.value).__name__}")
