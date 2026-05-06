"""
Tests for JSON Schema validation functions in stdlib/validation.

Coverage
--------
- json_schema_validate  (success, failure, schema from string)
- json_schema_is_valid  (bool result)
- json_schema_errors    (list of messages)
- json_schema_first_error (first error or None)
- json_schema_error_count (count)
- json_schema_from_file (file-based schema loading)
- json_schema_draft_version (draft detection)
- json_schema_infer (schema inference)
- Registration (all 8 names present in runtime)
"""

import json
import os
import tempfile

import pytest

from nexuslang.stdlib.validation import (
    json_schema_validate,
    json_schema_is_valid,
    json_schema_errors,
    json_schema_first_error,
    json_schema_error_count,
    json_schema_from_file,
    json_schema_draft_version,
    json_schema_infer,
    register_validation_functions,
)


# ---------------------------------------------------------------------------
# Shared fixtures / schemas
# ---------------------------------------------------------------------------

PERSON_SCHEMA = {
    "$schema": "http://json-schema.org/draft-07/schema#",
    "type": "object",
    "properties": {
        "name": {"type": "string"},
        "age":  {"type": "integer", "minimum": 0},
        "email": {"type": "string", "format": "email"},
    },
    "required": ["name", "age"],
}

VALID_PERSON   = {"name": "Alice", "age": 30}
INVALID_PERSON = {"name": "Bob",   "age": -5}     # age < minimum
MISSING_FIELD  = {"name": "Carol"}                 # age required

STRING_SCHEMA = {"type": "string"}
INT_SCHEMA    = {"type": "integer"}
ARRAY_SCHEMA  = {"type": "array", "items": {"type": "integer"}, "minItems": 1}


class _FakeRuntime:
    def __init__(self):
        self.functions = {}

    def register_function(self, name, fn):
        self.functions[name] = fn


# ---------------------------------------------------------------------------
# json_schema_validate
# ---------------------------------------------------------------------------

class TestJsonSchemaValidate:
    def test_valid_data_returns_none(self):
        assert json_schema_validate(VALID_PERSON, PERSON_SCHEMA) is None

    def test_invalid_minimum_raises(self):
        with pytest.raises(RuntimeError, match="validation failed"):
            json_schema_validate(INVALID_PERSON, PERSON_SCHEMA)

    def test_missing_required_field_raises(self):
        with pytest.raises(RuntimeError, match="validation failed"):
            json_schema_validate(MISSING_FIELD, PERSON_SCHEMA)

    def test_type_mismatch_raises(self):
        with pytest.raises(RuntimeError):
            json_schema_validate("not an object", PERSON_SCHEMA)

    def test_schema_as_json_string(self):
        schema_str = json.dumps(STRING_SCHEMA)
        assert json_schema_validate("hello", schema_str) is None

    def test_schema_as_json_string_invalid(self):
        schema_str = json.dumps(STRING_SCHEMA)
        with pytest.raises(RuntimeError):
            json_schema_validate(42, schema_str)

    def test_error_message_contains_path_for_nested(self):
        nested_schema = {
            "type": "object",
            "properties": {"child": {"type": "string"}},
            "required": ["child"],
        }
        with pytest.raises(RuntimeError) as exc_info:
            json_schema_validate({"child": 123}, nested_schema)
        # Should mention the property path
        assert "child" in str(exc_info.value)

    def test_simple_string_valid(self):
        json_schema_validate("hello", STRING_SCHEMA)

    def test_simple_integer_valid(self):
        json_schema_validate(42, INT_SCHEMA)

    def test_integer_schema_rejects_float_string(self):
        with pytest.raises(RuntimeError):
            json_schema_validate("42", INT_SCHEMA)

    def test_array_schema_valid(self):
        json_schema_validate([1, 2, 3], ARRAY_SCHEMA)

    def test_array_schema_empty_fails_min_items(self):
        with pytest.raises(RuntimeError):
            json_schema_validate([], ARRAY_SCHEMA)

    def test_null_type_schema(self):
        json_schema_validate(None, {"type": "null"})

    def test_boolean_type_schema(self):
        json_schema_validate(True, {"type": "boolean"})

    def test_enum_valid(self):
        json_schema_validate("red", {"enum": ["red", "green", "blue"]})

    def test_enum_invalid(self):
        with pytest.raises(RuntimeError):
            json_schema_validate("purple", {"enum": ["red", "green", "blue"]})

    def test_allof_valid(self):
        schema = {"allOf": [{"type": "integer"}, {"minimum": 0}]}
        json_schema_validate(5, schema)

    def test_allof_invalid(self):
        schema = {"allOf": [{"type": "integer"}, {"minimum": 0}]}
        with pytest.raises(RuntimeError):
            json_schema_validate(-1, schema)

    def test_anyof_valid(self):
        schema = {"anyOf": [{"type": "string"}, {"type": "integer"}]}
        json_schema_validate("text", schema)
        json_schema_validate(1, schema)

    def test_anyof_invalid(self):
        schema = {"anyOf": [{"type": "string"}, {"type": "integer"}]}
        with pytest.raises(RuntimeError):
            json_schema_validate(1.5, schema)

    def test_additional_properties_false(self):
        schema = {
            "type": "object",
            "properties": {"x": {"type": "integer"}},
            "additionalProperties": False,
        }
        with pytest.raises(RuntimeError):
            json_schema_validate({"x": 1, "y": 2}, schema)

    def test_pattern_keyword(self):
        schema = {"type": "string", "pattern": "^[a-z]+$"}
        json_schema_validate("abc", schema)
        with pytest.raises(RuntimeError):
            json_schema_validate("ABC", schema)


