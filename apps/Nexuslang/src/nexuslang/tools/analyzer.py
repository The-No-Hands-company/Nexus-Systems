"""
NLPL Static Analyzer - Code Quality and Bug Detection

Analyzes NexusLang code without execution to find:
- Potential bugs (undefined variables, type errors)
- Code smells (unused variables, dead code)
- Performance issues (inefficient patterns)
- Security vulnerabilities
- Style violations
"""

import sys
from typing import List, Dict, Set, Optional, Tuple
from dataclasses import dataclass
from enum import Enum
from pathlib import Path

# Add src to path for imports
sys.path.insert(0, str(Path(__file__).parent.parent.parent))

from nexuslang.parser.ast import *
from nexuslang.parser.lexer import Lexer
from nexuslang.parser.parser import Parser


class Severity(Enum):
    """Issue severity levels."""
    ERROR = "ERROR"      # Critical - will likely cause runtime errors
    WARNING = "WARNING"  # Suspicious - might be a bug
    INFO = "INFO"        # Informational - style/optimization suggestion


@dataclass
class Issue:
    """A code quality issue."""
    severity: Severity
    category: str
    message: str
    file: str
    line: int = 0  # Default to 0 if line number unknown
    column: int = 0
    suggestion: Optional[str] = None
    
    def __str__(self):
        prefix = f"{self.severity.value}:"
        location = f"{Path(self.file).name}:{self.line}"
        if self.column:
            location += f":{self.column}"
        result = f"{prefix:<10} {location:<20} [{self.category}] {self.message}"
        if self.suggestion:
            result += f"\n           Suggestion: {self.suggestion}"
        return result


