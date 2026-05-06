"""
NLPL Debugger Implementation

Features:
- Breakpoint system (line-based and conditional)
- Step execution (step into, step over, step out)
- Continue execution until next breakpoint
- Variable inspection at any point
- Stack trace visualization
- Call stack tracking
- Integration with REPL for interactive debugging
"""

import os
import sys
import time
import threading
from enum import Enum
from typing import Optional, Dict, List, Any, Callable, Set
from dataclasses import dataclass, field


class DebuggerState(Enum):
    """Current state of the debugger."""
    RUNNING = "running"           # Normal execution
    PAUSED = "paused"            # Paused at breakpoint or step
    STEPPING = "stepping"        # Single-step mode
    STEP_OVER = "step_over"      # Step over function calls
    STEP_OUT = "step_out"        # Step out of current function
    FINISHED = "finished"        # Program completed


@dataclass
class Breakpoint:
    """Represents a breakpoint in the code."""
    file: str
    line: int
    condition: Optional[str] = None  # Optional condition expression
    enabled: bool = True
    hit_count: int = 0
    temp: bool = False  # Temporary breakpoint (auto-remove after hit)
    
    def __str__(self) -> str:
        status = "enabled" if self.enabled else "disabled"
        cond = f" if {self.condition}" if self.condition else ""
        temp = " (temporary)" if self.temp else ""
        return f"Breakpoint at {self.file}:{self.line} [{status}]{cond}{temp} (hits: {self.hit_count})"


@dataclass
class CallFrame:
    """Represents a call stack frame."""
    function_name: str
    file: str
    line: int
    local_vars: Dict[str, Any] = field(default_factory=dict)
    
    def __str__(self) -> str:
        return f"{self.function_name} at {self.file}:{self.line}"


