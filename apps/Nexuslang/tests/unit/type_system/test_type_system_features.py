"""
Test cases for type-system feature coverage in NexusLang.
"""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'src'))

import unittest
from nexuslang.parser.lexer import Lexer
from nexuslang.parser.parser import Parser
from nexuslang.parser.ast import (
    AbstractClassDefinition, AbstractMethodDefinition, TraitDefinition,
    TypeAliasDefinition, TypeParameter, TypeConstraint, TypeGuard,
    MethodDefinition, Identifier
)

class TestTypeSystemFeatures(unittest.TestCase):
    """Test cases for type-system feature coverage."""
    
    def test_abstract_class_definition(self):
        """Test parsing of abstract class definitions."""
        source = """
        Create an abstract class called Shape with:
            An abstract method called calculate_area that returns a float.
            A concrete method called get_name that returns a string.
        """
        
        lexer = Lexer(source)
        tokens = lexer.tokenize()
        # Debug tokens
        for i, token in enumerate(tokens):
            print(f"{i}: {token.type} - {token.lexeme} (line {token.line}, col {token.column})")
        parser = Parser(tokens)
        ast = parser.parse()
        
        # Verify the AST structure
        self.assertIsInstance(ast.statements[0], AbstractClassDefinition)
        self.assertEqual(ast.statements[0].name, "Shape")
        self.assertEqual(len(ast.statements[0].abstract_methods), 1)
        self.assertEqual(len(ast.statements[0].concrete_methods), 1)
        
        # Verify abstract method
        abstract_method = ast.statements[0].abstract_methods[0]
        self.assertIsInstance(abstract_method, AbstractMethodDefinition)
        self.assertEqual(abstract_method.name, "calculate_area")
        self.assertEqual(abstract_method.return_type, "float")
        
        # Verify concrete method
        concrete_method = ast.statements[0].concrete_methods[0]
        self.assertIsInstance(concrete_method, MethodDefinition)
        self.assertEqual(concrete_method.name, "get_name")
        self.assertEqual(concrete_method.return_type, "string")
    
    def test_trait_definition(self):
        """Test parsing of trait definitions."""
        source = """
        Create a trait called Comparable with:
            A required method called compare_to that takes another Comparable and returns an integer.
            A provided method called equals that takes another Comparable and returns a boolean.
        """
        
        lexer = Lexer(source)
        tokens = lexer.tokenize()
        parser = Parser(tokens)
        ast = parser.parse()
        
        # Verify the AST structure
        self.assertIsInstance(ast.statements[0], TraitDefinition)
        self.assertEqual(ast.statements[0].name, "Comparable")
        self.assertEqual(len(ast.statements[0].required_methods), 1)
        self.assertEqual(len(ast.statements[0].provided_methods), 1)
        
        # Verify required method
        required_method = ast.statements[0].required_methods[0]
        self.assertEqual(required_method.name, "compare_to")
        self.assertEqual(len(required_method.parameters), 1)
        self.assertEqual(required_method.return_type, "integer")
        
        # Verify provided method
        provided_method = ast.statements[0].provided_methods[0]
        self.assertEqual(provided_method.name, "equals")
        self.assertEqual(len(provided_method.parameters), 1)
        self.assertEqual(provided_method.return_type, "boolean")
    
    def test_type_alias_definition(self):
        """Test parsing of type alias definitions."""
        source = """
        Create a type alias called StringMap that is a dictionary with string keys and string values.
        Create a type alias called NumberList that is a list of integers.
        """
        
        lexer = Lexer(source)
        tokens = lexer.tokenize()
        parser = Parser(tokens)
        ast = parser.parse()
        
        # Verify the AST structure
        self.assertEqual(len(ast.statements), 2)
        self.assertIsInstance(ast.statements[0], TypeAliasDefinition)
        self.assertIsInstance(ast.statements[1], TypeAliasDefinition)
        
        # Verify first type alias
        self.assertEqual(ast.statements[0].name, "StringMap")
        self.assertEqual(ast.statements[0].target_type, "dictionary<string, string>")
        
        # Verify second type alias
        self.assertEqual(ast.statements[1].name, "NumberList")
        self.assertEqual(ast.statements[1].target_type, "list<integer>")
    
    def test_generic_type_parameters(self):
        """Test parsing of generic type parameters with constraints."""
        source = """
        Create a class called Container with a generic type parameter T that extends Comparable.
        Create a class called NumberContainer with a generic type parameter N that extends Number.
        """
        
        lexer = Lexer(source)
        tokens = lexer.tokenize()
        parser = Parser(tokens)
        ast = parser.parse()
        
        # Verify the AST structure
        self.assertEqual(len(ast.statements), 2)
        
        # Verify first class
        self.assertEqual(ast.statements[0].name, "Container")
        self.assertEqual(len(ast.statements[0].generic_parameters), 1)
        type_param = ast.statements[0].generic_parameters[0]
        self.assertIsInstance(type_param, TypeParameter)
        self.assertEqual(type_param.name, "T")
        self.assertEqual(len(type_param.bounds), 1)
        self.assertEqual(type_param.bounds[0], "Comparable")
        
        # Verify second class
        self.assertEqual(ast.statements[1].name, "NumberContainer")
        self.assertEqual(len(ast.statements[1].generic_parameters), 1)
        type_param = ast.statements[1].generic_parameters[0]
        self.assertIsInstance(type_param, TypeParameter)
        self.assertEqual(type_param.name, "N")
        self.assertEqual(len(type_param.bounds), 1)
        self.assertEqual(type_param.bounds[0], "Number")
    
    def test_type_guards(self):
        """Test parsing of type guards."""
        source = """
        If x is a string then
            Create an integer called length and set it to the length of x.
        End if.
        
        If y is a list then
            Create an integer called size and set it to the size of y.
        End if.
        """
        
        lexer = Lexer(source)
        tokens = lexer.tokenize()
        parser = Parser(tokens)
        ast = parser.parse()
        
        # Verify the AST structure
        self.assertEqual(len(ast.statements), 2)
        
        # Verify first type guard
        self.assertIsInstance(ast.statements[0], TypeGuard)
        self.assertIsInstance(ast.statements[0].condition, Identifier)
        self.assertEqual(ast.statements[0].condition.name, "x")
        self.assertEqual(ast.statements[0].type_name, "string")
        
        # Verify second type guard
        self.assertIsInstance(ast.statements[1], TypeGuard)
        self.assertIsInstance(ast.statements[1].condition, Identifier)
        self.assertEqual(ast.statements[1].condition.name, "y")
        self.assertEqual(ast.statements[1].type_name, "list")

if __name__ == '__main__':
    unittest.main() 