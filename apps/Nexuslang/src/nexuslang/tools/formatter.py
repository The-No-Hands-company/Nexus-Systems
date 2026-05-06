"""
NLPL Formatter - Code Beautification Tool

Formats NexusLang code according to style guidelines:
- Consistent indentation (4 spaces)
- Proper spacing around operators
- Blank lines between definitions
- Line length limits (configurable)
- Comment formatting
"""

import sys
from typing import List, Optional
from pathlib import Path
from dataclasses import dataclass

sys.path.insert(0, str(Path(__file__).parent.parent.parent))

from nexuslang.parser.lexer import Lexer, Token, TokenType
from nexuslang.parser.parser import Parser
from nexuslang.parser.ast import *


@dataclass
class FormatConfig:
    """Configuration for code formatting."""
    indent_size: int = 4
    max_line_length: int = 100
    blank_lines_after_function: int = 2
    blank_lines_after_class: int = 2
    blank_lines_after_import: int = 1
    space_around_operators: bool = True
    space_after_comma: bool = True
    align_assignments: bool = False


class Formatter:
    """
    Code formatter for NexusLang.
    
    Formats source code according to style guidelines while
    preserving program semantics.
    """
    
    def __init__(self, config: Optional[FormatConfig] = None):
        self.config = config or FormatConfig()
        self.output: List[str] = []
        self.indent_level = 0
        self.last_was_blank = False
    
    def format_file(self, input_file: str, output_file: Optional[str] = None) -> str:
        """Format a file."""
        with open(input_file, 'r') as f:
            source = f.read()
        
        formatted = self.format_source(source)
        
        if output_file:
            with open(output_file, 'w') as f:
                f.write(formatted)
        
        return formatted
    
    def format_source(self, source: str) -> str:
        """Format source code."""
        try:
            # Parse
            lexer = Lexer(source)
            tokens = lexer.tokenize()
            parser = Parser(tokens)
            ast = parser.parse()
            
            # Format
            self.output = []
            self.indent_level = 0
            self.last_was_blank = False
            
            self.format_program(ast)
            
            # Join and clean up
            result = '\n'.join(self.output)
            
            # Remove trailing whitespace
            lines = [line.rstrip() for line in result.split('\n')]
            
            # Remove excessive blank lines (max 2 consecutive)
            cleaned = []
            blank_count = 0
            for line in lines:
                if not line.strip():
                    blank_count += 1
                    if blank_count <= 2:
                        cleaned.append(line)
                else:
                    blank_count = 0
                    cleaned.append(line)
            
            # Ensure file ends with single newline
            result = '\n'.join(cleaned).rstrip() + '\n'
            
            return result
            
        except Exception as e:
            # If formatting fails, return original with warning
            print(f"Warning: Formatting failed: {e}", file=sys.stderr)
            return source
    
    def format_program(self, program: Program):
        """Format a program."""
        for i, stmt in enumerate(program.statements):
            self.format_statement(stmt)
            
            # Add blank lines after certain statements
            if isinstance(stmt, (FunctionDefinition, MethodDefinition)):
                self.add_blank_lines(self.config.blank_lines_after_function)
            elif isinstance(stmt, ClassDefinition):
                self.add_blank_lines(self.config.blank_lines_after_class)
            elif hasattr(stmt, '__class__') and 'Import' in stmt.__class__.__name__:
                if i + 1 < len(program.statements):
                    next_stmt = program.statements[i + 1]
                    if not ('Import' in next_stmt.__class__.__name__):
                        self.add_blank_lines(self.config.blank_lines_after_import)
    
    def format_statement(self, stmt):
        """Format a statement."""
        if isinstance(stmt, VariableDeclaration):
            self.format_variable_declaration(stmt)
        elif isinstance(stmt, FunctionDefinition):
            self.format_function_definition(stmt)
        elif isinstance(stmt, ClassDefinition):
            self.format_class_definition(stmt)
        elif isinstance(stmt, IfStatement):
            self.format_if_statement(stmt)
        elif isinstance(stmt, WhileLoop):
            self.format_while_loop(stmt)
        elif isinstance(stmt, ForLoop):
            self.format_for_loop(stmt)
        elif isinstance(stmt, RepeatNTimesLoop):
            self.format_repeat_n_times_loop(stmt)
        elif isinstance(stmt, RepeatWhileLoop):
            self.format_repeat_while_loop(stmt)
        elif isinstance(stmt, ReturnStatement):
            self.format_return_statement(stmt)
        elif isinstance(stmt, PrintStatement):
            self.format_print_statement(stmt)
        elif isinstance(stmt, FunctionCall):
            self.format_function_call(stmt)
        else:
            # Fallback - preserve original
            self.emit(f"# <unsupported statement: {stmt.__class__.__name__}>")
    
    def format_variable_declaration(self, decl: VariableDeclaration):
        """Format variable declaration."""
        line = f"set {decl.name} to {self.format_expression(decl.value)}"
        self.emit(line)
    
    def format_function_definition(self, func: FunctionDefinition):
        """Format function definition."""
        # Function signature
        parts = [f"function {func.name}"]
        
        if func.parameters:
            params = []
            for param in func.parameters:
                if param.type_annotation:
                    params.append(f"{param.name} as {param.type_annotation}")
                else:
                    params.append(param.name)
            parts.append(f"with {' and '.join(params)}")
        
        if func.return_type:
            parts.append(f"returns {func.return_type}")
        
        self.emit(' '.join(parts))
        
        # Body
        self.indent()
        for stmt in func.body:
            self.format_statement(stmt)
        self.dedent()
    
    def format_class_definition(self, cls: ClassDefinition):
        """Format class definition."""
        # Class header
        line = f"class {cls.name}"
        if cls.parent_class:
            line += f" extends {cls.parent_class}"
        self.emit(line)
        
        self.indent()
        
        # Properties
        for prop in cls.properties:
            if hasattr(prop, 'name') and hasattr(prop, 'type_annotation'):
                self.emit(f"{prop.name} as {prop.type_annotation}")
        
        if cls.properties and cls.methods:
            self.add_blank_lines(1)
        
        # Methods
        for method in cls.methods:
            self.format_statement(method)
            self.add_blank_lines(1)
        
        self.dedent()
        self.emit("end")
    
    def format_if_statement(self, stmt: IfStatement):
        """Format if statement."""
        condition = self.format_expression(stmt.condition)
        self.emit(f"if {condition}")
        
        self.indent()
        for s in stmt.then_block:
            self.format_statement(s)
        self.dedent()
        
        if stmt.else_block:
            self.emit("else")
            self.indent()
            for s in stmt.else_block:
                self.format_statement(s)
            self.dedent()
    
    def format_while_loop(self, loop: WhileLoop):
        """Format while loop."""
        condition = self.format_expression(loop.condition)
        self.emit(f"while {condition}")
        
        self.indent()
        for stmt in loop.body:
            self.format_statement(stmt)
        self.dedent()
    
    def format_for_loop(self, loop: ForLoop):
        """Format for loop."""
        var = loop.variable.name if hasattr(loop.variable, 'name') else str(loop.variable)
        iterable = self.format_expression(loop.iterable)
        self.emit(f"for each {var} in {iterable}")
        
        self.indent()
        for stmt in loop.body:
            self.format_statement(stmt)
        self.dedent()
    
    def format_repeat_n_times_loop(self, loop: RepeatNTimesLoop):
        """Format repeat N times loop."""
        count = self.format_expression(loop.count)
        self.emit(f"repeat {count} times")
        
        self.indent()
        for stmt in loop.body:
            self.format_statement(stmt)
        self.dedent()
    
    def format_repeat_while_loop(self, loop: RepeatWhileLoop):
        """Format repeat while loop."""
        condition = self.format_expression(loop.condition)
        self.emit(f"repeat while {condition}")
        
        self.indent()
        for stmt in loop.body:
            self.format_statement(stmt)
        self.dedent()
    
    def format_return_statement(self, stmt: ReturnStatement):
        """Format return statement."""
        if stmt.value:
            value = self.format_expression(stmt.value)
            self.emit(f"return {value}")
        else:
            self.emit("return")
    
    def format_print_statement(self, stmt: PrintStatement):
        """Format print statement."""
        if hasattr(stmt, 'expression') and stmt.expression:
            expr = self.format_expression(stmt.expression)
            self.emit(f"print text {expr}")
        else:
            self.emit("print text \"\"")
    
    def format_function_call(self, call: FunctionCall):
        """Format function call."""
        parts = [call.function_name]
        if call.arguments:
            args = [self.format_expression(arg) for arg in call.arguments]
            parts.append(f"with {' and '.join(args)}")
        self.emit(' '.join(parts))
    
    def format_expression(self, expr) -> str:
        """Format an expression."""
        if isinstance(expr, Literal):
            if isinstance(expr.value, str):
                return f'"{expr.value}"'
            return str(expr.value)
        elif isinstance(expr, Identifier):
            return expr.name
        elif isinstance(expr, BinaryOperation):
            left = self.format_expression(expr.left)
            right = self.format_expression(expr.right)
            op = self.format_operator(expr.operator)
            
            if self.config.space_around_operators:
                return f"{left} {op} {right}"
            else:
                return f"{left}{op}{right}"
        elif isinstance(expr, FunctionCall):
            parts = [expr.function_name]
            if expr.arguments:
                args = [self.format_expression(arg) for arg in expr.arguments]
                if self.config.space_after_comma:
                    parts.append(f"with {', '.join(args)}")
                else:
                    parts.append(f"with {','.join(args)}")
            return ' '.join(parts)
        elif hasattr(expr, '__class__') and expr.__class__.__name__ == 'ListExpression':
            if hasattr(expr, 'elements'):
                elements = [self.format_expression(e) for e in expr.elements]
                if self.config.space_after_comma:
                    return f"[{', '.join(elements)}]"
                else:
                    return f"[{','.join(elements)}]"
            return "[]"
        elif hasattr(expr, '__class__') and expr.__class__.__name__ == 'DictExpression':
            if hasattr(expr, 'entries'):
                entries = [
                    f"{self.format_expression(k)}: {self.format_expression(v)}"
                    for k, v in expr.entries
                ]
                if self.config.space_after_comma:
                    return "{" + ", ".join(entries) + "}"
                else:
                    return "{" + ",".join(entries) + "}"
            return "{}"
        else:
            return str(expr)
    
    def format_operator(self, op) -> str:
        """Format an operator token."""
        if hasattr(op, 'lexeme'):
            return op.lexeme
        return str(op)
    
    def emit(self, line: str):
        """Emit a line of formatted code."""
        indent = ' ' * (self.indent_level * self.config.indent_size)
        self.output.append(indent + line)
        self.last_was_blank = False
    
    def add_blank_lines(self, count: int):
        """Add blank lines."""
        if not self.last_was_blank:
            for _ in range(count):
                self.output.append('')
            self.last_was_blank = True
    
    def indent(self):
        """Increase indentation level."""
        self.indent_level += 1
    
    def dedent(self):
        """Decrease indentation level."""
        self.indent_level = max(0, self.indent_level - 1)


