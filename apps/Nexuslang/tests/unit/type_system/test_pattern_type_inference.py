"""
Tests for pattern matching type inference.

This tests the enhanced type inference capabilities for pattern matching,
including proper type inference for pattern bindings.
"""

import pytest
from nexuslang.typesystem.types import (
    ListType, DictionaryType, FunctionType, GenericType,
    INTEGER_TYPE, STRING_TYPE, FLOAT_TYPE, BOOLEAN_TYPE, ANY_TYPE
)
from nexuslang.typesystem.type_inference import TypeInferenceEngine
from nexuslang.parser.ast import (
    Literal, IdentifierPattern, WildcardPattern, ListPattern,
    OptionPattern, ResultPattern, VariantPattern
)


class TestPatternBindingInference:
    """Test type inference for pattern bindings."""
    
    def test_identifier_pattern(self):
        """Test identifier pattern binds to full match type."""
        engine = TypeInferenceEngine()
        
        # case x => x gets type of matched value
        pattern = IdentifierPattern("x")
        bindings = engine.infer_pattern_binding_type(pattern, INTEGER_TYPE)
        
        assert "x" in bindings
        assert bindings["x"] == INTEGER_TYPE
    
    def test_wildcard_pattern(self):
        """Test wildcard pattern has no bindings."""
        engine = TypeInferenceEngine()
        
        # case _ => no bindings
        pattern = WildcardPattern()
        bindings = engine.infer_pattern_binding_type(pattern, INTEGER_TYPE)
        
        assert len(bindings) == 0
    
    def test_literal_pattern(self):
        """Test literal pattern has no bindings."""
        engine = TypeInferenceEngine()
        
        # case 42 => no bindings
        literal = Literal('integer', 42)
        pattern = type('LiteralPattern', (), {'value': literal})()
        bindings = engine.infer_pattern_binding_type(pattern, INTEGER_TYPE)
        
        assert len(bindings) == 0
    
    def test_option_some_pattern(self):
        """Test Option Some pattern unwraps inner type."""
        engine = TypeInferenceEngine()
        
        # case Some with value => value gets T from Option<T>
        pattern = OptionPattern("Some", "value")
        
        # Create Option<Integer> type
        option_type = type('GenericType', (), {
            'name': 'Option',
            'type_parameters': [INTEGER_TYPE]
        })()
        
        bindings = engine.infer_pattern_binding_type(pattern, option_type)
        
        assert "value" in bindings
        assert bindings["value"] == INTEGER_TYPE
    
    def test_option_none_pattern(self):
        """Test Option None pattern has no bindings."""
        engine = TypeInferenceEngine()
        
        # case None => no bindings
        pattern = OptionPattern("None", None)
        
        option_type = type('GenericType', (), {
            'name': 'Option',
            'type_parameters': [STRING_TYPE]
        })()
        
        bindings = engine.infer_pattern_binding_type(pattern, option_type)
        
        assert len(bindings) == 0
    
    def test_result_ok_pattern(self):
        """Test Result Ok pattern unwraps ok type."""
        engine = TypeInferenceEngine()
        
        # case Ok with value => value gets T from Result<T, E>
        pattern = ResultPattern("Ok", "value")
        
        # Create Result<String, Integer> type
        result_type = type('GenericType', (), {
            'name': 'Result',
            'type_parameters': [STRING_TYPE, INTEGER_TYPE]
        })()
        
        bindings = engine.infer_pattern_binding_type(pattern, result_type)
        
        assert "value" in bindings
        assert bindings["value"] == STRING_TYPE
    
    def test_result_err_pattern(self):
        """Test Result Err pattern unwraps error type."""
        engine = TypeInferenceEngine()
        
        # case Err with error => error gets E from Result<T, E>
        pattern = ResultPattern("Err", "error")
        
        # Create Result<String, Integer> type
        result_type = type('GenericType', (), {
            'name': 'Result',
            'type_parameters': [STRING_TYPE, INTEGER_TYPE]
        })()
        
        bindings = engine.infer_pattern_binding_type(pattern, result_type)
        
        assert "error" in bindings
        assert bindings["error"] == INTEGER_TYPE
    
    def test_list_pattern_elements(self):
        """Test list pattern binds elements to element type."""
        engine = TypeInferenceEngine()
        
        # case [first, second] => both get element type
        first_pattern = IdentifierPattern("first")
        second_pattern = IdentifierPattern("second")
        
        list_pattern = type('ListPattern', (), {
            'elements': [first_pattern, second_pattern],
            'rest': None
        })()
        
        list_type = ListType(INTEGER_TYPE)
        bindings = engine.infer_pattern_binding_type(list_pattern, list_type)
        
        assert "first" in bindings
        assert "second" in bindings
        assert bindings["first"] == INTEGER_TYPE
        assert bindings["second"] == INTEGER_TYPE
    
    def test_list_pattern_with_rest(self):
        """Test list pattern with rest captures remaining as list."""
        engine = TypeInferenceEngine()
        
        # case [head, ...tail] => head gets T, tail gets List<T>
        head_pattern = IdentifierPattern("head")
        
        list_pattern = type('ListPattern', (), {
            'elements': [head_pattern],
            'rest': 'tail'
        })()
        
        list_type = ListType(STRING_TYPE)
        bindings = engine.infer_pattern_binding_type(list_pattern, list_type)
        
        assert "head" in bindings
        assert "tail" in bindings
        assert bindings["head"] == STRING_TYPE
        assert isinstance(bindings["tail"], ListType)
        assert bindings["tail"].element_type == STRING_TYPE


class TestPatternMatchingIntegration:
    """Test integration of pattern type inference with type checker."""
    
    def test_match_expression_type_unification(self):
        """Test that match expression infers unified return type."""
        engine = TypeInferenceEngine()
        
        # match x with
        #   case 0 => "zero"
        #   case 1 => "one"
        #   case _ => "other"
        # All branches return String, so match type is String
        
        # In a real scenario, the type checker would use unify_types
        type1 = STRING_TYPE
        type2 = STRING_TYPE
        type3 = STRING_TYPE
        
        unified = engine.unify_types(type1, type2)
        assert unified == STRING_TYPE
        
        unified = engine.unify_types(unified, type3)
        assert unified == STRING_TYPE
    
    def test_match_expression_divergent_types(self):
        """Test match with incompatible branch types falls back to ANY."""
        engine = TypeInferenceEngine()
        
        # match x with
        #   case 0 => 42      (Integer)
        #   case 1 => "one"   (String)
        # Types can't unify => ANY_TYPE
        
        unified = engine.unify_types(INTEGER_TYPE, STRING_TYPE)
        # Should return None or ANY_TYPE for incompatible types
        assert unified is None or unified == ANY_TYPE


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
