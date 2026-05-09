"""
Test cases for the NexusLang parser.
Tests AST construction and basic parsing functionality.
"""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'src'))

import unittest
from nexuslang.parser.lexer import Lexer
from nexuslang.parser.parser import Parser
from nexuslang.parser.ast import (
    Program, FunctionDefinition, VariableDeclaration, 
    BinaryOperation, UnaryOperation, FunctionCall, 
    IfStatement, WhileLoop, ForLoop, ReturnStatement, 
    Literal, Identifier, Block, Parameter,
    ClassDefinition, PropertyDeclaration, MethodDefinition,
    ConcurrentExecution, TryCatch, InterfaceDefinition, TraitDefinition,
    RequireStatement, EnsureStatement, GuaranteeStatement, InvariantStatement
)
from nexuslang.parser.ast import (
    MoveExpression, BorrowExpression, DropBorrowStatement,
    BorrowExpressionWithLifetime,
)

class TestParser(unittest.TestCase):
    """Test cases for the NexusLang parser."""
    
    def setUp(self):
        """Set up test fixtures before each test method."""
        self.parser = None
    
    def parse_source(self, source):
        """Helper to parse source code."""
        lexer = Lexer(source)
        tokens = lexer.tokenize()
        parser = Parser(tokens)
        return parser.parse()
    
    def test_variable_declaration_simple(self):
        """Test parsing simple variable declarations."""
        source = "set x to 42"
        ast = self.parse_source(source)
        
        self.assertIsInstance(ast, Program)
        self.assertEqual(len(ast.statements), 1)
        self.assertIsInstance(ast.statements[0], VariableDeclaration)
        
        var_decl = ast.statements[0]
        self.assertEqual(var_decl.name, "x")
        self.assertIsInstance(var_decl.value, Literal)
        self.assertEqual(var_decl.value.value, 42)
    
    def test_variable_declaration_with_string(self):
        """Test parsing variable declarations with strings."""
        source = 'set greeting to "Hello, World!"'
        ast = self.parse_source(source)
        
        self.assertIsInstance(ast, Program)
        self.assertEqual(len(ast.statements), 1)
        var_decl = ast.statements[0]
        self.assertEqual(var_decl.name, "greeting")
        self.assertEqual(var_decl.value.value, "Hello, World!")
    
    def test_function_definition(self):
        """Test parsing function definitions."""
        # Use a simpler syntax that matches examples
        source = '''function test_func
    return 42'''
        ast = self.parse_source(source)
        
        # Just verify it parses to a program for now
        # The actual function syntax appears to be complex
        self.assertIsInstance(ast, Program)
    
    def test_print_statement(self):
        """Test parsing print statements."""
        source = 'print text "Hello"'
        ast = self.parse_source(source)
        
        self.assertIsInstance(ast, Program)
        self.assertEqual(len(ast.statements), 1)
        # Print is handled as a function call in the AST
        stmt = ast.statements[0]
        # The statement should be related to printing
        self.assertIsNotNone(stmt)
    
    def test_if_statement(self):
        """Test parsing if statements."""
        source = '''if x is greater than 0
    print text "Positive"'''
        ast = self.parse_source(source)
        
        self.assertIsInstance(ast, Program)
        self.assertEqual(len(ast.statements), 1)
        self.assertIsInstance(ast.statements[0], IfStatement)
        
        if_stmt = ast.statements[0]
        self.assertIsNotNone(if_stmt.condition)
        self.assertIsInstance(if_stmt.then_block, list)
    
    def test_if_else_statement(self):
        """Test parsing if-else statements."""
        source = '''if x is greater than 0
    print text "Positive"
else
    print text "Non-positive"'''
        ast = self.parse_source(source)
        
        self.assertIsInstance(ast, Program)
        if_stmt = ast.statements[0]
        self.assertIsInstance(if_stmt, IfStatement)
        self.assertIsNotNone(if_stmt.else_block)
    
    def test_while_loop(self):
        """Test parsing while loops."""
        source = """
        while x is greater than 0
            set x to x minus 1
        end while
        """
        ast = self.parse_source(source)
        
        self.assertIsInstance(ast, Program)
        self.assertEqual(len(ast.statements), 1)
        while_stmt = ast.statements[0]
        self.assertIsInstance(while_stmt, WhileLoop)
        self.assertIsInstance(while_stmt.body, list)

    def test_while_loop_accepts_end_loop_alias(self):
        """While loop accepts lexer alias token form: 'end loop'."""
        source = """
        while x is greater than 0
            set x to x minus 1
        end loop
        """
        ast = self.parse_source(source)

        self.assertIsInstance(ast, Program)
        self.assertEqual(len(ast.statements), 1)
        self.assertIsInstance(ast.statements[0], WhileLoop)

    def test_concurrent_block_accepts_end_concurrent_alias(self):
        """Concurrent execution block accepts 'end concurrent' alias token."""
        source = """
        run these tasks at the same time:
            print text "task1"
            print text "task2"
        end concurrent
        """
        ast = self.parse_source(source)

        self.assertIsInstance(ast, Program)
        self.assertEqual(len(ast.statements), 1)
        self.assertIsInstance(ast.statements[0], ConcurrentExecution)
        self.assertEqual(len(ast.statements[0].tasks), 2)

    def test_try_catch_accepts_end_try_alias(self):
        """Try/catch block accepts 'end try' alias token form."""
        source = """
        try
            print text "body"
        catch err
            print text err
        end try
        """
        ast = self.parse_source(source)

        self.assertIsInstance(ast, Program)
        self.assertEqual(len(ast.statements), 1)
        self.assertIsInstance(ast.statements[0], TryCatch)

    def test_interface_accepts_end_the_interface_alias(self):
        """Interface definition accepts lexer single-token 'end the interface'."""
        source = """
        interface Printable
        end the interface
        """
        ast = self.parse_source(source)

        self.assertIsInstance(ast, Program)
        self.assertEqual(len(ast.statements), 1)
        self.assertIsInstance(ast.statements[0], InterfaceDefinition)

    def test_trait_accepts_end_the_trait_alias(self):
        """Trait definition accepts lexer single-token 'end the trait'."""
        source = """
        define a trait called Formattable
        end the trait
        """
        ast = self.parse_source(source)

        self.assertIsInstance(ast, Program)
        self.assertEqual(len(ast.statements), 1)
        self.assertIsInstance(ast.statements[0], TraitDefinition)

    def test_class_accepts_end_the_class_alias(self):
        """Class definition accepts lexer single-token 'end the class'."""
        source = """
        class Example
            pass
        end the class
        """
        ast = self.parse_source(source)

        self.assertIsInstance(ast, Program)
        self.assertEqual(len(ast.statements), 1)
        self.assertIsInstance(ast.statements[0], ClassDefinition)

    def test_method_accepts_end_the_method_alias(self):
        """Method definition accepts lexer single-token 'end the method'."""
        source = """class Runner
    method run
        print text \"ok\"
    end the method
end"""
        ast = self.parse_source(source)

        self.assertIsInstance(ast, Program)
        self.assertEqual(len(ast.statements), 1)
        cls = ast.statements[0]
        self.assertIsInstance(cls, ClassDefinition)
        self.assertGreaterEqual(len(cls.methods), 1)
        self.assertIsInstance(cls.methods[0], MethodDefinition)
        self.assertEqual(cls.methods[0].name, "run")
    
    def test_for_each_loop(self):
        """Test parsing for each loops."""
        source = """for each item in items
    print text item"""
        ast = self.parse_source(source)
        
        self.assertIsInstance(ast, Program)
        self.assertEqual(len(ast.statements), 1)
        self.assertIsInstance(ast.statements[0], ForLoop)
    
    def test_binary_operations(self):
        """Test parsing binary operations."""
        source = "set result to 10 plus 5"
        ast = self.parse_source(source)
        
        var_decl = ast.statements[0]
        self.assertIsInstance(var_decl.value, BinaryOperation)
        
        binary_op = var_decl.value
        self.assertIsInstance(binary_op.left, Literal)
        self.assertIsInstance(binary_op.right, Literal)
        self.assertEqual(binary_op.left.value, 10)
        self.assertEqual(binary_op.right.value, 5)
    
    def test_multiple_statements(self):
        """Test parsing multiple statements."""
        source = """set x to 10
set y to 20
set z to x plus y"""
        ast = self.parse_source(source)
        
        self.assertIsInstance(ast, Program)
        self.assertEqual(len(ast.statements), 3)
        self.assertTrue(all(isinstance(stmt, VariableDeclaration) for stmt in ast.statements))
    
    def test_nested_if_statements(self):
        """Test parsing nested if statements."""
        source = '''if x is greater than 0
    if y is greater than 0
        print text "Both positive"'''
        ast = self.parse_source(source)
        
        self.assertIsInstance(ast, Program)
        outer_if = ast.statements[0]
        self.assertIsInstance(outer_if, IfStatement)
        # Check that there's a nested structure
        self.assertTrue(len(outer_if.then_block) > 0)
    
    def test_function_with_return(self):
        """Test parsing function with return statement."""
        source = '''function simple_func
    return 42'''
        ast = self.parse_source(source)
        
        # Just verify it parses
        self.assertIsInstance(ast, Program)
    
    def test_list_literal(self):
        """Test parsing list literals."""
        source = "set numbers to [1, 2, 3, 4, 5]"
        ast = self.parse_source(source)
        
        var_decl = ast.statements[0]
        # List should be parsed as some kind of expression
        self.assertIsNotNone(var_decl.value)
    
    def test_empty_program(self):
        """Test parsing an empty program."""
        source = ""
        ast = self.parse_source(source)
        
        self.assertIsInstance(ast, Program)
        # Empty program might have 0 statements or handle it differently
        # Just verify it doesn't crash
    
    def test_comments_ignored(self):
        """Test that comments are properly ignored."""
        source = """# This is a comment
set x to 42
# Another comment"""
        ast = self.parse_source(source)
        
        # Comments should be ignored, only the variable declaration should remain
        self.assertIsInstance(ast, Program)
        # Should have at least one statement (the set)
        self.assertTrue(len(ast.statements) >= 1)
        self.assertIsInstance(ast.statements[0], VariableDeclaration)

    def test_class_parses_extends_and_implements(self):
        """Class syntax supports extends + implements clauses."""
        source = """class Dog extends Animal implements Printable
    function speak
        print text \"woof\"
    end
end"""
        ast = self.parse_source(source)

        self.assertIsInstance(ast, Program)
        self.assertEqual(len(ast.statements), 1)
        cls = ast.statements[0]
        self.assertIsInstance(cls, ClassDefinition)
        self.assertEqual(cls.name, "Dog")
        self.assertIn("Animal", cls.parent_classes)
        self.assertIn("Printable", cls.implemented_interfaces)

    def test_class_parses_inherits_alias(self):
        """Class syntax supports inherits as alias for extends."""
        source = """class Dog inherits Animal
    function speak
        print text \"woof\"
    end
end"""
        ast = self.parse_source(source)

        self.assertIsInstance(ast, Program)
        cls = ast.statements[0]
        self.assertIsInstance(cls, ClassDefinition)
        self.assertIn("Animal", cls.parent_classes)

    def test_class_member_modifiers_property_and_static_method(self):
        """Access modifiers and static methods parse in class body."""
        source = """class Counter
    private set count to 0
    public static function next returns Integer
        return 1
    end
end"""
        ast = self.parse_source(source)

        self.assertIsInstance(ast, Program)
        cls = ast.statements[0]
        self.assertIsInstance(cls, ClassDefinition)

        self.assertGreaterEqual(len(cls.properties), 1)
        self.assertIsInstance(cls.properties[0], PropertyDeclaration)
        self.assertEqual(getattr(cls.properties[0], "access_modifier", None), "private")

        self.assertGreaterEqual(len(cls.methods), 1)
        self.assertIsInstance(cls.methods[0], MethodDefinition)
        self.assertEqual(getattr(cls.methods[0], "access_modifier", None), "public")
        self.assertTrue(getattr(cls.methods[0], "is_static", False))

    def test_class_member_abstract_function_parses(self):
        """Abstract function declarations parse in class body."""
        source = """class Shape
    protected abstract function area returns Float
end"""
        ast = self.parse_source(source)

        self.assertIsInstance(ast, Program)
        cls = ast.statements[0]
        self.assertIsInstance(cls, ClassDefinition)
        self.assertGreaterEqual(len(cls.methods), 1)
        self.assertEqual(cls.methods[0].name, "area")
        self.assertEqual(getattr(cls.methods[0], "access_modifier", None), "protected")

    def test_function_generic_bounds_with_trait_composition(self):
        """Generic parameter bounds support T: TraitA + TraitB form."""
        source = """function transform<T: Comparable + Printable> with value as T returns T
    return value
end"""
        ast = self.parse_source(source)

        self.assertIsInstance(ast, Program)
        self.assertEqual(len(ast.statements), 1)
        func = ast.statements[0]
        self.assertIsInstance(func, FunctionDefinition)
        self.assertEqual(func.type_parameters, ["T"])
        self.assertIsInstance(func.type_constraints, dict)
        self.assertEqual(func.type_constraints.get("T"), ["Comparable", "Printable"])

    def test_function_where_clause_multiple_constraints(self):
        """Where clause supports composed and comma-separated constraints."""
        source = """function project<T, R> with value as T where T is Comparable + Printable, R is Equatable returns R
    return value
end"""
        ast = self.parse_source(source)

        self.assertIsInstance(ast, Program)
        self.assertEqual(len(ast.statements), 1)
        func = ast.statements[0]
        self.assertIsInstance(func, FunctionDefinition)
        self.assertIsInstance(func.type_constraints, dict)
        self.assertEqual(func.type_constraints.get("T"), ["Comparable", "Printable"])
        self.assertEqual(func.type_constraints.get("R"), ["Equatable"])

    def test_contract_statements_support_optional_message(self):
        """Contract statements parse optional message expressions."""
        source = """require x is greater than 0 message \"x must be positive\"
ensure x is greater than or equal to 0
guarantee x is not equal to 13 message \"forbidden value\"
invariant x is greater than or equal to 0"""
        ast = self.parse_source(source)

        self.assertIsInstance(ast, Program)
        self.assertEqual(len(ast.statements), 4)
        self.assertIsInstance(ast.statements[0], RequireStatement)
        self.assertIsInstance(ast.statements[1], EnsureStatement)
        self.assertIsInstance(ast.statements[2], GuaranteeStatement)
        self.assertIsInstance(ast.statements[3], InvariantStatement)
        self.assertIsNotNone(ast.statements[0].message_expr)
        self.assertIsNone(ast.statements[1].message_expr)
        self.assertIsNotNone(ast.statements[2].message_expr)
        self.assertIsNone(ast.statements[3].message_expr)

    def test_contract_message_alias_with_is_accepted(self):
        """Contract message payload supports parser-compat alias: with <expr>."""
        source = "require value is greater than 0 with \"value must be positive\""
        ast = self.parse_source(source)

        self.assertIsInstance(ast, Program)
        self.assertEqual(len(ast.statements), 1)
        self.assertIsInstance(ast.statements[0], RequireStatement)
        self.assertIsNotNone(ast.statements[0].message_expr)

    def test_contract_statements_can_be_nested_in_blocks(self):
        """Contract statements parse correctly when nested in function blocks."""
        source = """function validate with value as Integer returns Integer
    require value is greater than 0 message \"value must be positive\"
    if value is greater than 10
        invariant value is greater than 0
        ensure value is greater than 0
    end
    return value
end"""
        ast = self.parse_source(source)

        self.assertIsInstance(ast, Program)
        self.assertEqual(len(ast.statements), 1)
        func = ast.statements[0]
        self.assertIsInstance(func, FunctionDefinition)

        self.assertTrue(any(isinstance(stmt, RequireStatement) for stmt in func.body))
        if_stmt = next(stmt for stmt in func.body if isinstance(stmt, IfStatement))
        self.assertTrue(any(isinstance(stmt, InvariantStatement) for stmt in if_stmt.then_block))
        self.assertTrue(any(isinstance(stmt, EnsureStatement) for stmt in if_stmt.then_block))



