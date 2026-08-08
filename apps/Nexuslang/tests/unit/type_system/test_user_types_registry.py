import pytest

from nexuslang.typesystem.types import ClassType, FunctionType, PrimitiveType, STRING_TYPE
from nexuslang.typesystem.user_types import TypeRegistry


class TestTypeRegistry:
    def test_register_type_and_lookup(self):
        reg = TypeRegistry()
        class_type = ClassType("Box", {"value": PrimitiveType("integer")}, {})

        reg.register_type(class_type)

        assert reg.get_type("Box") is class_type
        assert reg.inheritance_graph["Box"] == []

    def test_register_inheritance_requires_defined_parent(self):
        reg = TypeRegistry()

        with pytest.raises(ValueError, match="Parent class not defined: Base"):
            reg.register_inheritance("Child", "Base")

    def test_create_class_type_inherits_parent_properties_and_methods(self):
        reg = TypeRegistry()
        parent_method = FunctionType([PrimitiveType("integer")], PrimitiveType("integer"))
        reg.create_class_type(
            "Base",
            properties={"id": PrimitiveType("integer")},
            methods={"get_id": parent_method},
        )

        child_method = FunctionType([PrimitiveType("integer")], PrimitiveType("integer"))
        child = reg.create_class_type(
            "Child",
            properties={"name": STRING_TYPE},
            methods={"set_id": child_method},
            parent_classes=["Base"],
        )

        assert reg.is_subtype("Child", "Base") is True
        assert child.properties["id"].name == "integer"
        assert child.properties["name"].name == "string"
        assert child.methods["get_id"] is parent_method
        assert child.methods["set_id"] is child_method

    def test_inherited_properties_and_methods_traverse_multiple_generations(self):
        reg = TypeRegistry()
        base_method = FunctionType([PrimitiveType("integer")], PrimitiveType("integer"))
        reg.create_class_type("Base", properties={"base_prop": PrimitiveType("integer")}, methods={"base_fn": base_method})
        reg.create_class_type("Mid", properties={"mid_prop": PrimitiveType("integer")}, methods={}, parent_classes=["Base"])
        reg.create_class_type("Leaf", properties={"leaf_prop": PrimitiveType("integer")}, methods={}, parent_classes=["Mid"])

        props = reg.get_inherited_properties("Leaf")
        methods = reg.get_inherited_methods("Leaf")

        assert set(props) == {"mid_prop", "base_prop"}
        assert methods["base_fn"] is base_method

    def test_interface_registration_and_validation(self):
        reg = TypeRegistry()
        reg.register_interface("Displayable", ["render", "describe"])
        reg.create_class_type(
            "Widget",
            properties={},
            methods={"render": FunctionType([], PrimitiveType("string"))},
        )

        missing = reg.check_interface_implementation("Widget", "Displayable")
        assert missing == ["describe"]

        with pytest.raises(ValueError, match="Interface not defined: Missing"):
            reg.register_interface_implementation("Widget", "Missing")

    def test_interface_implementation_uses_inherited_methods(self):
        reg = TypeRegistry()
        reg.register_interface("Renderable", ["render"])
        reg.create_class_type("Base", methods={"render": FunctionType([], PrimitiveType("string"))})
        reg.create_class_type("Child", methods={}, parent_classes=["Base"])

        reg.register_interface_implementation("Child", "Renderable")
        assert reg.is_subtype("Child", "Renderable") is True
        assert reg.check_interface_implementation("Child", "Renderable") == []

    def test_get_inherited_helpers_return_empty_for_unknown_or_non_class(self):
        reg = TypeRegistry()
        reg.register_type(PrimitiveType("integer"))

        assert reg.get_inherited_properties("Missing") == {}
        assert reg.get_inherited_methods("Missing") == {}
        assert reg.get_inherited_properties("integer") == {}
        assert reg.get_inherited_methods("integer") == {}

    def test_create_generic_class_type_preserves_base_type_and_registers(self):
        reg = TypeRegistry()
        generic = reg.create_generic_class_type(
            "Box",
            ["T"],
            properties={"value": PrimitiveType("integer")},
            methods={"get_value": FunctionType([], PrimitiveType("integer"))},
        )

        assert reg.get_type("Box") is generic
        assert generic.name == "Box"
        assert generic.type_parameters[0][0] == "T"
        assert generic.base_type.name == "Box"
        assert generic.base_type.properties["value"].name == "integer"
        assert "get_value" in generic.base_type.methods
