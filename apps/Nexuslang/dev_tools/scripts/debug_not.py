import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../src'))

from nexuslang.parser.lexer import Lexer, TokenType
from nexuslang.parser.parser import Parser

# Just parse the if statement part
code = "not c"

lexer = Lexer(code)
tokens = list(lexer.tokenize())
print("Tokens:", [(t.type.name, t.lexeme) for t in tokens])

parser = Parser(tokens)
# Parser.__init__ already sets current_token, don't call advance()

print(f"Current token: {parser.current_token.type.name} = '{parser.current_token.lexeme}'")
print(f"Is NOT? {parser.current_token.type == TokenType.NOT}")

expr = parser.logical_not()

print(f"\nExpression type: {type(expr).__name__}")
if hasattr(expr, 'operator'):
    op = expr.operator
    print(f"Operator: {op.lexeme if hasattr(op, 'lexeme') else op}")
if hasattr(expr, 'operand'):
    print(f"Operand type: {type(expr.operand).__name__}")
    if hasattr(expr.operand, 'name'):
        print(f"Operand name: {expr.operand.name}")

