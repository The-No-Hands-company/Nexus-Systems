"""Focused branch coverage for core type objects and helper functions."""

import pytest

from nexuslang.typesystem.types import (
    ANY_TYPE,
    BOOLEAN_TYPE,
    FLOAT_TYPE,
    INTEGER_TYPE,
    STRING_TYPE,
    AwaitableType,
    ChannelType,
    ClassType,
    DictionaryType,
    ExistentialType,
    FunctionType,
    GenericParameter,
    GenericType,
    ListType,
    OptionType,
    PhantomType,
    PrimitiveType,
    ResultType,
    SetType,
    TraitType,
    TupleType,
    TypeAliasType,
    UnionType,
    Variance,
    _parse_type_arguments,
    get_type_by_name,
    infer_type,
)


def test_channel_and_awaitable_type_compatibility_and_common_supertype():
    channel_int = ChannelType(INTEGER_TYPE)
    channel_float = ChannelType(FLOAT_TYPE)
    awaitable_int = AwaitableType(INTEGER_TYPE)
    awaitable_float = AwaitableType(FLOAT_TYPE)

    assert channel_int.is_compatible_with(channel_float)
    assert channel_int.get_common_supertype(channel_float) == ChannelType(FLOAT_TYPE)
    assert channel_int.get_common_supertype(STRING_TYPE) == ANY_TYPE

    assert awaitable_int.is_compatible_with(awaitable_float)
    assert awaitable_int.get_common_supertype(awaitable_float) == AwaitableType(FLOAT_TYPE)
    assert awaitable_int.get_common_supertype(STRING_TYPE) == ANY_TYPE


def test_set_and_tuple_types_cover_mismatch_and_common_paths():
    set_int = SetType(INTEGER_TYPE)
    set_float = SetType(FLOAT_TYPE)

    assert set_int.is_compatible_with(set_float)
    assert set_int.get_common_supertype(set_float) == SetType(FLOAT_TYPE)
    assert set_int.get_common_supertype(STRING_TYPE) == ANY_TYPE

    tuple_a = TupleType([INTEGER_TYPE, STRING_TYPE])
    tuple_b = TupleType([FLOAT_TYPE, STRING_TYPE])
    tuple_short = TupleType([INTEGER_TYPE])

    assert tuple_a.is_compatible_with(tuple_b)
    assert not tuple_a.is_compatible_with(tuple_short)
    assert tuple_a.get_common_supertype(tuple_b) == TupleType([FLOAT_TYPE, STRING_TYPE])
    assert tuple_a.get_common_supertype(tuple_short) == ANY_TYPE


def test_function_type_covers_compatibility_and_common_supertype_edges():
    compatible = FunctionType([FLOAT_TYPE], INTEGER_TYPE)
    target = FunctionType([INTEGER_TYPE], FLOAT_TYPE)
    wrong_arity = FunctionType([INTEGER_TYPE, STRING_TYPE], FLOAT_TYPE)

    assert compatible.is_compatible_with(target)
    assert not compatible.is_compatible_with(wrong_arity)
    assert compatible.is_compatible_with(ANY_TYPE)

    common = compatible.get_common_supertype(target)
    assert common == FunctionType([FLOAT_TYPE], FLOAT_TYPE)
    assert compatible.get_common_supertype(wrong_arity) == ANY_TYPE


def test_class_type_covers_parent_and_structural_paths():
    parent = ClassType("Parent", {}, {}, [])
    child = ClassType("Child", {"value": INTEGER_TYPE}, {}, ["Parent"])
    needs_value_float = ClassType("NeedValue", {"value": FLOAT_TYPE}, {}, [])

    assert child.is_compatible_with(parent)
    assert child.is_compatible_with(needs_value_float)
    assert not parent.is_compatible_with(needs_value_float)

    lhs = ClassType(
        "Left",
        {"value": INTEGER_TYPE, "name": STRING_TYPE},
        {"render": FunctionType([], STRING_TYPE)},
        ["Base"],
    )
    rhs = ClassType(
        "Right",
        {"value": FLOAT_TYPE, "enabled": BOOLEAN_TYPE},
        {"render": FunctionType([], STRING_TYPE)},
        ["Base"],
    )

    common = lhs.get_common_supertype(rhs)
    assert isinstance(common, ClassType)
    assert common.parent_classes == ["Base"]
    assert common.properties["value"] == FLOAT_TYPE
    assert common.methods["render"] == FunctionType([], STRING_TYPE)


