"""
Test Phase 4b-d: Associated Types, Phantom Types, Existential Types
"""

from nexuslang.typesystem.types import (
    TraitType, PhantomType, ExistentialType, FunctionType,
    INTEGER_TYPE, STRING_TYPE, ANY_TYPE,
    COMPARABLE_TRAIT, PRINTABLE_TRAIT
)

def test_associated_types():
    """Test associated types in traits."""
    print("\n=== Phase 4b: Associated Types ===")
    
    # Create Iterator trait with associated type
    iterator_trait = TraitType(
        "Iterator",
        {"next": FunctionType([], ANY_TYPE)},
        associated_types=["Item"]
    )
    
    assert "Item" in iterator_trait.associated_types
    print(" Trait with associated type created")

def test_phantom_types():
    """Test phantom types for type-level programming."""
    print("\n=== Phase 4c: Phantom Types ===")
    
    # Phantom<i64> and Phantom<string> are different types
    phantom_int = PhantomType("Phantom", INTEGER_TYPE)
    phantom_str = PhantomType("Phantom", STRING_TYPE)
    
    assert phantom_int != phantom_str
    assert not phantom_int.is_compatible_with(phantom_str)
    print(" Phantom types with different parameters are distinct")
    
    # Same phantom types are compatible
    phantom_int2 = PhantomType("Phantom", INTEGER_TYPE)
    assert phantom_int == phantom_int2
    assert phantom_int.is_compatible_with(phantom_int2)
    print(" Same phantom types are compatible")

def test_existential_types():
    """Test existential types (impl Trait)."""
    print("\n=== Phase 4d: Existential Types ===")
    
    # impl Comparable
    existential = ExistentialType([COMPARABLE_TRAIT])
    
    # Integer implements Comparable
    assert existential.is_compatible_with(INTEGER_TYPE)
    print(" Existential type accepts implementing types")
    
    # impl Comparable + Printable
    multi_existential = ExistentialType([COMPARABLE_TRAIT, PRINTABLE_TRAIT])
    assert multi_existential.is_compatible_with(INTEGER_TYPE)
    print(" Multiple trait bounds work")

def main():
    print("Testing Phase 4b-d Advanced Features...")
    test_associated_types()
    test_phantom_types()
    test_existential_types()
    print("\n All Phase 4b-d tests passed!")

if __name__ == "__main__":
    main()
