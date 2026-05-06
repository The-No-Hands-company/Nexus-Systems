"""
Test enhanced generic constraints with multiple trait bounds.
"""

from nexuslang.typesystem.types import (
    ClassType, FunctionType, INTEGER_TYPE, BOOLEAN_TYPE, STRING_TYPE,
    COMPARABLE_TRAIT, EQUATABLE_TRAIT, PRINTABLE_TRAIT
)
from nexuslang.typesystem.generic_types import GenericTypeConstraint, GenericTypeContext

def test_multiple_trait_constraints():
    """Test generic constraints with multiple trait bounds."""
    print("Testing Enhanced Generic Constraints...")
    
    # Test 1: Single trait constraint
    print("\n1. Testing single trait constraint:")
    constraint = GenericTypeConstraint("T", [COMPARABLE_TRAIT])
    
    # Integer implements Comparable
    assert constraint.check(INTEGER_TYPE), "Integer should satisfy Comparable constraint"
    print("    Single trait constraint works")
    
    # Test 2: Multiple trait constraints (T: Comparable + Equatable)
    print("\n2. Testing multiple trait constraints:")
    multi_constraint = GenericTypeConstraint("T", [COMPARABLE_TRAIT, EQUATABLE_TRAIT])
    
    # Integer implements both
    assert multi_constraint.check(INTEGER_TYPE), "Integer should satisfy Comparable + Equatable"
    print("    Multiple trait constraints work")
    
    # Test 3: Class satisfying multiple constraints
    print("\n3. Testing class with multiple trait implementations:")
    
    # Create a Point class that implements all required traits
    point_class = ClassType(
        "Point",
        {"x": INTEGER_TYPE, "y": INTEGER_TYPE},
        {
            "compare": FunctionType([ClassType("Point", {}, {})], INTEGER_TYPE),
            "equals": FunctionType([ClassType("Point", {}, {})], BOOLEAN_TYPE),
            "to_string": FunctionType([], STRING_TYPE)
        }
    )
    
    # Should satisfy Comparable + Equatable + Printable
    triple_constraint = GenericTypeConstraint("T", [COMPARABLE_TRAIT, EQUATABLE_TRAIT, PRINTABLE_TRAIT])
    assert triple_constraint.check(point_class), "Point should satisfy all three traits"
    print("    Class satisfies multiple trait constraints")
    
    # Test 4: GenericTypeContext with constraints
    print("\n4. Testing GenericTypeContext:")
    
    context = GenericTypeContext()
    context.add_type_parameter("T", [COMPARABLE_TRAIT, EQUATABLE_TRAIT])
    
    # Check constraints
    substitutions = {"T": INTEGER_TYPE}
    assert context.check_constraints(substitutions), "Integer should satisfy T: Comparable + Equatable"
    print("    GenericTypeContext validates constraints correctly")
    
    # Test 5: Constraint string representation
    print("\n5. Testing constraint string representation:")
    constraint_str = str(multi_constraint)
    assert "Comparable" in constraint_str and "Equatable" in constraint_str
    print(f"    Constraint string: {constraint_str}")
    
    print("\n All enhanced generic constraint tests passed!")

if __name__ == "__main__":
    from nexuslang.typesystem.types import STRING_TYPE
    test_multiple_trait_constraints()
