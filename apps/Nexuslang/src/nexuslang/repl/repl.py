"""
NLPL Interactive REPL

Features:
- Command history with arrow keys
- Persistent history file
- Multi-line input support
- Auto-completion for keywords, variables, functions
- Error recovery (catch exceptions, continue running)
- Pretty-print results
- Special commands (:help, :quit, :clear, :vars, :funcs)
"""

import os
import sys
import readline
import atexit
from typing import Optional, List, Dict, Any

from ..parser.lexer import Lexer, TokenType
from ..parser.parser import Parser
from ..interpreter.interpreter import Interpreter
from ..runtime.runtime import Runtime
from ..stdlib import register_stdlib


class REPLCompleter:
    """Auto-completion for REPL commands and NexusLang keywords."""
    
    # NexusLang keywords for auto-completion
    KEYWORDS = [
        'set', 'to', 'function', 'with', 'returns', 'return', 'end',
        'if', 'else', 'while', 'for', 'each', 'in', 'break', 'continue',
        'class', 'struct', 'union', 'enum', 'extends', 'implements',
        'public', 'private', 'protected', 'static', 'const',
        'import', 'from', 'as', 'export',
        'try', 'catch', 'finally', 'throw',
        'and', 'or', 'not', 'is', 'null', 'true', 'false',
        'new', 'delete', 'sizeof', 'typeof',
        'print', 'text', 'line',
        'address', 'of', 'dereference', 'allocate', 'free',
        'called', 'named', 'the', 'a', 'an',
    ]
    
    # Special REPL commands
    COMMANDS = [
        ':help', ':quit', ':exit', ':clear', ':vars', ':funcs', 
        ':reset', ':history', ':debug', ':type-check',
    ]
    
    def __init__(self, interpreter: 'Interpreter'):
        self.interpreter = interpreter
        
    def complete(self, text: str, state: int) -> Optional[str]:
        """Generate auto-completion matches."""
        if state == 0:
            # First call - build match list
            self.matches = []
            
            # Special commands (if line starts with :)
            line = readline.get_line_buffer()
            if line.startswith(':'):
                self.matches = [cmd for cmd in self.COMMANDS if cmd.startswith(text)]
            else:
                # NexusLang keywords
                keyword_matches = [kw for kw in self.KEYWORDS if kw.startswith(text)]
                
                # Variables from current scope
                var_matches = []
                if hasattr(self.interpreter, 'current_scope') and self.interpreter.current_scope:
                    for scope in reversed(self.interpreter.current_scope):
                        var_matches.extend([var for var in scope.keys() if str(var).startswith(text)])
                
                # Functions
                func_matches = []
                if hasattr(self.interpreter, 'functions'):
                    func_matches = [func for func in self.interpreter.functions.keys() if str(func).startswith(text)]
                
                # Combine all matches
                self.matches = keyword_matches + var_matches + func_matches
                
                # Remove duplicates while preserving order
                seen = set()
                unique_matches = []
                for match in self.matches:
                    if match not in seen:
                        seen.add(match)
                        unique_matches.append(match)
                self.matches = unique_matches
        
        # Return next match or None
        try:
            return self.matches[state]
        except IndexError:
            return None


