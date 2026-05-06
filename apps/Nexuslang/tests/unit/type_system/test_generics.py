"""
Test suite for generic type system in NexusLang.
Tests generic type parsing, type inference, and type checking with generic types.
"""

import pytest
from nexuslang.parser.lexer import Lexer
from nexuslang.parser.parser import Parser
from nexuslang.parser.ast import ObjectInstantiation
from nexuslang.typesystem.types import (
    get_type_by_name, ListType, DictionaryType,
    INTEGER_TYPE, STRING_TYPE, FLOAT_TYPE, BOOLEAN_TYPE, ANY_TYPE
)
from nexuslang.typesystem.simple_inference import SimpleTypeInference


class TestGenericTypeParsing:
    """Test parsing of generic type annotations."""
    
    def parse_code(self, code: str):
        """Helper to parse code."""
        lexer = Lexer(code)
        tokens = lexer.tokenize()
        parser = Parser(tokens)
        return parser.parse()
    
    def test_parse_list_generic_syntax(self):
        """Test parsing List<T> syntax."""
        code = "function test that takes items as List<Integer> returns Boolean\n    return true"
        program = self.parse_code(code)
        func = program.statements[0]
        
        assert func.parameters[0].type_annotation == "List<Integer>"
        assert func.return_type == "Boolean"
    
    def test_parse_dict_generic_syntax(self):
        """Test parsing Dictionary<K, V> syntax."""
        code = "function test that takes data as Dictionary<String, Integer> returns Boolean\n    return true"
        program = self.parse_code(code)
        func = program.statements[0]
        
        assert func.parameters[0].type_annotation == "Dictionary<String, Integer>"
    
    def test_parse_nested_generics(self):
        """Test parsing nested generic types like List<List<T>>."""
        code = "function test that takes matrix as List<List<Integer>> returns Integer\n    return 0"
        program = self.parse_code(code)
        func = program.statements[0]
        
        assert func.parameters[0].type_annotation == "List<List<Integer>>"
    
    def test_parse_complex_nested_generics(self):
        """Test parsing complex nested generics like Dictionary<String, List<Integer>>."""
        code = "function test that takes data as Dictionary<String, List<Integer>> returns Boolean\n    return true"
        program = self.parse_code(code)
        func = program.statements[0]
        
        assert func.parameters[0].type_annotation == "Dictionary<String, List<Integer>>"
    
    def test_parse_natural_and_generic_syntax(self):
        """Test that both syntaxes work: List<T> and List of T."""
        code1 = "function test1 that takes items as List<Integer> returns Boolean\n    return true"
        code2 = "function test2 that takes items as List of Integer returns Boolean\n    return true"
        
        program1 = self.parse_code(code1)
        program2 = self.parse_code(code2)
        
        # Both should parse successfully
        assert len(program1.statements) == 1
        assert len(program2.statements) == 1


class TestGenericTypeObjects:
    """Test creation of type objects from generic type strings."""
    
    def test_list_generic_to_type_object(self):
        """Test converting List<Integer> string to ListType object."""
        list_type = get_type_by_name("List<Integer>")
        
        assert isinstance(list_type, ListType)
        assert list_type.element_type is INTEGER_TYPE
    
    def test_dictionary_generic_to_type_object(self):
        """Test converting Dictionary<String, Integer> to DictionaryType."""
        dict_type = get_type_by_name("Dictionary<String, Integer>")
        
        assert isinstance(dict_type, DictionaryType)
        assert dict_type.key_type is STRING_TYPE
        assert dict_type.value_type is INTEGER_TYPE
    
    def test_nested_list_to_type_object(self):
        """Test converting List<List<Integer>> to nested ListType."""
        nested_type = get_type_by_name("List<List<Integer>>")
        
        assert isinstance(nested_type, ListType)
        assert isinstance(nested_type.element_type, ListType)
        assert nested_type.element_type.element_type is INTEGER_TYPE
    
    def test_complex_nested_to_type_object(self):
        """Test converting Dictionary<String, List<Integer>> to complex type."""
        complex_type = get_type_by_name("Dictionary<String, List<Integer>>")
        
        assert isinstance(complex_type, DictionaryType)
        assert complex_type.key_type is STRING_TYPE
        assert isinstance(complex_type.value_type, ListType)
        assert complex_type.value_type.element_type is INTEGER_TYPE
    
    def test_natural_syntax_to_type_object(self):
        """Test that natural syntax also creates proper type objects."""
        list_type = get_type_by_name("List of Integer")
        
        assert isinstance(list_type, ListType)
        assert list_type.element_type is INTEGER_TYPE
    
    def test_dictionary_natural_syntax(self):
        """Test Dictionary of Key, Value syntax."""
        dict_type = get_type_by_name("Dictionary of String, Integer")
        
        assert isinstance(dict_type, DictionaryType)
        assert dict_type.key_type is STRING_TYPE
        assert dict_type.value_type is INTEGER_TYPE


