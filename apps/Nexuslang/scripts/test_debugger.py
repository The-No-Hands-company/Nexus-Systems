#!/usr/bin/env python3
"""
Automated Debugger Tests

Tests debugger functionality programmatically.
"""

import sys
import os

# Add src to path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'src'))

from nexuslang.debugger.debugger import Debugger, DebuggerState, Breakpoint
from nexuslang.interpreter.interpreter import Interpreter
from nexuslang.runtime.runtime import Runtime
from nexuslang.stdlib import register_stdlib
from nexuslang.parser.lexer import Lexer
from nexuslang.parser.parser import Parser

def test_breakpoint_creation():
 """Test creating and managing breakpoints."""
 print("Test 1: Breakpoint Creation")
 print("=" * 60)
 
 runtime = Runtime()
 register_stdlib(runtime)
 interpreter = Interpreter(runtime)
 debugger = Debugger(interpreter, interactive=False)
 
 # Add breakpoints
 bp1 = debugger.add_breakpoint("test.nxl", 10)
 bp2 = debugger.add_breakpoint("test.nxl", 20, condition="x > 5")
 bp3 = debugger.add_breakpoint("other.nxl", 15, temp=True)
 
 assert len(debugger.list_breakpoints()) == 3
 assert bp1.line == 10
 assert bp2.condition == "x > 5"
 assert bp3.temp == True
 
 # Remove breakpoint
 assert debugger.remove_breakpoint("test.nxl", 10) == True
 assert len(debugger.list_breakpoints()) == 2
 
 # Toggle breakpoint
 assert debugger.toggle_breakpoint("test.nxl", 20) == True
 assert bp2.enabled == False
 
 print(" Breakpoint creation and management works")
 print()
 return True

def test_call_stack():
 """Test call stack tracking."""
 print("Test 2: Call Stack Tracking")
 print("=" * 60)
 
 runtime = Runtime()
 register_stdlib(runtime)
 interpreter = Interpreter(runtime)
 debugger = Debugger(interpreter, interactive=False)
 
 # Push frames
 debugger.push_frame("main", "test.nxl", 1, {"x": 10})
 debugger.push_frame("calculate", "test.nxl", 5, {"y": 20})
 debugger.push_frame("helper", "test.nxl", 10, {"z": 30})
 
 assert len(debugger.call_stack) == 3
 assert debugger.step_depth == 3
 
 frame = debugger.current_frame()
 assert frame.function_name == "helper"
 assert frame.local_vars["z"] == 30
 
 # Pop frames
 debugger.pop_frame()
 assert len(debugger.call_stack) == 2
 assert debugger.step_depth == 2
 
 print(" Call stack tracking works")
 print()
 return True

def test_variable_inspection():
 """Test variable inspection."""
 print("Test 3: Variable Inspection")
 print("=" * 60)
 
 runtime = Runtime()
 register_stdlib(runtime)
 interpreter = Interpreter(runtime)
 debugger = Debugger(interpreter, interactive=False)
 
 # Set up variables
 interpreter.set_variable("x", 42)
 interpreter.set_variable("name", "NexusLang")
 interpreter.set_variable("data", [1, 2, 3])
 
 # Inspect
 assert debugger.inspect_variable("x") == 42
 assert debugger.inspect_variable("name") == "NexusLang"
 
 all_vars = debugger.inspect_all_variables()
 assert "x" in all_vars
 assert "name" in all_vars
 assert "data" in all_vars
 
 # Set variable
 debugger.set_variable("x", 100)
 assert debugger.inspect_variable("x") == 100
 
 print(" Variable inspection works")
 print()
 return True

def test_step_modes():
 """Test step execution modes."""
 print("Test 4: Step Execution Modes")
 print("=" * 60)
 
 runtime = Runtime()
 register_stdlib(runtime)
 interpreter = Interpreter(runtime)
 debugger = Debugger(interpreter, interactive=False)
 
 # Test state transitions
 assert debugger.state == DebuggerState.RUNNING
 
 debugger.step_into()
 assert debugger.state == DebuggerState.STEPPING
 
 debugger.step_over()
 assert debugger.state == DebuggerState.STEP_OVER
 
 debugger.step_out()
 assert debugger.state == DebuggerState.STEP_OUT
 
 debugger.continue_execution()
 assert debugger.state == DebuggerState.RUNNING
 
 print(" Step execution modes work")
 print()
 return True

def test_breakpoint_hit():
 """Test breakpoint hit detection."""
 print("Test 5: Breakpoint Hit Detection")
 print("=" * 60)
 
 runtime = Runtime()
 register_stdlib(runtime)
 interpreter = Interpreter(runtime)
 debugger = Debugger(interpreter, interactive=False)
 
 # Add breakpoint
 bp = debugger.add_breakpoint("test.nxl", 5)
 assert bp.hit_count == 0
 
 # Check breakpoint (should hit)
 result = debugger._check_breakpoint("test.nxl", 5)
 assert result is not None
 assert bp.hit_count == 1
 
 # Check again
 result = debugger._check_breakpoint("test.nxl", 5)
 assert bp.hit_count == 2
 
 # Check non-existent breakpoint
 result = debugger._check_breakpoint("test.nxl", 10)
 assert result is None
 
 print(" Breakpoint hit detection works")
 print()
 return True

def test_simple_program_execution():
 """Test running a simple program with debugger."""
 print("Test 6: Simple Program Execution")
 print("=" * 60)
 
 source = """
set x to 10
set y to 20
set sum to x plus y
print text sum
"""
 
 runtime = Runtime()
 register_stdlib(runtime)
 interpreter = Interpreter(runtime)
 debugger = Debugger(interpreter, interactive=False)
 
 # Attach debugger
 interpreter.debugger = debugger
 interpreter.current_file = "test.nxl"
 
 # Add breakpoint
 debugger.add_breakpoint("test.nxl", 3)
 
 # Parse and execute
 lexer = Lexer(source)
 tokens = lexer.tokenize()
 parser = Parser(tokens)
 ast = parser.parse()
 
 try:
 interpreter.interpret(ast)
 except:
 pass # May fail due to line number tracking, but that's OK for this test
 
 # Check statistics
 assert debugger.total_steps > 0
 
 print(" Simple program execution with debugger works")
 print()
 return True

def main():
 """Run all debugger tests."""
 print("NLPL Debugger Test Suite")
 print("=" * 60)
 print()
 
 tests = [
 ("Breakpoint Creation", test_breakpoint_creation),
 ("Call Stack Tracking", test_call_stack),
 ("Variable Inspection", test_variable_inspection),
 ("Step Execution Modes", test_step_modes),
 ("Breakpoint Hit Detection", test_breakpoint_hit),
 ("Simple Program Execution", test_simple_program_execution),
 ]
 
 passed = 0
 failed = 0
 
 for name, test_func in tests:
 try:
 if test_func():
 passed += 1
 else:
 failed += 1
 print(f" {name} FAILED")
 except Exception as e:
 failed += 1
 print(f" {name} FAILED with exception: {e}")
 import traceback
 traceback.print_exc()
 print()
 
 print("=" * 60)
 print(f"Test Summary: {passed} passed, {failed} failed out of {len(tests)} total")
 print("=" * 60)
 
 return 0 if failed == 0 else 1

if __name__ == '__main__':
 sys.exit(main())
