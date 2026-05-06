"""
Validation utilities for NexusLang.

Provides schema validation, input validation, and data validation functions.

Features:
- Type validation
- Range validation  
- Pattern validation (regex)
- Required fields validation
- Custom validators
- Schema-based validation

Example usage in NexusLang:
    # Validate types
    set is_valid to validate_type with value and "integer"
    
    # Validate range
    set in_range to validate_range with 5 and 1 and 10
    
    # Validate schema
    set schema to {"name": "string", "age": "integer"}
    set data to {"name": "Alice", "age": 30}
    set result to validate_schema with data and schema
"""

from ...runtime.runtime import Runtime
import json
import os
import re
from typing import Any, Dict, List, Optional, Union

try:
    import jsonschema
    from jsonschema import Draft4Validator, Draft7Validator, Draft201909Validator, Draft202012Validator
    from jsonschema.exceptions import ValidationError as _JSValidationError, SchemaError as _JSSchemaError
    _JSONSCHEMA_AVAILABLE = True
except ImportError:  # pragma: no cover
    _JSONSCHEMA_AVAILABLE = False


def _require_jsonschema() -> None:
    if not _JSONSCHEMA_AVAILABLE:
        raise RuntimeError(
            "JSON Schema validation requires the 'jsonschema' package. "
            "Install it with: pip install jsonschema>=4.17.0"
        )


def validate_type(value, expected_type):
    """
    Validate that a value matches the expected type.
    
    Args:
        value: Value to validate
        expected_type: Expected type name (string, integer, float, boolean, list, dict)
    
    Returns:
        True if type matches, False otherwise
    """
    type_map = {
        "string": str,
        "integer": int,
        "float": float,
        "boolean": bool,
        "list": list,
        "dict": dict,
        "none": type(None),
    }
    
    expected = type_map.get(expected_type.lower())
    if expected is None:
        raise ValueError(f"Unknown type: {expected_type}")
    
    return isinstance(value, expected)


def validate_range(value, min_value=None, max_value=None):
    """
    Validate that a numeric value is within a range.
    
    Args:
        value: Numeric value to validate
        min_value: Minimum value (inclusive), None for no minimum
        max_value: Maximum value (inclusive), None for no maximum
    
    Returns:
        True if value is in range, False otherwise
    """
    if min_value is not None and value < min_value:
        return False
    if max_value is not None and value > max_value:
        return False
    return True


def validate_length(value, min_length=None, max_length=None):
    """
    Validate that a value's length is within a range.
    
    Args:
        value: Value with length (string, list, dict, etc.)
        min_length: Minimum length (inclusive)
        max_length: Maximum length (inclusive)
    
    Returns:
        True if length is valid, False otherwise
    """
    try:
        length = len(value)
    except TypeError:
        return False
    
    if min_length is not None and length < min_length:
        return False
    if max_length is not None and length > max_length:
        return False
    return True


def validate_pattern(value, pattern):
    """
    Validate that a string matches a regex pattern.
    
    Args:
        value: String to validate
        pattern: Regular expression pattern
    
    Returns:
        True if pattern matches, False otherwise
    """
    if not isinstance(value, str):
        return False
    
    try:
        return bool(re.match(pattern, value))
    except re.error:
        raise ValueError(f"Invalid regex pattern: {pattern}")


def validate_email(email):
    """
    Validate that a string is a valid email address.
    
    Args:
        email: Email address to validate
    
    Returns:
        True if valid email, False otherwise
    """
    pattern = r'^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$'
    return validate_pattern(email, pattern)


def validate_url(url):
    """
    Validate that a string is a valid URL.
    
    Args:
        url: URL to validate
    
    Returns:
        True if valid URL, False otherwise
    """
    pattern = r'^https?://[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}(/.*)?$'
    return validate_pattern(url, pattern)