class Debugger:
    """
    Interactive debugger for NexusLang programs.
    
    Usage:
        debugger = Debugger(interpreter)
        debugger.add_breakpoint("file.nxl", 10)
        debugger.run()
    """
    
    def __init__(self, interpreter, interactive: bool = True):
        """
        Initialize the debugger.
        
        Args:
            interpreter: The NexusLang interpreter instance
            interactive: Enable interactive mode (pause at breakpoints)
        """
        self.interpreter = interpreter
        self.interactive = interactive
        
        # Debugger state
        self.state = DebuggerState.RUNNING
        self.breakpoints: Dict[str, Dict[int, Breakpoint]] = {}  # file -> line -> breakpoint
        self.call_stack: List[CallFrame] = []
        self.current_file: Optional[str] = None
        self.current_line: Optional[int] = None
        
        # Step execution tracking
        self.step_depth = 0  # For step over/out
        self.target_depth: Optional[int] = None
        
        # Statistics
        self.total_steps = 0
        self.breakpoints_hit = 0
        
        # Callbacks
        self.on_breakpoint: Optional[Callable[[Breakpoint, CallFrame], None]] = None
        self.on_step: Optional[Callable[[str, int], None]] = None
        self.on_exception: Optional[Callable[[Exception, CallFrame], None]] = None
        
        # Non-interactive pause mechanism (for DAP)
        self.pause_event = threading.Event()
        self.resume_event = threading.Event()
        self.resume_event.set()  # Start in resumed state
    
    # ============= Breakpoint Management =============
    
    def add_breakpoint(self, file: str, line: int, condition: Optional[str] = None, 
                      temp: bool = False) -> Breakpoint:
        """
        Add a breakpoint at the specified file and line.
        
        Args:
            file: File path (relative or absolute)
            line: Line number (1-indexed)
            condition: Optional condition expression (NLPL code)
            temp: Temporary breakpoint (auto-remove after hit)
            
        Returns:
            The created Breakpoint object
        """
        # Normalize file path
        file = os.path.normpath(file)
        
        if file not in self.breakpoints:
            self.breakpoints[file] = {}
        
        bp = Breakpoint(file=file, line=line, condition=condition, temp=temp)
        self.breakpoints[file][line] = bp
        
        if self.interactive:
            print(f"Breakpoint added: {bp}")
        
        return bp
    
    def remove_breakpoint(self, file: str, line: int) -> bool:
        """Remove a breakpoint."""
        file = os.path.normpath(file)
        
        if file in self.breakpoints and line in self.breakpoints[file]:
            bp = self.breakpoints[file][line]
            del self.breakpoints[file][line]
            
            if not self.breakpoints[file]:
                del self.breakpoints[file]
            
            if self.interactive:
                print(f"Breakpoint removed: {file}:{line}")
            return True
        
        return False
    
    def toggle_breakpoint(self, file: str, line: int) -> bool:
        """Toggle breakpoint enabled/disabled state."""
        file = os.path.normpath(file)
        
        if file in self.breakpoints and line in self.breakpoints[file]:
            bp = self.breakpoints[file][line]
            bp.enabled = not bp.enabled
            status = "enabled" if bp.enabled else "disabled"
            
            if self.interactive:
                print(f"Breakpoint {status}: {file}:{line}")
            return True
        
        return False
    
    def clear_breakpoints(self, file: Optional[str] = None):
        """Clear all breakpoints, or all breakpoints in a specific file."""
        if file:
            file = os.path.normpath(file)
            if file in self.breakpoints:
                count = len(self.breakpoints[file])
                del self.breakpoints[file]
                if self.interactive:
                    print(f"Cleared {count} breakpoint(s) in {file}")
        else:
            total = sum(len(bps) for bps in self.breakpoints.values())
            self.breakpoints.clear()
            if self.interactive:
                print(f"Cleared all {total} breakpoint(s)")
    
    def list_breakpoints(self) -> List[Breakpoint]:
        """Get list of all breakpoints."""
        breakpoints = []
        for file_bps in self.breakpoints.values():
            breakpoints.extend(file_bps.values())
        return sorted(breakpoints, key=lambda bp: (bp.file, bp.line))
    
    def _check_breakpoint(self, file: str, line: int) -> Optional[Breakpoint]:
        """Check if there's an active breakpoint at the given location."""
        file = os.path.normpath(file)
        
        if file not in self.breakpoints or line not in self.breakpoints[file]:
            return None
        
        bp = self.breakpoints[file][line]
        
        if not bp.enabled:
            return None
        
        # Check condition if present
        if bp.condition:
            try:
                # Evaluate condition in current interpreter scope
                from ..parser.lexer import Lexer
                from ..parser.parser import Parser
                
                lexer = Lexer(bp.condition)
                tokens = lexer.tokenize()
                parser = Parser(tokens)
                condition_ast = parser.parse_expression()
                
                result = self.interpreter.visit(condition_ast)
                if not result:
                    return None  # Condition not met
            except Exception as e:
                print(f"Error evaluating breakpoint condition: {e}")
                return None
        
        # Breakpoint hit
        bp.hit_count += 1
        self.breakpoints_hit += 1
        
        # Remove if temporary
        if bp.temp:
            self.remove_breakpoint(file, line)
        
        return bp
    
    # ============= Step Execution =============
    
    def step_into(self):
        """Step into next statement (including function calls)."""
        self.state = DebuggerState.STEPPING
        self.target_depth = None
        
        # Signal resume for non-interactive mode
        if not self.interactive:
            self.resume_event.set()
    
    def step_over(self):
        """Step over next statement (don't enter function calls)."""
        self.state = DebuggerState.STEP_OVER
        self.target_depth = self.step_depth
        
        # Signal resume for non-interactive mode
        if not self.interactive:
            self.resume_event.set()
    
    def step_out(self):
        """Step out of current function."""
        self.state = DebuggerState.STEP_OUT
        self.target_depth = self.step_depth - 1 if self.step_depth > 0 else 0
        
        # Signal resume for non-interactive mode
        if not self.interactive:
            self.resume_event.set()
    
    def continue_execution(self):
        """Continue execution until next breakpoint."""
        self.state = DebuggerState.RUNNING
        self.target_depth = None
        
        # Signal resume for non-interactive mode
        if not self.interactive:
            self.resume_event.set()
    
    def _should_pause(self, file: str, line: int) -> bool:
        """Determine if debugger should pause at current location."""
        # Always pause at breakpoints
        bp = self._check_breakpoint(file, line)
        if bp:
            return True
        
        # Check step modes
        if self.state == DebuggerState.STEPPING:
            return True
        
        if self.state == DebuggerState.STEP_OVER:
            return self.step_depth <= (self.target_depth or 0)
        
        if self.state == DebuggerState.STEP_OUT:
            return self.step_depth < (self.target_depth or 0)
        
        return False
    
    # ============= Call Stack Management =============
    
    def push_frame(self, function_name: str, file: str, line: int, local_vars: Dict[str, Any]):
        """Push a new call frame onto the stack."""
        frame = CallFrame(
            function_name=function_name,
            file=file,
            line=line,
            local_vars=local_vars.copy()
        )
        self.call_stack.append(frame)
        self.step_depth += 1
    
    def pop_frame(self):
        """Pop the top call frame from the stack."""
        if self.call_stack:
            self.call_stack.pop()
            self.step_depth = max(0, self.step_depth - 1)
    
    def current_frame(self) -> Optional[CallFrame]:
        """Get the current (top) call frame."""
        return self.call_stack[-1] if self.call_stack else None
    
    def print_stack_trace(self):
        """Print the current call stack."""
        if not self.call_stack:
            print("No call stack (not in function)")
            return
        
        print("\nCall Stack:")
        for i, frame in enumerate(reversed(self.call_stack)):
            marker = "→" if i == 0 else " "
            print(f"  {marker} #{len(self.call_stack) - i - 1}: {frame}")
    
    # ============= Variable Inspection =============
    
    def inspect_variable(self, name: str) -> Optional[Any]:
        """Get the value of a variable in the current scope."""
        try:
            return self.interpreter.get_variable(name)
        except Exception:
            return None
    
    def inspect_all_variables(self) -> Dict[str, Any]:
        """Get all variables in the current scope."""
        variables = {}
        
        if hasattr(self.interpreter, 'current_scope') and self.interpreter.current_scope:
            for scope in reversed(self.interpreter.current_scope):
                for name, value in scope.items():
                    if not str(name).startswith('_'):
                        variables[name] = value
        
        return variables
    
    def set_variable(self, name: str, value: Any):
        """Set a variable value during debugging."""
        self.interpreter.set_variable(name, value)
    
    # ============= Interactive Debugging =============
    
    def pause(self, file: str, line: int, reason: str = "breakpoint", bp: 'Optional[Breakpoint]' = None):
        """
        Pause execution and enter interactive debugging mode.
        
        Args:
            file: Current file
            line: Current line
            reason: Reason for pause (breakpoint, step, exception)
            bp: The breakpoint that triggered the pause, if any
        """
        self.current_file = file
        self.current_line = line
        self.state = DebuggerState.PAUSED
        
        # Display context
        print(f"\n{'='*60}")
        print(f"Paused at {file}:{line} ({reason})")
        print(f"{'='*60}")
        
        # Show source context
        self._show_source_context(file, line)
        
        # Show current frame
        frame = self.current_frame()
        if frame:
            print(f"\nCurrent function: {frame.function_name}")
        
        # Call callback if registered
        if self.on_breakpoint and reason == "breakpoint":
            if bp is None:
                bp = self._check_breakpoint(file, line)
            if bp:
                self.on_breakpoint(bp, frame)
        
        # Interactive command loop OR wait for DAP resume
        if self.interactive:
            self._debug_repl()
        else:
            # Non-interactive mode: wait for resume signal from DAP
            self._wait_for_resume()
    
    def _show_source_context(self, file: str, line: int, context: int = 5):
        """Show source code context around current line."""
        try:
            if not os.path.exists(file):
                print(f"Source file not found: {file}")
                return
            
            with open(file, 'r') as f:
                lines = f.readlines()
            
            start = max(0, line - context - 1)
            end = min(len(lines), line + context)
            
            print("\nSource:")
            for i in range(start, end):
                marker = "→" if i == line - 1 else " "
                line_num = f"{i + 1:4d}"
                print(f"  {marker} {line_num} | {lines[i].rstrip()}")
        
        except Exception as e:
            print(f"Error reading source: {e}")
    
    def _debug_repl(self):
        """Interactive debugging REPL."""
        print("\nDebugger Commands:")
        print("  c, continue    - Continue execution")
        print("  s, step        - Step into")
        print("  n, next        - Step over")
        print("  o, out         - Step out")
        print("  b <line>       - Set breakpoint at line")
        print("  d <line>       - Delete breakpoint")
        print("  l, list        - List breakpoints")
        print("  p <var>        - Print variable")
        print("  vars           - Show all variables")
        print("  stack, bt      - Show call stack")
        print("  h, help        - Show this help")
        print("  q, quit        - Quit debugger (stop program)")
        
        while self.state == DebuggerState.PAUSED:
            try:
                cmd = input("\n(nexuslang-dbg) ").strip().lower()
                
                if not cmd:
                    continue
                
                parts = cmd.split(maxsplit=1)
                command = parts[0]
                args = parts[1] if len(parts) > 1 else ""
                
                if command in ['c', 'continue']:
                    self.continue_execution()
                    break
                
                elif command in ['s', 'step']:
                    self.step_into()
                    break
                
                elif command in ['n', 'next']:
                    self.step_over()
                    break
                
                elif command in ['o', 'out']:
                    self.step_out()
                    break
                
                elif command == 'b':
                    if args:
                        try:
                            line = int(args)
                            self.add_breakpoint(self.current_file, line)
                        except ValueError:
                            print("Invalid line number")
                    else:
                        print("Usage: b <line>")
                
                elif command == 'd':
                    if args:
                        try:
                            line = int(args)
                            self.remove_breakpoint(self.current_file, line)
                        except ValueError:
                            print("Invalid line number")
                    else:
                        print("Usage: d <line>")
                
                elif command in ['l', 'list']:
                    bps = self.list_breakpoints()
                    if bps:
                        print("\nBreakpoints:")
                        for i, bp in enumerate(bps, 1):
                            print(f"  {i}. {bp}")
                    else:
                        print("No breakpoints set")
                
                elif command == 'p':
                    if args:
                        value = self.inspect_variable(args)
                        if value is not None:
                            print(f"{args} = {value}")
                        else:
                            print(f"Variable '{args}' not found")
                    else:
                        print("Usage: p <variable>")
                
                elif command == 'vars':
                    variables = self.inspect_all_variables()
                    if variables:
                        print("\nVariables:")
                        for name, value in variables.items():
                            print(f"  {name} = {value}")
                    else:
                        print("No variables in current scope")
                
                elif command in ['stack', 'bt']:
                    self.print_stack_trace()
                
                elif command in ['h', 'help']:
                    self._debug_repl()  # Show help again
                    return
                
                elif command in ['q', 'quit']:
                    print("Quitting debugger...")
                    self.state = DebuggerState.FINISHED
                    sys.exit(0)
                
                else:
                    print(f"Unknown command: {command}")
                    print("Type 'h' for help")
            
            except KeyboardInterrupt:
                print("\nUse 'q' to quit or 'c' to continue")
            except EOFError:
                print("\nQuitting debugger...")
                self.state = DebuggerState.FINISHED
                sys.exit(0)
    
    def _wait_for_resume(self):
        """
        Wait for resume signal in non-interactive mode (for DAP).
        This blocks until continue/step command is received via DAP.
        If no DAP client is attached (no on_breakpoint callback), auto-resumes
        to prevent deadlock.
        """
        # If no external listener is attached, auto-resume to avoid deadlock
        if self.on_breakpoint is None and self.on_step is None:
            self.state = DebuggerState.RUNNING
            return

        # Clear resume event (will be set by continue/step methods)
        self.resume_event.clear()
        
        # Wait for resume signal (with timeout to allow checking state)
        while self.state == DebuggerState.PAUSED:
            # Wait with timeout to allow state changes
            if self.resume_event.wait(timeout=0.1):
                break
            
            # Check if debugger was terminated
            if self.state == DebuggerState.FINISHED:
                break
        
        # Set resume event for next pause
        self.resume_event.set()
    
    # ============= Integration Points =============
    
    def trace_line(self, file: str, line: int):
        """
        Called by interpreter for each line execution.
        This is the main integration point.
        """
        self.current_file = file
        self.current_line = line
        self.total_steps += 1
        
        # Check if we should pause (breakpoint check already done inside _should_pause)
        bp = self._check_breakpoint(file, line)
        should_pause = bp is not None

        if not should_pause:
            # Check step modes
            if self.state in (DebuggerState.STEPPING,):
                should_pause = True
            elif self.state == DebuggerState.STEP_OVER:
                should_pause = self.step_depth <= (self.target_depth or 0)
            elif self.state == DebuggerState.STEP_OUT:
                should_pause = self.step_depth < (self.target_depth or 0)

        if should_pause:
            reason = "breakpoint" if bp else "step"
            self.pause(file, line, reason, bp=bp)
        
        # Call step callback
        if self.on_step:
            self.on_step(file, line)
    
    def trace_call(self, function_name: str, file: str, line: int, local_vars: Dict[str, Any]):
        """Called when entering a function."""
        self.push_frame(function_name, file, line, local_vars)
    
    def trace_return(self, function_name: str, return_value: Any):
        """Called when exiting a function."""
        self.pop_frame()
    
    def trace_exception(self, exception: Exception):
        """Called when an exception occurs."""
        frame = self.current_frame()
        
        print(f"\n{'='*60}")
        print(f"Exception: {type(exception).__name__}: {exception}")
        print(f"{'='*60}")
        
        if frame:
            print(f"In function: {frame.function_name}")
            print(f"At: {frame.file}:{frame.line}")
        
        self.print_stack_trace()
        
        if self.on_exception:
            self.on_exception(exception, frame)
        
        # Pause for inspection
        if self.interactive and self.current_file and self.current_line:
            self.pause(self.current_file, self.current_line, "exception")
    
    # ============= Statistics =============
    
    def print_statistics(self):
        """Print debugging session statistics."""
        print(f"\nDebugger Statistics:")
        print(f"  Total steps: {self.total_steps}")
        print(f"  Breakpoints hit: {self.breakpoints_hit}")
        print(f"  Active breakpoints: {sum(len(bps) for bps in self.breakpoints.values())}")
        print(f"  Max call depth: {len(self.call_stack)}")


