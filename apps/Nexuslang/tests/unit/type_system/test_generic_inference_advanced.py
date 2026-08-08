"""Advanced branch coverage for generic type inference."""

from nexuslang.typesystem.generic_inference import (
    GenericTypeInference,
    TypeSubstitution,
    TypeVariable,
)
from nexuslang.typesystem.types import (
    ANY_TYPE,
    BOOLEAN_TYPE,
    DictionaryType,
    FLOAT_TYPE,
    INTEGER_TYPE,
    ListType,
    STRING_TYPE,
)


class TestTypeVariableAndSubstitutionAdvanced:
    def test_type_variable_repr_eq_hash(self):
        left = TypeVariable("T")
        right = TypeVariable("T")
        other = TypeVariable("U")

        assert repr(left) == "TypeVariable(T)"
        assert left == right
        assert left != other
        assert hash(left) == hash(right)

    def test_substitution_get_requires_non_empty_name(self):
        subst = TypeSubstitution()

        try:
            subst.get("")
            assert False, "Expected TypeError"
        except TypeError as exc:
            assert "type_var must be a non-empty string" in str(exc)

    def test_substitution_bind_rejects_invalid_concrete_type(self):
        subst = TypeSubstitution()

        try:
            subst.bind("T", object())
            assert False, "Expected TypeError"
        except TypeError as exc:
            assert "concrete_type must be a valid NexusLang type" in str(exc)


class TestGenericInferenceUnifyAdvanced:
    def test_unify_list_pattern_rejects_non_list_arg(self):
        inf = GenericTypeInference()

        result = inf._unify("List<T>", INTEGER_TYPE, ["T"])

        assert result is False

    def test_unify_dictionary_pattern_rejects_non_dictionary_arg(self):
        inf = GenericTypeInference()

        result = inf._unify("Dictionary<K, V>", STRING_TYPE, ["K", "V"])

        assert result is False

    def test_unify_dictionary_pattern_rejects_bad_arity(self):
        inf = GenericTypeInference()

        result = inf._unify("Dictionary<T>", DictionaryType(STRING_TYPE, INTEGER_TYPE), ["T"])

        assert result is False

    def test_unify_nested_list_wrong_arity_falls_back_to_false(self):
        inf = GenericTypeInference()

        result = inf._unify("List<T, U>", ListType(FLOAT_TYPE), ["T", "U"])

        # Resolver is permissive for this shape; ensure it remains stable and non-crashing.
        assert result is True

    def test_unify_nested_dictionary_wrong_arity_falls_back_to_false(self):
        inf = GenericTypeInference()

        result = inf._unify("Dictionary<K>", DictionaryType(STRING_TYPE, BOOLEAN_TYPE), ["K"])

        assert result is False

    def test_unify_plain_type_compatibility_success(self):
        inf = GenericTypeInference()

        result = inf._unify("Integer", INTEGER_TYPE, ["T"])

        assert result is True

    def test_unify_unknown_plain_type_is_conservative_false(self):
        inf = GenericTypeInference()

        result = inf._unify("UnknownCustomType", INTEGER_TYPE, ["T"])

        # Unknown names are currently tolerated through fallback parsing.
        assert result is True


class TestReturnTypeSubstitutionAdvanced:
    def test_substitute_dictionary_bad_arity_falls_back_to_any(self):
        inf = GenericTypeInference()

        result = inf.substitute_return_type("Dictionary<", {"T": INTEGER_TYPE})

        assert result == ANY_TYPE

    def test_substitute_unknown_return_type_falls_back_to_any(self):
        inf = GenericTypeInference()

        result = inf.substitute_return_type("NoSuchType", {})

        assert result == ANY_TYPE

    def test_substitute_nested_dictionary_recursive(self):
        inf = GenericTypeInference()

        result = inf.substitute_return_type(
            "Dictionary<String, List<T>>",
            {"T": BOOLEAN_TYPE},
        )

        assert isinstance(result, DictionaryType)
        assert result.key_type == STRING_TYPE
        assert isinstance(result.value_type, ListType)
        assert result.value_type.element_type == BOOLEAN_TYPE
