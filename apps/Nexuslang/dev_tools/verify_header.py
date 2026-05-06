
import sys
import os
import shutil

# Ensure we can import the src directory
sys.path.append(os.path.abspath("src"))

try:
    from nexuslang.compiler import Compiler, CompilationTarget
    from nexuslang.parser.parser import Parser
    from nexuslang.parser.lexer import Lexer
    
    print("Verifying Header Generation...")
    
    # 1. Read source
    source_file = "test_export.nxl"
    with open(source_file, 'r') as f:
        source_code = f.read()
        
    # 2. Parse
    lexer = Lexer(source_code)
    tokens = lexer.tokenize()
    parser = Parser(tokens)
    ast = parser.parse()
    
    # 3. Compile with header generation enabled
    compiler = Compiler()
    compiler.options.generate_header = True
    
    output_c = "test_export.c"
    header_file = "test_export.h"
    
    # Clean up previous
    if os.path.exists(output_c): os.remove(output_c)
    if os.path.exists(header_file): os.remove(header_file)
    
    success, _ = compiler.compile(ast, CompilationTarget.C, output_c)
    
    if not success:
        print(" Compilation failed")
        sys.exit(1)
        
    # 4. Verify Header Content
    if not os.path.exists(header_file):
        print(f" Header file not generated: {header_file}")
        sys.exit(1)
        
    with open(header_file, 'r') as f:
        content = f.read()
    
    print(f" Header file generated: {header_file}")
    print("\n--- Header Content ---")
    print(content)
    print("----------------------\n")
    
    # Check for exported functions
    expected_prototypes = [
        "int64_t calc_add(int64_t x, int64_t y)",
        "int64_t calc_sub(int64_t x, int64_t y)"
    ]
    
    missing = []
    for proto in expected_prototypes:
        if proto not in content:
            missing.append(proto)
            
    if missing:
        print(" Missing prototypes:")
        for m in missing:
            print(f"  - {m}")
        sys.exit(1)
        
    # Check that internal helper is NOT exported
    if "internal_helper" in content:
        print(" Internal function 'internal_helper' was incorrectly exported")
        sys.exit(1)
        
    print(" Header content verification successful")
    
except Exception as e:
    print(f" Verification failed with exception: {e}")
    import traceback
    traceback.print_exc()
    sys.exit(1)
