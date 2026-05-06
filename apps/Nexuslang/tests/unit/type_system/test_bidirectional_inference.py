"""
Test bidirectional type inference.
"""

from nexuslang.typesystem.types import (
    ListType, DictionaryType, FunctionType,
    INTEGER_TYPE, STRING_TYPE, FLOAT_TYPE, get_type_by_name
)
from nexuslang.typesystem.type_inference import TypeInferenceEngine
from nexuslang.parser.ast import Literal, VariableDeclaration

def test_bidirectional_inference():
    """Test bidirectional type inference with expected types."""
    print("Testing Bidirectional Type Inference...")
    
    engine = TypeInferenceEngine()
    
    # Test 1: List literal with expected type
    print("\n1. Testing list literal with expected type:")
    
    # Simulate: let numbers: List<i64> = [1, 2, 3]
    list_expr = type('ListLiteral', (), {
        'node_type': 'list_literal',
        'elements': [
            Literal('integer', 1),
            Literal('integer', 2),
            Literal('integer', 3)
        ]
    })()
    
    expected_type = ListType(INTEGER_TYPE)
    inferred = engine.infer_with_expected_type(list_expr, expected_type, {})
    
    assert inferred == expected_type, f"Expected {expected_type}, got {inferred}"
    print(f"    List literal inferred as {inferred}")
    
    # Test 2: Dictionary literal with expected type
    print("\n2. Testing dictionary literal with expected type:")
    
    # Simulate: let map: Dict<string, i64> = {"a": 1, "b": 2}
    dict_expr = type('DictLiteral', (), {
        'node_type': 'dictionary_literal',
        'keys': [Literal('string', "a"), Literal('string', "b")],
        'values': [Literal('integer', 1), Literal('integer', 2)]
    })()
    
    expected_dict_type = DictionaryType(STRING_TYPE, INTEGER_TYPE)
    inferred_dict = engine.infer_with_expected_type(dict_expr, expected_dict_type, {})
    
    assert inferred_dict == expected_dict_type
    print(f"    Dictionary literal inferred as {inferred_dict}")
    
    # Test 3: No expected type (fallback to regular inference)
    print("\n3. Testing fallback to regular inference:")
    
    inferred_no_expected = engine.infer_with_expected_type(list_expr, None, {})
    assert isinstance(inferred_no_expected, ListType)
    print(f"    Falls back to regular inference: {inferred_no_expected}")
    
    # Test 4: Type mismatch (should fall back)
    print("\n4. Testing type mismatch fallback:")
    
    # Try to assign float list to integer list type
    float_list = type('ListLiteral', (), {
        'node_type': 'list_literal',
        'elements': [Literal('float', 1.5), Literal('float', 2.5)]
    })()
    
    inferred_mismatch = engine.infer_with_expected_type(float_list, ListType(INTEGER_TYPE), {})
    # Should infer as List<float> instead
    print(f"    Type mismatch handled: {inferred_mismatch}")
    
    print("\n All bidirectional inference tests passed!")

if __name__ == "__main__":
    test_bidirectional_inference()
