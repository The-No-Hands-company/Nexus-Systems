"""
NLPL Reflection Module.

Provides runtime introspection of NexusLang types, struct instances, and class instances.

Registered functions (callable from NexusLang programs):
    reflect_type_of(value) -> String
        Return the NexusLang type name: "Integer", "Float", "String", "Boolean",
        "Null", "Bytes", "List", "Dictionary", struct name, or class name.

    reflect_class_name(obj) -> String
        Return the class or struct name for an object instance.
        Returns the raw type name for primitive values.

    reflect_is_struct(value) -> Boolean
        True when value is a StructureInstance (struct or union).

    reflect_is_class_instance(value) -> Boolean
        True when value is an Object (class instance created via 'new ClassName').

    reflect_fields_of(struct_instance) -> Dictionary
        Return a dict mapping field names to current values for a struct.
        Returns {"error": "..."} if value is not a struct instance.

    reflect_struct_field_names(struct_instance) -> List
        Return a list of field name strings for a struct instance.
        Returns [] if value is not a struct instance.

    reflect_properties_of(class_instance) -> Dictionary
        Return a dict mapping property names to current values for a class instance.
        Returns {"error": "..."} if value is not a class instance.

    reflect_methods_of(class_instance) -> List
        Return a list of method name strings for a class instance.
        Returns [] if value is not a class instance.

    reflect_has_field(obj, field_name) -> Boolean
        True when a struct or class instance has the named field/property.

    reflect_get_field(obj, field_name) -> Any
        Get a field/property by name from a struct or class instance.
        Returns {"error": "..."} on failure.

    reflect_set_field(obj, field_name, value) -> Boolean
        Set a field/property by name on a struct or class instance.
        Returns True on success, False on failure.

    reflect_has_method(obj, method_name) -> Boolean
        True when a class instance has the named method.

    reflect_invoke(obj, method_name, args) -> Any
        Dynamically invoke a named method on a class instance.
        args must be a List (or empty list for no arguments).
        Returns the method's return value, or {"error": "..."} on failure.

    reflect_invoke_safe(obj, method_name, args, default) -> Any
        Like reflect_invoke but returns default instead of raising on any error.

    reflect_call(func_name, args) -> Any
        Call any registered NexusLang runtime function by name.
        args must be a List (or empty list for no arguments).
        Returns the function's return value, or {"error": "..."} if the
        function is not found or raises.

    reflect_is_instance_of(value, type_name) -> Boolean
        Check if value matches the named NexusLang type (case-insensitive).
        Understands aliases: "int"/"Integer", "str"/"String", etc.

    reflect_describe(obj) -> Dictionary
        Return a structured description: type, class_name, fields/properties,
        methods, and size (for structs).
"""

from typing import Any, Dict, List


# ---------------------------------------------------------------------------
# Internal helpers
# ---------------------------------------------------------------------------

def _get_type_name(value: Any) -> str:
    """Return the canonical NexusLang type name for any runtime value."""
    # Import lazily to avoid circular imports at module load time.
    try:
        from ...runtime.structures import StructureInstance
        if isinstance(value, StructureInstance):
            return value.definition.name
    except ImportError:
        pass

    try:
        from ...runtime.runtime import Object
        if isinstance(value, Object):
            return value.class_name
    except ImportError:
        pass

    if value is None:
        return "Null"
    # bool must come before int because bool is a subclass of int in Python.
    if isinstance(value, bool):
        return "Boolean"
    if isinstance(value, int):
        return "Integer"
    if isinstance(value, float):
        return "Float"
    if isinstance(value, str):
        return "String"
    if isinstance(value, bytes):
        return "Bytes"
    if isinstance(value, list):
        return "List"
    if isinstance(value, dict):
        return "Dictionary"
    if callable(value):
        return "Function"
    # Fallback: use the Python runtime type's name.
    return type(value).__name__


def _is_struct(value: Any) -> bool:
    try:
        from ...runtime.structures import StructureInstance
        return isinstance(value, StructureInstance)
    except ImportError:
        return False


def _is_object(value: Any) -> bool:
    try:
        from ...runtime.runtime import Object
        return isinstance(value, Object)
    except ImportError:
        return False


# ---------------------------------------------------------------------------
# Type introspection
# ---------------------------------------------------------------------------

def reflect_type_of(runtime, value: Any) -> str:
    """Return the NexusLang type name of value as a String."""
    return _get_type_name(value)


def reflect_class_name(runtime, obj: Any) -> str:
    """Return the class or struct name for an object instance.

    For structs: the struct definition name.
    For class instances: the class name from Object.class_name.
    For primitives: same as reflect_type_of.
    """
    return _get_type_name(obj)


def reflect_is_struct(runtime, value: Any) -> bool:
    """Return True when value is a StructureInstance."""
    return _is_struct(value)


