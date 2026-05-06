"""
Tests for the NexusLang stdlib reflection module (8.2 Reflection).

Groups:
    TestReflectTypeOf         -- reflect_type_of: all primitive and object kinds
    TestReflectIsInstanceOf   -- reflect_is_instance_of: exact + alias matching
    TestReflectStructUnit     -- struct introspection (fields_of, field_names,
                                 struct_size, is_struct, class_name)
    TestReflectClassUnit      -- class instance introspection (properties_of,
                                 methods_of, is_class_instance)
    TestReflectFieldAccess    -- reflect_has_field, reflect_get_field,
                                 reflect_set_field on both structs and classes
    TestReflectMethodAccess   -- reflect_has_method
    TestReflectDescribe       -- reflect_describe for all three value kinds
    TestReflectIntegration    -- end-to-end via NexusLang interpreter
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
# Fixtures
# ---------------------------------------------------------------------------

def _make_runtime():
    from nexuslang.runtime.runtime import Runtime
    from nexuslang.stdlib import register_stdlib
    rt = Runtime()
    register_stdlib(rt)
    return rt


def _make_struct_instance(struct_name, fields):
    """Helper: create a StructureInstance from a list of (field_name, type_str)."""
    from nexuslang.runtime.structures import StructDefinition, StructureInstance
    defn = StructDefinition(struct_name, fields)
    return StructureInstance(defn)


def _make_object(class_name, properties=None, methods=None):
    """Helper: create a runtime Object with optional properties and methods."""
    from nexuslang.runtime.runtime import Object
    obj = Object(class_name, properties or {})
    if methods:
        for name, func in methods.items():
            obj.add_method(name, func)
    return obj


def _rt():
    """Dummy runtime sentinel (reflection functions ignore it)."""
    return None


# ---------------------------------------------------------------------------
# 1. reflect_type_of
# ---------------------------------------------------------------------------

class TestReflectTypeOf:
    def setup_method(self):
        from nexuslang.stdlib.reflection import reflect_type_of
        self.fn = lambda v: reflect_type_of(_rt(), v)

    def test_none_is_null(self):
        assert self.fn(None) == "Null"

    def test_bool_true(self):
        assert self.fn(True) == "Boolean"

    def test_bool_false(self):
        assert self.fn(False) == "Boolean"

    def test_int_zero(self):
        assert self.fn(0) == "Integer"

    def test_int_positive(self):
        assert self.fn(42) == "Integer"

    def test_int_negative(self):
        assert self.fn(-7) == "Integer"

    def test_float(self):
        assert self.fn(3.14) == "Float"

    def test_string(self):
        assert self.fn("hello") == "String"

    def test_empty_string(self):
        assert self.fn("") == "String"

    def test_bytes(self):
        assert self.fn(b"\x00\xff") == "Bytes"

    def test_list(self):
        assert self.fn([1, 2, 3]) == "List"

    def test_empty_list(self):
        assert self.fn([]) == "List"

    def test_dict(self):
        assert self.fn({"a": 1}) == "Dictionary"

    def test_empty_dict(self):
        assert self.fn({}) == "Dictionary"

    def test_callable(self):
        assert self.fn(lambda: None) == "Function"

    def test_struct_instance(self):
        inst = _make_struct_instance("Point", [("x", "integer"), ("y", "integer")])
        assert self.fn(inst) == "Point"

    def test_class_instance(self):
        obj = _make_object("Animal")
        assert self.fn(obj) == "Animal"


# ---------------------------------------------------------------------------
# 2. reflect_is_instance_of
# ---------------------------------------------------------------------------

class TestReflectIsInstanceOf:
    def setup_method(self):
        from nexuslang.stdlib.reflection import reflect_is_instance_of
        self.fn = lambda v, t: reflect_is_instance_of(_rt(), v, t)

    def test_integer_exact(self):
        assert self.fn(5, "Integer") is True

    def test_integer_alias_int(self):
        assert self.fn(5, "int") is True

    def test_float_exact(self):
        assert self.fn(1.5, "Float") is True

    def test_float_alias_double(self):
        assert self.fn(1.5, "double") is True

    def test_string_exact(self):
        assert self.fn("hi", "String") is True

    def test_string_alias_str(self):
        assert self.fn("hi", "str") is True

    def test_boolean_exact(self):
        assert self.fn(True, "Boolean") is True

    def test_boolean_alias_bool(self):
        assert self.fn(False, "bool") is True

    def test_null_exact(self):
        assert self.fn(None, "Null") is True

    def test_null_alias_none(self):
        assert self.fn(None, "none") is True

    def test_list_exact(self):
        assert self.fn([], "List") is True

    def test_list_alias_array(self):
        assert self.fn([1], "array") is True

    def test_dictionary_exact(self):
        assert self.fn({}, "Dictionary") is True

    def test_dictionary_alias_dict(self):
        assert self.fn({"k": "v"}, "dict") is True

    def test_mismatched_returns_false(self):
        assert self.fn(5, "String") is False

    def test_struct_name_match(self):
        inst = _make_struct_instance("Vec3", [("x", "float"), ("y", "float"), ("z", "float")])
        assert self.fn(inst, "Vec3") is True

    def test_struct_name_mismatch(self):
        inst = _make_struct_instance("Vec3", [("x", "float")])
        assert self.fn(inst, "Point") is False

    def test_bool_is_not_integer(self):
        # Boolean and Integer are separate in NexusLang
        assert self.fn(True, "Integer") is False


# ---------------------------------------------------------------------------
# 3. Struct unit introspection
# ---------------------------------------------------------------------------

class TestReflectStructUnit:
    def setup_method(self):
        from nexuslang.stdlib.reflection import (
            reflect_is_struct, reflect_fields_of, reflect_struct_field_names,
            reflect_struct_size, reflect_class_name,
        )
        self.is_struct = lambda v: reflect_is_struct(_rt(), v)
        self.fields_of = lambda v: reflect_fields_of(_rt(), v)
        self.field_names = lambda v: reflect_struct_field_names(_rt(), v)
        self.struct_size = lambda v: reflect_struct_size(_rt(), v)
        self.class_name = lambda v: reflect_class_name(_rt(), v)

    def _point(self):
        inst = _make_struct_instance("Point", [("x", "integer"), ("y", "integer")])
        inst.set_field("x", 10)
        inst.set_field("y", 20)
        return inst

    def test_is_struct_true_for_struct(self):
        assert self.is_struct(self._point()) is True

    def test_is_struct_false_for_int(self):
        assert self.is_struct(42) is False

    def test_is_struct_false_for_object(self):
        obj = _make_object("Foo")
        assert self.is_struct(obj) is False

    def test_class_name_struct(self):
        assert self.class_name(self._point()) == "Point"

    def test_fields_of_returns_dict(self):
        result = self.fields_of(self._point())
        assert isinstance(result, dict)

    def test_fields_of_correct_keys(self):
        result = self.fields_of(self._point())
        assert set(result.keys()) == {"x", "y"}

    def test_fields_of_correct_values(self):
        result = self.fields_of(self._point())
        assert result["x"] == 10
        assert result["y"] == 20

    def test_fields_of_non_struct_error(self):
        result = self.fields_of(99)
        assert "error" in result

    def test_field_names_list(self):
        names = self.field_names(self._point())
        assert set(names) == {"x", "y"}

    def test_field_names_non_struct_empty(self):
        assert self.field_names("not a struct") == []

    def test_struct_size_positive(self):
        size = self.struct_size(self._point())
        assert size > 0

    def test_struct_size_non_struct_zero(self):
        assert self.struct_size(42) == 0


# ---------------------------------------------------------------------------
# 4. Class instance introspection
# ---------------------------------------------------------------------------

class TestReflectClassUnit:
    def setup_method(self):
        from nexuslang.stdlib.reflection import (
            reflect_is_class_instance, reflect_properties_of, reflect_methods_of,
            reflect_class_name,
        )
        self.is_class = lambda v: reflect_is_class_instance(_rt(), v)
        self.props_of = lambda v: reflect_properties_of(_rt(), v)
        self.methods_of = lambda v: reflect_methods_of(_rt(), v)
        self.class_name = lambda v: reflect_class_name(_rt(), v)

    def _animal(self):
        return _make_object("Animal", {"name": "Leo", "legs": 4},
                            {"speak": lambda self: "roar"})

    def test_is_class_instance_true(self):
        assert self.is_class(self._animal()) is True

    def test_is_class_instance_false_for_int(self):
        assert self.is_class(42) is False

    def test_is_class_instance_false_for_struct(self):
        s = _make_struct_instance("P", [("x", "integer")])
        assert self.is_class(s) is False

    def test_class_name_object(self):
        assert self.class_name(self._animal()) == "Animal"

    def test_properties_of_dict(self):
        result = self.props_of(self._animal())
        assert isinstance(result, dict)
        assert result["name"] == "Leo"
        assert result["legs"] == 4

    def test_properties_of_non_object_error(self):
        result = self.props_of("not an object")
        assert "error" in result

    def test_methods_of_list(self):
        result = self.methods_of(self._animal())
        assert "speak" in result

    def test_methods_of_non_object_empty(self):
        assert self.methods_of(42) == []


# ---------------------------------------------------------------------------
# 5. Field access
# ---------------------------------------------------------------------------

class TestReflectFieldAccess:
    def setup_method(self):
        from nexuslang.stdlib.reflection import (
            reflect_has_field, reflect_get_field, reflect_set_field,
        )
        self.has_field = lambda v, f: reflect_has_field(_rt(), v, f)
        self.get_field = lambda v, f: reflect_get_field(_rt(), v, f)
        self.set_field = lambda v, f, val: reflect_set_field(_rt(), v, f, val)

    def _struct(self):
        inst = _make_struct_instance("S", [("a", "integer"), ("b", "integer")])
        inst.set_field("a", 100)
        inst.set_field("b", 200)
        return inst

    def _obj(self):
        return _make_object("C", {"x": 1, "y": 2})

    # has_field — struct
    def test_has_field_struct_true(self):
        assert self.has_field(self._struct(), "a") is True

    def test_has_field_struct_false(self):
        assert self.has_field(self._struct(), "z") is False

    # has_field — class
    def test_has_field_class_true(self):
        assert self.has_field(self._obj(), "x") is True

    def test_has_field_class_false(self):
        assert self.has_field(self._obj(), "z") is False

    # has_field — primitive
    def test_has_field_primitive_false(self):
        assert self.has_field("string", "len") is False

    # get_field — struct
    def test_get_field_struct_existing(self):
        assert self.get_field(self._struct(), "a") == 100

    def test_get_field_struct_missing_error(self):
        result = self.get_field(self._struct(), "nonexistent")
        assert isinstance(result, dict) and "error" in result

    # get_field — class
    def test_get_field_class_existing(self):
        assert self.get_field(self._obj(), "x") == 1

    def test_get_field_class_missing_error(self):
        result = self.get_field(self._obj(), "missing")
        assert isinstance(result, dict) and "error" in result

    # get_field — primitive
    def test_get_field_primitive_error(self):
        result = self.get_field(42, "anything")
        assert isinstance(result, dict) and "error" in result

    # set_field — class
    def test_set_field_class_returns_true(self):
        obj = self._obj()
        assert self.set_field(obj, "x", 99) is True
        assert obj.properties["x"] == 99

    def test_set_field_class_new_property(self):
        obj = self._obj()
        assert self.set_field(obj, "new_prop", "hello") is True
        assert obj.properties["new_prop"] == "hello"

    # set_field — primitive
    def test_set_field_primitive_false(self):
        assert self.set_field(42, "x", 1) is False


# ---------------------------------------------------------------------------
# 6. Method inspection
# ---------------------------------------------------------------------------

class TestReflectMethodAccess:
    def setup_method(self):
        from nexuslang.stdlib.reflection import reflect_has_method
        self.has_method = lambda v, m: reflect_has_method(_rt(), v, m)

    def _obj_with_methods(self):
        return _make_object("Robot", {}, {"move": lambda s: None, "stop": lambda s: None})

    def test_has_method_existing(self):
        assert self.has_method(self._obj_with_methods(), "move") is True

    def test_has_method_missing(self):
        assert self.has_method(self._obj_with_methods(), "fly") is False

    def test_has_method_no_methods(self):
        obj = _make_object("Empty")
        assert self.has_method(obj, "anything") is False

    def test_has_method_non_object(self):
        assert self.has_method(42, "anything") is False


# ---------------------------------------------------------------------------
# 7. reflect_describe
# ---------------------------------------------------------------------------

class TestReflectDescribe:
    def setup_method(self):
        from nexuslang.stdlib.reflection import reflect_describe
        self.describe = lambda v: reflect_describe(_rt(), v)

    def test_describe_int_kind_primitive(self):
        d = self.describe(5)
        assert d["kind"] == "primitive"
        assert d["type_name"] == "Integer"
        assert d["value"] == 5

    def test_describe_string_kind_primitive(self):
        d = self.describe("hello")
        assert d["kind"] == "primitive"
        assert d["type_name"] == "String"

    def test_describe_null_primitive(self):
        d = self.describe(None)
        assert d["kind"] == "primitive"
        assert d["type_name"] == "Null"

    def test_describe_struct_kind(self):
        inst = _make_struct_instance("Rect", [("w", "integer"), ("h", "integer")])
        inst.set_field("w", 10)
        inst.set_field("h", 5)
        d = self.describe(inst)
        assert d["kind"] == "struct"
        assert d["type_name"] == "Rect"
        assert "fields" in d
        assert d["fields"]["w"] == 10
        assert d["fields"]["h"] == 5
        assert d["size_bytes"] > 0

    def test_describe_class_kind(self):
        obj = _make_object("Dog", {"breed": "Labrador"},
                           {"bark": lambda s: "woof"})
        d = self.describe(obj)
        assert d["kind"] == "class"
        assert d["type_name"] == "Dog"
        assert d["properties"]["breed"] == "Labrador"
        assert "bark" in d["methods"]

    def test_describe_list_primitive(self):
        d = self.describe([1, 2, 3])
        assert d["kind"] == "primitive"
        assert d["type_name"] == "List"

    def test_describe_dict_primitive(self):
        d = self.describe({"k": "v"})
        assert d["kind"] == "primitive"
        assert d["type_name"] == "Dictionary"


# ---------------------------------------------------------------------------
# 8. Integration via NexusLang interpreter
# ---------------------------------------------------------------------------

class TestReflectIntegration:
    """Run NexusLang programs that call reflect_* functions, verify outputs."""

    def _interp(self, src):
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

    def test_reflect_type_of_integer_via_nxl(self):
        i = self._interp(
            'set x to 42\n'
            'set t to reflect_type_of with x\n'
            'expect t to equal "Integer"'
        )
        # If no AssertionError, the test passed.

    def test_reflect_type_of_string_via_nxl(self):
        self._interp(
            'set s to "hello"\n'
            'set t to reflect_type_of with s\n'
            'expect t to equal "String"'
        )

    def test_reflect_type_of_float_via_nxl(self):
        self._interp(
            'set f to 3.14\n'
            'set t to reflect_type_of with f\n'
            'expect t to equal "Float"'
        )

    def test_reflect_type_of_list_via_nxl(self):
        self._interp(
            'set lst to [1, 2, 3]\n'
            'set t to reflect_type_of with lst\n'
            'expect t to equal "List"'
        )

    def test_reflect_type_of_boolean_via_nxl(self):
        self._interp(
            'set b to true\n'
            'set t to reflect_type_of with b\n'
            'expect t to equal "Boolean"'
        )

    def test_reflect_is_instance_of_integer_via_nxl(self):
        self._interp(
            'set n to 10\n'
            'set ok to reflect_is_instance_of with n and "Integer"\n'
            'expect ok to be true'
        )

    def test_reflect_is_instance_of_string_alias_via_nxl(self):
        self._interp(
            'set s to "world"\n'
            'set ok to reflect_is_instance_of with s and "str"\n'
            'expect ok to be true'
        )

    def test_reflect_is_instance_of_mismatch_via_nxl(self):
        self._interp(
            'set n to 5\n'
            'set ok to reflect_is_instance_of with n and "String"\n'
            'expect ok to be false'
        )

    def test_reflect_type_of_null_via_nxl(self):
        self._interp(
            'set n to null\n'
            'set t to reflect_type_of with n\n'
            'expect t to equal "Null"'
        )
