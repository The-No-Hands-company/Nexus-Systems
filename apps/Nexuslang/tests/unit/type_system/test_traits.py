"""
Test the trait system implementation.
"""

from nexuslang.typesystem.types import (
    TraitType, ClassType, FunctionType, PrimitiveType,
    INTEGER_TYPE, STRING_TYPE, BOOLEAN_TYPE,
    COMPARABLE_TRAIT, EQUATABLE_TRAIT, PRINTABLE_TRAIT
)

def test_trait_implementation():
    """Test if a class implements a trait."""
    print("Testing Trait System...")
    
    # Test 1: Primitive types implementing traits
    print("\n1. Testing primitive type trait implementation:")
    assert COMPARABLE_TRAIT.is_implemented_by(INTEGER_TYPE), "Integer should implement Comparable"
    assert EQUATABLE_TRAIT.is_implemented_by(STRING_TYPE), "String should implement Equatable"
    assert PRINTABLE_TRAIT.is_implemented_by(BOOLEAN_TYPE), "Boolean should implement Printable"
    print("    Primitive types correctly implement traits")
    
    # Test 2: Class implementing trait
    print("\n2. Testing class trait implementation:")
    
    # Create a Point class with compare method
    point_class = ClassType(
        "Point",
        {"x": INTEGER_TYPE, "y": INTEGER_TYPE},
        {
            "compare": FunctionType([PrimitiveType("Point")], INTEGER_TYPE),
            "equals": FunctionType([PrimitiveType("Point")], BOOLEAN_TYPE)
        }
    )
    
    assert COMPARABLE_TRAIT.is_implemented_by(point_class), "Point should implement Comparable"
    assert EQUATABLE_TRAIT.is_implemented_by(point_class), "Point should implement Equatable"
    print("    Point class correctly implements Comparable and Equatable traits")
    
    # Test 3: Class NOT implementing trait
    print("\n3. Testing class without trait implementation:")
    
    # Create a class without compare method
    simple_class = ClassType(
        "Simple",
        {"value": INTEGER_TYPE},
        {}
    )
    
    assert not COMPARABLE_TRAIT.is_implemented_by(simple_class), "Simple should NOT implement Comparable"
    print("    Simple class correctly does NOT implement Comparable")
    
    # Test 4: Trait compatibility
    print("\n4. Testing trait compatibility:")
    assert COMPARABLE_TRAIT.is_compatible_with(INTEGER_TYPE), "Comparable should be compatible with Integer"
    assert not COMPARABLE_TRAIT.is_compatible_with(simple_class), "Comparable should NOT be compatible with Simple"
    print("    Trait compatibility checks work correctly")
    
    print("\n All trait system tests passed!")

if __name__ == "__main__":
    test_trait_implementation()
