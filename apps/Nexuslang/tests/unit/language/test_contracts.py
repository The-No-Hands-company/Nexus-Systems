"""
Contract programming: require, ensure, guarantee, NLPLContractError.
Split from test_session_features.py.
"""

import sys
import os
import tempfile
import pytest
from pathlib import Path

_SRC = str(Path(__file__).resolve().parent.parent.parent.parent / "src")
if _SRC not in sys.path:
    sys.path.insert(0, _SRC)

class TestContractProgramming:
    def _interp(self, src):
        from nexuslang.interpreter.interpreter import Interpreter
        from nexuslang.runtime.runtime import Runtime
        from nexuslang.stdlib import register_stdlib
        from nexuslang.parser.parser import Parser
        from nexuslang.parser.lexer import Lexer
        rt = Runtime()
        register_stdlib(rt)
        prog = Parser(Lexer(src).tokenize()).parse()
        i = Interpreter(runtime=rt)
        i.interpret(prog)
        return i

    def test_require_passes_when_true(self):
        self._interp("set x to 5\nrequire x is greater than 0")

    def test_require_raises_when_false(self):
        from nexuslang.errors import NLPLContractError
        with pytest.raises(NLPLContractError):
            self._interp("set x to 0\nrequire x is greater than 5")

    def test_ensure_passes_when_true(self):
        self._interp("set result to 10\nensure result is greater than 0")

    def test_ensure_raises_when_false(self):
        from nexuslang.errors import NLPLContractError
        with pytest.raises(NLPLContractError):
            self._interp("set result to 0\nensure result is greater than 5")

    def test_guarantee_passes_when_true(self):
        self._interp("set n to 3\nguarantee n equals 3")

    def test_guarantee_raises_when_false(self):
        from nexuslang.errors import NLPLContractError
        with pytest.raises(NLPLContractError):
            self._interp("set n to 1\nguarantee n equals 2")

    def test_contract_error_is_importable(self):
        from nexuslang.errors import NLPLContractError
        assert issubclass(NLPLContractError, Exception)

    def test_require_with_literal_true(self):
        self._interp("require 1 equals 1")

    def test_require_with_literal_false(self):
        from nexuslang.errors import NLPLContractError
        with pytest.raises(NLPLContractError):
            self._interp("require 1 equals 2")

    def test_guarantee_contract_kind(self):
        from nexuslang.errors import NLPLContractError
        try:
            self._interp("guarantee 1 equals 2")
        except NLPLContractError as e:
            assert e.contract_kind == "guarantee"
        else:
            pytest.fail("Expected NLPLContractError")

    def test_require_contract_kind(self):
        from nexuslang.errors import NLPLContractError
        try:
            self._interp("require 1 equals 2")
        except NLPLContractError as e:
            assert e.contract_kind == "require"
        else:
            pytest.fail("Expected NLPLContractError")


# ============================================================
# Section 18 - ControlFlowChecker


# ============================================================
# Section 19 - Contract side-effect restrictions (typechecker)
# ============================================================

class TestContractSideEffectRestrictions:
    """Typechecker must reject side-effecting expressions inside contract conditions."""

    def _check_ast(self, *statements):
        from nexuslang.typesystem.typechecker import TypeChecker
        from nexuslang.parser.ast import Program
        prog = Program(list(statements))
        tc = TypeChecker()
        tc.check_program(prog)
        return tc.errors

    def test_assignment_in_require_condition_is_rejected(self):
        from nexuslang.parser.ast import VariableDeclaration, RequireStatement, Literal
        cond = VariableDeclaration('x', Literal('integer', 5))
        errors = self._check_ast(RequireStatement(condition=cond))
        assert any("must not contain assignments" in e for e in errors), \
            f"Expected side-effect error, got: {errors}"

    def test_assignment_in_ensure_condition_is_rejected(self):
        from nexuslang.parser.ast import VariableDeclaration, EnsureStatement, Literal
        cond = VariableDeclaration('y', Literal('integer', 0))
        errors = self._check_ast(EnsureStatement(condition=cond))
        assert any("must not contain assignments" in e for e in errors), \
            f"Expected side-effect error, got: {errors}"

    def test_assignment_in_guarantee_condition_is_rejected(self):
        from nexuslang.parser.ast import VariableDeclaration, GuaranteeStatement, Literal
        cond = VariableDeclaration('z', Literal('integer', 1))
        errors = self._check_ast(GuaranteeStatement(condition=cond))
        assert any("must not contain assignments" in e for e in errors), \
            f"Expected side-effect error, got: {errors}"

    def test_assignment_in_invariant_condition_is_rejected(self):
        from nexuslang.parser.ast import VariableDeclaration, InvariantStatement, Literal
        cond = VariableDeclaration('w', Literal('integer', 2))
        errors = self._check_ast(InvariantStatement(condition=cond))
        assert any("must not contain assignments" in e for e in errors), \
            f"Expected side-effect error, got: {errors}"

    def test_index_assignment_in_require_condition_is_rejected(self):
        from nexuslang.parser.ast import IndexAssignment, RequireStatement, Literal, Identifier
        cond = IndexAssignment(
            target=Identifier('arr'),
            value=Literal('integer', 1),
        )
        errors = self._check_ast(RequireStatement(condition=cond))
        assert any("must not contain assignments" in e for e in errors), \
            f"Expected side-effect error, got: {errors}"

    def test_member_assignment_in_require_condition_is_rejected(self):
        from nexuslang.parser.ast import MemberAssignment, RequireStatement, Literal, Identifier
        cond = MemberAssignment(
            target=Identifier('obj'),
            value=Literal('integer', 5),
        )
        errors = self._check_ast(RequireStatement(condition=cond))
        assert any("must not contain assignments" in e for e in errors), \
            f"Expected side-effect error, got: {errors}"

    def test_pure_boolean_condition_no_side_effect_error(self):
        """A boolean identifier as condition should not trigger side-effect error."""
        from nexuslang.parser.ast import VariableDeclaration, RequireStatement, Literal, Identifier
        errors = self._check_ast(
            VariableDeclaration('flag', Literal('boolean', True)),
            RequireStatement(condition=Identifier('flag')),
        )
        assert not any("must not contain assignments" in e for e in errors), \
            f"Unexpected side-effect error on pure condition: {errors}"

    def test_non_boolean_condition_still_gives_type_error(self):
        """A non-boolean, non-assignment condition raises a type error."""
        from nexuslang.parser.ast import VariableDeclaration, RequireStatement, Literal, Identifier
        errors = self._check_ast(
            VariableDeclaration('n', Literal('integer', 5)),
            RequireStatement(condition=Identifier('n')),
        )
        assert any("must be a boolean" in e for e in errors), \
            f"Expected boolean type error, got: {errors}"
# ============================================================

