"""
NLPL Development Tools - Interpreter Debugging Utilities
==========================================================

Tools for inspecting and debugging the interpreter execution phase.
"""

import sys
import os
from typing import Any, Dict, List, Optional
from pathlib import Path

# Add project root to path
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), '../..')))

from src.parser.lexer import Lexer
from src.parser.parser import Parser
from src.interpreter.interpreter import Interpreter
from src.runtime.runtime import Runtime
from src.stdlib import register_stdlib


class ExecutionTracer:
    """Trace execution flow through the interpreter."""
    
    def __init__(self, interpreter: Interpreter):
        self.interpreter = interpreter
        self.trace_log = []
        self.step_count = 0
        
    def trace_execute(self, node: Any, depth: int = 0) -> Any:
        """Execute a node with tracing."""
        indent = "  " * depth
        node_type = node.__class__.__name__ if hasattr(node, '__class__') else str(type(node))
        
        self.step_count += 1
        trace_entry = {
            'step': self.step_count,
            'depth': depth,
            'node_type': node_type,
            'scope_depth': len(self.interpreter.current_scope)
        }
        
        print(f"{indent}[{self.step_count}] Executing: {node_type}")
        
        # Execute the node
        result = self.interpreter.execute(node)
        
        trace_entry['result'] = str(result)[:50] if result else None
        self.trace_log.append(trace_entry)
        
        print(f"{indent}    Result: {result}")
        
        return result
    
    def print_trace_summary(self) -> None:
        """Print summary of execution trace."""
        print("\n" + "="*80)
        print(f"EXECUTION TRACE SUMMARY")
        print("="*80 + "\n")
        
        print(f"Total Steps: {self.step_count}")
        print(f"Max Scope Depth: {max(e['scope_depth'] for e in self.trace_log) if self.trace_log else 0}")
        
        print("\nNode Type Distribution:")
        node_types = {}
        for entry in self.trace_log:
            node_type = entry['node_type']
            node_types[node_type] = node_types.get(node_type, 0) + 1
        
        for node_type, count in sorted(node_types.items(), key=lambda x: x[1], reverse=True):
            print(f"  {node_type:30} {count:4d}")


class ScopeInspector:
    """Inspect and visualize interpreter scopes."""
    
    @staticmethod
    def print_scope_hierarchy(interpreter: Interpreter) -> None:
        """Print the current scope hierarchy."""
        print("\n" + "="*80)
        print("SCOPE HIERARCHY")
        print("="*80 + "\n")
        
        for i, scope in enumerate(interpreter.current_scope):
            scope_name = "Global" if i == 0 else f"Level {i}"
            print(f"{scope_name} Scope:")
            
            if not scope:
                print("  (empty)\n")
                continue
            
            for var_name, var_value in scope.items():
                value_str = str(var_value)
                if len(value_str) > 50:
                    value_str = value_str[:47] + "..."
                print(f"  {var_name:20} = {value_str}")
            print()
    
    @staticmethod
    def find_variable(interpreter: Interpreter, var_name: str) -> Optional[tuple]:
        """Find where a variable is defined in the scope chain."""
        for i, scope in enumerate(reversed(interpreter.current_scope)):
            if var_name in scope:
                level = len(interpreter.current_scope) - 1 - i
                return (level, scope[var_name])
        return None
    
    @staticmethod
    def print_variable_info(interpreter: Interpreter, var_name: str) -> None:
        """Print detailed information about a variable."""
        result = ScopeInspector.find_variable(interpreter, var_name)
        
        if result is None:
            print(f"\nVariable '{var_name}' not found in any scope")
            return
        
        level, value = result
        scope_name = "Global" if level == 0 else f"Level {level}"
        
        print(f"\nVariable: {var_name}")
        print(f"Scope: {scope_name}")
        print(f"Value: {value}")
        print(f"Type: {type(value).__name__}")


