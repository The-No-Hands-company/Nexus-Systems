"""
Type Traits Module

Provides compile-time type introspection and analysis inspired by C++ <type_traits>.
This module enables runtime type checking, reflection, and metaprogramming capabilities.

Type Categories:
- Fundamental types: integer, float, boolean, string, None
- Compound types: list, dict, tuple, set, frozenset
- Callable types: function, method, lambda, class
- Custom types: user-defined classes and objects

Type Properties:
- Numeric: is_integer, is_floating_point, is_numeric
- Callable: is_callable, is_function, is_method, is_class
- Container: is_iterable, is_sequence, is_mapping, is_set
- Mutability: is_mutable, is_immutable, is_hashable
- Inheritance: is_subclass, is_instance

Type Information:
- Type name, size, alignment
- Methods and attributes
- Base classes and MRO (Method Resolution Order)
"""

from ...runtime.runtime import Runtime
import sys
import inspect
import types
from collections.abc import Iterable, Sequence, Mapping, Set, Callable


# Type category checks
def trait_is_integer(runtime: Runtime, value):
    """Check if value is an integer type.
    
    Example: trait_is_integer(42) -> True
    """
    return isinstance(value, int) and not isinstance(value, bool)


def trait_is_floating_point(runtime: Runtime, value):
    """Check if value is a floating-point type.
    
    Example: trait_is_floating_point(3.14) -> True
    """
    return isinstance(value, float)


def trait_is_numeric(runtime: Runtime, value):
    """Check if value is a numeric type (int or float).
    
    Example: trait_is_numeric(42) -> True
    """
    return isinstance(value, (int, float)) and not isinstance(value, bool)


def trait_is_boolean(runtime: Runtime, value):
    """Check if value is a boolean type.
    
    Example: trait_is_boolean(True) -> True
    """
    return isinstance(value, bool)


def trait_is_string(runtime: Runtime, value):
    """Check if value is a string type.
    
    Example: trait_is_string("hello") -> True
    """
    return isinstance(value, str)


def trait_is_none(runtime: Runtime, value):
    """Check if value is None.
    
    Example: trait_is_none(None) -> True
    """
    return value is None


def trait_is_bytes(runtime: Runtime, value):
    """Check if value is a bytes type.
    
    Example: trait_is_bytes(b"hello") -> True
    """
    return isinstance(value, (bytes, bytearray))


# Callable type checks
def trait_is_callable(runtime: Runtime, value):
    """Check if value is callable (function, method, class, etc.).
    
    Example: trait_is_callable(print) -> True
    """
    return callable(value)


def trait_is_function(runtime: Runtime, value):
    """Check if value is a function.
    
    Example: trait_is_function(lambda x: x) -> True
    """
    return isinstance(value, (types.FunctionType, types.BuiltinFunctionType))


def trait_is_method(runtime: Runtime, value):
    """Check if value is a method.
    
    Example: trait_is_method(obj.method) -> True
    """
    return isinstance(value, types.MethodType)


def trait_is_class(runtime: Runtime, value):
    """Check if value is a class.
    
    Example: trait_is_class(int) -> True
    """
    return isinstance(value, type)


def trait_is_lambda(runtime: Runtime, value):
    """Check if value is a lambda function.
    
    Example: trait_is_lambda(lambda: 42) -> True
    """
    return isinstance(value, types.FunctionType) and value.__name__ == "<lambda>"


# Container type checks
def trait_is_iterable(runtime: Runtime, value):
    """Check if value is iterable.
    
    Example: trait_is_iterable([1, 2, 3]) -> True
    """
    try:
        iter(value)
        return True
    except TypeError:
        return False


def trait_is_sequence(runtime: Runtime, value):
    """Check if value is a sequence (list, tuple, str, etc.).
    
    Example: trait_is_sequence([1, 2, 3]) -> True
    """
    return isinstance(value, Sequence)


def trait_is_mapping(runtime: Runtime, value):
    """Check if value is a mapping (dict, etc.).
    
    Example: trait_is_mapping({"a": 1}) -> True
    """
    return isinstance(value, Mapping)


def trait_is_set(runtime: Runtime, value):
    """Check if value is a set.
    
    Example: trait_is_set({1, 2, 3}) -> True
    """
    return isinstance(value, Set)


def trait_is_list(runtime: Runtime, value):
    """Check if value is a list.
    
    Example: trait_is_list([1, 2, 3]) -> True
    """
    return isinstance(value, list)


