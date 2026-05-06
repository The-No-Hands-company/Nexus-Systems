"""
Test Exception Type Support
============================

Tests for NLPL's exception type support in:
1. AST nodes
2. Parser
3. LLVM IR generator
"""

import sys
from pathlib import Path

# Add project root to path
sys.path.insert(0, str(Path(__file__).parent.parent / 'src'))

from nexuslang.parser.lexer import Lexer
from nexuslang.parser.parser import Parser
from nexuslang.parser.ast import TryCatch, RaiseStatement
from nexuslang.compiler.backends.llvm_ir_generator import LLVMIRGenerator


def test_ast_exception_type_field():
    """Test that AST nodes have exception_type field."""
    print("=" * 80)
    print("Test 1: AST Exception Type Field")
    print("=" * 80)
    
    # Test TryCatch node
    try_catch = TryCatch(
        try_block=[],
        catch_block=[],
        exception_var="e",
        exception_type="ValueError",
        line_number=1
    )
    
    assert hasattr(try_catch, 'exception_type'), "TryCatch should have exception_type field"
    assert try_catch.exception_type == "ValueError", "Exception type should be ValueError"
    
    print(f" TryCatch node has exception_type field: {try_catch.exception_type}")
    
    # Test RaiseStatement node
    raise_stmt = RaiseStatement(
        exception_type="RuntimeError",
        message=None,
        line_number=1
    )
    
    assert hasattr(raise_stmt, 'exception_type'), "RaiseStatement should have exception_type field"
    assert raise_stmt.exception_type == "RuntimeError", "Exception type should be RuntimeError"
    
    print(f" RaiseStatement node has exception_type field: {raise_stmt.exception_type}")
    print()


def test_parser_exception_type_capture():
    """Test that parser captures exception types."""
    print("=" * 80)
    print("Test 2: Parser Exception Type Capture")
    print("=" * 80)
    
    # Test 1: Generic catch (no type)
    code1 = """
try
    set x to 10
catch e
    print text "error"
end
"""
    
    lexer1 = Lexer(code1)
    tokens1 = lexer1.tokenize()
    parser1 = Parser(tokens1)
    ast1 = parser1.parse()
    
    try_catch1 = ast1.statements[0]
    assert try_catch1.node_type == "try_catch", "Should be try_catch node"
    assert try_catch1.exception_var == "e", "Exception var should be 'e'"
    assert try_catch1.exception_type is None, "Exception type should be None for generic catch"
    
    print(f" Generic catch: exception_var={try_catch1.exception_var}, exception_type={try_catch1.exception_type}")
    
    # Test 2: Typed catch (with 'as ExceptionType')
    code2 = """
try
    set x to 10
catch e as ValueError
    print text "error"
end
"""
    
    lexer2 = Lexer(code2)
    tokens2 = lexer2.tokenize()
    parser2 = Parser(tokens2)
    ast2 = parser2.parse()
    
    try_catch2 = ast2.statements[0]
    assert try_catch2.node_type == "try_catch", "Should be try_catch node"
    assert try_catch2.exception_var == "e", "Exception var should be 'e'"
    assert try_catch2.exception_type == "ValueError", "Exception type should be ValueError"
    
    print(f" Typed catch: exception_var={try_catch2.exception_var}, exception_type={try_catch2.exception_type}")
    
    # Test 3: Raise with specific type
    code3 = """
raise ValueError with message "test"
"""
    
    lexer3 = Lexer(code3)
    tokens3 = lexer3.tokenize()
    parser3 = Parser(tokens3)
    ast3 = parser3.parse()
    
    raise_stmt = ast3.statements[0]
    assert raise_stmt.node_type == "raise_statement", "Should be raise_statement node"
    assert raise_stmt.exception_type == "ValueError", "Exception type should be ValueError"
    
    print(f" Raise statement: exception_type={raise_stmt.exception_type}")
    print()


