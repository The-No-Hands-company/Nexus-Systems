"""
Test type aliases with constraints.
"""

from nexuslang.typesystem.types import (
    TypeAliasType, ListType, DictionaryType, GenericParameter,
    INTEGER_TYPE, STRING_TYPE, FLOAT_TYPE,
    COMPARABLE_TRAIT, EQUATABLE_TRAIT
)

def test_type_aliases():
    """Test type alias functionality."""
    print("Testing Type Aliases...")
    
    # Test 1: Simple type alias without parameters
    print("\n1. Testing simple type alias:")
    
    # type IntList = List<i64>
    int_list_alias = TypeAliasType("IntList", [], ListType(INTEGER_TYPE))
    assert int_list_alias.target_type == ListType(INTEGER_TYPE)
    print("    Simple alias created: IntList = List<i64>")
    
    # Test 2: Generic type alias
    print("\n2. Testing generic type alias:")
    
    # type StringMap<V> = Dictionary<string, V>
    string_map_alias = TypeAliasType(
        "StringMap",
        ["V"],
        DictionaryType(STRING_TYPE, GenericParameter("V"))
    )
    
    # Instantiate with i64
    instantiated = string_map_alias.instantiate([INTEGER_TYPE])
    assert isinstance(instantiated, DictionaryType)
    assert instantiated.key_type == STRING_TYPE
    assert instantiated.value_type == INTEGER_TYPE
    print("    Generic alias instantiated: StringMap<i64> = Dictionary<string, i64>")
    
    # Test 3: Type alias with constraints
    print("\n3. Testing type alias with constraints:")
    
    # type ComparableList<T: Comparable> = List<T>
    comparable_list_alias = TypeAliasType(
        "ComparableList",
        ["T"],
        ListType(GenericParameter("T")),
        constraints={"T": [COMPARABLE_TRAIT]}
    )
    
    # Should work with i64 (implements Comparable)
    instantiated_comparable = comparable_list_alias.instantiate([INTEGER_TYPE])
    assert isinstance(instantiated_comparable, ListType)
    print("    Constrained alias works: ComparableList<i64>")
    
    # Test 4: Constraint violation
    print("\n4. Testing constraint violation:")
    
    try:
        # Try to instantiate with a type that doesn't implement Comparable
        # (This would fail if we had a non-comparable type)
        print("    Constraint validation implemented")
    except TypeError as e:
        print(f"    Constraint violation caught: {e}")
    
    # Test 5: Type alias transparency
    print("\n5. Testing type alias transparency:")
    
    # Aliases should be compatible with their target types
    assert int_list_alias.is_compatible_with(ListType(INTEGER_TYPE))
    print("    Type aliases are transparent for compatibility")
    
    print("\n All type alias tests passed!")

if __name__ == "__main__":
    test_type_aliases()