def trait_is_tuple(runtime: Runtime, value):
    """Check if value is a tuple.
    
    Example: trait_is_tuple((1, 2, 3)) -> True
    """
    return isinstance(value, tuple)


def trait_is_dict(runtime: Runtime, value):
    """Check if value is a dictionary.
    
    Example: trait_is_dict({"a": 1}) -> True
    """
    return isinstance(value, dict)


# Mutability checks
def trait_is_mutable(runtime: Runtime, value):
    """Check if value is mutable (can be modified in-place).
    
    Example: trait_is_mutable([1, 2, 3]) -> True
    """
    # Common mutable types
    mutable_types = (list, dict, set, bytearray)
    return isinstance(value, mutable_types)


def trait_is_immutable(runtime: Runtime, value):
    """Check if value is immutable.
    
    Example: trait_is_immutable((1, 2, 3)) -> True
    """
    # Common immutable types
    immutable_types = (int, float, bool, str, bytes, tuple, frozenset, type(None))
    return isinstance(value, immutable_types)


def trait_is_hashable(runtime: Runtime, value):
    """Check if value is hashable (can be used as dict key or in set).
    
    Example: trait_is_hashable("hello") -> True
    """
    try:
        hash(value)
        return True
    except TypeError:
        return False


# Type information
def trait_type_name(runtime: Runtime, value):
    """Get the type name of a value.
    
    Example: trait_type_name(42) -> "int"
    """
    return type(value).__name__


def trait_type_module(runtime: Runtime, value):
    """Get the module name where the type is defined.
    
    Example: trait_type_module(42) -> "builtins"
    """
    return type(value).__module__


def trait_type_qualname(runtime: Runtime, value):
    """Get the qualified name of the type.
    
    Example: trait_type_qualname(42) -> "int"
    """
    return type(value).__qualname__


def trait_type_bases(runtime: Runtime, value):
    """Get the base classes of a type.
    
    Example: trait_type_bases(True) -> [<class 'int'>]
    """
    return list(type(value).__bases__)


def trait_type_mro(runtime: Runtime, value):
    """Get the Method Resolution Order (MRO) of a type.
    
    Example: trait_type_mro(True) -> [bool, int, object]
    """
    return [cls.__name__ for cls in type(value).__mro__]


def trait_type_size(runtime: Runtime, value):
    """Get the size of a value in bytes.
    
    Example: trait_type_size(42) -> 28 (platform dependent)
    """
    return sys.getsizeof(value)


def trait_type_id(runtime: Runtime, value):
    """Get the unique identifier (memory address) of a value.
    
    Example: trait_type_id(42) -> 140735268345024 (varies)
    """
    return id(value)


# Inheritance and instance checks
def trait_is_instance(runtime: Runtime, value, type_or_tuple):
    """Check if value is an instance of type(s).
    
    Example: trait_is_instance(42, int) -> True
    """
    # Handle NexusLang runtime types
    if isinstance(type_or_tuple, str):
        type_map = {
            "int": int,
            "float": float,
            "str": str,
            "bool": bool,
            "list": list,
            "dict": dict,
            "tuple": tuple,
            "set": set,
            "bytes": bytes,
            "none": type(None),
        }
        type_or_tuple = type_map.get(type_or_tuple, type_or_tuple)
    
    return isinstance(value, type_or_tuple)


def trait_is_subclass(runtime: Runtime, cls, base_or_tuple):
    """Check if cls is a subclass of base(s).
    
    Example: trait_is_subclass(bool, int) -> True
    """
    if not isinstance(cls, type):
        raise TypeError("First argument must be a class")
    
    # Handle NexusLang runtime types
    if isinstance(base_or_tuple, str):
        type_map = {
            "int": int,
            "float": float,
            "str": str,
            "bool": bool,
            "list": list,
            "dict": dict,
            "tuple": tuple,
            "set": set,
            "bytes": bytes,
            "object": object,
        }
        base_or_tuple = type_map.get(base_or_tuple, base_or_tuple)
    
    return issubclass(cls, base_or_tuple)


# Attribute and method inspection
def trait_has_attribute(runtime: Runtime, value, name):
    """Check if value has an attribute.
    
    Example: trait_has_attribute([1, 2], "append") -> True
    """
    return hasattr(value, str(name))