def reflect_is_class_instance(runtime, value: Any) -> bool:
    """Return True when value is a class Object instance."""
    return _is_object(value)


def reflect_is_instance_of(runtime, value: Any, type_name: str) -> bool:
    """Check if value is an instance of the named NexusLang type.

    Performs case-insensitive matching and understands aliases such as
    "int" for "Integer" and "str" for "String".
    """
    actual = _get_type_name(value)
    # Direct match (case-insensitive).
    if actual.lower() == type_name.lower():
        return True

    # Aliases map: canonical lower -> set of accepted lower aliases.
    _aliases: Dict[str, List[str]] = {
        "integer": ["int", "integer", "long", "number"],
        "float": ["float", "double", "real", "number"],
        "string": ["str", "string", "text"],
        "boolean": ["bool", "boolean"],
        "null": ["null", "none", "nil"],
        "list": ["list", "array"],
        "dictionary": ["dictionary", "dict", "map"],
        "bytes": ["bytes", "bytearray", "binary"],
        "function": ["function", "callable", "func"],
    }

    actual_lower = actual.lower()
    type_lower = type_name.lower()

    for canonical, aliases in _aliases.items():
        if actual_lower in aliases and type_lower in aliases:
            return True

    return False


# ---------------------------------------------------------------------------
# Struct introspection
# ---------------------------------------------------------------------------

def reflect_fields_of(runtime, obj: Any) -> Dict[str, Any]:
    """Return a dict mapping field names to current values for a struct instance.

    Returns {"error": "<message>"} if obj is not a StructureInstance.
    """
    if not _is_struct(obj):
        return {"error": f"Not a struct instance (got {_get_type_name(obj)})"}

    result: Dict[str, Any] = {}
    for name in obj.definition.fields:
        try:
            result[name] = obj.get_field(name)
        except Exception:
            result[name] = None
    return result


def reflect_struct_field_names(runtime, obj: Any) -> List[str]:
    """Return a list of field name strings for a struct instance.

    Returns [] if obj is not a StructureInstance.
    """
    if not _is_struct(obj):
        return []
    return list(obj.definition.fields.keys())


def reflect_struct_size(runtime, obj: Any) -> int:
    """Return the total size in bytes of the struct instance's memory layout.

    Returns 0 if obj is not a StructureInstance.
    """
    if not _is_struct(obj):
        return 0
    return obj.definition.size


# ---------------------------------------------------------------------------
# Class instance introspection
# ---------------------------------------------------------------------------

def reflect_properties_of(runtime, obj: Any) -> Dict[str, Any]:
    """Return a dict mapping property names to current values for a class instance.

    Returns {"error": "<message>"} if obj is not an Object.
    """
    if not _is_object(obj):
        return {"error": f"Not a class instance (got {_get_type_name(obj)})"}
    return dict(obj.properties)


def reflect_methods_of(runtime, obj: Any) -> List[str]:
    """Return a sorted list of method name strings for a class instance.

    Returns [] if obj is not an Object.
    """
    if not _is_object(obj):
        return []
    return sorted(obj.methods.keys())


# ---------------------------------------------------------------------------
# Common field/property access
# ---------------------------------------------------------------------------

def reflect_has_field(runtime, obj: Any, field_name: str) -> bool:
    """Return True when a struct or class instance has the named field/property."""
    if _is_struct(obj):
        return field_name in obj.definition.fields
    if _is_object(obj):
        return field_name in obj.properties
    return False


def reflect_get_field(runtime, obj: Any, field_name: str) -> Any:
    """Get a field/property value by name from a struct or class instance.

    Returns {"error": "<message>"} on failure.
    """
    if _is_struct(obj):
        try:
            return obj.get_field(field_name)
        except AttributeError:
            return {"error": f"Struct '{obj.definition.name}' has no field '{field_name}'"}
        except Exception as exc:
            return {"error": str(exc)}

    if _is_object(obj):
        if field_name not in obj.properties:
            return {"error": f"Class '{obj.class_name}' has no property '{field_name}'"}
        return obj.properties[field_name]

    return {"error": f"Not an object (got {_get_type_name(obj)})"}


def reflect_set_field(runtime, obj: Any, field_name: str, value: Any) -> bool:
    """Set a field/property value by name on a struct or class instance.

    Returns True on success, False on failure.
    """
    if _is_struct(obj):
        try:
            obj.set_field(field_name, value)
            return True
        except (AttributeError, TypeError, RuntimeError):
            return False

    if _is_object(obj):
        obj.set_property(field_name, value)
        return True

    return False


# ---------------------------------------------------------------------------
# Method inspection
# ---------------------------------------------------------------------------

def reflect_has_method(runtime, obj: Any, method_name: str) -> bool:
    """Return True when a class instance has the named method."""
    if _is_object(obj):
        return method_name in obj.methods
    return False