# ---------------------------------------------------------------------------
# json_schema_is_valid
# ---------------------------------------------------------------------------

class TestJsonSchemaIsValid:
    def test_returns_true_for_valid(self):
        assert json_schema_is_valid(VALID_PERSON, PERSON_SCHEMA) is True

    def test_returns_false_for_invalid(self):
        assert json_schema_is_valid(INVALID_PERSON, PERSON_SCHEMA) is False

    def test_returns_false_for_missing_required(self):
        assert json_schema_is_valid(MISSING_FIELD, PERSON_SCHEMA) is False

    def test_string_schema_true(self):
        assert json_schema_is_valid("hi", STRING_SCHEMA) is True

    def test_string_schema_false(self):
        assert json_schema_is_valid(123, STRING_SCHEMA) is False

    def test_schema_as_json_string(self):
        assert json_schema_is_valid("hi", json.dumps(STRING_SCHEMA)) is True

    def test_invalid_schema_does_not_raise(self):
        bad_schema = {"type": "not-a-real-type"}
        # jsonschema raises UnknownType internally; is_valid must never propagate it
        result = json_schema_is_valid("anything", bad_schema)
        assert isinstance(result, bool)

    def test_empty_schema_always_valid(self):
        assert json_schema_is_valid({"anything": 1}, {}) is True


# ---------------------------------------------------------------------------
# json_schema_errors
# ---------------------------------------------------------------------------

class TestJsonSchemaErrors:
    def test_empty_list_for_valid_data(self):
        assert json_schema_errors(VALID_PERSON, PERSON_SCHEMA) == []

    def test_non_empty_list_for_invalid(self):
        errs = json_schema_errors(INVALID_PERSON, PERSON_SCHEMA)
        assert isinstance(errs, list)
        assert len(errs) >= 1

    def test_each_error_is_string(self):
        errs = json_schema_errors(INVALID_PERSON, PERSON_SCHEMA)
        for e in errs:
            assert isinstance(e, str)

    def test_multiple_errors_for_multiple_violations(self):
        bad = {"name": 123, "age": -1}
        errs = json_schema_errors(bad, PERSON_SCHEMA)
        assert len(errs) >= 2

    def test_schema_as_json_string(self):
        errs = json_schema_errors(42, json.dumps(STRING_SCHEMA))
        assert len(errs) >= 1


# ---------------------------------------------------------------------------
# json_schema_first_error
# ---------------------------------------------------------------------------

class TestJsonSchemaFirstError:
    def test_none_for_valid(self):
        assert json_schema_first_error(VALID_PERSON, PERSON_SCHEMA) is None

    def test_string_for_invalid(self):
        err = json_schema_first_error(INVALID_PERSON, PERSON_SCHEMA)
        assert isinstance(err, str)
        assert len(err) > 0

    def test_none_for_valid_string(self):
        assert json_schema_first_error("hi", STRING_SCHEMA) is None

    def test_error_for_invalid_string(self):
        assert json_schema_first_error(42, STRING_SCHEMA) is not None


# ---------------------------------------------------------------------------
# json_schema_error_count
# ---------------------------------------------------------------------------

class TestJsonSchemaErrorCount:
    def test_zero_for_valid(self):
        assert json_schema_error_count(VALID_PERSON, PERSON_SCHEMA) == 0

    def test_positive_for_invalid(self):
        assert json_schema_error_count(INVALID_PERSON, PERSON_SCHEMA) >= 1

    def test_greater_count_for_more_violations(self):
        bad = {"name": 123, "age": -1}
        assert json_schema_error_count(bad, PERSON_SCHEMA) >= 2

    def test_zero_for_empty_schema(self):
        assert json_schema_error_count({"x": 1}, {}) == 0


# ---------------------------------------------------------------------------
# json_schema_from_file
# ---------------------------------------------------------------------------