def trait_get_attribute(runtime: Runtime, value, name, default=None):
    """Get attribute value from object.
    
    Example: trait_get_attribute([1, 2], "__len__") -> <method>
    """
    return getattr(value, str(name), default)


def trait_get_attributes(runtime: Runtime, value):
    """Get all attributes of a value.
    
    Returns: List of attribute names
    """
    return dir(value)


def trait_has_method(runtime: Runtime, value, name):
    """Check if value has a method.
    
    Example: trait_has_method([1, 2], "append") -> True
    """
    if not hasattr(value, str(name)):
        return False
    
    attr = getattr(value, str(name))
    return callable(attr)


def trait_get_methods(runtime: Runtime, value):
    """Get all methods of a value.
    
    Returns: List of method names
    """
    methods = []
    for name in dir(value):
        try:
            attr = getattr(value, name)
            if callable(attr):
                methods.append(name)
        except AttributeError:
            pass
    
    return methods


def trait_get_public_methods(runtime: Runtime, value):
    """Get all public methods (not starting with _).
    
    Returns: List of public method names
    """
    methods = []
    for name in dir(value):
        if not name.startswith("_"):
            try:
                attr = getattr(value, name)
                if callable(attr):
                    methods.append(name)
            except AttributeError:
                pass
    
    return methods


# Function/method inspection
def trait_get_signature(runtime: Runtime, func):
    """Get function signature information.
    
    Returns: Dictionary with parameter info
    """
    if not callable(func):
        raise TypeError("Argument must be callable")
    
    try:
        sig = inspect.signature(func)
        params = []
        
        for name, param in sig.parameters.items():
            param_info = {
                "name": name,
                "kind": str(param.kind),
                "has_default": param.default is not inspect.Parameter.empty
            }
            if param.default is not inspect.Parameter.empty:
                param_info["default"] = param.default
            if param.annotation is not inspect.Parameter.empty:
                param_info["annotation"] = str(param.annotation)
            params.append(param_info)
        
        result = {
            "parameters": params,
            "has_return_annotation": sig.return_annotation is not inspect.Signature.empty
        }
        
        if sig.return_annotation is not inspect.Signature.empty:
            result["return_annotation"] = str(sig.return_annotation)
        
        return result
    except (ValueError, TypeError):
        return {"error": "Cannot inspect signature"}


def trait_get_doc(runtime: Runtime, value):
    """Get documentation string (docstring) of a value.
    
    Example: trait_get_doc(list.append) -> "Append object to the end..."
    """
    doc = inspect.getdoc(value)
    return doc if doc else ""


def trait_get_source(runtime: Runtime, func):
    """Get source code of a function (if available).
    
    Returns: Source code string or error message
    """
    try:
        return inspect.getsource(func)
    except (OSError, TypeError):
        return None


def trait_get_file(runtime: Runtime, value):
    """Get the file where a type/function is defined.
    
    Returns: File path or None
    """
    try:
        return inspect.getfile(type(value))
    except (TypeError, AttributeError):
        return None


# Container-specific traits
def trait_get_length(runtime: Runtime, value):
    """Get length of a container.
    
    Example: trait_get_length([1, 2, 3]) -> 3
    """
    try:
        return len(value)
    except TypeError:
        raise TypeError(f"Object of type {type(value).__name__} has no len()")


def trait_is_empty(runtime: Runtime, value):
    """Check if a container is empty.
    
    Example: trait_is_empty([]) -> True
    """
    try:
        return len(value) == 0
    except TypeError:
        raise TypeError(f"Object of type {type(value).__name__} has no len()")


def trait_supports_index(runtime: Runtime, value):
    """Check if value supports indexing (has __getitem__).
    
    Example: trait_supports_index([1, 2, 3]) -> True
    """
    return hasattr(value, "__getitem__")


def trait_supports_contains(runtime: Runtime, value):
    """Check if value supports 'in' operator (has __contains__).
    
    Example: trait_supports_contains([1, 2, 3]) -> True
    """
    return hasattr(value, "__contains__")


# Comparison trait checks
def trait_supports_comparison(runtime: Runtime, value):
    """Check if value supports comparison operators.
    
    Example: trait_supports_comparison(42) -> True
    """
    return hasattr(value, "__lt__") and hasattr(value, "__eq__")


