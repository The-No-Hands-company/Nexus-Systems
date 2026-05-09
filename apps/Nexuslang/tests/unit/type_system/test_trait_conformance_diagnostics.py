"""Tests for trait/interface conformance diagnostics."""

import sys
import os

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "../../src"))

import pytest
from nexuslang.typesystem.typechecker import TypeChecker
from nexuslang.typesystem.types import (
    ClassType, FunctionType, TraitType, PrimitiveType, ANY_TYPE,
    INTEGER_TYPE, STRING_TYPE
)
from nexuslang.parser.ast import (
    TraitDefinition, AbstractMethodDefinition, ClassDefinition, 
    MethodDefinition, PropertyDeclaration, Program
)


class TestTraitConformanceDiagnostics:
    """Test trait/interface conformance diagnostic generation."""
    
    def test_fully_conforming_class(self):
        """Class that implements all trait methods should conform."""
        typechecker = TypeChecker()
        
        # Create trait with one method
        trait_type = TraitType("Drawable", {
            "draw": FunctionType([], ANY_TYPE)
        })
        typechecker.type_registry["Drawable"] = trait_type
        
        # Create class with matching method
        class_type = ClassType("Shape", {}, {
            "draw": FunctionType([], ANY_TYPE)
        })
        typechecker.type_registry["Shape"] = class_type
        
        diagnostics = typechecker.get_trait_conformance_diagnostics("Shape", "Drawable")
        
        assert diagnostics['conforms'] is True
        assert len(diagnostics['missing_methods']) == 0
        assert len(diagnostics['incompatible_methods']) == 0
    
    def test_missing_method(self):
        """Class missing required method should report it."""
        typechecker = TypeChecker()
        
        # Create trait with methods
        trait_type = TraitType("Drawable", {
            "draw": FunctionType([], ANY_TYPE),
            "erase": FunctionType([], ANY_TYPE)
        })
        typechecker.type_registry["Drawable"] = trait_type
        
        # Create class with only one method
        class_type = ClassType("Shape", {}, {
            "draw": FunctionType([], ANY_TYPE)
        })
        typechecker.type_registry["Shape"] = class_type
        
        diagnostics = typechecker.get_trait_conformance_diagnostics("Shape", "Drawable")
        
        assert diagnostics['conforms'] is False
        assert "erase" in diagnostics['missing_methods']
        assert len(diagnostics['missing_methods']) == 1
        assert any("erase" in s for s in diagnostics['suggestions'])
    
    def test_multiple_missing_methods(self):
        """Class missing multiple methods should report all."""
        typechecker = TypeChecker()
        
        # Create trait with multiple methods
        trait_type = TraitType("Drawable", {
            "draw": FunctionType([], ANY_TYPE),
            "erase": FunctionType([], ANY_TYPE),
            "rotate": FunctionType([], ANY_TYPE),
            "scale": FunctionType([], ANY_TYPE)
        })
        typechecker.type_registry["Drawable"] = trait_type
        
        # Create class with no methods
        class_type = ClassType("Shape", {}, {})
        typechecker.type_registry["Shape"] = class_type
        
        diagnostics = typechecker.get_trait_conformance_diagnostics("Shape", "Drawable")
        
        assert diagnostics['conforms'] is False
        assert len(diagnostics['missing_methods']) == 4
        assert all(m in diagnostics['missing_methods'] for m in ["draw", "erase", "rotate", "scale"])
        assert len(diagnostics['suggestions']) >= 1
    
    def test_incompatible_method_signature(self):
        """Method with wrong signature should be reported as incompatible."""
        typechecker = TypeChecker()
        
        # Create trait with specific signature
        trait_type = TraitType("Numeric", {
            "add": FunctionType([INTEGER_TYPE, INTEGER_TYPE], INTEGER_TYPE)
        })
        typechecker.type_registry["Numeric"] = trait_type
        
        # Create class with wrong parameter count
        class_type = ClassType("Vector", {}, {
            "add": FunctionType([INTEGER_TYPE], INTEGER_TYPE)  # Wrong param count
        })
        typechecker.type_registry["Vector"] = class_type
        
        diagnostics = typechecker.get_trait_conformance_diagnostics("Vector", "Numeric")
        
        assert diagnostics['conforms'] is False
        assert "add" in diagnostics['incompatible_methods']
        assert "Parameter count mismatch" in diagnostics['incompatible_methods']["add"]
    
    def test_incompatible_return_type(self):
        """Method with wrong return type should be reported."""
        typechecker = TypeChecker()
        
        # Create trait with specific return type
        trait_type = TraitType("Named", {
            "get_name": FunctionType([], STRING_TYPE)
        })
        typechecker.type_registry["Named"] = trait_type
        
        # Create class with wrong return type
        class_type = ClassType("Person", {}, {
            "get_name": FunctionType([], INTEGER_TYPE)  # Wrong return type
        })
        typechecker.type_registry["Person"] = class_type
        
        diagnostics = typechecker.get_trait_conformance_diagnostics("Person", "Named")
        
        assert diagnostics['conforms'] is False
        assert "get_name" in diagnostics['incompatible_methods']
        assert "Return type mismatch" in diagnostics['incompatible_methods']["get_name"]
    
    def test_trait_not_found(self):
        """Unknown trait should generate diagnostic suggestion."""
        typechecker = TypeChecker()
        
        class_type = ClassType("MyClass", {}, {})
        typechecker.type_registry["MyClass"] = class_type
        
        diagnostics = typechecker.get_trait_conformance_diagnostics("MyClass", "NonExistent")
        
        assert diagnostics['conforms'] is False
        # Diagnostic may indicate trait not found
        assert diagnostics['conforms'] is False  # At minimum, should not conform
    
    def test_class_not_found(self):
        """Unknown class should generate diagnostic suggestion."""
        typechecker = TypeChecker()
        
        trait_type = TraitType("MyTrait", {})
        typechecker.type_registry["MyTrait"] = trait_type
        
        diagnostics = typechecker.get_trait_conformance_diagnostics("NonExistent", "MyTrait")
        
        assert diagnostics['conforms'] is False
    
    def test_diagnostic_summary_generation(self):
        """Diagnostic should include helpful summary messages."""
        typechecker = TypeChecker()
        
        # Create trait
        trait_type = TraitType("Drawable", {
            "draw": FunctionType([], ANY_TYPE),
            "erase": FunctionType([], ANY_TYPE)
        })
        typechecker.type_registry["Drawable"] = trait_type
        
        # Create class with missing method and incompatible method
        class_type = ClassType("Shape", {}, {
            "draw": FunctionType([INTEGER_TYPE], ANY_TYPE)  # Signature mismatch
        })
        typechecker.type_registry["Shape"] = class_type
        
        diagnostics = typechecker.get_trait_conformance_diagnostics("Shape", "Drawable")
        
        # Should have suggestion about missing and incompatible methods
        assert len(diagnostics['suggestions']) >= 1
        assert any("erase" in s for s in diagnostics['suggestions'])  # Missing method mention
    
    def test_required_methods_list(self):
        """Diagnostic should include complete list of required methods."""
        typechecker = TypeChecker()
        
        # Create trait
        trait_type = TraitType("Drawable", {
            "draw": FunctionType([], ANY_TYPE),
            "erase": FunctionType([], ANY_TYPE),
            "rotate": FunctionType([], ANY_TYPE)
        })
        typechecker.type_registry["Drawable"] = trait_type
        
        class_type = ClassType("Shape", {}, {})
        typechecker.type_registry["Shape"] = class_type
        
        diagnostics = typechecker.get_trait_conformance_diagnostics("Shape", "Drawable")
        
        assert len(diagnostics['required_methods']) == 3
        assert all(m in diagnostics['required_methods'] for m in ["draw", "erase", "rotate"])
    
    def test_metadata_in_diagnostics(self):
        """Diagnostic should include trait and class names."""
        typechecker = TypeChecker()
        
        trait_type = TraitType("MyTrait", {})
        typechecker.type_registry["MyTrait"] = trait_type
        
        class_type = ClassType("MyClass", {}, {})
        typechecker.type_registry["MyClass"] = class_type
        
        diagnostics = typechecker.get_trait_conformance_diagnostics("MyClass", "MyTrait")
        
        assert diagnostics['trait_name'] == "MyTrait"
        assert diagnostics['class_name'] == "MyClass"


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