class TestGenericTypeInference:
    """Test type inference with generic types."""
    
    def parse_code(self, code: str):
        """Helper to parse code."""
        lexer = Lexer(code)
        tokens = lexer.tokenize()
        parser = Parser(tokens)
        return parser.parse()
    
    def test_infer_list_parameter_type(self):
        """Test that function parameters with List<T> are correctly inferred."""
        code = """
function sum that takes numbers as List<Integer> returns Integer
    return 0
"""
        program = self.parse_code(code)
        func = program.statements[0]
        
        # Get parameter type
        param_type = get_type_by_name(func.parameters[0].type_annotation)
        
        assert isinstance(param_type, ListType)
        assert param_type.element_type is INTEGER_TYPE
    
    def test_infer_dictionary_parameter_type(self):
        """Test that Dictionary<K, V> parameters are correctly inferred."""
        code = """
function lookup that takes data as Dictionary<String, Float> returns Float
    return 0.0
"""
        program = self.parse_code(code)
        func = program.statements[0]
        
        param_type = get_type_by_name(func.parameters[0].type_annotation)
        
        assert isinstance(param_type, DictionaryType)
        assert param_type.key_type is STRING_TYPE
        assert param_type.value_type is FLOAT_TYPE
    
    def test_infer_function_return_type_generic(self):
        """Test inferring return types with generics."""
        code = """
function get_items that takes count as Integer returns List<String>
    set x to 1
"""
        program = self.parse_code(code)
        func = program.statements[0]
        
        return_type = get_type_by_name(func.return_type)
        
        assert isinstance(return_type, ListType)
        assert return_type.element_type is STRING_TYPE


class TestGenericTypeCompatibility:
    """Test type compatibility with generic types."""
    
    def test_list_type_compatibility(self):
        """Test that List<Integer> is compatible with List<Integer>."""
        list_int1 = get_type_by_name("List<Integer>")
        list_int2 = get_type_by_name("List<Integer>")
        
        assert list_int1.is_compatible_with(list_int2)
    
    def test_list_type_incompatibility(self):
        """Test that List<Integer> is not compatible with List<String>."""
        list_int = get_type_by_name("List<Integer>")
        list_str = get_type_by_name("List<String>")
        
        assert not list_int.is_compatible_with(list_str)
    
    def test_nested_list_compatibility(self):
        """Test compatibility of nested generic types."""
        nested1 = get_type_by_name("List<List<Integer>>")
        nested2 = get_type_by_name("List<List<Integer>>")
        
        assert nested1.is_compatible_with(nested2)
    
    def test_dictionary_type_compatibility(self):
        """Test Dictionary type compatibility."""
        dict1 = get_type_by_name("Dictionary<String, Integer>")
        dict2 = get_type_by_name("Dictionary<String, Integer>")
        
        assert dict1.is_compatible_with(dict2)
    
    def test_dictionary_type_incompatibility(self):
        """Test Dictionary with different value types are incompatible."""
        dict_int = get_type_by_name("Dictionary<String, Integer>")
        dict_str = get_type_by_name("Dictionary<String, String>")
        
        assert not dict_int.is_compatible_with(dict_str)
    
    def test_generic_with_any_type(self):
        """Test that generic types are compatible with ANY_TYPE."""
        list_int = get_type_by_name("List<Integer>")
        
        assert list_int.is_compatible_with(ANY_TYPE)


