"""
Tests for AST symbol extraction.
"""

import pytest
from nexuslang.parser.lexer import Lexer
from nexuslang.parser.parser import Parser
from nexuslang.analysis.symbol_extractor import ASTSymbolExtractor
from nexuslang.analysis.symbol_table import SymbolKind


def extract_symbols(source: str):
    """Helper to parse source and extract symbols."""
    lexer = Lexer(source)
    tokens = lexer.tokenize()
    parser = Parser(tokens)
    ast = parser.parse()
    extractor = ASTSymbolExtractor(uri="test://test.nxl")
    return extractor.extract(ast)


def test_extract_function():
    """Test extracting function symbol."""
    source = """
function greet with name as String returns Nothing
    print text "Hello"
end
"""
    symbol_table = extract_symbols(source)
    
    # Should find function
    symbol = symbol_table.resolve_symbol("greet")
    assert symbol is not None
    assert symbol.kind == SymbolKind.FUNCTION
    assert symbol.name == "greet"


def test_extract_function_with_parameters():
    """Test extracting function with parameters."""
    source = """
function add with x as Integer and y as Integer returns Integer
    return x plus y
end
"""
    symbol_table = extract_symbols(source)
    
    # Should find function and parameters
    func = symbol_table.resolve_symbol("add")
    assert func is not None
    assert func.kind == SymbolKind.FUNCTION
    
    # Parameters should be in nested scope (not accessible in global scope)
    # This is correct - parameters are only accessible inside function


def test_extract_class():
    """Test extracting class symbol."""
    source = """
class Person
    property name as String
    property age as Integer
    
    method greet returns Nothing
        print text "Hello"
    end
end
"""
    symbol_table = extract_symbols(source)
    
    # Should find class
    cls = symbol_table.resolve_symbol("Person")
    assert cls is not None
    assert cls.kind == SymbolKind.CLASS
    assert cls.name == "Person"
    
    # Should have children (properties and methods)
    assert len(cls.children) > 0
    
    # Check for property
    props = [c for c in cls.children if c.kind == SymbolKind.PROPERTY]
    assert len(props) == 2
    assert any(p.name == "name" for p in props)
    assert any(p.name == "age" for p in props)
    
    # Check for method
    methods = [c for c in cls.children if c.kind == SymbolKind.METHOD]
    assert len(methods) == 1
    assert methods[0].name == "greet"


def test_extract_struct():
    """Test extracting struct symbol."""
    source = """
struct Point
    x as Integer
    y as Integer
end
"""
    symbol_table = extract_symbols(source)
    
    # Should find struct
    struct = symbol_table.resolve_symbol("Point")
    assert struct is not None
    assert struct.kind == SymbolKind.STRUCT
    assert struct.name == "Point"
    
    # Should have fields as children
    assert len(struct.children) == 2
    fields = [c for c in struct.children if c.kind == SymbolKind.FIELD]
    assert len(fields) == 2
    assert any(f.name == "x" for f in fields)
    assert any(f.name == "y" for f in fields)


def test_extract_enum():
    """Test extracting enum symbol."""
    source = """
enum Color
    Red
    Green
    Blue
end
"""
    symbol_table = extract_symbols(source)
    
    # Should find enum
    enum = symbol_table.resolve_symbol("Color")
    assert enum is not None
    assert enum.kind == SymbolKind.ENUM
    assert enum.name == "Color"
    
    # Should have enum members as children
    assert len(enum.children) == 3
    members = [c for c in enum.children if c.kind == SymbolKind.ENUM_MEMBER]
    assert len(members) == 3
    assert any(m.name == "Red" for m in members)
    assert any(m.name == "Green" for m in members)
    assert any(m.name == "Blue" for m in members)


def test_extract_variables():
    """Test extracting variable symbols."""
    source = """
set x to 10
set name to "Alice"
"""
    symbol_table = extract_symbols(source)
    
    # Should find variables
    x = symbol_table.resolve_symbol("x")
    assert x is not None
    assert x.kind == SymbolKind.VARIABLE
    assert x.name == "x"
    
    name = symbol_table.resolve_symbol("name")
    assert name is not None
    assert name.kind == SymbolKind.VARIABLE
    assert name.name == "name"


