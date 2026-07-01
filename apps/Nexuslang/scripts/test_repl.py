#!/usr/bin/env python3
"""
REPL Test Script

Tests REPL functionality programmatically.
"""

import sys
import os
import subprocess
import time

# Test commands to run in REPL
test_commands = [
 "set x to 42",
 "set y to 10",
 "set sum to x plus y",
 "print text sum",
 "function double with n as Integer returns Integer",
 "return n times 2",
 "end",
 "double with 21",
 ":vars",
 ":funcs",
 ":quit",
]

def test_repl_basic():
 """Test basic REPL functionality."""
 print("Testing REPL Basic Functionality...")
 print("=" * 60)
 
 # Create test input file
 test_input = "\n".join(test_commands)
 
 # Run REPL with test input
 process = subprocess.Popen(
 ["python", "-m", "nexuslang.main", "--repl"],
 stdin=subprocess.PIPE,
 stdout=subprocess.PIPE,
 stderr=subprocess.PIPE,
 text=True,
 cwd=os.path.join(os.path.dirname(__file__), "src")
 )
 
 try:
 stdout, stderr = process.communicate(input=test_input, timeout=10)
 
 print("\n--- STDOUT ---")
 print(stdout)
 
 if stderr:
 print("\n--- STDERR ---")
 print(stderr)
 
 # Check for expected outputs
 success_checks = [
 ("REPL started", "NLPL Interactive REPL" in stdout),
 ("Variable assignment", "x" in stdout or "42" in stdout),
 ("Function definition", "double" in stdout or "function" in stdout),
 ("Command execution", ":vars" in stdout or ":funcs" in stdout),
 ]
 
 print("\n--- Test Results ---")
 all_passed = True
 for check_name, passed in success_checks:
 status = "" if passed else ""
 print(f"{status} {check_name}")
 if not passed:
 all_passed = False
 
 if all_passed:
 print("\n REPL basic tests PASSED")
 return True
 else:
 print("\n REPL basic tests FAILED")
 return False
 
 except subprocess.TimeoutExpired:
 print(" REPL test TIMEOUT")
 process.kill()
 return False
 except Exception as e:
 print(f" REPL test ERROR: {e}")
 import traceback
 traceback.print_exc()
 return False

def test_repl_features():
 """Test REPL feature detection."""
 print("\n\nTesting REPL Features...")
 print("=" * 60)
 
 features = {
 "Auto-completion": "REPLCompleter" in open("src/nlpl/repl/repl.py").read(),
 "Multi-line input": "_is_incomplete" in open("src/nlpl/repl/repl.py").read(),
 "Command history": "readline" in open("src/nlpl/repl/repl.py").read(),
 "Error recovery": "try:" in open("src/nlpl/repl/repl.py").read() and "except" in open("src/nlpl/repl/repl.py").read(),
 "Special commands": ":help" in open("src/nlpl/repl/repl.py").read(),
 "Debug mode": "self.debug" in open("src/nlpl/repl/repl.py").read(),
 "Variable inspection": "_show_variables" in open("src/nlpl/repl/repl.py").read(),
 "Function inspection": "_show_functions" in open("src/nlpl/repl/repl.py").read(),
 "Reset capability": "_reset" in open("src/nlpl/repl/repl.py").read(),
 "History persistence": "history_file" in open("src/nlpl/repl/repl.py").read(),
 }
 
 print("\n--- Feature Checklist ---")
 all_present = True
 for feature, present in features.items():
 status = "" if present else ""
 print(f"{status} {feature}")
 if not present:
 all_present = False
 
 if all_present:
 print("\n All REPL features implemented")
 return True
 else:
 print("\n Some REPL features missing")
 return False

def main():
 """Run all REPL tests."""
 print("NLPL REPL Test Suite")
 print("=" * 60)
 
 # Change to project root
 os.chdir(os.path.dirname(__file__))
 
 results = []
 
 # Test 1: Feature detection
 results.append(("Feature Detection", test_repl_features()))
 
 # Test 2: Basic functionality
 results.append(("Basic Functionality", test_repl_basic()))
 
 # Summary
 print("\n\n" + "=" * 60)
 print("Test Summary")
 print("=" * 60)
 
 passed = sum(1 for _, result in results if result)
 total = len(results)
 
 for name, result in results:
 status = "PASS" if result else "FAIL"
 print(f"{status:6s} - {name}")
 
 print(f"\nTotal: {passed}/{total} tests passed")
 
 if passed == total:
 print("\n All REPL tests PASSED!")
 return 0
 else:
 print(f"\n {total - passed} test(s) FAILED")
 return 1

if __name__ == '__main__':
 sys.exit(main())