def main():
    """Entry point for running debugger directly."""
    import argparse
    
    parser = argparse.ArgumentParser(description='NexusLang Debugger')
    parser.add_argument('file', help='The NexusLang file to debug')
    parser.add_argument('--break', '-b', dest='breakpoints', action='append',
                       help='Set breakpoint at line (can be used multiple times)')
    parser.add_argument('--no-interactive', action='store_true',
                       help='Disable interactive mode')
    
    args = parser.parse_args()
    
    # Import interpreter
    from ..main import run_program
    from ..interpreter.interpreter import Interpreter
    from ..runtime.runtime import Runtime
    from ..stdlib import register_stdlib
    
    # Read source
    with open(args.file, 'r') as f:
        source = f.read()
    
    # Setup interpreter with debugger
    runtime = Runtime()
    register_stdlib(runtime)
    interpreter = Interpreter(runtime)
    
    debugger = Debugger(interpreter, interactive=not args.no_interactive)
    
    # Set breakpoints from command line
    if args.breakpoints:
        for bp in args.breakpoints:
            try:
                line = int(bp)
                debugger.add_breakpoint(args.file, line)
            except ValueError:
                print(f"Invalid breakpoint: {bp}")
    
    # Attach debugger to interpreter
    interpreter.debugger = debugger
    
    print(f"Debugging: {args.file}")
    print(f"Breakpoints: {len(debugger.list_breakpoints())}")
    print()
    
    # Run program
    try:
        from ..parser.lexer import Lexer
        from ..parser.parser import Parser
        
        lexer = Lexer(source)
        tokens = lexer.tokenize()
        parser = Parser(tokens)
        ast = parser.parse()
        
        interpreter.interpret(ast)
        
        print("\nProgram finished")
        debugger.print_statistics()
    
    except Exception as e:
        print(f"\nProgram terminated with error: {e}")
        debugger.trace_exception(e)
        import traceback
        traceback.print_exc()


if __name__ == '__main__':
    main()
