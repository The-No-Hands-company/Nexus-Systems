"""
Test NexusLang Formatter
===================

Tests for the NexusLang code formatter.
"""

import sys
from pathlib import Path

# Add project root to path
sys.path.insert(0, str(Path(__file__).parent.parent / 'src'))

from nexuslang.lsp.formatter import NLPLFormatter


def test_basic_formatting():
    """Test basic formatting rules."""
    formatter = NLPLFormatter()
    
    # Test case 1: Indentation
    code = """
function test
set x to 5
if x is greater than 3
print text "yes"
end
end
"""
    
    expected = """function test
    set x to 5
    if x is greater than 3
        print text "yes"
    end
end
"""
    
    result = formatter.format(code.strip())
    print("Test 1 - Basic Indentation:")
    print("Input:")
    print(repr(code.strip()))
    print("\nExpected:")
    print(repr(expected.strip()))
    print("\nResult:")
    print(repr(result.strip()))
    print("\nMatch:", result.strip() == expected.strip())
    print("-" * 80)
    
    # Test case 2: Spacing normalization
    code2 = """
set   x   to   5
set y    to    10
"""
    
    expected2 = """set x to 5
set y to 10
"""
    
    result2 = formatter.format(code2.strip())
    print("\nTest 2 - Spacing Normalization:")
    print("Input:")
    print(repr(code2.strip()))
    print("\nExpected:")
    print(repr(expected2.strip()))
    print("\nResult:")
    print(repr(result2.strip()))
    print("\nMatch:", result2.strip() == expected2.strip())
    print("-" * 80)
    
    # Test case 3: Class formatting
    code3 = """
class Student
private set name to String
public function initialize with name as String
set this.name to name
end
end
"""
    
    expected3 = """class Student
    private set name to String
    
    public function initialize with name as String
        set this.name to name
    end

end
"""
    
    result3 = formatter.format(code3.strip())
    print("\nTest 3 - Class Formatting:")
    print("Input:")
    print(repr(code3.strip()))
    print("\nExpected:")
    print(repr(expected3.strip()))
    print("\nResult:")
    print(repr(result3.strip()))
    print("\nMatch:", result3.strip() == expected3.strip())
    print("-" * 80)


def test_real_world_example():
    """Test formatting on a real-world example."""
    formatter = NLPLFormatter()
    
    code = """
# Calculate average of numbers
function calculate_average with numbers as List of Float returns Float
if numbers is empty
return 0.0
end
set total to 0.0
for each number in numbers
set total to total plus number
end
return total divided by length of numbers
end

# Main program
set grades to [85.5, 92.0, 88.5]
set avg to calculate_average with grades
print text "Average: " plus convert avg to string
"""
    
    result = formatter.format(code.strip())
    print("\nTest 4 - Real World Example:")
    print("Formatted code:")
    print(result)
    print("-" * 80)


def test_lsp_edits():
    """Test LSP text edit generation."""
    formatter = NLPLFormatter()
    
    code = """set x to 5
set y to 10"""
    
    edits = formatter.get_formatting_edits(code)
    print("\nTest 5 - LSP Text Edits:")
    print("Input code:")
    print(repr(code))
    print("\nGenerated edits:")
    for edit in edits:
        print(f"  Range: {edit['range']}")
        print(f"  New text: {repr(edit['newText'])}")
    print("-" * 80)


if __name__ == '__main__':
    print("=" * 80)
    print("NLPL Formatter Tests")
    print("=" * 80)
    
    test_basic_formatting()
    test_real_world_example()
    test_lsp_edits()
    
    print("\n" + "=" * 80)
    print("All tests completed!")
    print("=" * 80)
