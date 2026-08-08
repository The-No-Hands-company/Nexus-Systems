"""Coverage slice for dispatcher fall-through and inline-assembly branch arcs."""

from types import SimpleNamespace

import pytest

from nexuslang.parser.ast import Identifier
from nexuslang.typesystem.typechecker import TypeCheckError, TypeChecker, TypeEnvironment
from nexuslang.typesystem.types import ANY_TYPE, INTEGER_TYPE


def _checker() -> TypeChecker:
    return TypeChecker(enable_ownership_passes=False)


def _env() -> TypeEnvironment:
    return TypeEnvironment()


def _node(class_name: str, **attrs):
    cls = type(class_name, (), {})
    obj = cls()
    for key, value in attrs.items():
        setattr(obj, key, value)
    return obj


class TestCheckStatementDispatcherFallback:
    def test_dispatch_falls_through_to_ownership_handler(self, monkeypatch):
        checker = _checker()
        env = _env()
        stmt = _node("MovedValue")

        monkeypatch.setattr(checker, "_check_import_statement", lambda s, e: (False, None), raising=False)
        monkeypatch.setattr(checker, "_check_data_structure_statement", lambda s, e: (False, None), raising=False)
        monkeypatch.setattr(checker, "_check_collection_expression", lambda s, e: (False, None), raising=False)
        monkeypatch.setattr(checker, "_check_ffi_statement", lambda s, e: (False, None), raising=False)
        monkeypatch.setattr(checker, "_check_inline_assembly_statement", lambda s, e: (False, None), raising=False)
        monkeypatch.setattr(checker, "_check_match_expression_statement", lambda s, e: (False, None), raising=False)

        result = checker.check_statement(stmt, env)

        assert result == ANY_TYPE

    def test_dispatch_falls_through_to_match_handler(self, monkeypatch):
        checker = _checker()
        env = _env()
        stmt = _node("UnknownNode")

        monkeypatch.setattr(checker, "_check_import_statement", lambda s, e: (False, None), raising=False)
        monkeypatch.setattr(checker, "_check_data_structure_statement", lambda s, e: (False, None), raising=False)
        monkeypatch.setattr(checker, "_check_collection_expression", lambda s, e: (False, None), raising=False)
        monkeypatch.setattr(checker, "_check_ffi_statement", lambda s, e: (False, None), raising=False)
        monkeypatch.setattr(checker, "_check_inline_assembly_statement", lambda s, e: (False, None), raising=False)
        monkeypatch.setattr(checker, "_check_match_expression_statement", lambda s, e: (True, INTEGER_TYPE), raising=False)

        result = checker.check_statement(stmt, env)

        assert result == INTEGER_TYPE

    def test_dispatch_raises_for_totally_unsupported_statement(self, monkeypatch):
        checker = _checker()
        env = _env()
        stmt = _node("TotallyUnsupported")

        monkeypatch.setattr(checker, "_check_import_statement", lambda s, e: (False, None), raising=False)
        monkeypatch.setattr(checker, "_check_data_structure_statement", lambda s, e: (False, None), raising=False)
        monkeypatch.setattr(checker, "_check_collection_expression", lambda s, e: (False, None), raising=False)
        monkeypatch.setattr(checker, "_check_ffi_statement", lambda s, e: (False, None), raising=False)
        monkeypatch.setattr(checker, "_check_inline_assembly_statement", lambda s, e: (False, None), raising=False)
        monkeypatch.setattr(checker, "_check_match_expression_statement", lambda s, e: (False, None), raising=False)
        monkeypatch.setattr(checker, "_check_ownership_statement", lambda s, e: (False, None), raising=False)

        with pytest.raises(TypeCheckError, match="Unsupported statement type: TotallyUnsupported"):
            checker.check_statement(stmt, env)


class TestInlineAssemblyHasattrArcs:
    def test_inline_asm_without_inputs_attribute_still_handles_outputs_and_clobbers(self, monkeypatch):
        checker = _checker()
        env = _env()
        env.define_variable("out", INTEGER_TYPE)

        asm_node = _node(
            "InlineAssembly",
            outputs=[("=r", Identifier("out"))],
            clobbers=["memory"],
            line_number=12,
        )

        # Bypass strict shape checks to exercise hasattr-based fallback branches.
        monkeypatch.setattr(checker, "_require_inline_assembly_shape", lambda s: None, raising=False)

        handled, result = checker._check_inline_assembly_statement(asm_node, env)

        assert handled is True
        assert result == INTEGER_TYPE
        assert not checker.errors

    def test_inline_asm_without_outputs_attribute_still_handles_inputs_and_clobbers(self, monkeypatch):
        checker = _checker()
        env = _env()

        asm_node = _node(
            "InlineAssembly",
            inputs=[("r", Identifier("in"))],
            clobbers=["cc"],
            line_number=13,
        )

        monkeypatch.setattr(checker, "_require_inline_assembly_shape", lambda s: None, raising=False)
        monkeypatch.setattr(checker, "check_expression", lambda expr, e: INTEGER_TYPE, raising=False)

        handled, result = checker._check_inline_assembly_statement(asm_node, env)

        assert handled is True
        assert result == INTEGER_TYPE
        assert not any("output constraint" in err for err in checker.errors)

    def test_inline_asm_without_clobbers_attribute_still_handles_inputs_outputs(self, monkeypatch):
        checker = _checker()
        env = _env()
        env.define_variable("dst", INTEGER_TYPE)

        asm_node = _node(
            "InlineAssembly",
            inputs=[("r", Identifier("src"))],
            outputs=[("=r", Identifier("dst"))],
            line_number=14,
        )

        monkeypatch.setattr(checker, "_require_inline_assembly_shape", lambda s: None, raising=False)
        monkeypatch.setattr(checker, "check_expression", lambda expr, e: INTEGER_TYPE, raising=False)

        handled, result = checker._check_inline_assembly_statement(asm_node, env)

        assert handled is True
        assert result == INTEGER_TYPE
        assert not any("clobber" in err for err in checker.errors)

    def test_inline_asm_with_no_operand_attributes_returns_integer(self, monkeypatch):
        checker = _checker()
        env = _env()
        asm_node = _node("InlineAssembly", line_number=15)

        monkeypatch.setattr(checker, "_require_inline_assembly_shape", lambda s: None, raising=False)

        handled, result = checker._check_inline_assembly_statement(asm_node, env)

        assert handled is True
        assert result == INTEGER_TYPE

    def test_inline_asm_invalid_and_duplicate_clobber_errors_accumulate(self):
        checker = _checker()
        env = _env()
        asm_node = _node(
            "InlineAssembly",
            inputs=[],
            outputs=[],
            clobbers=["bad reg", "rax", "rax"],
            line_number=16,
        )

        handled, result = checker._check_inline_assembly_statement(asm_node, env)

        assert handled is True
        assert result == INTEGER_TYPE
        assert any("Invalid inline assembly clobber" in err for err in checker.errors)
        assert any("Duplicate inline assembly clobber 'rax'" in err for err in checker.errors)


class TestMatchAndOwnershipHelpersSmoke:
    def test_match_helper_non_match_node_returns_false_none(self):
        checker = _checker()
        handled, result = checker._check_match_expression_statement(_node("NotMatch"), _env())
        assert handled is False
        assert result is None

    def test_ownership_helper_non_ownership_node_returns_false_none(self):
        checker = _checker()
        handled, result = checker._check_ownership_statement(_node("NotOwnership"), _env())
        assert handled is False
        assert result is None