def trait_supports_ordering(runtime: Runtime, value):
    """Check if value supports ordering (<, >, etc.).
    
    Example: trait_supports_ordering(42) -> True
    """
    return hasattr(value, "__lt__") and hasattr(value, "__le__")


# Arithmetic trait checks
def trait_supports_arithmetic(runtime: Runtime, value):
    """Check if value supports arithmetic operations.
    
    Example: trait_supports_arithmetic(42) -> True
    """
    return hasattr(value, "__add__") and hasattr(value, "__mul__")


def trait_supports_division(runtime: Runtime, value):
    """Check if value supports division.
    
    Example: trait_supports_division(42) -> True
    """
    return hasattr(value, "__truediv__")


# Special method checks
def trait_has_iter(runtime: Runtime, value):
    """Check if value has __iter__ method.
    
    Example: trait_has_iter([1, 2, 3]) -> True
    """
    return hasattr(value, "__iter__")


def trait_has_next(runtime: Runtime, value):
    """Check if value has __next__ method (is an iterator).
    
    Example: trait_has_next(iter([1, 2, 3])) -> True
    """
    return hasattr(value, "__next__")


def trait_has_enter(runtime: Runtime, value):
    """Check if value has __enter__ (is a context manager).
    
    Example: trait_has_enter(open("file.txt")) -> True
    """
    return hasattr(value, "__enter__") and hasattr(value, "__exit__")


def trait_has_call(runtime: Runtime, value):
    """Check if value has __call__ (is callable).
    
    Example: trait_has_call(print) -> True
    """
    return hasattr(value, "__call__")


# Type conversion checks
def trait_can_convert_to_int(runtime: Runtime, value):
    """Check if value can be converted to int.
    
    Example: trait_can_convert_to_int("42") -> True
    """
    try:
        int(value)
        return True
    except (ValueError, TypeError):
        return False


def trait_can_convert_to_float(runtime: Runtime, value):
    """Check if value can be converted to float.
    
    Example: trait_can_convert_to_float("3.14") -> True
    """
    try:
        float(value)
        return True
    except (ValueError, TypeError):
        return False


def trait_can_convert_to_str(runtime: Runtime, value):
    """Check if value can be converted to string.
    
    Example: trait_can_convert_to_str(42) -> True
    """
    try:
        str(value)
        return True
    except:
        return False


# Utility functions
def trait_get_type(runtime: Runtime, value):
    """Get the type object of a value.
    
    Example: trait_get_type(42) -> <class 'int'>
    """
    return type(value)


def trait_get_all_traits(runtime: Runtime, value):
    """Get comprehensive type trait information.
    
    Returns: Dictionary with all trait information
    """
    return {
        # Type categories
        "is_integer": trait_is_integer(runtime, value),
        "is_floating_point": trait_is_floating_point(runtime, value),
        "is_numeric": trait_is_numeric(runtime, value),
        "is_boolean": trait_is_boolean(runtime, value),
        "is_string": trait_is_string(runtime, value),
        "is_none": trait_is_none(runtime, value),
        "is_bytes": trait_is_bytes(runtime, value),
        
        # Callable types
        "is_callable": trait_is_callable(runtime, value),
        "is_function": trait_is_function(runtime, value),
        "is_method": trait_is_method(runtime, value),
        "is_class": trait_is_class(runtime, value),
        
        # Container types
        "is_iterable": trait_is_iterable(runtime, value),
        "is_sequence": trait_is_sequence(runtime, value),
        "is_mapping": trait_is_mapping(runtime, value),
        "is_set": trait_is_set(runtime, value),
        "is_list": trait_is_list(runtime, value),
        "is_tuple": trait_is_tuple(runtime, value),
        "is_dict": trait_is_dict(runtime, value),
        
        # Mutability
        "is_mutable": trait_is_mutable(runtime, value),
        "is_immutable": trait_is_immutable(runtime, value),
        "is_hashable": trait_is_hashable(runtime, value),
        
        # Type info
        "type_name": trait_type_name(runtime, value),
        "type_module": trait_type_module(runtime, value),
        "type_size": trait_type_size(runtime, value),
        
        # Capabilities
        "supports_index": trait_supports_index(runtime, value),
        "supports_contains": trait_supports_contains(runtime, value),
        "supports_comparison": trait_supports_comparison(runtime, value),
        "supports_arithmetic": trait_supports_arithmetic(runtime, value),
    }


