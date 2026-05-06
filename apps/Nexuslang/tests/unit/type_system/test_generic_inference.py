"""
Tests for generic type parameter inference.
"""

import pytest
from nexuslang.typesystem.generic_inference import (
    GenericTypeInference, TypeSubstitution, infer_generic_types
)
from nexuslang.typesystem.types import (
    ListType, DictionaryType, INTEGER_TYPE, STRING_TYPE, FLOAT_TYPE, BOOLEAN_TYPE
)


class TestTypeSubstitution:
    """Test type variable substitution mechanism."""
    
    def test_bind_type_variable(self):
        """Test binding a type variable to a concrete type."""
        subst = TypeSubstitution()
        assert subst.bind('T', INTEGER_TYPE) is True
        assert subst.get('T') is INTEGER_TYPE
    
    def test_consistent_binding(self):
        """Test that consistent bindings are allowed."""
        subst = TypeSubstitution()
        subst.bind('T', INTEGER_TYPE)
        # Rebinding to compatible type should succeed
        assert subst.bind('T', INTEGER_TYPE) is True
    
    def test_apply_substitution(self):
        """Test applying substitutions to type annotations."""
        subst = TypeSubstitution()
        subst.bind('T', INTEGER_TYPE)
        
        result = subst.apply('List<T>')
        assert 'integer' in result.lower()


class TestGenericTypeInference:
    """Test generic type inference from arguments."""
    
    def test_infer_single_type_variable(self):
        """Test inferring a single type variable from argument."""
        inferrer = GenericTypeInference()
        
        type_params = ['T']
        param_types = ['T']
        arg_types = [INTEGER_TYPE]
        
        result = inferrer.infer_from_arguments(type_params, param_types, arg_types)
        
        assert 'T' in result
        assert result['T'] is INTEGER_TYPE
    
    def test_infer_from_list_argument(self):
        """Test inferring type variable from List<T> argument."""
        inferrer = GenericTypeInference()
        
        type_params = ['T']
        param_types = ['List<T>']
        arg_types = [ListType(STRING_TYPE)]
        
        result = inferrer.infer_from_arguments(type_params, param_types, arg_types)
        
        assert 'T' in result
        assert result['T'] is STRING_TYPE
    
    def test_infer_from_dictionary_argument(self):
        """Test inferring type variables from Dictionary<K, V> argument."""
        inferrer = GenericTypeInference()
        
        type_params = ['K', 'V']
        param_types = ['Dictionary<K, V>']
        arg_types = [DictionaryType(STRING_TYPE, INTEGER_TYPE)]
        
        result = inferrer.infer_from_arguments(type_params, param_types, arg_types)
        
        assert 'K' in result
        assert 'V' in result
        assert result['K'] is STRING_TYPE
        assert result['V'] is INTEGER_TYPE
    
    def test_infer_from_nested_list(self):
        """Test inferring from nested generic types."""
        inferrer = GenericTypeInference()
        
        type_params = ['T']
        param_types = ['List<List<T>>']
        arg_types = [ListType(ListType(FLOAT_TYPE))]
        
        result = inferrer.infer_from_arguments(type_params, param_types, arg_types)
        
        assert 'T' in result
        assert result['T'] is FLOAT_TYPE
    
    def test_infer_multiple_parameters(self):
        """Test inferring from multiple function parameters."""
        inferrer = GenericTypeInference()
        
        type_params = ['T', 'R']
        param_types = ['List<T>', 'R']
        arg_types = [ListType(INTEGER_TYPE), STRING_TYPE]
        
        result = inferrer.infer_from_arguments(type_params, param_types, arg_types)
        
        assert 'T' in result
        assert 'R' in result
        assert result['T'] is INTEGER_TYPE
        assert result['R'] is STRING_TYPE
    
    def test_substitute_return_type_simple(self):
        """Test substituting a simple type variable in return type."""
        inferrer = GenericTypeInference()
        
        substitutions = {'T': INTEGER_TYPE}
        result = inferrer.substitute_return_type('T', substitutions)
        
        assert result is INTEGER_TYPE
    
    def test_substitute_return_type_list(self):
        """Test substituting type variable in List<T> return type."""
        inferrer = GenericTypeInference()
        
        substitutions = {'T': STRING_TYPE}
        result = inferrer.substitute_return_type('List<T>', substitutions)
        
        assert isinstance(result, ListType)
        assert result.element_type is STRING_TYPE
    
    def test_substitute_return_type_dictionary(self):
        """Test substituting type variables in Dictionary<K, V> return type."""
        inferrer = GenericTypeInference()
        
        substitutions = {'K': STRING_TYPE, 'V': INTEGER_TYPE}
        result = inferrer.substitute_return_type('Dictionary<K, V>', substitutions)
        
        assert isinstance(result, DictionaryType)
        assert result.key_type is STRING_TYPE
        assert result.value_type is INTEGER_TYPE
    
    def test_substitute_nested_return_type(self):
        """Test substituting in nested generic return type."""
        inferrer = GenericTypeInference()
        
        substitutions = {'T': BOOLEAN_TYPE}
        result = inferrer.substitute_return_type('List<List<T>>', substitutions)
        
        assert isinstance(result, ListType)
        assert isinstance(result.element_type, ListType)
        assert result.element_type.element_type is BOOLEAN_TYPE


class TestInferGenericTypesFunction:
    """Test the convenience function for generic type inference."""
    
    def test_infer_map_function(self):
        """Test inferring types for a map-like function."""
        # function map<T, R> that takes items as List<T> returns List<R>
        # Called with: map([1, 2, 3])
        
        type_params = ['T', 'R']
        param_types = ['List<T>']
        arg_types = [ListType(INTEGER_TYPE)]
        
        result = infer_generic_types(type_params, param_types, arg_types)
        
        assert 'T' in result
        assert result['T'] is INTEGER_TYPE
    
    def test_infer_identity_function(self):
        """Test inferring types for an identity function."""
        # function identity<T> that takes value as T returns T
        # Called with: identity("hello")
        
        type_params = ['T']
        param_types = ['T']
        arg_types = [STRING_TYPE]
        
        result = infer_generic_types(type_params, param_types, arg_types)
        
        assert 'T' in result
        assert result['T'] is STRING_TYPE
    
    def test_infer_filter_function(self):
        """Test inferring types for a filter-like function."""
        # function filter<T> that takes items as List<T> returns List<T>
        # Called with: filter([1.0, 2.0, 3.0])
        
        type_params = ['T']
        param_types = ['List<T>']
        arg_types = [ListType(FLOAT_TYPE)]
        
        result = infer_generic_types(type_params, param_types, arg_types)
        
        assert 'T' in result
        assert result['T'] is FLOAT_TYPE


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