def validate_required(data, required_fields):
    """
    Validate that all required fields are present in data.
    
    Args:
        data: Dictionary to validate
        required_fields: List of required field names
    
    Returns:
        Dictionary with {valid: bool, missing: [fields]}
    """
    if not isinstance(data, dict):
        return {"valid": False, "missing": required_fields}
    
    missing = [field for field in required_fields if field not in data]
    
    return {
        "valid": len(missing) == 0,
        "missing": missing
    }


def validate_schema(data, schema):
    """
    Validate data against a schema definition.
    
    Schema format:
    {
        "field_name": "type" or {"type": "...", "required": bool, "min": ..., "max": ...}
    }
    
    Args:
        data: Dictionary to validate
        schema: Schema definition
    
    Returns:
        Dictionary with {valid: bool, errors: [error_messages]}
    """
    if not isinstance(data, dict):
        return {"valid": False, "errors": ["Data must be a dictionary"]}
    
    errors = []
    
    for field, rules in schema.items():
        # Handle simple type string
        if isinstance(rules, str):
            rules = {"type": rules, "required": True}
        
        # Check required
        if rules.get("required", False) and field not in data:
            errors.append(f"Missing required field: {field}")
            continue
        
        # Skip validation if field not present and not required
        if field not in data:
            continue
        
        value = data[field]
        
        # Validate type
        if "type" in rules:
            if not validate_type(value, rules["type"]):
                errors.append(f"Field '{field}' must be type {rules['type']}")
                continue
        
        # Validate range for numbers
        if "min" in rules or "max" in rules:
            if not validate_range(value, rules.get("min"), rules.get("max")):
                errors.append(f"Field '{field}' out of range")
        
        # Validate length for strings/lists
        if "min_length" in rules or "max_length" in rules:
            if not validate_length(value, rules.get("min_length"), rules.get("max_length")):
                errors.append(f"Field '{field}' length invalid")
        
        # Validate pattern for strings
        if "pattern" in rules:
            if not validate_pattern(str(value), rules["pattern"]):
                errors.append(f"Field '{field}' does not match pattern")
        
        # Validate choices
        if "choices" in rules:
            if value not in rules["choices"]:
                errors.append(f"Field '{field}' must be one of {rules['choices']}")
    
    return {
        "valid": len(errors) == 0,
        "errors": errors
    }


def validate_not_empty(value):
    """
    Validate that a value is not empty.
    
    Args:
        value: Value to validate
    
    Returns:
        True if not empty, False otherwise
    """
    if value is None:
        return False
    if isinstance(value, (str, list, dict)):
        return len(value) > 0
    return True


def validate_in_list(value, allowed_values):
    """
    Validate that a value is in a list of allowed values.
    
    Args:
        value: Value to validate
        allowed_values: List of allowed values
    
    Returns:
        True if value is allowed, False otherwise
    """
    return value in allowed_values


def validate_unique(values):
    """
    Validate that all values in a list are unique.
    
    Args:
        values: List of values
    
    Returns:
        True if all unique, False otherwise
    """
    if not isinstance(values, list):
        return False
    return len(values) == len(set(values))


def sanitize_string(value, allowed_chars=None):
    """
    Sanitize a string by removing or replacing disallowed characters.
    
    Args:
        value: String to sanitize
        allowed_chars: Regex pattern of allowed characters (default: alphanumeric + spaces)
    
    Returns:
        Sanitized string
    """
    if not isinstance(value, str):
        return str(value)
    
    if allowed_chars is None:
        allowed_chars = r'[a-zA-Z0-9 ]'
    
    # Keep only allowed characters
    return re.sub(f'[^{allowed_chars[1:-1]}]', '', value)


def sanitize_html(value):
    """
    Sanitize HTML by escaping special characters.
    
    Args:
        value: String potentially containing HTML
    
    Returns:
        HTML-escaped string
    """
    if not isinstance(value, str):
        return str(value)
    
    replacements = {
        '<': '&lt;',
        '>': '&gt;',
        '&': '&amp;',
        '"': '&quot;',
        "'": '&#x27;',
    }
    
    for char, replacement in replacements.items():
        value = value.replace(char, replacement)
    
    return value


# ---------------------------------------------------------------------------
# JSON Schema validation (RFC-compliant, powered by the jsonschema library)
# ---------------------------------------------------------------------------