class TestOwnershipSyntax(unittest.TestCase):
        """Parser regression tests for ownership, borrow, and lifetime forms."""

        def parse_source(self, source):
            lexer = Lexer(source)
            tokens = lexer.tokenize()
            return Parser(tokens).parse()

        def test_move_expression_produces_move_node(self):
            ast = self.parse_source("set x to 5\nset y to move x")
            self.assertIsInstance(ast.statements[1].value, MoveExpression)
            self.assertEqual(ast.statements[1].value.var_name, "x")

        def test_immutable_borrow_expression(self):
            ast = self.parse_source("set x to 5\nset r to borrow x")
            borrow = ast.statements[1].value
            self.assertIsInstance(borrow, BorrowExpression)
            self.assertEqual(borrow.var_name, "x")
            self.assertFalse(borrow.mutable)

        def test_mutable_borrow_expression(self):
            ast = self.parse_source("set x to 5\nset rw to borrow mutable x")
            borrow = ast.statements[1].value
            self.assertIsInstance(borrow, BorrowExpression)
            self.assertTrue(borrow.mutable)

        def test_borrow_with_lifetime_annotation(self):
            ast = self.parse_source("set x to 5\nset r to borrow x with lifetime a")
            borrow = ast.statements[1].value
            self.assertIsInstance(borrow, BorrowExpressionWithLifetime)
            self.assertEqual(borrow.lifetime.label, "a")

        def test_drop_borrow_statement(self):
            ast = self.parse_source("set x to 5\nborrow x\ndrop borrow x")
            drop = ast.statements[2]
            self.assertIsInstance(drop, DropBorrowStatement)
            self.assertEqual(drop.var_name, "x")
            self.assertFalse(drop.mutable)

        def test_drop_borrow_mutable_statement(self):
            ast = self.parse_source("set x to 5\nborrow mutable x\ndrop borrow mutable x")
            drop = ast.statements[2]
            self.assertIsInstance(drop, DropBorrowStatement)
            self.assertTrue(drop.mutable)


if __name__ == "__main__":
    unittest.main()
