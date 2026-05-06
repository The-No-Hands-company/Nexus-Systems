"""
Tests for NexusLang 8.2 Attribute System.

Groups:
    TestAttributeDeclaration    -- `attribute Name [with ...]` parsing and registration
    TestAttributeOnClass        -- @Attribute decorator applied to class definitions
    TestReflectHasAttribute     -- reflect_has_attribute: True/False checks
    TestReflectListAttributes   -- reflect_list_attributes: returns all applied names
    TestReflectGetAttribute     -- reflect_get_attribute: returns property dict
    TestReflectAttributeValue   -- reflect_attribute_value: returns single property
    TestAttributeIntegration    -- end-to-end via full NexusLang interpreter pipeline
"""

import sys
import os

_PROJECT_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), "..", "..", "..")
)
if _PROJECT_ROOT not in sys.path:
    sys.path.insert(0, _PROJECT_ROOT)

import pytest


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _make_runtime():
    from nexuslang.runtime.runtime import Runtime
    from nexuslang.stdlib import register_stdlib
    rt = Runtime()
    register_stdlib(rt)
    return rt


def _make_object(class_name, properties=None):
    from nexuslang.runtime.runtime import Object
    return Object(class_name, properties or {})


def _interp(src):
    """Parse and execute NexusLang source; return the Interpreter instance."""
    from nexuslang.interpreter.interpreter import Interpreter
    from nexuslang.runtime.runtime import Runtime
    from nexuslang.stdlib import register_stdlib
    from nexuslang.parser.parser import Parser
    from nexuslang.parser.lexer import Lexer
    rt = Runtime()
    register_stdlib(rt)
    prog = Parser(Lexer(src).tokenize()).parse()
    i = Interpreter(runtime=rt)
    i.interpret(prog)
    return i


# ---------------------------------------------------------------------------
# 1. Attribute declaration
# ---------------------------------------------------------------------------

class TestAttributeDeclaration:
    def test_simple_attribute_registers(self):
        i = _interp("attribute Serializable")
        assert "Serializable" in i.attribute_definitions

    def test_attribute_with_one_property(self):
        i = _interp("attribute JsonProperty with key as String")
        assert "JsonProperty" in i.attribute_definitions
        props = i.attribute_definitions["JsonProperty"]["properties"]
        assert len(props) == 1
        assert props[0][0] == "key"
        assert props[0][1] == "String"

    def test_attribute_with_two_properties_comma(self):
        i = _interp("attribute Range with lo as Integer, hi as Integer")
        assert "Range" in i.attribute_definitions
        props = i.attribute_definitions["Range"]["properties"]
        assert len(props) == 2
        assert props[0][0] == "lo"
        assert props[1][0] == "hi"

    def test_attribute_with_two_properties_and(self):
        i = _interp("attribute Range with lo as Integer and hi as Integer")
        assert "Range" in i.attribute_definitions
        props = i.attribute_definitions["Range"]["properties"]
        assert len(props) == 2
        assert props[0][0] == "lo"
        assert props[1][0] == "hi"

    def test_attribute_without_properties_has_empty_list(self):
        i = _interp("attribute Marker")
        assert i.attribute_definitions["Marker"]["properties"] == []

    def test_multiple_attributes_all_registered(self):
        src = (
            "attribute Serializable\n"
            "attribute Cacheable\n"
            "attribute Deprecated\n"
        )
        i = _interp(src)
        assert "Serializable" in i.attribute_definitions
        assert "Cacheable" in i.attribute_definitions
        assert "Deprecated" in i.attribute_definitions

    def test_attribute_declaration_returns_name(self):
        # No runtime error means the declaration succeeded
        i = _interp("attribute Frozen")
        assert "Frozen" in i.attribute_definitions


# ---------------------------------------------------------------------------
# 2. Attribute application on class definitions
# ---------------------------------------------------------------------------