class InteractiveDebugger:
    """Interactive step-through debugger for NexusLang code."""
    
    def __init__(self, source_code: str):
        self.source_code = source_code
        self.runtime = Runtime()
        register_stdlib(self.runtime)
        self.interpreter = Interpreter(self.runtime)
        
        # Parse the code
        lexer = Lexer(source_code)
        tokens = lexer.tokenize()
        parser = Parser(tokens)
        self.ast = parser.parse()
        
        self.current_statement = 0
        self.breakpoints = set()
        
    def run_interactive(self) -> None:
        """Run the debugger in interactive mode."""
        print("\n" + "="*80)
        print("NLPL INTERACTIVE DEBUGGER")
        print("="*80)
        print("\nCommands:")
        print("  n/next    - Execute next statement")
        print("  s/scope   - Show scope hierarchy")
        print("  v <name>  - Show variable value")
        print("  c/continue- Continue execution")
        print("  b <line>  - Set breakpoint")
        print("  q/quit    - Quit debugger")
        print("="*80 + "\n")
        
        statements = self.ast.statements if hasattr(self.ast, 'statements') else []
        
        while self.current_statement < len(statements):
            stmt = statements[self.current_statement]
            print(f"\n[Statement {self.current_statement + 1}/{len(statements)}]")
            print(f"Type: {stmt.__class__.__name__}")
            
            # Show source if available
            if hasattr(stmt, 'line_number'):
                lines = self.source_code.split('\n')
                if 0 <= stmt.line_number - 1 < len(lines):
                    print(f"Source: {lines[stmt.line_number - 1].strip()}")
            
            # Get command
            cmd = input("\n(nlpl-debug) ").strip().lower()
            
            if cmd in ['n', 'next', '']:
                # Execute statement
                try:
                    result = self.interpreter.execute(stmt)
                    print(f"Result: {result}")
                except Exception as e:
                    print(f"Error: {e}")
                self.current_statement += 1
                
            elif cmd in ['s', 'scope']:
                ScopeInspector.print_scope_hierarchy(self.interpreter)
                
            elif cmd.startswith('v '):
                var_name = cmd[2:].strip()
                ScopeInspector.print_variable_info(self.interpreter, var_name)
                
            elif cmd in ['c', 'continue']:
                # Execute rest
                try:
                    while self.current_statement < len(statements):
                        self.interpreter.execute(statements[self.current_statement])
                        self.current_statement += 1
                    print("Execution complete")
                except Exception as e:
                    print(f"Error: {e}")
                break
                
            elif cmd in ['q', 'quit']:
                print("Debugging session ended")
                break
                
            else:
                print(f"Unknown command: {cmd}")
        
        print("\nFinal scope state:")
        ScopeInspector.print_scope_hierarchy(self.interpreter)


def main():
    """Main entry point for interpreter debugging tools."""
    import argparse
    
    parser = argparse.ArgumentParser(description='NLPL Interpreter Debugging Tools')
    parser.add_argument('file', help='NLPL file to debug')
    parser.add_argument('-i', '--interactive', action='store_true', help='Run interactive debugger')
    parser.add_argument('-t', '--trace', action='store_true', help='Trace execution')
    parser.add_argument('-s', '--scope', action='store_true', help='Show final scope state')
    
    args = parser.parse_args()
    
    # Read the file
    try:
        with open(args.file, 'r') as f:
            source_code = f.read()
    except FileNotFoundError:
        print(f"Error: File '{args.file}' not found")
        return
    
    if args.interactive:
        # Run interactive debugger
        try:
            debugger = InteractiveDebugger(source_code)
            debugger.run_interactive()
        except Exception as e:
            print(f"Error initializing debugger: {e}")
            import traceback
            traceback.print_exc()
    else:
        # Run with optional tracing
        try:
            runtime = Runtime()
            register_stdlib(runtime)
            interpreter = Interpreter(runtime)
            
            lexer = Lexer(source_code)
            tokens = lexer.tokenize()
            parser_obj = Parser(tokens)
            ast_tree = parser_obj.parse()
            
            if args.trace:
                tracer = ExecutionTracer(interpreter)
                # Trace execution
                for stmt in ast_tree.statements:
                    tracer.trace_execute(stmt)
                tracer.print_trace_summary()
            else:
                interpreter.interpret(ast_tree)
            
            if args.scope:
                ScopeInspector.print_scope_hierarchy(interpreter)
                
        except Exception as e:
            print(f"Error: {e}")
            import traceback
            traceback.print_exc()


if __name__ == '__main__':
    main()
