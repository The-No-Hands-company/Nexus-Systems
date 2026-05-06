"""
AST Symbol Extractor
====================

Walks the AST and extracts symbols into a SymbolTable.
Handles scope tracking and reference resolution.
"""

from typing import Optional
from ..parser.ast import *
from .symbol_table import SymbolTable, Symbol, SymbolKind, SymbolLocation


class ASTSymbolExtractor:
    """
    Extracts symbols from an AST and populates a SymbolTable.
    
    Features:
    - Scope-aware symbol extraction
    - Reference tracking
    - Support for all NexusLang constructs
    """
    
    def __init__(self, uri: str):
        self.uri = uri
        self.symbol_table = SymbolTable()
        self.current_class = None  # Track current class for methods
    
    def extract(self, program: Program) -> SymbolTable:
        """
        Extract all symbols from a program AST.
        
        Args:
            program: Root Program AST node
            
        Returns:
            SymbolTable with all extracted symbols
        """
        self.visit_program(program)
        return self.symbol_table
    
    def visit_program(self, node: Program):
        """Visit Program node and extract top-level symbols."""
        for statement in node.statements:
            self.visit_statement(statement)
    
    def visit_statement(self, node):
        """Dispatch statement to appropriate visitor."""
        if isinstance(node, FunctionDefinition):
            self.visit_function_definition(node)
        elif isinstance(node, ClassDefinition):
            self.visit_class_definition(node)
        elif isinstance(node, StructDefinition):
            self.visit_struct_definition(node)
        elif isinstance(node, UnionDefinition):
            self.visit_union_definition(node)
        elif isinstance(node, EnumDefinition):
            self.visit_enum_definition(node)
        elif isinstance(node, InterfaceDefinition):
            self.visit_interface_definition(node)
        elif isinstance(node, TraitDefinition):
            self.visit_trait_definition(node)
        elif isinstance(node, VariableDeclaration):
            self.visit_variable_declaration(node)
        elif isinstance(node, ImportStatement):
            self.visit_import_statement(node)
        elif isinstance(node, IfStatement):
            self.visit_if_statement(node)
        elif isinstance(node, WhileLoop):
            self.visit_while_loop(node)
        elif isinstance(node, ForLoop):
            self.visit_for_loop(node)
        elif hasattr(node, 'body') and isinstance(node.body, list):
            # Generic block with body
            for stmt in node.body:
                self.visit_statement(stmt)
    
    def visit_function_definition(self, node: FunctionDefinition):
        """Extract function symbol."""
        symbol = Symbol(
            name=node.name,
            kind=SymbolKind.FUNCTION,
            location=SymbolLocation(
                uri=self.uri,
                line=node.line_number or 0,
                column=0
            ),
            scope_level=self.symbol_table.current_scope.level,
            type_annotation=str(node.return_type) if node.return_type else None,
            is_exported=getattr(node, 'is_exported', False)
        )
        
        # Add to current class if inside a class
        if self.current_class:
            self.current_class.add_child(symbol)
        
        self.symbol_table.define_symbol(symbol)
        
        # Enter function scope
        self.symbol_table.enter_scope("function")
        
        # Add parameters as symbols
        for param in node.parameters:
            param_symbol = Symbol(
                name=param.name,
                kind=SymbolKind.VARIABLE,
                location=SymbolLocation(
                    uri=self.uri,
                    line=node.line_number or 0,
                    column=0
                ),
                scope_level=self.symbol_table.current_scope.level,
                type_annotation=str(param.type) if hasattr(param, 'type') and param.type else None
            )
            self.symbol_table.define_symbol(param_symbol)
        
        # Visit function body
        if node.body:
            for stmt in node.body:
                self.visit_statement(stmt)
        
        # Exit function scope
        self.symbol_table.exit_scope()
    
    def visit_class_definition(self, node):
        """Extract class symbol and its members."""
        symbol = Symbol(
            name=node.name,
            kind=SymbolKind.CLASS,
            location=SymbolLocation(
                uri=self.uri,
                line=getattr(node, 'line_number', 0),
                column=0
            ),
            scope_level=self.symbol_table.current_scope.level
        )
        
        self.symbol_table.define_symbol(symbol)
        
        # Enter class scope
        self.symbol_table.enter_scope("class")
        old_class = self.current_class
        self.current_class = symbol
        
        # Visit properties
        if hasattr(node, 'properties'):
            for prop in node.properties:
                self.visit_property(prop, symbol)
        
        # Visit methods
        if hasattr(node, 'methods'):
            for method in node.methods:
                self.visit_method(method, symbol)
        
        # Exit class scope
        self.current_class = old_class
        self.symbol_table.exit_scope()
    
    def visit_struct_definition(self, node):
        """Extract struct symbol and fields."""
        symbol = Symbol(
            name=node.name,
            kind=SymbolKind.STRUCT,
            location=SymbolLocation(
                uri=self.uri,
                line=getattr(node, 'line_number', 0),
                column=0
            ),
            scope_level=self.symbol_table.current_scope.level
        )
        
        self.symbol_table.define_symbol(symbol)
        
        # Enter struct scope
        self.symbol_table.enter_scope("struct")
        old_class = self.current_class
        self.current_class = symbol
        
        # Visit fields
        if hasattr(node, 'fields'):
            for field_def in node.fields:
                field_symbol = Symbol(
                    name=field_def.name,
                    kind=SymbolKind.FIELD,
                    location=SymbolLocation(
                        uri=self.uri,
                        line=0,
                        column=0
                    ),
                    scope_level=self.symbol_table.current_scope.level,
                    type_annotation=str(field_def.type) if hasattr(field_def, 'type') else None
                )
                symbol.add_child(field_symbol)
                self.symbol_table.define_symbol(field_symbol)
        
        # Visit methods if struct has them
        if hasattr(node, 'methods'):
            for method in node.methods:
                self.visit_method(method, symbol)
        
        # Exit struct scope
        self.current_class = old_class
        self.symbol_table.exit_scope()
    
    def visit_union_definition(self, node):
        """Extract union symbol."""
        symbol = Symbol(
            name=node.name,
            kind=SymbolKind.STRUCT,  # Treat union as struct-like
            location=SymbolLocation(
                uri=self.uri,
                line=getattr(node, 'line_number', 0),
                column=0
            ),
            scope_level=self.symbol_table.current_scope.level
        )
        
        self.symbol_table.define_symbol(symbol)
    
    def visit_enum_definition(self, node):
        """Extract enum symbol and members."""
        symbol = Symbol(
            name=node.name,
            kind=SymbolKind.ENUM,
            location=SymbolLocation(
                uri=self.uri,
                line=getattr(node, 'line_number', 0),
                column=0
            ),
            scope_level=self.symbol_table.current_scope.level
        )
        
        self.symbol_table.define_symbol(symbol)
        
        # Visit enum members
        if hasattr(node, 'members'):
            for member in node.members:
                member_name = member.name if hasattr(member, 'name') else str(member)
                member_symbol = Symbol(
                    name=member_name,
                    kind=SymbolKind.ENUM_MEMBER,
                    location=SymbolLocation(
                        uri=self.uri,
                        line=0,
                        column=0
                    ),
                    scope_level=self.symbol_table.current_scope.level
                )
                symbol.add_child(member_symbol)
                self.symbol_table.define_symbol(member_symbol)
    
    def visit_interface_definition(self, node):
        """Extract interface symbol."""
        symbol = Symbol(
            name=node.name,
            kind=SymbolKind.INTERFACE,
            location=SymbolLocation(
                uri=self.uri,
                line=getattr(node, 'line_number', 0),
                column=0
            ),
            scope_level=self.symbol_table.current_scope.level
        )
        
        self.symbol_table.define_symbol(symbol)
    
    def visit_trait_definition(self, node):
        """Extract trait symbol."""
        symbol = Symbol(
            name=node.name,
            kind=SymbolKind.INTERFACE,  # Treat trait as interface-like
            location=SymbolLocation(
                uri=self.uri,
                line=getattr(node, 'line_number', 0),
                column=0
            ),
            scope_level=self.symbol_table.current_scope.level
        )
        
        self.symbol_table.define_symbol(symbol)
    
    def visit_variable_declaration(self, node: VariableDeclaration):
        """Extract variable symbol."""
        symbol = Symbol(
            name=node.name,
            kind=SymbolKind.VARIABLE,
            location=SymbolLocation(
                uri=self.uri,
                line=0,
                column=0
            ),
            scope_level=self.symbol_table.current_scope.level,
            type_annotation=str(node.type_annotation) if node.type_annotation else None
        )
        
        self.symbol_table.define_symbol(symbol)
    
    def visit_property(self, node, parent_symbol: Symbol):
        """Extract property symbol."""
        prop_symbol = Symbol(
            name=node.name,
            kind=SymbolKind.PROPERTY,
            location=SymbolLocation(
                uri=self.uri,
                line=0,
                column=0
            ),
            scope_level=self.symbol_table.current_scope.level,
            type_annotation=str(node.type) if hasattr(node, 'type') else None
        )
        
        parent_symbol.add_child(prop_symbol)
        self.symbol_table.define_symbol(prop_symbol)
    
    def visit_method(self, node, parent_symbol: Symbol):
        """Extract method symbol."""
        method_symbol = Symbol(
            name=node.name,
            kind=SymbolKind.METHOD,
            location=SymbolLocation(
                uri=self.uri,
                line=getattr(node, 'line_number', 0),
                column=0
            ),
            scope_level=self.symbol_table.current_scope.level,
            type_annotation=str(node.return_type) if hasattr(node, 'return_type') and node.return_type else None
        )
        
        parent_symbol.add_child(method_symbol)
        self.symbol_table.define_symbol(method_symbol)
        
        # Enter method scope
        self.symbol_table.enter_scope("function")
        
        # Add parameters
        if hasattr(node, 'parameters'):
            for param in node.parameters:
                param_symbol = Symbol(
                    name=param.name,
                    kind=SymbolKind.VARIABLE,
                    location=SymbolLocation(
                        uri=self.uri,
                        line=0,
                        column=0
                    ),
                    scope_level=self.symbol_table.current_scope.level
                )
                self.symbol_table.define_symbol(param_symbol)
        
        # Visit method body
        if hasattr(node, 'body') and node.body:
            for stmt in node.body:
                self.visit_statement(stmt)
        
        # Exit method scope
        self.symbol_table.exit_scope()
    
    def visit_import_statement(self, node):
        """Extract import symbol."""
        # Import creates symbols for imported names
        if hasattr(node, 'module_name'):
            symbol = Symbol(
                name=node.module_name,
                kind=SymbolKind.MODULE,
                location=SymbolLocation(
                    uri=self.uri,
                    line=0,
                    column=0
                ),
                scope_level=self.symbol_table.current_scope.level
            )
            self.symbol_table.define_symbol(symbol)
    
    def visit_if_statement(self, node: IfStatement):
        """Visit if statement and its blocks."""
        # Visit then block
        self.symbol_table.enter_scope("block")
        for stmt in node.then_block:
            self.visit_statement(stmt)
        self.symbol_table.exit_scope()
        
        # Visit else block if present
        if node.else_block:
            self.symbol_table.enter_scope("block")
            for stmt in node.else_block:
                self.visit_statement(stmt)
            self.symbol_table.exit_scope()
    
    def visit_while_loop(self, node):
        """Visit while loop body."""
        if hasattr(node, 'body'):
            self.symbol_table.enter_scope("block")
            for stmt in node.body:
                self.visit_statement(stmt)
            self.symbol_table.exit_scope()
    
    def visit_for_loop(self, node):
        """Visit for loop and extract iterator variable."""
        self.symbol_table.enter_scope("block")
        
        # Extract iterator variable
        if hasattr(node, 'variable'):
            var_symbol = Symbol(
                name=node.variable,
                kind=SymbolKind.VARIABLE,
                location=SymbolLocation(
                    uri=self.uri,
                    line=getattr(node, 'line_number', 0),
                    column=0
                ),
                scope_level=self.symbol_table.current_scope.level
            )
            self.symbol_table.define_symbol(var_symbol)
        
        # Visit loop body
        if hasattr(node, 'body'):
            for stmt in node.body:
                self.visit_statement(stmt)
        
        self.symbol_table.exit_scope()


__all__ = ['ASTSymbolExtractor']