# Map of $schema URI substrings to validator class names for version detection
_SCHEMA_DRAFT_MAP = [
    ("2020-12", "Draft 2020-12"),
    ("2019-09", "Draft 2019-09"),
    ("draft-07", "Draft 7"),
    ("draft-06", "Draft 6"),
    ("draft-04", "Draft 4"),
    ("draft-03", "Draft 3"),
]


def _best_validator_for(schema: Dict) -> Any:
    """Return the most appropriate jsonschema Validator class for *schema*."""
    _require_jsonschema()
    meta = schema.get("$schema", "") if isinstance(schema, dict) else ""
    if "2020-12" in meta:
        return Draft202012Validator
    if "2019-09" in meta:
        return Draft201909Validator
    if "draft-07" in meta or "draft-7" in meta:
        return Draft7Validator
    # Default to Draft 7 — widest real-world compatibility
    return Draft7Validator


def json_schema_validate(data: Any, schema: Any) -> None:
    """Validate *data* against a JSON Schema dict.

    Raises RuntimeError with a descriptive message on the first violation.
    Returns None on success.

    Args:
        data:   The value to validate (dict, list, scalar, …).
        schema: A dict representing a valid JSON Schema.

    Example (NexusLang):
        json_schema_validate with data and schema
    """
    _require_jsonschema()
    if isinstance(schema, str):
        schema = json.loads(schema)
    try:
        _best_validator_for(schema)(schema).validate(data)
    except _JSValidationError as exc:
        path = " -> ".join(str(p) for p in exc.absolute_path) or "<root>"
        raise RuntimeError(f"JSON Schema validation failed at '{path}': {exc.message}") from None
    except _JSSchemaError as exc:
        raise RuntimeError(f"Invalid JSON Schema: {exc.message}") from None


def json_schema_is_valid(data: Any, schema: Any) -> bool:
    """Return True if *data* conforms to *schema*, False otherwise.

    Does not raise on validation failure.

    Args:
        data:   Value to validate.
        schema: JSON Schema dict or JSON string.
    """
    _require_jsonschema()
    if isinstance(schema, str):
        schema = json.loads(schema)
    try:
        return _best_validator_for(schema)(schema).is_valid(data)
    except Exception:
        # Catches SchemaError, UnknownType, and any other jsonschema internal error
        return False


def json_schema_errors(data: Any, schema: Any) -> List[str]:
    """Return a list of human-readable error strings for all violations.

    Returns an empty list when *data* is valid.

    Args:
        data:   Value to validate.
        schema: JSON Schema dict or JSON string.
    """
    _require_jsonschema()
    if isinstance(schema, str):
        schema = json.loads(schema)
    try:
        validator = _best_validator_for(schema)(schema)
        return [e.message for e in validator.iter_errors(data)]
    except _JSSchemaError as exc:
        return [f"Invalid JSON Schema: {exc.message}"]


def json_schema_first_error(data: Any, schema: Any) -> Optional[str]:
    """Return the first validation error message, or None if *data* is valid."""
    _require_jsonschema()
    if isinstance(schema, str):
        schema = json.loads(schema)
    try:
        errors = list(_best_validator_for(schema)(schema).iter_errors(data))
        return errors[0].message if errors else None
    except _JSSchemaError as exc:
        return f"Invalid JSON Schema: {exc.message}"


def json_schema_error_count(data: Any, schema: Any) -> int:
    """Return the total number of JSON Schema violations in *data*.

    Returns 0 when valid.
    """
    _require_jsonschema()
    if isinstance(schema, str):
        schema = json.loads(schema)
    try:
        return sum(1 for _ in _best_validator_for(schema)(schema).iter_errors(data))
    except _JSSchemaError:
        return 1


