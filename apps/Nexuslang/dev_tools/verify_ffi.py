import os
import subprocess
import sys
import textwrap
from nexuslang.parser.lexer import Lexer
from nexuslang.parser.parser import Parser
from nexuslang.compiler import create_compiler, CompilationTarget

def verify_ffi():
    # 1. Compile C library
    print("Building local C library...")
    try:
        subprocess.run(['gcc', '-shared', '-fPIC', '-o', 'libmylib.so', 'mylib.c'], check=True)
        print(" libmylib.so built")
    except subprocess.CalledProcessError as e:
        print(f"Failed to build C library: {e}")
        return False
        
    cwd = os.getcwd()
    
    # 2. Parse and compile NexusLang
    print("\nCompiling NexusLang...")
    with open('test_ffi.nxl', 'r') as f:
        source = f.read()
        
    try:
        lexer = Lexer(source)
        tokens = lexer.tokenize()
        parser = Parser(tokens)
        ast = parser.parse()
    except Exception as e:
        print(f"Parsing failed: {e}")
        return False
        
    try:
        compiler = create_compiler()
        # Add current directory to library search paths
        compiler.options.library_search_paths.append(cwd)
        
        output_file = "test_ffi" # Executable name
        success = compiler.compile_and_link(ast, CompilationTarget.C, output_file)
        
        if not success:
            print("Compilation/Linking failed")
            return False
            
    except Exception as e:
        print(f"Compilation exception: {e}")
        import traceback
        traceback.print_exc()
        return False
        
    # 3. Run executable
    print("\nRunning executable...")
    env = os.environ.copy()
    env['LD_LIBRARY_PATH'] = cwd + ':' + env.get('LD_LIBRARY_PATH', '')
    
    try:
        result = subprocess.run(['./test_ffi'], env=env, capture_output=True, text=True)
        print("Output:")
        print(result.stdout)
        
        if "C Library: Adding 10 + 20" in result.stdout and "NLPL: Result from C: 30" in result.stdout:
            print("\nVerification SUCCESS: FFI working correctly")
            return True
        else:
            print("\nVerification FAILED: Output mismatch")
            print("Stderr:", result.stderr)
            return False
            
    except Exception as e:
        print(f"Execution failed: {e}")
        return False

if __name__ == "__main__":
    verify_ffi()
