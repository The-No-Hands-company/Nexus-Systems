"""Focused coverage tests for type system integration entrypoint."""

from types import SimpleNamespace

from nexuslang.parser.ast import (
    ClassDefinition,
    MethodDefinition,
    Program,
    PropertyDeclaration,
    VariableDeclaration,
    FunctionDefinition,
    Literal,
)
from nexuslang.typesystem.integration import TypeSystem


def _snapshot_registry(registry):
    return (
        dict(registry.types),
        {k: list(v) for k, v in registry.inheritance_graph.items()},
        dict(registry.interfaces),
    )


def _restore_registry(registry, snapshot):
    types, inheritance, interfaces = snapshot
    registry.types.clear()
    registry.types.update(types)
    registry.inheritance_graph.clear()
    registry.inheritance_graph.update(inheritance)
    registry.interfaces.clear()
    registry.interfaces.update(interfaces)


def test_register_class_collects_properties_methods_and_interfaces():
    ts = TypeSystem()
    snap = _snapshot_registry(ts.type_registry)
    try:
        class_def = ClassDefinition(
            name="Invoice",
            properties=[PropertyDeclaration("total", type_annotation="Float")],
            methods=[
                MethodDefinition(
                    name="compute",
                    parameters=[SimpleNamespace(type_annotation="Integer")],
                    return_type="Integer",
                    body=[],
                )
            ],
            implemented_interfaces=[],
        )

        ts._register_class(class_def)

        invoice = ts.type_registry.get_type("Invoice")
        assert invoice is not None
        assert "total" in invoice.properties
        assert "compute" in invoice.methods
    finally:
        _restore_registry(ts.type_registry, snap)


def test_register_generic_class_path_registers_generic_type():
    ts = TypeSystem()
    snap = _snapshot_registry(ts.type_registry)
    try:
        class_def = ClassDefinition(
            name="Box",
            properties=[PropertyDeclaration("value", type_annotation="Any")],
            methods=[],
            generic_parameters=["T"],
        )

        ts._register_class(class_def)

        box = ts.type_registry.get_type("Box")
        assert box is not None
        assert box.name == "Box"
        assert box.base_type.name == "Box"
        assert box.type_parameters[0][0] == "T"
    finally:
        _restore_registry(ts.type_registry, snap)


def test_register_types_scans_program_and_analyze_program_uses_type_checker(monkeypatch):
    ts = TypeSystem()
    snap = _snapshot_registry(ts.type_registry)
    try:
        program = Program(
            [
                ClassDefinition("Customer", properties=[PropertyDeclaration("name", "String")]),
                VariableDeclaration("x", Literal("integer", 1)),
            ]
        )

        observed = {}

        def fake_check_program(ast_program):
            observed["program"] = ast_program
            return ["ok"]

        monkeypatch.setattr(ts.type_checker, "check_program", fake_check_program)

        result = ts.analyze_program(program)

        assert result == ["ok"]
        assert observed["program"] is program
        assert ts.type_registry.get_type("Customer") is not None
    finally:
        _restore_registry(ts.type_registry, snap)


def test_type_inference_entrypoints_forward_to_inference_engine(monkeypatch):
    ts = TypeSystem()

    var_decl = VariableDeclaration("item", Literal("integer", 5))
    fn_def = FunctionDefinition("f", parameters=[], body=[])

    monkeypatch.setattr(ts.type_inference, "infer_variable_declaration", lambda node, env: "VAR-TYPE")
    monkeypatch.setattr(ts.type_inference, "infer_function_return_type", lambda node, env: "RET-TYPE")

    assert ts.infer_variable_type(var_decl, env={"x": 1}) == "VAR-TYPE"
    assert ts.infer_function_return_type(fn_def, env={"x": 1}) == "RET-TYPE"


def test_register_interface_pass_through():
    ts = TypeSystem()
    snap = _snapshot_registry(ts.type_registry)
    try:
        ts.register_interface("Reportable", ["render", "export"])
        assert ts.type_registry.interfaces["Reportable"] == ["render", "export"]
    finally:
        _restore_registry(ts.type_registry, snap)