class TestJsonSchemaFromFile:
    def _write_schema(self, tmpdir, schema):
        path = os.path.join(tmpdir, "schema.json")
        with open(path, "w", encoding="utf-8") as fh:
            json.dump(schema, fh)
        return path

    def test_valid_data_returns_none(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            path = self._write_schema(tmpdir, PERSON_SCHEMA)
            assert json_schema_from_file(VALID_PERSON, path) is None

    def test_invalid_data_raises(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            path = self._write_schema(tmpdir, PERSON_SCHEMA)
            with pytest.raises(RuntimeError):
                json_schema_from_file(INVALID_PERSON, path)

    def test_missing_file_raises_file_not_found(self):
        with pytest.raises(FileNotFoundError):
            json_schema_from_file({}, "/nonexistent/path/schema.json")

    def test_string_schema_file(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            path = self._write_schema(tmpdir, STRING_SCHEMA)
            json_schema_from_file("valid", path)

    def test_string_schema_file_invalid(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            path = self._write_schema(tmpdir, STRING_SCHEMA)
            with pytest.raises(RuntimeError):
                json_schema_from_file(99, path)


# ---------------------------------------------------------------------------
# json_schema_draft_version
# ---------------------------------------------------------------------------

class TestJsonSchemaDraftVersion:
    def test_detects_draft_07(self):
        schema = {"$schema": "http://json-schema.org/draft-07/schema#"}
        assert json_schema_draft_version(schema) == "Draft 7"

    def test_detects_draft_04(self):
        schema = {"$schema": "http://json-schema.org/draft-04/schema#"}
        assert json_schema_draft_version(schema) == "Draft 4"

    def test_detects_2019_09(self):
        schema = {"$schema": "https://json-schema.org/draft/2019-09/schema"}
        assert json_schema_draft_version(schema) == "Draft 2019-09"

    def test_detects_2020_12(self):
        schema = {"$schema": "https://json-schema.org/draft/2020-12/schema"}
        assert json_schema_draft_version(schema) == "Draft 2020-12"

    def test_unknown_when_no_schema_keyword(self):
        assert json_schema_draft_version({"type": "string"}) == "unknown"

    def test_unknown_for_non_dict(self):
        assert json_schema_draft_version("not a schema") == "unknown"

    def test_accepts_json_string_input(self):
        schema_str = json.dumps({"$schema": "http://json-schema.org/draft-07/schema#"})
        assert json_schema_draft_version(schema_str) == "Draft 7"

    def test_bad_json_string_returns_unknown(self):
        assert json_schema_draft_version("{not json}") == "unknown"


# ---------------------------------------------------------------------------
# json_schema_infer
# ---------------------------------------------------------------------------

class TestJsonSchemaInfer:
    def test_returns_dict(self):
        assert isinstance(json_schema_infer({"x": 1}), dict)

    def test_has_schema_keyword(self):
        result = json_schema_infer({"x": 1})
        assert "$schema" in result

    def test_object_type(self):
        result = json_schema_infer({"name": "Alice"})
        assert result["type"] == "object"
        assert "properties" in result
        assert result["properties"]["name"]["type"] == "string"

    def test_required_contains_all_keys(self):
        result = json_schema_infer({"a": 1, "b": "x"})
        assert set(result.get("required", [])) == {"a", "b"}

    def test_string_scalar(self):
        result = json_schema_infer("hello")
        assert result["type"] == "string"

    def test_integer_scalar(self):
        result = json_schema_infer(42)
        assert result["type"] == "integer"

    def test_float_scalar(self):
        result = json_schema_infer(3.14)
        assert result["type"] == "number"

    def test_bool_scalar(self):
        result = json_schema_infer(True)
        assert result["type"] == "boolean"

    def test_null_scalar(self):
        result = json_schema_infer(None)
        assert result["type"] == "null"

    def test_array_with_items(self):
        result = json_schema_infer([1, 2, 3])
        assert result["type"] == "array"
        assert result["items"]["type"] == "integer"

    def test_empty_array(self):
        result = json_schema_infer([])
        assert result["type"] == "array"

    def test_nested_object(self):
        result = json_schema_infer({"address": {"city": "London"}})
        city_schema = result["properties"]["address"]["properties"]["city"]
        assert city_schema["type"] == "string"

    def test_inferred_schema_validates_original(self):
        data = {"name": "Alice", "age": 30}
        schema = json_schema_infer(data)
        assert json_schema_is_valid(data, schema)


# ---------------------------------------------------------------------------
# Registration
# ---------------------------------------------------------------------------

class TestJsonSchemaRegistration:
    EXPECTED = {
        "json_schema_validate",
        "json_schema_is_valid",
        "json_schema_errors",
        "json_schema_first_error",
        "json_schema_error_count",
        "json_schema_from_file",
        "json_schema_draft_version",
        "json_schema_infer",
    }

    def setup_method(self):
        self._rt = _FakeRuntime()
        register_validation_functions(self._rt)

    def test_all_json_schema_names_registered(self):
        missing = self.EXPECTED - set(self._rt.functions.keys())
        assert not missing, f"Missing: {missing}"

    def test_all_registered_callables(self):
        for name in self.EXPECTED:
            assert callable(self._rt.functions[name]), f"{name} is not callable"

    def test_existing_validate_functions_still_registered(self):
        for name in ("validate_type", "validate_range", "validate_email",
                     "validate_schema", "sanitize_html"):
            assert name in self._rt.functions