def test_llvm_ir_exception_type_mapping():
    """Test that LLVM IR generator maps exception types correctly."""
    print("=" * 80)
    print("Test 3: LLVM IR Exception Type Mapping")
    print("=" * 80)
    
    code = """
try
    raise ValueError with message "test error"
catch e as ValueError
    print text e
end
"""
    
    lexer = Lexer(code)
    tokens = lexer.tokenize()
    parser = Parser(tokens)
    ast = parser.parse()
    
    # Generate LLVM IR
    generator = LLVMIRGenerator()
    llvm_ir = generator.generate(ast)
    
    # Check that LLVM IR contains exception type handling
    assert '@_ZTI10ValueError' in llvm_ir or 'ValueError' in llvm_ir, \
        "LLVM IR should reference ValueError typeinfo"
    
    assert 'landingpad' in llvm_ir, "LLVM IR should contain landingpad instruction"
    assert '__cxa_begin_catch' in llvm_ir, "LLVM IR should contain __cxa_begin_catch call"
    assert '__cxa_end_catch' in llvm_ir, "LLVM IR should contain __cxa_end_catch call"
    
    print(" LLVM IR contains exception handling instructions")
    print(" LLVM IR references exception type information")
    
    # Print relevant sections
    print("\nRelevant LLVM IR sections:")
    for line in llvm_ir.split('\n'):
        if 'landingpad' in line or 'ValueError' in line or '__cxa' in line:
            print(f"  {line.strip()}")
    
    print()


def test_multiple_exception_types():
    """Test handling of multiple different exception types."""
    print("=" * 80)
    print("Test 4: Multiple Exception Types")
    print("=" * 80)
    
    exception_types = [
        "ValueError",
        "RuntimeError",
        "TypeError",
        "IndexError",
        "Error"  # Generic
    ]
    
    for exc_type in exception_types:
        code = f"""
try
    raise {exc_type} with message "test"
catch e as {exc_type}
    print text e
end
"""
        
        lexer = Lexer(code)
        tokens = lexer.tokenize()
        parser = Parser(tokens)
        ast = parser.parse()
        
        try_catch = ast.statements[0]
        assert try_catch.exception_type == exc_type, f"Should capture {exc_type}"
        
        print(f" {exc_type}: Correctly parsed and captured")
    
    print()


def test_nested_exception_handling():
    """Test nested try-catch blocks with different exception types."""
    print("=" * 80)
    print("Test 5: Nested Exception Handling")
    print("=" * 80)
    
    code = """
try
    try
        raise ValueError with message "inner"
    catch e as ValueError
        print text e
    end
catch outer as RuntimeError
    print text outer
end
"""
    
    lexer = Lexer(code)
    tokens = lexer.tokenize()
    parser = Parser(tokens)
    ast = parser.parse()
    
    outer_try = ast.statements[0]
    assert outer_try.node_type == "try_catch", "Outer should be try_catch"
    assert outer_try.exception_type == "RuntimeError", "Outer should catch RuntimeError"
    
    inner_try = outer_try.try_block[0]
    assert inner_try.node_type == "try_catch", "Inner should be try_catch"
    assert inner_try.exception_type == "ValueError", "Inner should catch ValueError"
    
    print(f" Outer try-catch: exception_type={outer_try.exception_type}")
    print(f" Inner try-catch: exception_type={inner_try.exception_type}")
    print(" Nested exception handling correctly parsed")
    print()


def test_exception_in_function():
    """Test exception handling in function context."""
    print("=" * 80)
    print("Test 6: Exception in Function")
    print("=" * 80)
    
    code = """
function divide with a as Integer and b as Integer returns Integer
    if b is equal to 0
        raise ValueError with message "Division by zero"
    end
    return a divided by b
end

try
    set result to divide with 10 and 0
catch e as ValueError
    print text e
end
"""
    
    lexer = Lexer(code)
    tokens = lexer.tokenize()
    parser = Parser(tokens)
    ast = parser.parse()
    
    # Check function definition
    func_def = ast.statements[0]
    assert func_def.node_type == "function_definition", "Should be function definition"
    
    # Check raise statement in function
    raise_stmt = func_def.body[0].then_block[0]  # Inside if statement
    assert raise_stmt.node_type == "raise_statement", "Should be raise statement"
    assert raise_stmt.exception_type == "ValueError", "Should raise ValueError"
    
    # Check try-catch
    try_catch = ast.statements[1]
    assert try_catch.node_type == "try_catch", "Should be try_catch"
    assert try_catch.exception_type == "ValueError", "Should catch ValueError"
    
    print(f" Function raises: {raise_stmt.exception_type}")
    print(f" Try-catch catches: {try_catch.exception_type}")
    print(" Exception handling in function context works correctly")
    print()


if __name__ == '__main__':
    print("\n" + "=" * 80)
    print("NLPL Exception Type Support Tests")
    print("=" * 80 + "\n")
    
    try:
        test_ast_exception_type_field()
        test_parser_exception_type_capture()
        test_llvm_ir_exception_type_mapping()
        test_multiple_exception_types()
        test_nested_exception_handling()
        test_exception_in_function()
        
        print("=" * 80)
        print(" All tests passed!")
        print("=" * 80)
        
    except AssertionError as e:
        print(f"\n Test failed: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)
    except Exception as e:
        print(f"\n Error: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)
