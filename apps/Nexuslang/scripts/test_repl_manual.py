#!/usr/bin/env python3
"""
Manual REPL Test

Interactive test scenarios for the NexusLang REPL.
Run manually to verify REPL functionality.
"""

print("""
NLPL REPL Manual Test Suite
=============================

This script guides you through testing the REPL interactively.
Start the REPL in another terminal with:

 python -m nexuslang.main

Or:
 
 python nxl_repl.py

Then test these scenarios:

TEST 1: Basic Variable Assignment
----------------------------------
>>> set x to 42
>>> set name to "NexusLang"
>>> print text name
>>> :vars

Expected: Should show x=42, name="NexusLang"

TEST 2: Multi-line Function Definition
---------------------------------------
>>> function greet with name as String returns String
... return "Hello, " plus name
... end
>>> greet with "World"

Expected: Should return "Hello, World"

TEST 3: Multi-line Control Flow
--------------------------------
>>> function factorial with n as Integer returns Integer
... if n is less than or equal to 1
... return 1
... end
... return n times factorial with n minus 1
... end
>>> factorial with 5

Expected: Should return 120

TEST 4: Tab Completion
----------------------
>>> set test_var to 123
>>> te<TAB>

Expected: Should auto-complete to "test_var"

TEST 5: Command History
------------------------
>>> set a to 1
>>> set b to 2
>>> <UP ARROW>
>>> <UP ARROW>

Expected: Should cycle through previous commands

TEST 6: Error Recovery
----------------------
>>> set x to "invalid" plus 42
>>> set y to 100

Expected: First command should error, but REPL continues

TEST 7: Special Commands
-------------------------
>>> :help
>>> :vars
>>> :funcs
>>> :debug
>>> :type-check
>>> :clear
>>> :history

Expected: Each command should work properly

TEST 8: Complex Expression
--------------------------
>>> set numbers to [1, 2, 3, 4, 5]
>>> set total to 0
>>> for each num in numbers
... set total to total plus num
... end
>>> print text total

Expected: Should print 15

TEST 9: Struct Definition
-------------------------
>>> struct Point
... x as Integer
... y as Integer
... end
>>> set p to new Point
>>> set p.x to 10
>>> set p.y to 20
>>> :vars

Expected: Should show Point struct with x=10, y=20

TEST 10: Exit REPL
------------------
>>> :quit

Expected: Should exit cleanly

AUTOMATED FEATURE CHECK
========================
""")

import os

features_to_check = [
 ("Auto-completion (REPLCompleter class)", "REPLCompleter"),
 ("Multi-line input detection (_is_incomplete)", "_is_incomplete"),
 ("Command history (readline)", "readline"),
 ("Error recovery (try/except)", "try:"),
 ("Special commands (:help, :vars, etc.)", ":help"),
 ("Debug mode toggle", "self.debug"),
 ("Variable inspection", "_show_variables"),
 ("Function inspection", "_show_functions"),
 ("REPL reset", "_reset"),
 ("History persistence", "history_file"),
 ("Pretty-print results", "_format_value"),
 ("Prompt handling", "_get_prompt"),
]

repl_file = "src/nlpl/repl/repl.py"
if os.path.exists(repl_file):
 with open(repl_file) as f:
 content = f.read()
 
 print("Feature Implementation Check:")
 print("-" * 60)
 
 all_ok = True
 for feature_name, search_term in features_to_check:
 found = search_term in content
 status = "" if found else ""
 print(f"{status} {feature_name}")
 if not found:
 all_ok = False
 
 print()
 if all_ok:
 print(" All features implemented!")
 else:
 print(" Some features missing")
else:
 print(f"Error: {repl_file} not found")

print("""

Quick Start Commands:
=====================

# Start REPL:
python -m nexuslang.main

# Start REPL with debug:
python -m nexuslang.main --debug

# Start REPL without type checking:
python -m nexuslang.main --no-type-check

# Or use convenience script:
python nxl_repl.py

# Run a file then enter REPL:
python -m nexuslang.main examples/01_basic_concepts.nlpl --repl
""")