class TestGenericTypeEdgeCases:
    """Test edge cases and special scenarios with generic types."""
    
    def test_triple_nested_generics(self):
        """Test List<List<List<Integer>>>."""
        triple_nested = get_type_by_name("List<List<List<Integer>>>")
        
        assert isinstance(triple_nested, ListType)
        assert isinstance(triple_nested.element_type, ListType)
        assert isinstance(triple_nested.element_type.element_type, ListType)
        assert triple_nested.element_type.element_type.element_type is INTEGER_TYPE
    
    def test_multiple_dictionary_nesting(self):
        """Test Dictionary<String, Dictionary<String, Integer>>."""
        nested_dict = get_type_by_name("Dictionary<String, Dictionary<String, Integer>>")
        
        assert isinstance(nested_dict, DictionaryType)
        assert nested_dict.key_type is STRING_TYPE
        assert isinstance(nested_dict.value_type, DictionaryType)
        assert nested_dict.value_type.key_type is STRING_TYPE
        assert nested_dict.value_type.value_type is INTEGER_TYPE
    
    def test_mixed_nesting(self):
        """Test List<Dictionary<String, List<Integer>>>."""
        mixed = get_type_by_name("List<Dictionary<String, List<Integer>>>")
        
        assert isinstance(mixed, ListType)
        assert isinstance(mixed.element_type, DictionaryType)
        assert isinstance(mixed.element_type.value_type, ListType)
        assert mixed.element_type.value_type.element_type is INTEGER_TYPE
    
    def test_case_insensitive_generic_types(self):
        """Test that generic type names are case-insensitive."""
        list1 = get_type_by_name("List<integer>")
        list2 = get_type_by_name("list<Integer>")
        list3 = get_type_by_name("LIST<INTEGER>")
        
        # All should create ListType with INTEGER_TYPE element
        assert isinstance(list1, ListType)
        assert isinstance(list2, ListType)
        assert isinstance(list3, ListType)
        assert list1.element_type is INTEGER_TYPE
        assert list2.element_type is INTEGER_TYPE
        assert list3.element_type is INTEGER_TYPE


class TestGenericClassesAndFunctions:
    """Test parsing of generic classes and function definitions."""
    
    def parse_code(self, code: str):
        """Helper to parse code."""
        lexer = Lexer(code)
        tokens = lexer.tokenize()
        parser = Parser(tokens)
        return parser.parse()
    
    def test_parse_generic_function_single_param(self):
        """Test parsing function with single generic type parameter."""
        code = "function identity<T> that takes value as T returns T\n    return value"
        program = self.parse_code(code)
        func = program.statements[0]
        
        assert func.name == "identity"
        assert func.type_parameters == ['T']
        assert func.return_type == "T"
    
    def test_parse_generic_function_multiple_params(self):
        """Test parsing function with multiple generic type parameters."""
        code = "function map<T, R> that takes items as List<T> and mapper as String returns List<R>\n    return items"
        program = self.parse_code(code)
        func = program.statements[0]
        
        assert func.name == "map"
        assert func.type_parameters == ['T', 'R']
        assert func.return_type == "List<R>"
    
    def test_parse_generic_class_single_param(self):
        """Test parsing class with single generic type parameter."""
        code = """class Container<T>
    pass"""
        program = self.parse_code(code)
        cls = program.statements[0]
        
        assert cls.name == "Container"
        assert cls.generic_parameters == ['T']
    
    def test_parse_generic_class_multiple_params(self):
        """Test parsing class with multiple generic type parameters."""
        code = """class Pair<K, V>
    pass"""
        program = self.parse_code(code)
        cls = program.statements[0]
        
        assert cls.name == "Pair"
        assert cls.generic_parameters == ['K', 'V']
    
    def test_parse_generic_class_with_inheritance(self):
        """Test parsing generic class that extends another class."""
        code = """class Node<T> extends BaseNode
    pass"""
        program = self.parse_code(code)
        cls = program.statements[0]
        
        assert cls.name == "Node"
        assert cls.generic_parameters == ['T']
        assert cls.parent_classes == ['BaseNode']
    
    def test_parse_nested_generic_in_function(self):
        """Test parsing function with nested generic types."""
        code = "function process<T> that takes matrix as List<List<T>> returns T\n    return matrix"
        program = self.parse_code(code)
        func = program.statements[0]
        
        assert func.name == "process"
        assert func.type_parameters == ['T']
        assert func.parameters[0].type_annotation == "List<List<T>>"


