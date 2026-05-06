import textwrap
from nexuslang.parser.lexer import Lexer
from nexuslang.parser.parser import Parser
from nexuslang.compiler import create_compiler, CompilationTarget

def test_panic_compilation():
    source = textwrap.dedent("""
    function test_panic that takes nothing
        panic with "This is a panic"
    """)
    
    print("Compiling source:")
    print(source)
    
    # Parse
    try:
        lexer = Lexer(source)
        tokens = lexer.tokenize()
        parser = Parser(tokens)
        ast = parser.parse()
    except Exception as e:
        print(f"Parsing failed: {e}")
        return False
    
    # Compile
    try:
        compiler = create_compiler()
        output_file = "test_panic.ll"
        success = compiler.compile(ast, CompilationTarget.LLVM_IR, output_file)
        
        if not success:
            print("Compilation failed")
            return False
            
        # Check output
        with open(output_file, 'r') as f:
            ir_code = f.read()
            
        print("\nGenerated IR Code:")
        print(ir_code)
        
        if 'call void @nxl_panic' in ir_code and 'unreachable' in ir_code:
            print("\nVerification SUCCESS: Panic code generated correctly")
            return True
        else:
            print("\nVerification FAILED: Panic code not found")
            return False

    except Exception as e:
        print(f"Compilation failed with exception: {e}")
        import traceback
        traceback.print_exc()
        return False

if __name__ == "__main__":
    test_panic_compilation()