def json_schema_from_file(data: Any, schema_path: str) -> None:
    """Load a JSON Schema from *schema_path* and validate *data* against it.

    Raises FileNotFoundError if the file is missing.
    Raises RuntimeError on validation failure (same format as json_schema_validate).

    Args:
        data:        Value to validate.
        schema_path: Path to a .json file containing a JSON Schema object.
    """
    _require_jsonschema()
    abs_path = os.path.abspath(str(schema_path))
    if not os.path.isfile(abs_path):
        raise FileNotFoundError(f"Schema file not found: {abs_path}")
    with open(abs_path, encoding="utf-8") as fh:
        schema = json.load(fh)
    json_schema_validate(data, schema)


def json_schema_draft_version(schema: Any) -> str:
    """Return the declared JSON Schema draft version string.

    Inspects the ``$schema`` keyword.  Returns ``'unknown'`` when absent.

    Args:
        schema: JSON Schema dict or JSON string.

    Returns:
        One of ``'Draft 2020-12'``, ``'Draft 2019-09'``, ``'Draft 7'``,
        ``'Draft 6'``, ``'Draft 4'``, ``'Draft 3'``, or ``'unknown'``.
    """
    if isinstance(schema, str):
        try:
            schema = json.loads(schema)
        except json.JSONDecodeError:
            return "unknown"
    if not isinstance(schema, dict):
        return "unknown"
    meta = schema.get("$schema", "")
    for needle, label in _SCHEMA_DRAFT_MAP:
        if needle in meta:
            return label
    return "unknown" if not meta else "unknown"


def json_schema_infer(data: Any) -> Dict:
    """Infer a simple JSON Schema Draft 7 object from a sample *data* value.

    Useful for quick schema bootstrapping.  The returned schema captures
    ``type``, ``properties`` / ``items``, and marks all top-level dict keys
    as ``required``.

    Args:
        data: Any JSON-compatible Python value.

    Returns:
        A dict representing a JSON Schema.
    """
    def _infer(value: Any) -> Dict:
        if isinstance(value, bool):
            return {"type": "boolean"}
        if isinstance(value, int):
            return {"type": "integer"}
        if isinstance(value, float):
            return {"type": "number"}
        if isinstance(value, str):
            return {"type": "string"}
        if value is None:
            return {"type": "null"}
        if isinstance(value, list):
            if value:
                return {"type": "array", "items": _infer(value[0])}
            return {"type": "array"}
        if isinstance(value, dict):
            props = {k: _infer(v) for k, v in value.items()}
            schema: Dict = {
                "type": "object",
                "properties": props,
            }
            if props:
                schema["required"] = list(props.keys())
            return schema
        return {}

    result = _infer(data)
    result["$schema"] = "http://json-schema.org/draft-07/schema#"
    return result


def register_validation_functions(runtime: Runtime) -> None:
    """Register validation functions with the runtime."""
    
    # Type validation
    runtime.register_function("validate_type", validate_type)
    runtime.register_function("validate_range", validate_range)
    runtime.register_function("validate_length", validate_length)
    runtime.register_function("validate_pattern", validate_pattern)
    
    # Common validations
    runtime.register_function("validate_email", validate_email)
    runtime.register_function("validate_url", validate_url)
    runtime.register_function("validate_required", validate_required)
    runtime.register_function("validate_not_empty", validate_not_empty)
    runtime.register_function("validate_in_list", validate_in_list)
    runtime.register_function("validate_unique", validate_unique)
    
    # Schema validation
    runtime.register_function("validate_schema", validate_schema)
    
    # Sanitization
    runtime.register_function("sanitize_string", sanitize_string)
    runtime.register_function("sanitize_html", sanitize_html)

    # JSON Schema validation
    runtime.register_function("json_schema_validate", json_schema_validate)
    runtime.register_function("json_schema_is_valid", json_schema_is_valid)
    runtime.register_function("json_schema_errors", json_schema_errors)
    runtime.register_function("json_schema_first_error", json_schema_first_error)
    runtime.register_function("json_schema_error_count", json_schema_error_count)
    runtime.register_function("json_schema_from_file", json_schema_from_file)
    runtime.register_function("json_schema_draft_version", json_schema_draft_version)
    runtime.register_function("json_schema_infer", json_schema_infer)