class TestAttributeOnClass:
    def test_attribute_on_class_no_error(self):
        src = (
            "attribute Serializable\n"
            "@Serializable\n"
            "class Widget\n"
            "    property name as String\n"
            "end\n"
        )
        i = _interp(src)
        assert "Widget" in i.classes

    def test_attribute_stored_on_class_node(self):
        src = (
            "attribute Serializable\n"
            "@Serializable\n"
            "class Item\n"
            "    property value as Integer\n"
            "end\n"
        )
        i = _interp(src)
        class_node = i.classes["Item"]
        assert hasattr(class_node, "_applied_attributes")
        assert "Serializable" in class_node._applied_attributes

    def test_attribute_mirrored_to_runtime(self):
        src = (
            "attribute Serializable\n"
            "@Serializable\n"
            "class Box\n"
            "    property size as Integer\n"
            "end\n"
        )
        i = _interp(src)
        assert "Box" in i.runtime._class_attributes
        assert "Serializable" in i.runtime._class_attributes["Box"]

    def test_attribute_with_string_property_stored(self):
        src = (
            'attribute JsonProperty with key as String\n'
            '@JsonProperty with key "user_name"\n'
            "class User\n"
            "    property name as String\n"
            "end\n"
        )
        i = _interp(src)
        class_node = i.classes["User"]
        assert hasattr(class_node, "_applied_attributes")
        attr = class_node._applied_attributes.get("JsonProperty", {})
        assert attr.get("key") == "user_name"

    def test_multiple_attributes_stored_on_class(self):
        src = (
            "attribute Serializable\n"
            "attribute Cacheable\n"
            "@Serializable\n"
            "@Cacheable\n"
            "class Product\n"
            "    property price as Float\n"
            "end\n"
        )
        i = _interp(src)
        class_node = i.classes["Product"]
        assert "Serializable" in class_node._applied_attributes
        assert "Cacheable" in class_node._applied_attributes

    def test_attribute_does_not_break_instantiation(self):
        src = (
            "attribute Serializable\n"
            "@Serializable\n"
            "class Point\n"
            "    property x as Integer\n"
            "    property y as Integer\n"
            "end\n"
            "set p to new Point\n"
            "set p.x to 10\n"
            "set p.y to 20\n"
        )
        i = _interp(src)
        p = i.get_variable("p")
        assert p.properties["x"] == 10
        assert p.properties["y"] == 20


# ---------------------------------------------------------------------------
# 3. reflect_has_attribute — Python-level unit tests
# ---------------------------------------------------------------------------

class TestReflectHasAttribute:
    def setup_method(self):
        from nexuslang.stdlib.reflection import reflect_has_attribute
        self.rt = _make_runtime()
        self.fn = lambda obj, name: reflect_has_attribute(self.rt, obj, name)

    def test_returns_true_when_attribute_present(self):
        obj = _make_object("Widget")
        self.rt._class_attributes["Widget"] = {"Serializable": {}}
        assert self.fn(obj, "Serializable") is True

    def test_returns_false_when_attribute_absent(self):
        obj = _make_object("Widget")
        self.rt._class_attributes["Widget"] = {"Serializable": {}}
        assert self.fn(obj, "Cacheable") is False

    def test_returns_false_for_class_with_no_attributes(self):
        obj = _make_object("EmptyClass")
        assert self.fn(obj, "Serializable") is False

    def test_returns_false_for_integer(self):
        assert self.fn(42, "Attr") is False

    def test_returns_false_for_none(self):
        assert self.fn(None, "Attr") is False

    def test_returns_false_for_string_value(self):
        assert self.fn("hello", "Attr") is False

    def test_correct_scoping_between_classes(self):
        a = _make_object("ClassA")
        b = _make_object("ClassB")
        self.rt._class_attributes["ClassA"] = {"Attr1": {}}
        self.rt._class_attributes["ClassB"] = {"Attr2": {}}
        assert self.fn(a, "Attr1") is True
        assert self.fn(a, "Attr2") is False
        assert self.fn(b, "Attr2") is True
        assert self.fn(b, "Attr1") is False


# ---------------------------------------------------------------------------
# 4. reflect_list_attributes — Python-level unit tests
# ---------------------------------------------------------------------------

class TestReflectListAttributes:
    def setup_method(self):
        from nexuslang.stdlib.reflection import reflect_list_attributes
        self.rt = _make_runtime()
        self.fn = lambda obj: reflect_list_attributes(self.rt, obj)

    def test_empty_list_for_class_with_no_attributes(self):
        obj = _make_object("Plain")
        assert self.fn(obj) == []

    def test_single_attribute(self):
        obj = _make_object("Widget")
        self.rt._class_attributes["Widget"] = {"Serializable": {}}
        assert self.fn(obj) == ["Serializable"]

    def test_multiple_attributes_sorted(self):
        obj = _make_object("Service")
        self.rt._class_attributes["Service"] = {
            "Cacheable": {},
            "Serializable": {},
            "Auditable": {},
        }
        result = self.fn(obj)
        assert result == sorted(["Cacheable", "Serializable", "Auditable"])

    def test_empty_list_for_integer(self):
        assert self.fn(99) == []

    def test_empty_list_for_list_value(self):
        assert self.fn([1, 2]) == []

    def test_returns_list_type(self):
        obj = _make_object("Entity")
        assert isinstance(self.fn(obj), list)


# ---------------------------------------------------------------------------
# 5. reflect_get_attribute — Python-level unit tests
# ---------------------------------------------------------------------------