# ---------------------------------------------------------------------------
# Dynamic invocation
# ---------------------------------------------------------------------------

def reflect_invoke(runtime, obj: Any, method_name: str, args: list = None) -> Any:
    """Dynamically invoke a named method on a class instance.

    Parameters
    ----------
    obj:
        A class instance (Object).  Struct instances are not supported since
        structs have fields, not callable methods.
    method_name:
        The name of the method to call.
    args:
        A list of positional arguments to pass.  Pass [] or None for no args.

    Returns
    -------
    The method's return value on success, or ``{"error": "<message>"}``
    if obj is not a class instance, the method does not exist, or the invocation
    raises an exception.
    """
    if args is None:
        args = []
    if not isinstance(args, list):
        args = list(args)

    if not _is_object(obj):
        return {"error": f"reflect_invoke: expected a class instance, got {_get_type_name(obj)}"}

    if method_name not in obj.methods:
        available = sorted(obj.methods.keys())
        return {
            "error": (
                f"reflect_invoke: '{obj.class_name}' has no method '{method_name}'. "
                f"Available: {available}"
            )
        }

    try:
        return obj.invoke_method(method_name, *args)
    except Exception as exc:
        return {"error": f"reflect_invoke: {method_name}() raised {type(exc).__name__}: {exc}"}


def reflect_invoke_safe(runtime, obj: Any, method_name: str, args: list = None, default: Any = None) -> Any:
    """Like reflect_invoke but returns *default* on any failure instead of an error dict.

    Useful when the caller does not need to distinguish between a missing method
    and a method that raised—they just want a fallback value.
    """
    result = reflect_invoke(runtime, obj, method_name, args)
    if isinstance(result, dict) and "error" in result:
        return default
    return result


def reflect_call(runtime, func_name: str, args: list = None) -> Any:
    """Call any registered NexusLang runtime function by name.

    Parameters
    ----------
    func_name:
        The name under which the function was registered with the runtime.
    args:
        A list of positional arguments to pass.  Pass [] or None for no args.

    Returns
    -------
    The function's return value on success, or ``{"error": "<message>"}``
    if the function is not registered or raises.
    """
    if args is None:
        args = []
    if not isinstance(args, list):
        args = list(args)

    if func_name not in runtime.functions:
        registered = sorted(runtime.functions.keys())
        return {
            "error": (
                f"reflect_call: no registered function named '{func_name}'. "
                f"({len(registered)} functions registered)"
            )
        }

    try:
        return runtime.functions[func_name](*args)
    except Exception as exc:
        return {"error": f"reflect_call: {func_name}() raised {type(exc).__name__}: {exc}"}


# ---------------------------------------------------------------------------
# Describe — combined introspection summary
# ---------------------------------------------------------------------------

def reflect_describe(runtime, obj: Any) -> Dict[str, Any]:
    """Return a structured description of any NexusLang value.

    For struct instances::
        {
            "kind": "struct",
            "type_name": "<StructName>",
            "fields": {"field1": value1, ...},
            "size_bytes": <int>
        }

    For class instances::
        {
            "kind": "class",
            "type_name": "<ClassName>",
            "properties": {"prop1": value1, ...},
            "methods": ["method1", ...]
        }

    For primitives and other values::
        {
            "kind": "primitive",
            "type_name": "<TypeName>",
            "value": <value>
        }
    """
    if _is_struct(obj):
        fields: Dict[str, Any] = {}
        for name in obj.definition.fields:
            try:
                fields[name] = obj.get_field(name)
            except Exception:
                fields[name] = None
        return {
            "kind": "struct",
            "type_name": obj.definition.name,
            "fields": fields,
            "size_bytes": obj.definition.size,
        }

    if _is_object(obj):
        return {
            "kind": "class",
            "type_name": obj.class_name,
            "properties": dict(obj.properties),
            "methods": sorted(obj.methods.keys()),
        }

    return {
        "kind": "primitive",
        "type_name": _get_type_name(obj),
        "value": obj,
    }


# ---------------------------------------------------------------------------
# Attribute reflection helpers
# ---------------------------------------------------------------------------


def _get_applied_attributes(runtime, obj) -> dict:
    """Internal helper: return the dict of applied attributes for *obj*.

    Checks in this order:

    1. Class instance (Object) -> ``runtime._class_attributes[obj.class_name]``
    2. Any object with ``_applied_attributes`` directly set on it (e.g. function
       values/AST nodes that had attribute decorators applied).
    """
    if _is_object(obj):
        class_name = obj.class_name
        return getattr(runtime, '_class_attributes', {}).get(class_name, {})
    if hasattr(obj, '_applied_attributes'):
        return obj._applied_attributes
    return {}


