#!/usr/bin/env python3
"""
NLPL Development Tools - Unified CLI
=====================================

Main command-line interface for all NexusLang development and debugging tools.

Usage:
    nlpl-dev <command> [options]

Commands:
    lex         Lexer debugging tools
    parse       Parser debugging tools
    debug       Interpreter debugging tools
    run         Run NexusLang code with enhanced debugging
    test        Testing utilities
    doctor      Check NexusLang development environment

Examples:
    nlpl-dev lex myfile.nlpl --visualize
    nlpl-dev parse myfile.nlpl --tree
    nlpl-dev debug myfile.nlpl --interactive
    nlpl-dev run myfile.nlpl --trace --scope
    nlpl-dev doctor
"""

import sys
import os
import subprocess
from pathlib import Path

# Add project root to path
PROJECT_ROOT = Path(__file__).parent.parent
sys.path.insert(0, str(PROJECT_ROOT))


def print_banner():
    """Print the NexusLang dev tools banner."""
    banner = """

                    NexusLang Development Tools Suite                            
                    NexusLang                   

"""
    print(banner)


def run_lexer_tools(args):
    """Run lexer debugging tools."""
    from dev_tools.lexer_tools import token_debugger
    sys.argv = ['token_debugger.py'] + args
    token_debugger.main()


def run_parser_tools(args):
    """Run parser debugging tools."""
    from dev_tools.parser_tools import ast_debugger
    sys.argv = ['ast_debugger.py'] + args
    ast_debugger.main()


def run_interpreter_tools(args):
    """Run interpreter debugging tools."""
    from dev_tools.interpreter_tools import execution_debugger
    sys.argv = ['execution_debugger.py'] + args
    execution_debugger.main()


def run_nxl_enhanced(args):
    """Run NexusLang with enhanced debugging options."""
    import argparse
    
    parser = argparse.ArgumentParser(description='Run NexusLang with debugging')
    parser.add_argument('file', help='NLPL file to run')
    parser.add_argument('--trace', action='store_true', help='Trace execution')
    parser.add_argument('--scope', action='store_true', help='Show scope state')
    parser.add_argument('--tokens', action='store_true', help='Show tokens')
    parser.add_argument('--ast', action='store_true', help='Show AST')
    parser.add_argument('--stats', action='store_true', help='Show statistics')
    parser.add_argument('--debug', action='store_true', help='Enable all debugging')
    
    parsed_args = parser.parse_args(args)
    
    print(f"\nRunning: {parsed_args.file}\n")
    print("="*80)
    
    # Read source
    try:
        with open(parsed_args.file, 'r') as f:
            source_code = f.read()
    except FileNotFoundError:
        print(f"Error: File '{parsed_args.file}' not found")
        return
    
    from src.parser.lexer import Lexer
    from src.parser.parser import Parser
    from src.interpreter.interpreter import Interpreter
    from src.runtime.runtime import Runtime
    from src.stdlib import register_stdlib
    
    # Tokenize
    lexer = Lexer(source_code)
    tokens = lexer.tokenize()
    
    if parsed_args.tokens or parsed_args.debug:
        print("\nTOKENS:")
        print("-"*80)
        for i, token in enumerate(tokens[:30]):
            print(f"  {i:3d}. {token.type.name:25} '{token.lexeme if hasattr(token, 'lexeme') else ''}'")
        if len(tokens) > 30:
            print(f"  ... and {len(tokens) - 30} more tokens")
        print()
    
    # Parse
    parser = Parser(tokens)
    ast_tree = parser.parse()
    
    if parsed_args.ast or parsed_args.debug:
        print("\nAST:")
        print("-"*80)
        from dev_tools.parser_tools.ast_debugger import ASTVisualizer
        ASTVisualizer.tree_view(ast_tree)
        print()
    
    # Execute
    runtime = Runtime()
    register_stdlib(runtime)
    interpreter = Interpreter(runtime)
    
    print("\nEXECUTION:")
    print("-"*80)
    
    if parsed_args.trace or parsed_args.debug:
        from dev_tools.interpreter_tools.execution_debugger import ExecutionTracer
        tracer = ExecutionTracer(interpreter)
        for stmt in ast_tree.statements:
            tracer.trace_execute(stmt)
        print()
        tracer.print_trace_summary()
    else:
        result = interpreter.interpret(ast_tree)
        print(f"Result: {result}")
    
    if parsed_args.scope or parsed_args.debug:
        from dev_tools.interpreter_tools.execution_debugger import ScopeInspector
        ScopeInspector.print_scope_hierarchy(interpreter)
    
    if parsed_args.stats or parsed_args.debug:
        from dev_tools.lexer_tools.token_debugger import TokenStatistics
        stats = TokenStatistics.analyze(tokens)
        TokenStatistics.print_statistics(stats)


def run_doctor():
    """Check NexusLang development environment."""
    print("\nNLPL Development Environment Check")
    print("="*80 + "\n")
    
    checks = []
    
    # Check Python version
    import sys
    py_version = f"{sys.version_info.major}.{sys.version_info.minor}.{sys.version_info.micro}"
    py_ok = sys.version_info >= (3, 8)
    checks.append(("Python version", py_version, py_ok))
    
    # Check required packages
    packages = [
        'pytest',
        'black',
        'isort',
        'flake8',
    ]
    
    for package in packages:
        try:
            __import__(package)
            checks.append((f"Package: {package}", "installed", True))
        except ImportError:
            checks.append((f"Package: {package}", "missing", False))
    
    # Check project structure
    required_dirs = [
        'src',
        'src/parser',
        'src/interpreter',
        'src/runtime',
        'src/stdlib',
        'tests',
        'examples',
        'dev_tools',
    ]
    
    for dir_path in required_dirs:
        full_path = PROJECT_ROOT / dir_path
        exists = full_path.exists()
        checks.append((f"Directory: {dir_path}", "exists" if exists else "missing", exists))
    
    # Check key files
    required_files = [
        'src/main.py',
        'src/parser/lexer.py',
        'src/parser/parser.py',
        'src/parser/ast.py',
        'requirements.txt',
    ]
    
    for file_path in required_files:
        full_path = PROJECT_ROOT / file_path
        exists = full_path.is_file()
        checks.append((f"File: {file_path}", "exists" if exists else "missing", exists))
    
    # Print results
    all_ok = True
    for name, status, ok in checks:
        symbol = "" if ok else ""
        print(f"{symbol} {name:40} {status}")
        if not ok:
            all_ok = False
    
    print("\n" + "="*80)
    if all_ok:
        print(" All checks passed! Environment is ready.")
    else:
        print(" Some checks failed. Please fix the issues above.")
        print("\nTo install missing packages:")
        print("  pip install -r requirements.txt")
    print()


def print_help():
    """Print help information."""
    print_banner()
    print(__doc__)


def main():
    """Main entry point."""
    if len(sys.argv) < 2:
        print_help()
        return
    
    command = sys.argv[1]
    args = sys.argv[2:]
    
    commands = {
        'lex': run_lexer_tools,
        'parse': run_parser_tools,
        'debug': run_interpreter_tools,
        'run': run_nxl_enhanced,
        'doctor': lambda _: run_doctor(),
        'help': lambda _: print_help(),
        '--help': lambda _: print_help(),
        '-h': lambda _: print_help(),
    }
    
    if command in commands:
        try:
            commands[command](args)
        except Exception as e:
            print(f"\nError: {e}")
            import traceback
            traceback.print_exc()
    else:
        print(f"Unknown command: {command}")
        print("Run 'nlpl-dev help' for usage information")


if __name__ == '__main__':
    main()
