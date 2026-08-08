"""Focused coverage slices for switch/contracts, try-catch, and interface-style handlers."""

from types import SimpleNamespace

import pytest

import nexuslang.typesystem.typechecker as typechecker_module
from nexuslang.parser.ast import (
    AbstractMethodDefinition,
    FunctionDefinition,
    GuaranteeStatement,
    Identifier,
    InterfaceDefinition,
    InvariantStatement,
    Literal,
    MethodDefinition,
    Parameter,
    RequireStatement,
    EnsureStatement,
    ReturnStatement,
    SwitchCase,
    SwitchStatement,
    TryCatch,
    VariableDeclaration,
)
from nexuslang.typesystem.typechecker import TypeChecker, TypeEnvironment
from nexuslang.typesystem.types import (
    ANY_TYPE,
    BOOLEAN_TYPE,
    ClassType,
    FunctionType,
    INTEGER_TYPE,
    NULL_TYPE,
    STRING_TYPE,
    UnionType,
)


def _checker() -> TypeChecker:
    return TypeChecker(enable_ownership_passes=False)


def _env() -> TypeEnvironment:
    return TypeEnvironment()


class _FakeClassType:
    def __init__(
        self,
        name,
        properties,
        methods,
        parent_classes=None,
        is_abstract=False,
        is_trait=False,
    ):
        self.name = name
        self.properties = properties
        self.methods = methods
        self.parent_classes = parent_classes or []
        self.is_abstract = is_abstract
        self.is_trait = is_trait


class _FakeGenericContext:
    def __init__(self):
        self.params = []

    def add_type_parameter(self, param):
        self.params.append(param)


class TestSwitchStatementSlice:
    def test_switch_statement_reports_incompatible_and_duplicate_cases(self):
        checker = _checker()
        env = _env()

        statement = SwitchStatement(
            expression=Literal("integer", 1),
            cases=[
                SwitchCase(Literal("integer", 1), [Literal("integer", 10)]),
                SwitchCase(Literal("string", "two"), [Literal("string", "branch")]),
                SwitchCase(Literal("integer", 1), [Literal("boolean", True)]),
            ],
            default_case=[Literal("string", "fallback")],
        )

        result = checker.check_switch_statement(statement, env)

        assert isinstance(result, UnionType)
        assert len(result.types) == 4
        assert any("incompatible with switch expression type" in error for error in checker.errors)
        assert any("duplicate case value 1" in error for error in checker.errors)

    def test_switch_statement_without_cases_returns_null_type(self):
        checker = _checker()

        result = checker.check_switch_statement(
            SwitchStatement(expression=Literal("integer", 1), cases=[]),
            _env(),
        )

        assert result == NULL_TYPE


class TestContractSlice:
    def test_require_statement_reports_mutation_non_boolean_and_non_string_message(self):
        checker = _checker()
        env = _env()

        node = RequireStatement(
            condition=VariableDeclaration("value", Literal("integer", 1)),
            message_expr=Literal("integer", 99),
        )

        result = checker.check_require_statement(node, env)

        assert result == BOOLEAN_TYPE
        assert any("Require condition must not contain assignments or mutations" in error for error in checker.errors)
        assert any("Require condition must be a boolean" in error for error in checker.errors)
        assert any("Require message must be a string" in error for error in checker.errors)

    @pytest.mark.parametrize(
        "factory",
        [EnsureStatement, GuaranteeStatement, InvariantStatement],
    )
    def test_contract_wrappers_accept_boolean_condition_and_string_message(self, factory):
        checker = _checker()
        env = _env()

        result = checker.check_statement(
            factory(Literal("boolean", True), Literal("string", "ok")),
            env,
        )

        assert result == BOOLEAN_TYPE
        assert checker.errors == []

    def test_has_side_effects_detects_nested_assignment_nodes(self):
        checker = _checker()

        nested = SimpleNamespace(arguments=[VariableDeclaration("temp", Literal("integer", 1))])

        assert checker._has_side_effects(nested) is True
        assert checker._has_side_effects(SimpleNamespace(arguments=[Literal("integer", 1)])) is False