class TestGenericInstantiation:
    """Test parsing of generic class instantiation with type arguments."""
    
    def parse_code(self, code: str):
        """Helper to parse code."""
        lexer = Lexer(code)
        tokens = lexer.tokenize()
        parser = Parser(tokens)
        return parser.parse()
    
    def test_parse_simple_generic_instantiation(self):
        """Test parsing 'new Container<Integer>'."""
        code = "set obj to new Container<Integer>"
        program = self.parse_code(code)
        assignment = program.statements[0]
        
        assert isinstance(assignment.value, ObjectInstantiation)
        assert assignment.value.class_name == "Container"
        assert assignment.value.type_arguments == ["Integer"]
    
    def test_parse_generic_instantiation_with_args(self):
        """Test parsing 'new Container<String> with [args]'."""
        code = 'set obj to new Container<String> with ["initial"]'
        program = self.parse_code(code)
        assignment = program.statements[0]
        
        assert isinstance(assignment.value, ObjectInstantiation)
        assert assignment.value.class_name == "Container"
        assert assignment.value.type_arguments == ["String"]
        assert len(assignment.value.arguments) == 1
    
    def test_parse_multiple_type_arguments(self):
        """Test parsing 'new Pair<Integer, String>'."""
        code = "set obj to new Pair<Integer, String>"
        program = self.parse_code(code)
        assignment = program.statements[0]
        
        assert isinstance(assignment.value, ObjectInstantiation)
        assert assignment.value.class_name == "Pair"
        assert assignment.value.type_arguments == ["Integer", "String"]
    
    def test_parse_nested_generic_instantiation(self):
        """Test parsing 'new Container<List<Integer>>'."""
        code = "set obj to new Container<List<Integer>>"
        program = self.parse_code(code)
        assignment = program.statements[0]
        
        assert isinstance(assignment.value, ObjectInstantiation)
        assert assignment.value.class_name == "Container"
        # Nested generics are parsed as a single compound type string
        assert assignment.value.type_arguments == ["List<Integer>"]
    
    def test_parse_deeply_nested_generics(self):
        """Test parsing 'new Box<Dictionary<String, List<Integer>>>'."""
        code = "set obj to new Box<Dictionary<String, List<Integer>>>"
        program = self.parse_code(code)
        assignment = program.statements[0]
        
        assert isinstance(assignment.value, ObjectInstantiation)
        assert assignment.value.class_name == "Box"
        assert assignment.value.type_arguments == ["Dictionary<String, List<Integer>>"]
    
    def test_parse_generic_with_constructor_args(self):
        """Test parsing generic instantiation with both type args and constructor args."""
        code = 'set obj to new Map<String, Integer> with ["initial_key", 42]'
        program = self.parse_code(code)
        assignment = program.statements[0]
        
        assert isinstance(assignment.value, ObjectInstantiation)
        assert assignment.value.class_name == "Map"
        assert assignment.value.type_arguments == ["String", "Integer"]
        assert len(assignment.value.arguments) == 2
    
    def test_non_generic_instantiation_unchanged(self):
        """Test that non-generic instantiation still works."""
        code = "set obj to new Person with [name, age]"
        program = self.parse_code(code)
        assignment = program.statements[0]
        
        assert isinstance(assignment.value, ObjectInstantiation)
        assert assignment.value.class_name == "Person"
        assert assignment.value.type_arguments == []
        assert len(assignment.value.arguments) == 2


if __name__ == "__main__":
    pytest.main([__file__, "-v"])

