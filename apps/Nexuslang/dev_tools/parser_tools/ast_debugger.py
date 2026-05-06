"""
NLPL Development Tools - Parser Debugging Utilities
====================================================

Tools for inspecting and debugging the parser/AST generation phase.
"""

import sys
import os
import json
from typing import Any, Optional, List
from pathlib import Path

# Add project root to path
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), '../..')))

from src.parser.lexer import Lexer
from src.parser.parser import Parser
from src.parser import ast


class ASTVisualizer:
    """Visualize Abstract Syntax Trees in various formats."""
    
    @staticmethod
    def tree_view(node: Any, indent: int = 0, prefix: str = "", is_last: bool = True) -> None:
        """Display AST as a tree structure."""
        
        # Determine connector
        connector = " " if is_last else " "
        extension = "    " if is_last else "   "
        
        # Get node information
        if hasattr(node, '__class__'):
            node_type = node.__class__.__name__
        else:
            node_type = type(node).__name__
        
        # Display node
        display = f"{node_type}"
        
        # Add value information
        if hasattr(node, 'name'):
            display += f" (name: {node.name})"
        elif hasattr(node, 'value'):
            display += f" (value: {node.value})"
        elif hasattr(node, 'type') and hasattr(node, 'value'):
            display += f" ({node.type}: {node.value})"
        
        print(f"{prefix}{connector}{display}")
        
        # Get children
        children = []
        if isinstance(node, ast.Program):
            children = node.statements
        elif hasattr(node, 'body') and isinstance(node.body, list):
            children = node.body
        elif hasattr(node, 'children') and isinstance(node.children, list):
            children = node.children
        elif hasattr(node, 'parameters') and isinstance(node.parameters, list):
            # Show parameters
            for i, param in enumerate(node.parameters):
                is_last_param = (i == len(node.parameters) - 1) and not hasattr(node, 'body')
                ASTVisualizer.tree_view(param, indent + 1, prefix + extension, is_last_param)
            if hasattr(node, 'body') and isinstance(node.body, list):
                children = node.body
        
        # Recursively display children
        for i, child in enumerate(children):
            is_last_child = (i == len(children) - 1)
            ASTVisualizer.tree_view(child, indent + 1, prefix + extension, is_last_child)
    
    @staticmethod
    def json_dump(node: Any) -> dict:
        """Convert AST to JSON-serializable dictionary."""
        
        if isinstance(node, (str, int, float, bool, type(None))):
            return node
        
        if isinstance(node, list):
            return [ASTVisualizer.json_dump(item) for item in node]
        
        result = {
            '_type': node.__class__.__name__
        }
        
        # Add all attributes
        for attr_name in dir(node):
            if attr_name.startswith('_'):
                continue
            if callable(getattr(node, attr_name)):
                continue
            
            attr_value = getattr(node, attr_name)
            
            if isinstance(attr_value, (str, int, float, bool, type(None))):
                result[attr_name] = attr_value
            elif isinstance(attr_value, list):
                result[attr_name] = [ASTVisualizer.json_dump(item) for item in attr_value]
            elif hasattr(attr_value, '__dict__'):
                result[attr_name] = ASTVisualizer.json_dump(attr_value)
        
        return result
    
    @staticmethod
    def save_json(node: Any, output_file: str) -> None:
        """Save AST as JSON file."""
        ast_dict = ASTVisualizer.json_dump(node)
        with open(output_file, 'w') as f:
            json.dump(ast_dict, f, indent=2)
        print(f"AST saved to: {output_file}")


class ParseErrorAnalyzer:
    """Analyze and provide insights about parse errors."""
    
    @staticmethod
    def analyze_error(error: Exception, source_code: str, tokens: List = None) -> None:
        """Analyze a parse error and provide detailed context."""
        print("\n" + "="*80)
        print("PARSE ERROR ANALYSIS")
        print("="*80 + "\n")
        
        error_msg = str(error)
        print(f"Error Type: {type(error).__name__}")
        print(f"Error Message: {error_msg}\n")
        
        # Try to extract line/column information
        if "line" in error_msg.lower():
            # Extract context
            lines = source_code.split('\n')
            
            # Try to find line number in error message
            import re
            line_match = re.search(r'line (\d+)', error_msg)
            col_match = re.search(r'column (\d+)', error_msg)
            
            if line_match:
                line_num = int(line_match.group(1))
                col_num = int(col_match.group(1)) if col_match else 0
                
                print("Context:")
                print("-" * 80)
                
                # Show surrounding lines
                start_line = max(0, line_num - 3)
                end_line = min(len(lines), line_num + 2)
                
                for i in range(start_line, end_line):
                    line_marker = ">>> " if i == line_num - 1 else "    "
                    print(f"{line_marker}{i+1:4d} | {lines[i]}")
                    
                    if i == line_num - 1 and col_num > 0:
                        # Show column pointer
                        print(f"         {' ' * col_num}^")
                
                print("-" * 80)
        
        # Show tokens if available
        if tokens:
            print(f"\nTokens around error location:")
            print("-" * 80)
            # Show a few tokens
            for i, token in enumerate(tokens[:20]):
                print(f"  {i}: {token.type.name:25} '{token.lexeme if hasattr(token, 'lexeme') else ''}'")
            if len(tokens) > 20:
                print(f"  ... and {len(tokens) - 20} more tokens")