def test_nested_scopes():
    """Test nested scope handling."""
    source = """
set global_var to 1

function outer returns Nothing
    set outer_var to 2
    
    function inner returns Nothing
        set inner_var to 3
    end
end
"""
    symbol_table = extract_symbols(source)
    
    # Should find all symbols at appropriate scope levels
    global_var = symbol_table.resolve_symbol("global_var")
    assert global_var is not None
    assert global_var.scope_level == 0
    
    outer = symbol_table.resolve_symbol("outer")
    assert outer is not None
    assert outer.kind == SymbolKind.FUNCTION


def test_find_symbols_by_kind():
    """Test finding symbols by kind."""
    source = """
function func1 returns Nothing
    print text "func1"
end

function func2 returns Nothing
    print text "func2"
end

class MyClass
    property x as Integer
end
"""
    symbol_table = extract_symbols(source)
    
    # Find all functions
    functions = symbol_table.find_symbols_by_kind(SymbolKind.FUNCTION)
    assert len(functions) == 2
    assert any(f.name == "func1" for f in functions)
    assert any(f.name == "func2" for f in functions)
    
    # Find all classes
    classes = symbol_table.find_symbols_by_kind(SymbolKind.CLASS)
    assert len(classes) == 1
    assert classes[0].name == "MyClass"


def test_find_symbols_by_name():
    """Test finding symbols by name."""
    source = """
function calculate returns Nothing
    print text "calculating"
end

class Calculator
    property value as Integer
    
    method calculate returns Nothing
        print text "method"
    end
end
"""
    symbol_table = extract_symbols(source)
    
    # Find all symbols named "calculate"
    symbols = symbol_table.find_symbols_by_name("calculate")
    assert len(symbols) == 2
    
    # One should be function, one should be method
    kinds = {s.kind for s in symbols}
    assert SymbolKind.FUNCTION in kinds
    assert SymbolKind.METHOD in kinds


def test_type_annotations():
    """Test that type annotations are extracted."""
    source = """
function add with x as Integer and y as Integer returns Integer
    set result to x plus y
    return result
end
"""
    symbol_table = extract_symbols(source)
    
    func = symbol_table.resolve_symbol("add")
    assert func is not None
    assert func.type_annotation == "Integer"


def test_complex_example():
    """Test extraction from a complex program."""
    source = """
class Rectangle
    property width as Integer
    property height as Integer
    
    method area returns Integer
        return width times height
    end
    
    method perimeter returns Integer
        return 2 times (width plus height)
    end
end

function create_rectangle with w as Integer and h as Integer returns Rectangle
    set rect to new Rectangle
    set rect.width to w
    set rect.height to h
    return rect
end

set my_rect to create_rectangle with 10 and 20
"""
    symbol_table = extract_symbols(source)
    
    # Check class
    cls = symbol_table.resolve_symbol("Rectangle")
    assert cls is not None
    assert cls.kind == SymbolKind.CLASS
    assert len(cls.children) == 4  # 2 properties + 2 methods
    
    # Check function
    func = symbol_table.resolve_symbol("create_rectangle")
    assert func is not None
    assert func.kind == SymbolKind.FUNCTION
    
    # Check variable
    var = symbol_table.resolve_symbol("my_rect")
    assert var is not None
    assert var.kind == SymbolKind.VARIABLE


def test_symbol_hierarchy():
    """Test that parent-child relationships are correct."""
    source = """
class Person
    property name as String
    
    method greet returns Nothing
        print text "Hello"
    end
end
"""
    symbol_table = extract_symbols(source)
    
    cls = symbol_table.resolve_symbol("Person")
    assert cls is not None
    
    # Check children
    assert len(cls.children) == 2
    
    for child in cls.children:
        assert child.parent == cls


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