class REPL:
    """Interactive REPL for NexusLang."""
    
    def __init__(self, debug: bool = False, type_check: bool = True, history_file: Optional[str] = None):
        """
        Initialize the REPL.
        
        Args:
            debug: Enable debug mode (show tokens and AST)
            type_check: Enable type checking
            history_file: Path to history file (default: ~/.nxl_history)
        """
        self.debug = debug
        self.type_check = type_check
        
        # Initialize runtime and register stdlib
        self.runtime = Runtime()
        register_stdlib(self.runtime)
        
        # Initialize interpreter
        self.interpreter = Interpreter(self.runtime, enable_type_checking=type_check)
        
        # Multi-line input buffer
        self.buffer = []
        self.in_multiline = False
        
        # Command history
        self.history_file = history_file or os.path.expanduser('~/.nxl_history')
        self._setup_readline()
        
        # Statistics
        self.line_count = 0
        self.command_count = 0
        
    def _setup_readline(self):
        """Configure readline for history and auto-completion."""
        # Set up auto-completion
        completer = REPLCompleter(self.interpreter)
        readline.set_completer(completer.complete)
        readline.parse_and_bind('tab: complete')
        
        # Load history
        if os.path.exists(self.history_file):
            try:
                readline.read_history_file(self.history_file)
            except Exception as e:
                print(f"Warning: Could not load history: {e}")
        
        # Save history on exit
        atexit.register(self._save_history)
        
    def _save_history(self):
        """Save command history to file."""
        try:
            readline.write_history_file(self.history_file)
        except Exception as e:
            print(f"Warning: Could not save history: {e}")
    
    def _is_incomplete(self, code: str) -> bool:
        """
        Check if code is incomplete (needs more lines).
        
        Heuristics:
        - Ends with keywords that require continuation (function, if, while, for, class, struct)
        - Has unmatched brackets/parens
        - Ends with backslash (explicit continuation)
        - Inside a block (no 'end' keyword yet for function/class/struct/etc.)
        """
        stripped = code.strip()
        
        # Explicit continuation
        if stripped.endswith('\\'):
            return True
        
        # Count block start/end keywords
        lines = code.split('\n')
        block_depth = 0
        
        for line in lines:
            line_stripped = line.strip().lower()
            words = line_stripped.split()
            
            if not words:
                continue
                
            # Block starters
            if words[0] in ['function', 'class', 'struct', 'union', 'if', 'while', 'for', 'try']:
                block_depth += 1
            elif 'else' in words or 'catch' in words or 'finally' in words:
                # These continue blocks
                pass
            elif words[0] == 'end':
                block_depth -= 1
        
        # If we're still in a block, need more input
        if block_depth > 0:
            return True
        
        # Check for unmatched brackets
        parens = code.count('(') - code.count(')')
        brackets = code.count('[') - code.count(']')
        braces = code.count('{') - code.count('}')
        
        return parens > 0 or brackets > 0 or braces > 0
    
    def _execute(self, code: str) -> Any:
        """
        Execute NexusLang code and return result.
        
        Args:
            code: NexusLang source code
            
        Returns:
            Execution result or None
        """
        try:
            # Lexer
            lexer = Lexer(code)
            tokens = lexer.tokenize()
            
            if self.debug:
                print("\n--- Tokens ---")
                for token in tokens:
                    print(f"  {token}")
            
            # Parser
            parser = Parser(tokens)
            ast = parser.parse()
            
            if self.debug:
                print("\n--- AST ---")
                self._print_ast(ast)
            
            # Interpreter
            result = self.interpreter.interpret(ast)
            return result
            
        except Exception as e:
            print(f"Error: {e}")
            if self.debug:
                import traceback
                traceback.print_exc()
            return None
    
    def _print_ast(self, node, indent: int = 0):
        """Pretty-print AST for debugging."""
        indent_str = "  " * indent
        if hasattr(node, 'statements'):
            print(f"{indent_str}{node}")
            for stmt in node.statements:
                self._print_ast(stmt, indent + 1)
        elif hasattr(node, 'body') and isinstance(node.body, list):
            print(f"{indent_str}{node}")
            for stmt in node.body:
                self._print_ast(stmt, indent + 1)
        else:
            print(f"{indent_str}{node}")
    
    def _handle_command(self, command: str) -> bool:
        """
        Handle special REPL commands.
        
        Returns:
            True if should exit REPL, False otherwise
        """
        cmd = command.strip().lower()
        
        if cmd in [':quit', ':exit', ':q']:
            print("Goodbye!")
            return True
            
        elif cmd == ':help':
            self._show_help()
            
        elif cmd == ':clear':
            os.system('clear' if os.name != 'nt' else 'cls')
            
        elif cmd == ':vars':
            self._show_variables()
            
        elif cmd == ':funcs':
            self._show_functions()
            
        elif cmd == ':reset':
            self._reset()
            
        elif cmd == ':history':
            self._show_history()
            
        elif cmd == ':debug':
            self.debug = not self.debug
            print(f"Debug mode: {'enabled' if self.debug else 'disabled'}")
            
        elif cmd == ':type-check':
            self.type_check = not self.type_check
            self.interpreter.enable_type_checking = self.type_check
            print(f"Type checking: {'enabled' if self.type_check else 'disabled'}")
            
        else:
            print(f"Unknown command: {command}")
            print("Type :help for available commands")
            
        return False
    
    def _show_help(self):
        """Show help message."""
        print("""
NLPL Interactive REPL
=====================

Special Commands:
  :help         - Show this help message
  :quit, :exit  - Exit the REPL
  :clear        - Clear the screen
  :vars         - Show all variables in current scope
  :funcs        - Show all defined functions
  :reset        - Reset the interpreter state
  :history      - Show command history
  :debug        - Toggle debug mode (show tokens/AST)
  :type-check   - Toggle type checking

Features:
  - Use Tab for auto-completion
  - Use Up/Down arrows for history
  - Multi-line input: Continue typing after incomplete statements
  - Use backslash (\\) for explicit line continuation

Examples:
  > set x to 42
  > print text "Hello, NLPL!"
  > function greet with name as String
  ... print text "Hello, " plus name
  ... end
  > greet with "World"
        """)
    
    def _show_variables(self):
        """Show all variables in current scope."""
        if not self.interpreter.current_scope:
            print("No variables defined")
            return
            
        print("\nVariables:")
        for scope_idx, scope in enumerate(reversed(self.interpreter.current_scope)):
            if scope:
                print(f"  Scope {len(self.interpreter.current_scope) - scope_idx}:")
                for name, value in scope.items():
                    # Skip internal/special variables
                    if not str(name).startswith('_'):
                        value_str = self._format_value(value)
                        print(f"    {name} = {value_str}")
    
    def _show_functions(self):
        """Show all defined functions."""
        if not self.interpreter.functions:
            print("No functions defined")
            return
            
        print("\nFunctions:")
        for name, func_node in self.interpreter.functions.items():
            params = []
            if hasattr(func_node, 'parameters'):
                for param in func_node.parameters:
                    if hasattr(param, 'name') and hasattr(param, 'param_type'):
                        params.append(f"{param.name} as {param.param_type}")
                    elif hasattr(param, 'name'):
                        params.append(param.name)
            
            return_type = ""
            if hasattr(func_node, 'return_type') and func_node.return_type:
                return_type = f" returns {func_node.return_type}"
            
            params_str = ", ".join(params) if params else ""
            print(f"  {name}({params_str}){return_type}")
    
    def _reset(self):
        """Reset the interpreter state."""
        print("Resetting interpreter...")
        self.runtime = Runtime()
        register_stdlib(self.runtime)
        self.interpreter = Interpreter(self.runtime, enable_type_checking=self.type_check)
        self.buffer = []
        self.in_multiline = False
        print("Interpreter reset complete")
    
    def _show_history(self):
        """Show command history."""
        history_len = readline.get_current_history_length()
        if history_len == 0:
            print("No history")
            return
            
        print("\nHistory:")
        for i in range(1, min(history_len + 1, 21)):  # Show last 20
            line = readline.get_history_item(history_len - 20 + i)
            if line:
                print(f"  {i:3d}: {line}")
    
    def _format_value(self, value: Any, max_len: int = 80) -> str:
        """Format a value for display."""
        if value is None:
            return "null"
        
        # Handle NexusLang objects
        if hasattr(value, '__class__') and hasattr(value.__class__, '__name__'):
            class_name = value.__class__.__name__
            if class_name.startswith('Structure'):
                return f"<struct {class_name}>"
            elif class_name.startswith('Union'):
                return f"<union {class_name}>"
            elif class_name == 'FunctionDefinition':
                return f"<function {getattr(value, 'name', 'anonymous')}>"
        
        # Standard Python types
        value_str = str(value)
        if len(value_str) > max_len:
            return value_str[:max_len-3] + "..."
        return value_str
    
    def _get_prompt(self) -> str:
        """Get the appropriate prompt string."""
        if self.in_multiline:
            return "... "
        return ">>> "
    
    def run(self):
        """Run the REPL main loop."""
        print("NLPL Interactive REPL")
        print("Version: 0.1.0")
        print("Type :help for help, :quit to exit")
        print()
        
        while True:
            try:
                # Get input
                prompt = self._get_prompt()
                line = input(prompt)
                
                # Skip empty lines
                if not line.strip():
                    if self.in_multiline:
                        # Empty line in multiline mode - execute buffer
                        code = '\n'.join(self.buffer)
                        self.buffer = []
                        self.in_multiline = False
                        
                        if code.strip():
                            self.command_count += 1
                            result = self._execute(code)
                            if result is not None:
                                print(f"=> {self._format_value(result)}")
                    continue
                
                # Handle special commands
                if line.startswith(':'):
                    if self._handle_command(line):
                        break
                    continue
                
                # Add to buffer
                self.buffer.append(line)
                self.line_count += 1
                
                # Check if we need more input
                code = '\n'.join(self.buffer)
                if self._is_incomplete(code):
                    self.in_multiline = True
                    continue
                
                # Execute complete code
                self.in_multiline = False
                self.command_count += 1
                
                result = self._execute(code)
                if result is not None:
                    print(f"=> {self._format_value(result)}")
                
                # Clear buffer
                self.buffer = []
                
            except KeyboardInterrupt:
                print("\nKeyboardInterrupt (use :quit to exit)")
                self.buffer = []
                self.in_multiline = False
                
            except EOFError:
                print("\nGoodbye!")
                break
                
            except Exception as e:
                print(f"REPL Error: {e}")
                if self.debug:
                    import traceback
                    traceback.print_exc()
                self.buffer = []
                self.in_multiline = False


def main():
    """Entry point for running REPL directly."""
    import argparse
    
    parser = argparse.ArgumentParser(description='NLPL Interactive REPL')
    parser.add_argument('--debug', action='store_true', help='Enable debug mode')
    parser.add_argument('--no-type-check', action='store_true', help='Disable type checking')
    parser.add_argument('--history', help='Path to history file (default: ~/.nxl_history)')
    
    args = parser.parse_args()
    
    repl = REPL(
        debug=args.debug,
        type_check=not args.no_type_check,
        history_file=args.history
    )
    
    try:
        repl.run()
    except Exception as e:
        print(f"Fatal error: {e}")
        if args.debug:
            import traceback
            traceback.print_exc()
        sys.exit(1)


if __name__ == '__main__':
    main()
