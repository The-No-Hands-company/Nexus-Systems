#!/usr/bin/env python3
"""Test enhanced error messages."""

import sys
sys.path.insert(0, 'src')

from nexuslang.errors import NxlSyntaxError, NxlNameError, NxlTypeError

# Test 1: Syntax error with context
source = """set x to 5
set y to 10
set z to x plus
print text z
"""

print("=" * 80)
print("TEST 1: Syntax Error with Full Context")
print("=" * 80)

try:
    raise NxlSyntaxError(
        "Unexpected end of expression",
        line=3,
        column=17,
        error_type_key="unexpected_token",
        expected="value or identifier",
        got="newline",
        full_source=source
    )
except NxlSyntaxError as e:
    print(e)

print("\n\n")

# Test 2: Name error with suggestions
source2 = """set counter to 0
set total to 100

while countr is less than 10
    set counter to counter plus 1
end
"""

print("=" * 80)
print("TEST 2: Name Error with Did-You-Mean")
print("=" * 80)

try:
    raise NxlNameError(
        name="countr",
        line=4,
        column=7,
        available_names=["counter", "total", "print", "set"],
        error_type_key="undefined_variable",
        full_source=source2
    )
except NxlNameError as e:
    print(e)

print("\n\n")

# Test 3: Type error
source3 = """set age to 25
set name to "Alice"
set result to age plus name
"""

print("=" * 80)
print("TEST 3: Type Error with Type Info")
print("=" * 80)

try:
    raise NxlTypeError(
        "Cannot add Integer and String",
        line=3,
        column=15,
        expected_type="Integer",
        got_type="String",
        error_type_key="type_mismatch",
        full_source=source3
    )
except NxlTypeError as e:
    print(e)
