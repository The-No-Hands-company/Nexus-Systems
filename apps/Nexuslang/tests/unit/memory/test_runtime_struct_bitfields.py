"""Regression tests for low-level struct/union bit-field support."""

import pytest

from nexuslang.runtime.structures import StructDefinition, StructureInstance
from tests.helpers.utils import execute_nxl


class TestRuntimeStructBitFields:
    def test_layout_packs_adjacent_bit_fields(self):
        definition = StructDefinition(
            "Flags",
            [
                ("low", "Integer", 3),
                ("mid", "Integer", 5),
                ("high", "Integer", 8),
            ],
        )

        assert definition.fields["low"].offset == 0
        assert definition.fields["low"].bit_offset == 0
        assert definition.fields["low"].bit_width == 3

        assert definition.fields["mid"].offset == 0
        assert definition.fields["mid"].bit_offset == 3
        assert definition.fields["mid"].bit_width == 5

        assert definition.fields["high"].offset == 0
        assert definition.fields["high"].bit_offset == 8
        assert definition.fields["high"].bit_width == 8

        # Integer-backed bit fields should pack into one 8-byte storage unit.
        assert definition.size == 8

    def test_layout_spills_to_next_storage_unit(self):
        definition = StructDefinition(
            "WideFlags",
            [
                ("a", "Integer", 63),
                ("b", "Integer", 2),
            ],
        )

        assert definition.fields["a"].offset == 0
        assert definition.fields["a"].bit_offset == 0
        assert definition.fields["b"].offset == 8
        assert definition.fields["b"].bit_offset == 0
        assert definition.size == 16

    def test_set_get_roundtrip_for_bit_fields(self):
        definition = StructDefinition(
            "Flags",
            [
                ("low", "Integer", 3),
                ("high", "Integer", 5),
            ],
        )
        instance = StructureInstance(definition)

        instance.set_field("low", 5)
        instance.set_field("high", 17)

        assert instance.get_field("low") == 5
        assert instance.get_field("high") == 17

    def test_bitfield_value_range_is_enforced(self):
        definition = StructDefinition("Flags", [("low", "Integer", 3)])
        instance = StructureInstance(definition)

        with pytest.raises(ValueError):
            instance.set_field("low", 8)

    def test_interpreter_keeps_bitfield_metadata(self):
        code = """
        struct Flags
            low as Integer with 3 bits
            high as Integer with 5 bits
        end
        """

        _, interpreter = execute_nxl(code)
        definition = interpreter.classes["Flags"]

        assert definition.fields["low"].bit_width == 3
        assert definition.fields["low"].bit_offset == 0
        assert definition.fields["high"].bit_width == 5
        assert definition.fields["high"].bit_offset == 3
