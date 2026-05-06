#!/usr/bin/env python3
"""
Test for-each loop parsing and C code generation.
"""

import sys
sys.path.insert(0, '/run/media/zajferx/Data/dev/The-No-hands-Company/projects/NexusLang/src')

from nexuslang.parser.lexer import Lexer
from nexuslang.parser.parser import Parser
from nexuslang.parser.ast import ForLoop
from nexuslang.compiler.backends.c_generator import CCodeGenerator

def test_for_each_loop():
    """Test for-each loop syntax."""
    
    test_cases = [
        ("Simple for-each", 'for each item in items\nprint item\nend'),
        ("For-each with array", 'set nums to [1, 2, 3]\nfor each num in nums\nprint num\nend'),
    ]
    
    print("="*70)
    print("FOR-EACH LOOP TEST")
    print("="*70)
    
    for name, code in test_cases:
        print(f"\n{name}:")
        print(f"NLPL: {code}")
        
        try:
            lexer = Lexer(code)
            tokens = lexer.tokenize()
            parser = Parser(tokens)
            ast = parser.parse()
            
            # Check if we got a ForLoop
            has_for_loop = any(isinstance(stmt, ForLoop) for stmt in ast.statements)
            
            if has_for_loop:
                print("   Parsing successful - ForLoop found")
                
                # Try to generate C code
                generator = CCodeGenerator(target="c")
                c_code = generator.generate(ast)
                
                print("\n  Generated C (snippet):")
                # Show just the main body
                lines = c_code.split('\n')
                in_main = False
                for line in lines:
                    if 'int main' in line:
                        in_main = True
                    if in_main and line.strip() and not line.strip().startswith('/*'):
                        print(f"    {line}")
                    if in_main and line.strip() == '}' and 'return' in lines[lines.index(line)-1]:
                        break
            else:
                print("   No ForLoop found in AST")
                print(f"  AST statements: {[type(s).__name__ for s in ast.statements]}")
                
        except Exception as e:
            print(f"   Error: {e}")
            import traceback
            traceback.print_exc()

if __name__ == "__main__":
    test_for_each_loop()