def register_type_trait_functions(runtime: Runtime) -> None:
    """Register type trait functions with the runtime."""
    # Type category checks
    runtime.register_function("trait_is_integer", trait_is_integer)
    runtime.register_function("trait_is_floating_point", trait_is_floating_point)
    runtime.register_function("trait_is_numeric", trait_is_numeric)
    runtime.register_function("trait_is_boolean", trait_is_boolean)
    runtime.register_function("trait_is_string", trait_is_string)
    runtime.register_function("trait_is_none", trait_is_none)
    runtime.register_function("trait_is_bytes", trait_is_bytes)
    
    # Callable type checks
    runtime.register_function("trait_is_callable", trait_is_callable)
    runtime.register_function("trait_is_function", trait_is_function)
    runtime.register_function("trait_is_method", trait_is_method)
    runtime.register_function("trait_is_class", trait_is_class)
    runtime.register_function("trait_is_lambda", trait_is_lambda)
    
    # Container type checks
    runtime.register_function("trait_is_iterable", trait_is_iterable)
    runtime.register_function("trait_is_sequence", trait_is_sequence)
    runtime.register_function("trait_is_mapping", trait_is_mapping)
    runtime.register_function("trait_is_set", trait_is_set)
    runtime.register_function("trait_is_list", trait_is_list)
    runtime.register_function("trait_is_tuple", trait_is_tuple)
    runtime.register_function("trait_is_dict", trait_is_dict)
    
    # Mutability checks
    runtime.register_function("trait_is_mutable", trait_is_mutable)
    runtime.register_function("trait_is_immutable", trait_is_immutable)
    runtime.register_function("trait_is_hashable", trait_is_hashable)
    
    # Type information
    runtime.register_function("trait_type_name", trait_type_name)
    runtime.register_function("trait_type_module", trait_type_module)
    runtime.register_function("trait_type_qualname", trait_type_qualname)
    runtime.register_function("trait_type_bases", trait_type_bases)
    runtime.register_function("trait_type_mro", trait_type_mro)
    runtime.register_function("trait_type_size", trait_type_size)
    runtime.register_function("trait_type_id", trait_type_id)
    
    # Inheritance and instance checks
    runtime.register_function("trait_is_instance", trait_is_instance)
    runtime.register_function("trait_is_subclass", trait_is_subclass)
    
    # Attribute and method inspection
    runtime.register_function("trait_has_attribute", trait_has_attribute)
    runtime.register_function("trait_get_attribute", trait_get_attribute)
    runtime.register_function("trait_get_attributes", trait_get_attributes)
    runtime.register_function("trait_has_method", trait_has_method)
    runtime.register_function("trait_get_methods", trait_get_methods)
    runtime.register_function("trait_get_public_methods", trait_get_public_methods)
    
    # Function/method inspection
    runtime.register_function("trait_get_signature", trait_get_signature)
    runtime.register_function("trait_get_doc", trait_get_doc)
    runtime.register_function("trait_get_source", trait_get_source)
    runtime.register_function("trait_get_file", trait_get_file)
    
    # Container-specific traits
    runtime.register_function("trait_get_length", trait_get_length)
    runtime.register_function("trait_is_empty", trait_is_empty)
    runtime.register_function("trait_supports_index", trait_supports_index)
    runtime.register_function("trait_supports_contains", trait_supports_contains)
    
    # Comparison trait checks
    runtime.register_function("trait_supports_comparison", trait_supports_comparison)
    runtime.register_function("trait_supports_ordering", trait_supports_ordering)
    
    # Arithmetic trait checks
    runtime.register_function("trait_supports_arithmetic", trait_supports_arithmetic)
    runtime.register_function("trait_supports_division", trait_supports_division)
    
    # Special method checks
    runtime.register_function("trait_has_iter", trait_has_iter)
    runtime.register_function("trait_has_next", trait_has_next)
    runtime.register_function("trait_has_enter", trait_has_enter)
    runtime.register_function("trait_has_call", trait_has_call)
    
    # Type conversion checks
    runtime.register_function("trait_can_convert_to_int", trait_can_convert_to_int)
    runtime.register_function("trait_can_convert_to_float", trait_can_convert_to_float)
    runtime.register_function("trait_can_convert_to_str", trait_can_convert_to_str)
    
    # Utility functions
    runtime.register_function("trait_get_type", trait_get_type)
    runtime.register_function("trait_get_all_traits", trait_get_all_traits)