def test_generic_type_variance_and_substitution_paths():
    box_t = GenericType("Box", [("T", Variance.INVARIANT)], ListType(GenericParameter("T")))
    box_u = GenericType("Box", [("U", Variance.INVARIANT)], ListType(GenericParameter("U")))
    box_covariant = GenericType("Box", [("U", Variance.COVARIANT)], ListType(GenericParameter("U")))

    assert box_t.is_compatible_with(box_u)
    assert not box_t.is_compatible_with(box_covariant)
    assert box_t.is_compatible_with(ANY_TYPE)

    common = box_t.get_common_supertype(box_u)
    assert isinstance(common, GenericType)
    assert common.name == "Box"

    nested = FunctionType(
        [DictionaryType(GenericParameter("T"), ListType(GenericParameter("T")))],
        UnionType([GenericParameter("T"), STRING_TYPE]),
    )
    substituted = box_t._substitute_types(nested, {"T": "U"})

    assert isinstance(substituted, FunctionType)
    assert substituted.param_types[0] == DictionaryType(GenericParameter("U"), ListType(GenericParameter("U")))


def test_option_and_result_type_behavior_and_result_base_type():
    option_int = OptionType(INTEGER_TYPE)
    option_float = OptionType(FLOAT_TYPE)

    assert option_int.is_compatible_with(option_float)
    assert not option_int.is_compatible_with(STRING_TYPE)
    assert option_int.get_common_supertype(option_float) == OptionType(FLOAT_TYPE)

    result_a = ResultType(INTEGER_TYPE, STRING_TYPE)
    result_b = ResultType(FLOAT_TYPE, STRING_TYPE)

    assert result_a.is_compatible_with(result_b)
    assert result_a.get_common_supertype(result_b) == ResultType(FLOAT_TYPE, STRING_TYPE)
    assert result_a.base_type.is_compatible_with(INTEGER_TYPE)
    assert "Union[" in str(result_a.base_type)


def test_type_alias_instantiation_and_constraint_validation():
    alias = TypeAliasType("Vec", ["T"], ListType(GenericParameter("T")))

    assert alias.instantiate([INTEGER_TYPE]) == ListType(INTEGER_TYPE)

    with pytest.raises(TypeError):
        alias.instantiate([])

    printable_like = TraitType(
        "PrintableLike",
        {"to_string": FunctionType([], STRING_TYPE)},
    )
    constrained = TypeAliasType(
        "Constrained",
        ["T"],
        GenericParameter("T"),
        constraints={"T": [printable_like]},
    )
    with pytest.raises(TypeError):
        constrained.instantiate([ClassType("NoString", {}, {}, [])])


def test_phantom_and_existential_types_cover_compatibility_paths():
    phantom_int = PhantomType("Id", INTEGER_TYPE)
    phantom_float = PhantomType("Id", FLOAT_TYPE)

    assert phantom_int.is_compatible_with(PhantomType("Id", INTEGER_TYPE))
    assert not phantom_int.is_compatible_with(phantom_float)
    assert phantom_int.get_common_supertype(phantom_float) == ANY_TYPE

    comparable = TraitType("Comparable", {"compare": FunctionType([ANY_TYPE], INTEGER_TYPE)})
    existential = ExistentialType([comparable])
    comparable_class = ClassType("NumberLike", {}, {"compare": FunctionType([ANY_TYPE], INTEGER_TYPE)}, [])

    assert existential.is_compatible_with(comparable_class)
    assert not existential.is_compatible_with(PrimitiveType("boolean"))
    assert existential.get_common_supertype(STRING_TYPE) == ANY_TYPE


def test_type_helpers_cover_specialized_parsing_and_inference_paths():
    assert _parse_type_arguments("Dictionary<String, List<Integer>>, Awaitable<Float>") == [
        "Dictionary<String, List<Integer>>",
        "Awaitable<Float>",
    ]

    assert get_type_by_name("Dictionary<Integer, String, Float>") == DictionaryType(ANY_TYPE, ANY_TYPE)
    assert get_type_by_name("Channel") == ChannelType(ANY_TYPE)
    assert get_type_by_name("Awaitable<Float>") == AwaitableType(FLOAT_TYPE)
    assert get_type_by_name("task of Integer") == AwaitableType(INTEGER_TYPE)
    assert get_type_by_name("Promise of String") == AwaitableType(STRING_TYPE)
    assert get_type_by_name("Comparable").name == "Comparable"
    assert get_type_by_name("Unknown<Thing>") == ANY_TYPE

    assert infer_type([1, 2.0]) == ListType(FLOAT_TYPE)
    assert infer_type({"k": 1, 2: 2.5}) == DictionaryType(ANY_TYPE, FLOAT_TYPE)
    assert infer_type({}) == DictionaryType(ANY_TYPE, ANY_TYPE)
