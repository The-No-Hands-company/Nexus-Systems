"""High-yield coverage slice for typechecker.check_class_definition."""

from types import SimpleNamespace

from nexuslang.parser.ast import ClassDefinition, MethodDefinition, PropertyDeclaration, Literal
from nexuslang.typesystem.typechecker import TypeChecker, TypeEnvironment
from nexuslang.typesystem.types import ANY_TYPE, ClassType


class _FakeHKTRegistry:
    def __init__(self, known=None):
        self.known = set(known or [])

    def get(self, name):
        return object() if name in self.known else None


def _checker() -> TypeChecker:
    return TypeChecker(enable_ownership_passes=False)


def _env() -> TypeEnvironment:
    return TypeEnvironment()


def test_check_class_definition_reuses_existing_registered_type():
    checker = _checker()
    env = _env()

    existing = ClassType("Already", {"x": ANY_TYPE}, {}, None)
    checker.type_registry["Already"] = existing

    definition = ClassDefinition("Already", properties=[], methods=[])

    result = checker.check_class_definition(definition, env)

    assert result is existing
    assert env.get_variable_type("Already") is existing


def test_check_class_definition_collects_properties_and_methods():
    checker = _checker()
    env = _env()

    method = MethodDefinition(
        name="compute",
        parameters=[SimpleNamespace(name="value", type_annotation="Integer")],
        body=[Literal("integer", 1)],
        return_type="Integer",
    )
    definition = ClassDefinition(
        name="Invoice",
        properties=[
            PropertyDeclaration("total", type_annotation="Float"),
            PropertyDeclaration("meta"),
        ],
        methods=[method],
    )

    result = checker.check_class_definition(definition, env)

    assert isinstance(result, ClassType)
    assert result.name == "Invoice"
    assert "total" in result.properties
    assert "meta" in result.properties
    assert "compute" in result.methods
    assert env.get_variable_type("Invoice") == result


def test_check_class_definition_reports_missing_parent_class():
    checker = _checker()
    env = _env()

    definition = ClassDefinition(
        name="Child",
        properties=[],
        methods=[],
        parent_classes=["MissingParent"],
    )
    definition.line_number = 12

    checker.check_class_definition(definition, env)

    assert any("Parent class 'MissingParent' not defined" in err for err in checker.errors)


def test_check_class_definition_reports_missing_interface():
    checker = _checker()
    env = _env()

    definition = ClassDefinition(
        name="Renderer",
        properties=[],
        methods=[],
        implemented_interfaces=["Renderable"],
    )
    definition.line_number = 21

    checker.check_class_definition(definition, env)

    assert any("Interface 'Renderable' not defined" in err for err in checker.errors)


def test_check_class_definition_reports_missing_interface_methods():
    checker = _checker()
    env = _env()

    checker.type_registry["Reportable"] = ClassType("Reportable", {}, {}, None)
    checker.type_registry.register_interface("Reportable", ["render", "export"])

    definition = ClassDefinition(
        name="InvoiceReport",
        properties=[],
        methods=[MethodDefinition("render", [], body=[])],
        implemented_interfaces=["Reportable"],
    )
    definition.line_number = 33

    checker.check_class_definition(definition, env)

    assert any("does not implement methods required by interface 'Reportable'" in err for err in checker.errors)


def test_check_class_definition_invokes_hkt_validation_for_known_interface():
    checker = _checker()
    env = _env()

    checker.type_registry["Functor"] = ClassType("Functor", {}, {}, None)
    checker.type_registry.register_interface("Functor", [])
    checker.hkt_registry = _FakeHKTRegistry({"Functor"})

    calls = []

    def _capture_hkt(class_name, interface_name, line_number):
        calls.append((class_name, interface_name, line_number))

    checker.check_hkt_implementation = _capture_hkt

    definition = ClassDefinition(
        name="Box",
        properties=[],
        methods=[],
        implemented_interfaces=["Functor"],
    )
    definition.line_number = 44

    checker.check_class_definition(definition, env)

    assert calls == [("Box", "Functor", 44)]