def reflect_has_attribute(runtime, obj: Any, attr_name: str) -> bool:
    """Return True if *obj* has the named attribute applied.

    Works for class instances, or any value that had an attribute decorator
    applied to it.
    """
    return attr_name in _get_applied_attributes(runtime, obj)


def reflect_list_attributes(runtime, obj: Any) -> List[str]:
    """Return a sorted list of attribute names applied to *obj*."""
    return sorted(_get_applied_attributes(runtime, obj).keys())


def reflect_get_attribute(runtime, obj: Any, attr_name: str) -> Dict[str, Any]:
    """Return the property dict for the named attribute applied to *obj*.

    Returns ``{"error": "<message>"}`` if the attribute is not present.
    """
    attrs = _get_applied_attributes(runtime, obj)
    if attr_name not in attrs:
        return {"error": f"Attribute '{attr_name}' not found on object"}
    return dict(attrs[attr_name])


def reflect_attribute_value(runtime, obj: Any, attr_name: str, prop_name: str) -> Any:
    """Return the value of a single property within a named attribute application.

    Returns ``None`` if the attribute or property does not exist.
    """
    attrs = _get_applied_attributes(runtime, obj)
    if attr_name not in attrs:
        return None
    return attrs[attr_name].get(prop_name)


# ---------------------------------------------------------------------------
# Registration
# ---------------------------------------------------------------------------

def register_reflection_functions(runtime) -> None:
    """Register all reflection functions with the NexusLang runtime."""
    runtime.register_function("reflect_type_of",
                              lambda v: reflect_type_of(runtime, v))
    runtime.register_function("reflect_class_name",
                              lambda v: reflect_class_name(runtime, v))
    runtime.register_function("reflect_is_struct",
                              lambda v: reflect_is_struct(runtime, v))
    runtime.register_function("reflect_is_class_instance",
                              lambda v: reflect_is_class_instance(runtime, v))
    runtime.register_function("reflect_is_instance_of",
                              lambda v, t: reflect_is_instance_of(runtime, v, t))

    # Struct introspection
    runtime.register_function("reflect_fields_of",
                              lambda v: reflect_fields_of(runtime, v))
    runtime.register_function("reflect_struct_field_names",
                              lambda v: reflect_struct_field_names(runtime, v))
    runtime.register_function("reflect_struct_size",
                              lambda v: reflect_struct_size(runtime, v))

    # Class instance introspection
    runtime.register_function("reflect_properties_of",
                              lambda v: reflect_properties_of(runtime, v))
    runtime.register_function("reflect_methods_of",
                              lambda v: reflect_methods_of(runtime, v))

    # Common field access
    runtime.register_function("reflect_has_field",
                              lambda v, f: reflect_has_field(runtime, v, f))
    runtime.register_function("reflect_get_field",
                              lambda v, f: reflect_get_field(runtime, v, f))
    runtime.register_function("reflect_set_field",
                              lambda v, f, val: reflect_set_field(runtime, v, f, val))

    # Method inspection
    runtime.register_function("reflect_has_method",
                              lambda v, m: reflect_has_method(runtime, v, m))

    # Dynamic invocation
    runtime.register_function("reflect_invoke",
                              lambda obj, name, args=None: reflect_invoke(runtime, obj, name, args))
    runtime.register_function("reflect_invoke_safe",
                              lambda obj, name, args=None, default=None: reflect_invoke_safe(runtime, obj, name, args, default))
    runtime.register_function("reflect_call",
                              lambda name, args=None: reflect_call(runtime, name, args))

    # Summary
    runtime.register_function("reflect_describe",
                              lambda v: reflect_describe(runtime, v))

    # Attribute reflection
    runtime.register_function("reflect_has_attribute",
                              lambda v, a: reflect_has_attribute(runtime, v, a))
    runtime.register_function("reflect_list_attributes",
                              lambda v: reflect_list_attributes(runtime, v))
    runtime.register_function("reflect_get_attribute",
                              lambda v, a: reflect_get_attribute(runtime, v, a))
    runtime.register_function("reflect_attribute_value",
                              lambda v, a, p: reflect_attribute_value(runtime, v, a, p))


__all__ = [
    "reflect_type_of",
    "reflect_class_name",
    "reflect_is_struct",
    "reflect_is_class_instance",
    "reflect_is_instance_of",
    "reflect_fields_of",
    "reflect_struct_field_names",
    "reflect_struct_size",
    "reflect_properties_of",
    "reflect_methods_of",
    "reflect_has_field",
    "reflect_get_field",
    "reflect_set_field",
    "reflect_has_method",
    "reflect_invoke",
    "reflect_invoke_safe",
    "reflect_call",
    "reflect_describe",
    "reflect_has_attribute",
    "reflect_list_attributes",
    "reflect_get_attribute",
    "reflect_attribute_value",
    "register_reflection_functions",
]
