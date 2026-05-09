"""Regression tests for LLVM class member/property type resolution."""

import os
import sys

import pytest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', '..', 'src'))

from nexuslang.compiler.backends.llvm_ir_generator import LLVMIRGenerator
from nexuslang.parser.ast import Identifier, MemberAccess, VariantPattern


def test_member_access_type_inference_uses_declared_property_type():
    generator = LLVMIRGenerator()
    generator.class_types['Widget'] = {
        'properties': [{'name': 'label', 'type': 'String', 'visibility': 'public'}],
        'methods': [],
        'parent': None,
        'interfaces': [],
    }
    generator.local_vars['widget'] = ('%Widget*', '%widget')

    llvm_type = generator._infer_member_access_type_expr(MemberAccess(Identifier('widget'), 'label'))

    assert llvm_type == 'i8*'


def test_class_property_access_without_type_metadata_fails_fast():
    generator = LLVMIRGenerator()
    generator.class_types['Broken'] = {
        'properties': [{'name': 'value', 'visibility': 'public'}],
        'methods': [],
        'parent': None,
        'interfaces': [],
    }

    with pytest.raises(RuntimeError, match="Unable to resolve type metadata for property 'value'"):
        generator._generate_class_property_access('Broken', 'value', '%obj')


def test_variant_pattern_binding_without_property_type_metadata_fails_fast():
    generator = LLVMIRGenerator()
    generator.class_metadata['Result'] = {
        'properties': [{'name': 'value'}],
    }

    with pytest.raises(RuntimeError, match="Unable to resolve type metadata for property 'value'"):
        generator._generate_variant_pattern_binding(VariantPattern('Ok', ['payload']), '%match', 'Result', '')