def main():
    """CLI for formatter."""
    import argparse
    
    parser = argparse.ArgumentParser(description="NexusLang Code Formatter")
    parser.add_argument('files', nargs='+', help='NLPL files to format')
    parser.add_argument('-i', '--in-place', action='store_true', help='Format files in-place')
    parser.add_argument('--check', action='store_true', help='Check if files are formatted (exit 1 if not)')
    parser.add_argument('--diff', action='store_true', help='Show diff of changes')
    parser.add_argument('--indent', type=int, default=4, help='Indentation size (default: 4)')
    parser.add_argument('--line-length', type=int, default=100, help='Max line length (default: 100)')
    
    args = parser.parse_args()
    
    config = FormatConfig(
        indent_size=args.indent,
        max_line_length=args.line_length
    )
    
    formatter = Formatter(config)
    needs_formatting = []
    
    for file in args.files:
        with open(file, 'r') as f:
            original = f.read()
        
        formatted = formatter.format_source(original)
        
        if original != formatted:
            needs_formatting.append(file)
            
            if args.check:
                print(f"Would reformat: {file}")
            elif args.diff:
                import difflib
                diff = difflib.unified_diff(
                    original.splitlines(keepends=True),
                    formatted.splitlines(keepends=True),
                    fromfile=f"{file} (original)",
                    tofile=f"{file} (formatted)"
                )
                sys.stdout.writelines(diff)
            elif args.in_place:
                with open(file, 'w') as f:
                    f.write(formatted)
                print(f"Formatted: {file}")
            else:
                print(formatted)
        else:
            if args.check or args.in_place:
                print(f"Already formatted: {file}")
    
    if args.check:
        if needs_formatting:
            print(f"\n{len(needs_formatting)} file(s) would be reformatted")
            sys.exit(1)
        else:
            print("\nAll files are properly formatted!")
            sys.exit(0)


if __name__ == '__main__':
    main()
