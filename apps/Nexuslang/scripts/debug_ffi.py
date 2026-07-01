import sys
sys.path.insert(0, 'src')

from nexuslang.parser.lexer import Lexer
from nexuslang.parser.parser import Parser

code = '''
extern function strlen with str as Pointer returns Integer from library "c"
'''

lexer = Lexer(code)
tokens = lexer.scan_tokens()
parser = Parser(tokens)
ast = parser.parse()

print("=== AST Statements ===")
for stmt in ast.statements:
    print(f"Class: {stmt.__class__.__name__}")
    print(f"node_type: {stmt.node_type if hasattr(stmt, 'node_type') else 'N/A'}")
    
    # Show how execute() will dispatch it
    import re
    class_name = stmt.__class__.__name__
    method_name = f"execute_{re.sub(r'(?<!^)(?=[A-Z])', '_', class_name).lower()}"
    print(f"Expected method: {method_name}")