class TestReflectGetAttribute:
    def setup_method(self):
        from nexuslang.stdlib.reflection import reflect_get_attribute
        self.rt = _make_runtime()
        self.fn = lambda obj, name: reflect_get_attribute(self.rt, obj, name)

    def test_returns_dict_for_present_attribute(self):
        obj = _make_object("Widget")
        self.rt._class_attributes["Widget"] = {"Serializable": {}}
        assert isinstance(self.fn(obj, "Serializable"), dict)

    def test_returns_empty_dict_for_no_properties(self):
        obj = _make_object("Tag")
        self.rt._class_attributes["Tag"] = {"Marker": {}}
        assert self.fn(obj, "Marker") == {}

    def test_returns_property_values(self):
        obj = _make_object("User")
        self.rt._class_attributes["User"] = {"JsonProperty": {"key": "uid"}}
        result = self.fn(obj, "JsonProperty")
        assert result == {"key": "uid"}

    def test_returns_error_dict_for_absent_attribute(self):
        obj = _make_object("Item")
        result = self.fn(obj, "NonExistent")
        assert "error" in result

    def test_error_message_contains_attribute_name(self):
        obj = _make_object("Thing")
        result = self.fn(obj, "MissingAttr")
        assert "MissingAttr" in result["error"]

    def test_multiple_properties_returned(self):
        obj = _make_object("Score")
        self.rt._class_attributes["Score"] = {
            "Range": {"min": 0, "max": 100}
        }
        result = self.fn(obj, "Range")
        assert result["min"] == 0
        assert result["max"] == 100


# ---------------------------------------------------------------------------
# 6. reflect_attribute_value — Python-level unit tests
# ---------------------------------------------------------------------------

class TestReflectAttributeValue:
    def setup_method(self):
        from nexuslang.stdlib.reflection import reflect_attribute_value
        self.rt = _make_runtime()
        self.fn = lambda obj, attr, prop: reflect_attribute_value(self.rt, obj, attr, prop)

    def test_returns_string_property_value(self):
        obj = _make_object("User")
        self.rt._class_attributes["User"] = {"JsonProperty": {"key": "user_name"}}
        assert self.fn(obj, "JsonProperty", "key") == "user_name"

    def test_returns_integer_property_value(self):
        obj = _make_object("Limit")
        self.rt._class_attributes["Limit"] = {"Range": {"min": 1, "max": 99}}
        assert self.fn(obj, "Range", "min") == 1
        assert self.fn(obj, "Range", "max") == 99

    def test_returns_none_for_absent_attribute(self):
        obj = _make_object("Item")
        assert self.fn(obj, "NonExistent", "prop") is None

    def test_returns_none_for_absent_property(self):
        obj = _make_object("Flag")
        self.rt._class_attributes["Flag"] = {"Marker": {}}
        assert self.fn(obj, "Marker", "nonexistent") is None

    def test_returns_none_for_non_object(self):
        assert self.fn(42, "Attr", "prop") is None


# ---------------------------------------------------------------------------
# 7. End-to-end integration via NexusLang interpreter
# ---------------------------------------------------------------------------

class TestAttributeIntegration:
    def test_reflect_has_attribute_true_via_nxl(self):
        src = (
            "attribute Serializable\n"
            "@Serializable\n"
            "class Config\n"
            "    property debug as Boolean\n"
            "end\n"
            "set c to new Config\n"
            'set result to reflect_has_attribute with c and "Serializable"\n'
        )
        i = _interp(src)
        assert i.get_variable("result") is True

    def test_reflect_has_attribute_false_when_not_applied(self):
        src = (
            "attribute Serializable\n"
            "class Config\n"
            "    property debug as Boolean\n"
            "end\n"
            "set c to new Config\n"
            'set result to reflect_has_attribute with c and "Serializable"\n'
        )
        i = _interp(src)
        assert i.get_variable("result") is False

    def test_reflect_list_attributes_single(self):
        src = (
            "attribute Serializable\n"
            "@Serializable\n"
            "class Doc\n"
            "    property title as String\n"
            "end\n"
            "set d to new Doc\n"
            "set attrs to reflect_list_attributes with d\n"
        )
        i = _interp(src)
        assert i.get_variable("attrs") == ["Serializable"]

    def test_reflect_list_attributes_multiple(self):
        src = (
            "attribute Serializable\n"
            "attribute Cacheable\n"
            "@Serializable\n"
            "@Cacheable\n"
            "class Record\n"
            "    property id as Integer\n"
            "end\n"
            "set r to new Record\n"
            "set attrs to reflect_list_attributes with r\n"
        )
        i = _interp(src)
        result = i.get_variable("attrs")
        assert sorted(result) == ["Cacheable", "Serializable"]

    def test_reflect_get_attribute_marker(self):
        src = (
            "attribute Frozen\n"
            "@Frozen\n"
            "class Constant\n"
            "    property value as Integer\n"
            "end\n"
            "set obj to new Constant\n"
            'set result to reflect_get_attribute with obj and "Frozen"\n'
        )
        i = _interp(src)
        result = i.get_variable("result")
        assert isinstance(result, dict)
        assert "error" not in result

    def test_reflect_has_attribute_false_unknown_class(self):
        src = (
            "set x to 42\n"
            'set result to reflect_has_attribute with x and "Serializable"\n'
        )
        i = _interp(src)
        assert i.get_variable("result") is False