class SyntaxValidator:
    """Validate NexusLang syntax and suggest corrections."""
    
    @staticmethod
    def validate(source_code: str) -> List[dict]:
        """Validate syntax and return list of issues."""
        issues = []
        
        # Check for common syntax issues
        lines = source_code.split('\n')
        
        for line_num, line in enumerate(lines, 1):
            stripped = line.strip()
            
            # Skip comments and empty lines
            if stripped.startswith('#') or not stripped:
                continue
            
            # Check for common issues
            if stripped.startswith('set') and ' to ' not in stripped:
                issues.append({
                    'line': line_num,
                    'type': 'syntax',
                    'message': "Variable assignment missing 'to' keyword",
                    'suggestion': "Use: set <name> to <value>"
                })
            
            if stripped.startswith('function') and ' with ' not in stripped and 'returns' in stripped:
                issues.append({
                    'line': line_num,
                    'type': 'syntax',
                    'message': "Function definition may be malformed",
                    'suggestion': "Use: function <name> with <params> returns <type>"
                })
        
        return issues
    
    @staticmethod
    def print_issues(issues: List[dict]) -> None:
        """Print validation issues."""
        if not issues:
            print("\n No syntax issues found!")
            return
        
        print(f"\n{'='*80}")
        print(f"SYNTAX VALIDATION ISSUES ({len(issues)} found)")
        print(f"{'='*80}\n")
        
        for issue in issues:
            print(f"Line {issue['line']}: {issue['message']}")
            if 'suggestion' in issue:
                print(f"  Suggestion: {issue['suggestion']}")
            print()


def main():
    """Main entry point for parser debugging tools."""
    import argparse
    
    parser = argparse.ArgumentParser(description='NLPL Parser Debugging Tools')
    parser.add_argument('file', help='NLPL file to parse')
    parser.add_argument('-t', '--tree', action='store_true', help='Show AST tree view')
    parser.add_argument('-j', '--json', action='store_true', help='Output AST as JSON')
    parser.add_argument('-v', '--validate', action='store_true', help='Validate syntax')
    parser.add_argument('-o', '--output', help='Output file for JSON dump')
    parser.add_argument('--show-tokens', action='store_true', help='Show tokens before parsing')
    
    args = parser.parse_args()
    
    # Read the file
    try:
        with open(args.file, 'r') as f:
            source_code = f.read()
    except FileNotFoundError:
        print(f"Error: File '{args.file}' not found")
        return
    
    print(f"\nParsing: {args.file}\n")
    
    # Validate syntax if requested
    if args.validate:
        issues = SyntaxValidator.validate(source_code)
        SyntaxValidator.print_issues(issues)
        if issues:
            print("\nContinuing with parse attempt...\n")
    
    # Tokenize
    try:
        lexer = Lexer(source_code)
        tokens = lexer.tokenize()
        
        if args.show_tokens:
            print("Tokens:")
            for i, token in enumerate(tokens[:30]):
                print(f"  {i:3d}. {token.type.name:25} '{token.lexeme if hasattr(token, 'lexeme') else ''}'")
            if len(tokens) > 30:
                print(f"  ... and {len(tokens) - 30} more tokens\n")
    except Exception as e:
        print(f"Lexer Error: {e}")
        return
    
    # Parse
    try:
        parser_obj = Parser(tokens)
        ast_tree = parser_obj.parse()
        
        print(" Parsing successful!\n")
        
        # Show tree view
        if args.tree or not args.json:
            print("="*80)
            print("AST TREE VIEW")
            print("="*80 + "\n")
            ASTVisualizer.tree_view(ast_tree)
        
        # Save JSON
        if args.json:
            output_file = args.output or args.file.replace('.nxl', '_ast.json')
            ASTVisualizer.save_json(ast_tree, output_file)
        
    except Exception as e:
        ParseErrorAnalyzer.analyze_error(e, source_code, tokens)


if __name__ == '__main__':
    main()
