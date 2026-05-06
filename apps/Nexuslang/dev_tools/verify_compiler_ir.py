
import sys
import os

# Add src to path
sys.path.append(os.path.join(os.getcwd(), 'src'))

from nexuslang.parser.lexer import Lexer
from nexuslang.parser.parser import Parser
from nexuslang.compiler.backends.llvm_ir_generator import LLVMIRGenerator

def verify_ir():
    source_file = 'test_programs/foundation/bitwise_verification.nxl'
    with open(source_file, 'r') as f:
        source_code = f.read()

    print(f"Compiling {source_file}...")
    
    # 1. Parse
    lexer = Lexer(source_code)
    tokens = lexer.tokenize()
    parser = Parser(tokens)
    ast = parser.parse()
    
    # 2. Generate IR
    generator = LLVMIRGenerator('llvm_ir')
    ir_code = generator.generate(ast)
    
    print("\n--- Generated LLVM IR ---\n")
    print(ir_code)
    print("\n-------------------------\n")
    
    # 3. Verification checks
    checks = [
        ('and i64', "Bitwise AND"),
        ('or i64', "Bitwise OR"),
        ('xor i64', "Bitwise XOR"),
        ('shl i64', "Shift Left"),
        ('ashr i64', "Shift Right"),
    ]
    
    all_passed = True
    for check_str, description in checks:
        if check_str in ir_code:
            print(f"PASS: {description} found")
        else:
            print(f"FAIL: {description} NOT found")
            all_passed = False
            
    if all_passed:
        print("\nSUCCESS: All compiler checks passed.")
    else:
        print("\nFAILURE: Some compiler checks failed.")
        sys.exit(1)

if __name__ == "__main__":
    verify_ir()
