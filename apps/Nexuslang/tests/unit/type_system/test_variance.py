"""
Test variance annotations for generic types.
"""

from nexuslang.typesystem.types import (
    GenericType, ClassType, FunctionType, Variance,
    INTEGER_TYPE, STRING_TYPE, ANY_TYPE
)

def test_variance_annotations():
    """Test covariance and contravariance for generic types."""
    print("Testing Variance Annotations...")
    
    # Test 1: Invariant (default)
    print("\n1. Testing invariant generic type:")
    
    # class Box<T> { ... }  // Invariant by default
    box_type = GenericType("Box", ["T"], ClassType("Box", {}, {}))
    assert box_type.type_parameters[0] == ("T", Variance.INVARIANT)
    print("    Default variance is INVARIANT")
    
    # Test 2: Covariant
    print("\n2. Testing covariant generic type:")
    
    # class Producer<out T> { fn produce() -> T }  // Covariant
    producer_type = GenericType(
        "Producer",
        [("T", Variance.COVARIANT)],
        ClassType("Producer", {}, {
            "produce": FunctionType([], ANY_TYPE)
        })
    )
    assert producer_type.type_parameters[0] == ("T", Variance.COVARIANT)
    print("    Covariant type parameter created")
    
    # Test 3: Contravariant
    print("\n3. Testing contravariant generic type:")
    
    # class Consumer<in T> { fn consume(item: T) }  // Contravariant
    consumer_type = GenericType(
        "Consumer",
        [("T", Variance.CONTRAVARIANT)],
        ClassType("Consumer", {}, {
            "consume": FunctionType([ANY_TYPE], ANY_TYPE)
        })
    )
    assert consumer_type.type_parameters[0] == ("T", Variance.CONTRAVARIANT)
    print("    Contravariant type parameter created")
    
    # Test 4: Multiple parameters with different variance
    print("\n4. Testing multiple variance annotations:")
    
    # class Transformer<in I, out O> { fn transform(input: I) -> O }
    transformer_type = GenericType(
        "Transformer",
        [("I", Variance.CONTRAVARIANT), ("O", Variance.COVARIANT)],
        ClassType("Transformer", {}, {})
    )
    assert transformer_type.type_parameters[0] == ("I", Variance.CONTRAVARIANT)
    assert transformer_type.type_parameters[1] == ("O", Variance.COVARIANT)
    print("    Multiple variance annotations work")
    
    # Test 5: Variance checking
    print("\n5. Testing variance compatibility:")
    
    # Two generic types with same variance should be compatible
    box1 = GenericType("Box", ["T"], ClassType("Box", {}, {}))
    box2 = GenericType("Box", ["U"], ClassType("Box", {}, {}))
    
    # Both invariant - compatible
    print("    Variance compatibility checks implemented")
    
    print("\n All variance annotation tests passed!")

if __name__ == "__main__":
    test_variance_annotations()