class TestTryCatchSlice:
    def test_try_catch_list_form_binds_exception_type_and_flags_unreachable(self):
        checker = _checker()
        env = _env()

        unreachable = Literal("integer", 2)
        unreachable.line_number = 77
        node = TryCatch(
            try_block=[ReturnStatement(Literal("integer", 1)), unreachable],
            catch_block=[Identifier("err")],
            exception_var="err",
            exception_type="RuntimeError",
        )

        result = checker.check_try_catch(node, env)

        assert isinstance(result, UnionType)
        assert any(isinstance(option, ClassType) and option.name == "RuntimeError" for option in result.types)
        assert any("Unreachable code after 'ReturnStatement'" in error for error in checker.errors)

    def test_try_catch_block_form_uses_string_for_untyped_exception_var(self):
        checker = _checker()
        env = _env()

        block = lambda statements: SimpleNamespace(statements=statements)
        node = TryCatch(
            try_block=block([Literal("integer", 1)]),
            catch_block=block([Identifier("message")]),
            exception_var="message",
            exception_type=None,
        )

        result = checker.check_try_catch(node, env)

        assert isinstance(result, UnionType)
        assert INTEGER_TYPE in result.types
        assert STRING_TYPE in result.types
        assert checker.errors == []


class TestInterfaceAndAbstractionsSlice:
    def test_interface_definition_registers_required_methods_and_types(self):
        checker = _checker()
        env = _env()

        definition = InterfaceDefinition(
            name="Runnable",
            methods=[
                MethodDefinition(
                    name="run",
                    parameters=[Parameter("count", "Integer")],
                    body=[],
                    return_type="String",
                )
            ],
        )

        result = checker.check_interface_definition(definition, env)

        assert isinstance(result, ClassType)
        assert env.get_variable_type("Runnable") == result
        assert checker.type_registry._interfaces["Runnable"] == {"run"}
        assert isinstance(result.methods["run"], FunctionType)
        assert result.methods["run"].param_types == [INTEGER_TYPE]
        assert result.methods["run"].return_type == STRING_TYPE

    def test_abstract_class_definition_tracks_abstract_methods_and_interfaces(self, monkeypatch):
        checker = _checker()

        generic_context = _FakeGenericContext()
        property_calls = []
        method_calls = []
        interface_calls = []

        monkeypatch.setattr(typechecker_module, "ClassType", _FakeClassType)
        monkeypatch.setattr(
            checker,
            "generic_registry",
            SimpleNamespace(create_context=lambda name: generic_context),
            raising=False,
        )
        monkeypatch.setattr(checker, "check_property_definition", lambda prop: property_calls.append(prop.name), raising=False)
        monkeypatch.setattr(checker, "check_method_definition", lambda method: method_calls.append(method.name), raising=False)
        monkeypatch.setattr(
            checker,
            "check_interface_implementation",
            lambda class_name, interface_name: interface_calls.append((class_name, interface_name)),
            raising=False,
        )

        definition = type(
            "AbstractClassDefinition",
            (),
            {
                "name": "WidgetBase",
                "parent_classes": ["Base"],
                "type_parameters": ["T"],
                "properties": [SimpleNamespace(name="size")],
                "methods": [
                    AbstractMethodDefinition("render", [], "String"),
                    MethodDefinition("measure", [], body=[], return_type="Integer"),
                ],
                "implemented_interfaces": ["Renderable"],
            },
        )()

        result = checker.check_abstract_class_definition(definition)

        assert result.name == "WidgetBase"
        assert result.is_abstract is True
        assert checker.abstract_methods["WidgetBase"] == {"render"}
        assert property_calls == ["size"]
        assert method_calls == ["render", "measure"]
        assert interface_calls == [("WidgetBase", "Renderable")]
        assert generic_context.params == ["T"]
        assert checker.current_class is None

    def test_trait_definition_tracks_required_methods(self, monkeypatch):
        checker = _checker()

        generic_context = _FakeGenericContext()
        method_calls = []

        monkeypatch.setattr(typechecker_module, "ClassType", _FakeClassType)
        monkeypatch.setattr(
            checker,
            "generic_registry",
            SimpleNamespace(create_context=lambda name: generic_context),
            raising=False,
        )
        monkeypatch.setattr(checker, "check_method_definition", lambda method: method_calls.append(method.name), raising=False)

        definition = type(
            "TraitDefinition",
            (),
            {
                "name": "Serializable",
                "type_parameters": ["T"],
                "methods": [
                    AbstractMethodDefinition("serialize", [], "String"),
                    MethodDefinition("reset", [], body=[], return_type=None),
                ],
            },
        )()

        result = checker.check_trait_definition(definition)

        assert result.name == "Serializable"
        assert result.is_trait is True
        assert checker.trait_methods["Serializable"] == {"serialize"}
        assert method_calls == ["serialize", "reset"]
        assert generic_context.params == ["T"]
        assert checker.current_trait is None

    def test_check_interface_implementation_raises_for_missing_method(self):
        checker = _checker()

        iface_method = FunctionType([], STRING_TYPE)
        checker.type_registry["Printable"] = ClassType("Printable", {}, {"print": iface_method})
        checker.type_registry["Document"] = ClassType("Document", {}, {})

        with pytest.raises(TypeError, match="does not implement method print"):
            checker.check_interface_implementation("Document", "Printable")

    def test_check_interface_implementation_raises_for_incompatible_signature(self, monkeypatch):
        checker = _checker()

        iface_method = FunctionType([], STRING_TYPE)
        class_method = FunctionType([], ANY_TYPE)
        checker.type_registry["Printable"] = ClassType("Printable", {}, {"print": iface_method})
        checker.type_registry["Document"] = ClassType("Document", {}, {"print": class_method})
        monkeypatch.setattr(checker, "types_compatible", lambda left, right: False, raising=False)

        with pytest.raises(TypeError, match="Method print in class Document is not compatible"):
            checker.check_interface_implementation("Document", "Printable")

    def test_check_abstract_method_implementation_raises_for_missing_method(self):
        checker = _checker()
        checker.abstract_methods["WidgetBase"] = {"render"}
        checker.type_registry["WidgetBase"] = ClassType("WidgetBase", {}, {})

        with pytest.raises(TypeError, match="does not implement abstract method render"):
            checker.check_abstract_method_implementation("WidgetBase")

    def test_check_trait_method_implementation_raises_for_missing_method(self):
        checker = _checker()
        checker.trait_methods["Serializable"] = {"serialize"}
        checker.type_registry["Blob"] = ClassType("Blob", {}, {})

        with pytest.raises(TypeError, match="does not implement trait method serialize from trait Serializable"):
            checker.check_trait_method_implementation("Blob", "Serializable")

    def test_check_trait_method_implementation_raises_for_incompatible_signature(self, monkeypatch):
        checker = _checker()
        checker.trait_methods["Serializable"] = {"serialize"}

        trait_method = FunctionType([], STRING_TYPE)
        class_method = FunctionType([], ANY_TYPE)

        checker.type_registry["Serializable"] = ClassType("Serializable", {}, {"serialize": trait_method})
        checker.type_registry["Blob"] = ClassType("Blob", {}, {"serialize": class_method})
        monkeypatch.setattr(checker, "types_compatible", lambda left, right: False, raising=False)

        with pytest.raises(TypeError, match="Trait method serialize in class Blob is not compatible with trait Serializable"):
            checker.check_trait_method_implementation("Blob", "Serializable")

    def test_check_trait_method_implementation_accepts_compatible_signature(self, monkeypatch):
        checker = _checker()
        checker.trait_methods["Serializable"] = {"serialize"}

        trait_method = FunctionType([], STRING_TYPE)
        class_method = FunctionType([], STRING_TYPE)

        checker.type_registry["Serializable"] = ClassType("Serializable", {}, {"serialize": trait_method})
        checker.type_registry["Blob"] = ClassType("Blob", {}, {"serialize": class_method})
        monkeypatch.setattr(checker, "types_compatible", lambda left, right: True, raising=False)

        checker.check_trait_method_implementation("Blob", "Serializable")