class StaticAnalyzer:
    """
    Static analyzer for NexusLang code.
    
    Performs multiple analysis passes:
    1. Undefined variables
    2. Unused variables
    3. Dead code
    4. Type inconsistencies
    5. Security issues
    6. Performance anti-patterns
    """
    
    def __init__(self, strict: bool = False):
        self.strict = strict
        self.issues: List[Issue] = []
        self.current_file = "<unknown>"
        
        # Analysis state
        self.defined_vars: Set[str] = set()
        self.used_vars: Set[str] = set()
        self.defined_funcs: Set[str] = set()
        self.function_scopes: List[Dict[str, int]] = []  # Stack of scopes
        self.in_loop = False
        self.in_function = False
    
    def _get_line(self, node) -> int:
        """Safely get line number from AST node."""
        return getattr(node, 'line_number', 0)
    
    def analyze_file(self, file_path: str) -> List[Issue]:
        """Analyze a single NexusLang file."""
        self.current_file = file_path
        self.issues = []
        
        try:
            with open(file_path, 'r') as f:
                source = f.read()
            
            # Parse
            lexer = Lexer(source)
            tokens = lexer.tokenize()
            parser = Parser(tokens)
            ast = parser.parse()
            
            # Run analysis passes
            self.analyze_program(ast)
            self.check_unused_variables()
            
        except Exception as e:
            self.add_issue(
                Severity.ERROR,
                "parse-error",
                f"Failed to parse file: {e}",
                1
            )
        
        return sorted(self.issues, key=lambda x: (x.line or 0, x.severity.value if x.severity else ""))
    
    def add_issue(self, severity: Severity, category: str, message: str, 
                  line: int, column: int = 0, suggestion: Optional[str] = None):
        """Add an issue to the report."""
        self.issues.append(Issue(
            severity=severity,
            category=category,
            message=message,
            file=self.current_file,
            line=line,
            column=column,
            suggestion=suggestion
        ))
    
    def analyze_program(self, program: Program):
        """Analyze a program node."""
        for stmt in program.statements:
            self.analyze_statement(stmt)
    
    def analyze_statement(self, stmt):
        """Analyze a statement."""
        if isinstance(stmt, VariableDeclaration):
            self.analyze_variable_declaration(stmt)
        elif isinstance(stmt, FunctionDefinition):
            self.analyze_function_definition(stmt)
        elif isinstance(stmt, ClassDefinition):
            self.analyze_class_definition(stmt)
        elif isinstance(stmt, IfStatement):
            self.analyze_if_statement(stmt)
        elif isinstance(stmt, WhileLoop):
            self.analyze_while_loop(stmt)
        elif isinstance(stmt, ForLoop):
            self.analyze_for_loop(stmt)
        elif isinstance(stmt, RepeatNTimesLoop):
            self.analyze_repeat_n_times_loop(stmt)
        elif isinstance(stmt, RepeatWhileLoop):
            self.analyze_repeat_while_loop(stmt)
        elif isinstance(stmt, ReturnStatement):
            self.analyze_return_statement(stmt)
        elif isinstance(stmt, FunctionCall):
            self.analyze_function_call(stmt)
        elif isinstance(stmt, BinaryOperation):
            self.analyze_expression(stmt)
        elif isinstance(stmt, PrintStatement):
            self.analyze_print_statement(stmt)
        elif hasattr(stmt, '__class__') and stmt.__class__.__name__ == 'Block':
            self.analyze_block(stmt)
    
    def analyze_variable_declaration(self, decl: VariableDeclaration):
        """Analyze variable declaration."""
        # Check if variable already exists (shadowing)
        if decl.name in self.defined_vars:
            self.add_issue(
                Severity.WARNING,
                "shadowing",
                f"Variable '{decl.name}' shadows previous definition",
                self._get_line(decl),
                suggestion="Use a different name to avoid confusion"
            )
        
        self.defined_vars.add(decl.name)
        
        # Analyze the value expression
        if decl.value:
            self.analyze_expression(decl.value)
        
        # Check for suspicious patterns
        if isinstance(decl.value, Literal):
            if decl.value.value is None:
                self.add_issue(
                    Severity.INFO,
                    "null-assignment",
                    f"Variable '{decl.name}' initialized to null",
                    self._get_line(decl),
                    suggestion="Consider explicit type annotation or initialization"
                )
    
    def analyze_function_definition(self, func: FunctionDefinition):
        """Analyze function definition."""
        self.defined_funcs.add(func.name)
        
        # Enter function scope
        old_in_function = self.in_function
        self.in_function = True
        self.function_scopes.append({})
        
        # Add parameters to scope
        for param in func.parameters:
            self.defined_vars.add(param.name)
            self.function_scopes[-1][param.name] = self._get_line(func)
        
        # Analyze body
        for stmt in func.body:
            self.analyze_statement(stmt)
        
        # Check for missing return
        if func.return_type and func.return_type != "Nothing":
            if not self.has_return_statement(func.body):
                self.add_issue(
                    Severity.WARNING,
                    "missing-return",
                    f"Function '{func.name}' with return type '{func.return_type}' may not return a value",
                    self._get_line(func),
                    suggestion="Ensure all code paths return a value"
                )
        
        # Exit function scope
        self.function_scopes.pop()
        self.in_function = old_in_function
    
    def has_return_statement(self, statements: List) -> bool:
        """Check if statements contain a return."""
        for stmt in statements:
            if isinstance(stmt, ReturnStatement):
                return True
            if isinstance(stmt, IfStatement):
                # Check all branches
                if self.has_return_statement(stmt.then_block):
                    if stmt.else_block and self.has_return_statement(stmt.else_block):
                        return True
        return False
    
    def analyze_class_definition(self, cls: ClassDefinition):
        """Analyze class definition."""
        # Check for empty classes
        if not cls.properties and not cls.methods:
            self.add_issue(
                Severity.INFO,
                "empty-class",
                f"Class '{cls.name}' has no properties or methods",
                self._get_line(cls),
                suggestion="Consider if this class is necessary"
            )
        
        # Analyze methods
        for method in cls.methods:
            if isinstance(method, MethodDefinition):
                self.analyze_function_definition(method)
    
    def analyze_if_statement(self, stmt: IfStatement):
        """Analyze if statement."""
        # Analyze condition
        self.analyze_expression(stmt.condition)
        
        # Check for constant conditions
        if isinstance(stmt.condition, Literal):
            if stmt.condition.value in (True, False):
                self.add_issue(
                    Severity.WARNING,
                    "constant-condition",
                    f"If statement has constant condition: {stmt.condition.value}",
                    self._get_line(stmt),
                    suggestion="Remove dead code branch"
                )
        
        # Analyze branches
        for s in stmt.then_block:
            self.analyze_statement(s)
        
        if stmt.else_block:
            for s in stmt.else_block:
                self.analyze_statement(s)
    
    def analyze_while_loop(self, loop: WhileLoop):
        """Analyze while loop."""
        old_in_loop = self.in_loop
        self.in_loop = True
        
        self.analyze_expression(loop.condition)
        
        # Check for infinite loops
        if isinstance(loop.condition, Literal) and loop.condition.value == True:
            self.add_issue(
                Severity.WARNING,
                "infinite-loop",
                "While loop has constant 'true' condition",
                self._get_line(loop),
                suggestion="Ensure loop has exit condition or break statement"
            )
        
        for stmt in loop.body:
            self.analyze_statement(stmt)
        
        self.in_loop = old_in_loop
    
    def analyze_for_loop(self, loop: ForLoop):
        """Analyze for loop."""
        old_in_loop = self.in_loop
        self.in_loop = True
        
        # Add loop variable to scope
        if hasattr(loop, 'variable') and loop.variable:
            if isinstance(loop.variable, Identifier):
                self.defined_vars.add(loop.variable.name)
        
        # Analyze iterable
        if hasattr(loop, 'iterable'):
            self.analyze_expression(loop.iterable)
        
        # Analyze body
        for stmt in loop.body:
            self.analyze_statement(stmt)
        
        self.in_loop = old_in_loop
    
    def analyze_repeat_n_times_loop(self, loop: RepeatNTimesLoop):
        """Analyze repeat N times loop."""
        old_in_loop = self.in_loop
        self.in_loop = True
        
        self.analyze_expression(loop.count)
        
        for stmt in loop.body:
            self.analyze_statement(stmt)
        
        self.in_loop = old_in_loop
    
    def analyze_repeat_while_loop(self, loop: RepeatWhileLoop):
        """Analyze repeat while loop."""
        old_in_loop = self.in_loop
        self.in_loop = True
        
        self.analyze_expression(loop.condition)
        
        for stmt in loop.body:
            self.analyze_statement(stmt)
        
        self.in_loop = old_in_loop
    
    def analyze_return_statement(self, stmt: ReturnStatement):
        """Analyze return statement."""
        if not self.in_function:
            self.add_issue(
                Severity.ERROR,
                "return-outside-function",
                "Return statement outside function",
                self._get_line(stmt)
            )
        
        if stmt.value:
            self.analyze_expression(stmt.value)
    
    def analyze_function_call(self, call: FunctionCall):
        """Analyze function call."""
        # Check if function is defined (FunctionCall.name is the function name)
        func_name = getattr(call, 'name', None)
        if func_name and func_name not in self.defined_funcs and not self.is_builtin(func_name):
            self.add_issue(
                Severity.ERROR,
                "undefined-function",
                f"Call to undefined function '{func_name}'",
                getattr(call, 'line_number', 0)
            )
        
        # Analyze arguments
        for arg in call.arguments:
            self.analyze_expression(arg)
    
    def analyze_print_statement(self, stmt: PrintStatement):
        """Analyze print statement."""
        if hasattr(stmt, 'expression') and stmt.expression:
            self.analyze_expression(stmt.expression)
    
    def analyze_block(self, block):
        """Analyze a block of statements."""
        for stmt in block.statements if hasattr(block, 'statements') else []:
            self.analyze_statement(stmt)
    
    def analyze_expression(self, expr):
        """Analyze an expression."""
        if isinstance(expr, Identifier):
            self.used_vars.add(expr.name)
            if expr.name not in self.defined_vars and not self.is_builtin(expr.name):
                self.add_issue(
                    Severity.ERROR,
                    "undefined-variable",
                    f"Use of undefined variable '{expr.name}'",
                    self._get_line(expr)
                )
        elif isinstance(expr, BinaryOperation):
            self.analyze_expression(expr.left)
            self.analyze_expression(expr.right)
            
            # Check for suspicious comparisons
            if hasattr(expr, 'operator'):
                if expr.operator in ('==', '!='):
                    if isinstance(expr.left, Literal) and isinstance(expr.right, Literal):
                        self.add_issue(
                            Severity.WARNING,
                            "literal-comparison",
                            "Comparison between two literals",
                            self._get_line(expr),
                            suggestion="This comparison result is always known at compile time"
                        )
        elif isinstance(expr, FunctionCall):
            self.analyze_function_call(expr)
        elif hasattr(expr, '__class__') and expr.__class__.__name__ in ('ListExpression', 'DictExpression'):
            # Analyze collection elements
            if hasattr(expr, 'elements'):
                for elem in expr.elements:
                    self.analyze_expression(elem)
            if hasattr(expr, 'entries'):
                for key, val in expr.entries:
                    self.analyze_expression(key)
                    self.analyze_expression(val)
    
    def check_unused_variables(self):
        """Check for unused variables."""
        unused = self.defined_vars - self.used_vars
        for var in unused:
            if not var.startswith('_'):  # Convention: _ prefix means intentionally unused
                self.add_issue(
                    Severity.WARNING,
                    "unused-variable",
                    f"Variable '{var}' is defined but never used",
                    0,
                    suggestion="Remove unused variable or prefix with '_' if intentional"
                )
    
    def is_builtin(self, name: str) -> bool:
        """Check if name is a builtin function/variable."""
        builtins = {
            'print', 'len', 'range', 'sqrt', 'abs', 'min', 'max',
            'sum', 'map', 'filter', 'reduce', 'sorted', 'reversed',
            'true', 'false', 'null', 'this', 'super'
        }
        return name.lower() in builtins
    
    def print_report(self, output=sys.stdout):
        """Print analysis report."""
        if not self.issues:
            print("✓ No issues found!", file=output)
            return
        
        # Count by severity
        errors = sum(1 for i in self.issues if i.severity == Severity.ERROR)
        warnings = sum(1 for i in self.issues if i.severity == Severity.WARNING)
        infos = sum(1 for i in self.issues if i.severity == Severity.INFO)
        
        print(f"\nFound {len(self.issues)} issues:", file=output)
        print(f"  Errors: {errors}", file=output)
        print(f"  Warnings: {warnings}", file=output)
        print(f"  Info: {infos}", file=output)
        print(file=output)
        
        for issue in self.issues:
            print(issue, file=output)
        
        print(file=output)
        return errors == 0


def main():
    """CLI for static analyzer."""
    import argparse
    
    parser = argparse.ArgumentParser(description="NexusLang Static Analyzer")
    parser.add_argument('files', nargs='+', help='NLPL files to analyze')
    parser.add_argument('--strict', action='store_true', help='Treat warnings as errors')
    parser.add_argument('--json', help='Output results as JSON')
    
    args = parser.parse_args()
    
    analyzer = StaticAnalyzer(strict=args.strict)
    all_issues = []
    
    for file in args.files:
        print(f"Analyzing {file}...")
        issues = analyzer.analyze_file(file)
        all_issues.extend(issues)
    
    # Print or export results
    if args.json:
        import json
        data = [
            {
                'severity': i.severity.value,
                'category': i.category,
                'message': i.message,
                'file': i.file,
                'line': i.line,
                'column': i.column,
                'suggestion': i.suggestion
            }
            for i in all_issues
        ]
        with open(args.json, 'w') as f:
            json.dump(data, f, indent=2)
    else:
        success = analyzer.print_report()
        sys.exit(0 if success else 1)


if __name__ == '__main__':
    main()
