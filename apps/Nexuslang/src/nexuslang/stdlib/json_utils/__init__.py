"""
JSON parsing and serialization for NexusLang.
"""

import json
from typing import Any, Optional
from ...runtime.runtime import Runtime


def parse_json(json_string: str) -> Any:
    """Parse JSON string to Python object (dict/list/etc)."""
    try:
        return json.loads(json_string)
    except json.JSONDecodeError as e:
        raise ValueError(f"Invalid JSON: {e}")


def to_json(obj: Any, pretty: bool = False) -> str:
    """Convert Python object to JSON string."""
    try:
        if pretty:
            return json.dumps(obj, indent=2, ensure_ascii=False)
        return json.dumps(obj, ensure_ascii=False)
    except (TypeError, ValueError) as e:
        raise ValueError(f"Cannot convert to JSON: {e}")


def parse_json_file(filepath: str) -> Any:
    """Read and parse JSON file."""
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            return json.load(f)
    except json.JSONDecodeError as e:
        raise ValueError(f"Invalid JSON in file {filepath}: {e}")
    except FileNotFoundError:
        raise FileNotFoundError(f"File not found: {filepath}")


def write_json_file(filepath: str, obj: Any, pretty: bool = True) -> bool:
    """Write object to JSON file."""
    try:
        with open(filepath, 'w', encoding='utf-8') as f:
            if pretty:
                json.dump(obj, f, indent=2, ensure_ascii=False)
            else:
                json.dump(obj, f, ensure_ascii=False)
        return True
    except (TypeError, ValueError, IOError) as e:
        print(f"Error writing JSON file: {e}")
        return False


def is_valid_json(json_string: str) -> bool:
    """Check if string is valid JSON."""
    try:
        json.loads(json_string)
        return True
    except json.JSONDecodeError:
        return False


def pretty_json(obj: Any, indent: int = 2) -> str:
    """Convert object to pretty-printed JSON string with sorted keys."""
    try:
        return json.dumps(obj, indent=indent, ensure_ascii=False, sort_keys=True)
    except (TypeError, ValueError) as e:
        raise ValueError(f"Cannot convert to JSON: {e}")


def json_get(obj: Any, path: str, default: Any = None) -> Any:
    """
    Get value from JSON object using dot notation path.
    Examples: json_get(data, "user.name"), json_get(data, "items.0.price")
    Returns default if path not found.
    """
    try:
        keys = path.split('.')
        current = obj
        
        for key in keys:
            # Handle array indices
            if key.isdigit():
                index = int(key)
                if isinstance(current, list) and 0 <= index < len(current):
                    current = current[index]
                else:
                    return default
            # Handle dict keys
            elif isinstance(current, dict):
                if key in current:
                    current = current[key]
                else:
                    return default
            else:
                return default
        
        return current
    except Exception:
        return default


def json_set(obj: Any, path: str, value: Any) -> bool:
    """
    Set value in JSON object using dot notation path.
    Creates intermediate dicts/lists as needed. Returns True if successful.
    """
    try:
        keys = path.split('.')
        if not keys:
            return False
        
        current = obj
        for i, key in enumerate(keys[:-1]):
            if key.isdigit():
                index = int(key)
                if not isinstance(current, list):
                    return False
                # Extend list if needed
                while len(current) <= index:
                    current.append({})
                current = current[index]
            else:
                if not isinstance(current, dict):
                    return False
                # Create intermediate dict if needed
                if key not in current:
                    next_key = keys[i + 1]
                    current[key] = [] if next_key.isdigit() else {}
                current = current[key]
        
        # Set final value
        final_key = keys[-1]
        if final_key.isdigit():
            index = int(final_key)
            if isinstance(current, list):
                while len(current) <= index:
                    current.append(None)
                current[index] = value
                return True
        elif isinstance(current, dict):
            current[final_key] = value
            return True
        
        return False
    except Exception:
        return False


def register_json_functions(runtime: Runtime) -> None:
    """Register JSON functions with the runtime."""
    runtime.register_function("parse_json", parse_json)
    runtime.register_function("to_json", to_json)
    runtime.register_function("parse_json_file", parse_json_file)
    runtime.register_function("write_json_file", write_json_file)
    runtime.register_function("is_valid_json", is_valid_json)
    runtime.register_function("pretty_json", pretty_json)
    runtime.register_function("json_get", json_get)
    runtime.register_function("json_set", json_set)
    
    # Aliases
    runtime.register_function("json_parse", parse_json)
    runtime.register_function("json_stringify", to_json)
    runtime.register_function("json_load", parse_json_file)
    runtime.register_function("json_dump", write_json_file)
    runtime.register_function("json_loads", parse_json)
    runtime.register_function("json_dumps", to_json)
    runtime.register_function("read_json_file", parse_json_file)
