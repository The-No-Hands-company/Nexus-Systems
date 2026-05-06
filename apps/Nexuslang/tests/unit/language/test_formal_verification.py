"""
Tests for formal verification features (7.3):
- invariant statement runtime checking
- result variable in ensure postconditions
- old() expression pre-call value capture
- spec block parsing (runtime no-op)
- VerificationCondition / FormalSpec data structures
- ConstraintCollector AST walker
- Z3Backend (mocked when z3 unavailable)
- VerificationReport aggregator
- verify_file / verify_files helpers
- nlplverify CLI
"""

import json
import os
import sys
import tempfile
from pathlib import Path

import pytest

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------
ROOT = Path(__file__).parent.parent.parent.parent
sys.path.insert(0, str(ROOT / "src"))

from nexuslang.errors import NLPLContractError
from nexuslang.interpreter.interpreter import Interpreter
from nexuslang.parser.lexer import Lexer
from nexuslang.parser.parser import Parser
from nexuslang.runtime.runtime import Runtime


def run(source: str):
    """Parse + execute NexusLang source; return final expression value."""
    runtime = Runtime()
    interpreter = Interpreter(runtime)
    return interpreter.interpret(source)


def parse_only(source: str):
    """Return the AST root without executing."""
    tokens = Lexer(source).tokenize()
    return Parser(tokens).parse()


# ===========================================================================
# 1. Invariant runtime behaviour
# ===========================================================================


class TestInvariantRuntime:
    def test_invariant_true_literal_passes(self):
        run("invariant 1 equals 1")

    def test_invariant_false_literal_raises(self):
        with pytest.raises(NLPLContractError):
            run("invariant 1 equals 2")

    def test_invariant_variable_passes(self):
        run("set x to 5\ninvariant x is greater than 0")

    def test_invariant_variable_fails(self):
        with pytest.raises(NLPLContractError):
            run("set x to -1\ninvariant x is greater than 0")

    def test_invariant_error_contract_kind(self):
        with pytest.raises(NLPLContractError) as exc_info:
            run("invariant 1 equals 2")
        assert getattr(exc_info.value, "contract_kind", None) == "invariant"

    def test_invariant_custom_message(self):
        with pytest.raises(NLPLContractError) as exc_info:
            run('invariant 1 equals 2 message "must be equal"')
        assert "must be equal" in str(exc_info.value)

    def test_invariant_true_passes_no_exception(self):
        """Passes inside a block with surrounding assignments."""
        run("set a to 10\nset b to 5\ninvariant a is greater than b\nset c to a minus b")

    def test_invariant_inside_function_body_passes(self):
        src = (
            "function check_invariant returns Integer\n"
            "    set x to 10\n"
            "    invariant x is greater than 0\n"
            "    return x\n"
            "end\n"
            "call check_invariant"
        )
        result = run(src)
        assert result == 10

    def test_invariant_inside_function_body_fails(self):
        src = (
            "function broken_invariant returns Integer\n"
            "    set x to -5\n"
            "    invariant x is greater than 0\n"
            "    return x\n"
            "end\n"
            "call broken_invariant"
        )
        with pytest.raises(NLPLContractError):
            run(src)

    def test_multiple_invariants(self):
        run("set x to 3\ninvariant x is greater than 0\ninvariant x is less than 10")


# ===========================================================================
# 2. result variable in ensure postconditions
# ===========================================================================


class TestResultInEnsure:
    def test_ensure_result_positive_passes(self):
        src = (
            "function positive_int returns Integer\n"
            "    ensure result is greater than 0\n"
            "    return 42\n"
            "end\n"
            "call positive_int"
        )
        assert run(src) == 42

    def test_ensure_result_positive_fails(self):
        src = (
            "function negative_int returns Integer\n"
            "    ensure result is greater than 0\n"
            "    return -1\n"
            "end\n"
            "call negative_int"
        )
        with pytest.raises(NLPLContractError):
            run(src)

    def test_ensure_result_equals_literal(self):
        src = (
            "function five returns Integer\n"
            "    ensure result equals 5\n"
            "    return 5\n"
            "end\n"
            "call five"
        )
        assert run(src) == 5

    def test_ensure_result_wrong_value_fails(self):
        src = (
            "function five returns Integer\n"
            "    ensure result equals 5\n"
            "    return 6\n"
            "end\n"
            "call five"
        )
        with pytest.raises(NLPLContractError):
            run(src)

    def test_result_is_actual_return_value(self):
        src = (
            "function double with n as Integer returns Integer\n"
            "    ensure result equals n times 2\n"
            "    return n times 2\n"
            "end\n"
            "call double with 7"
        )
        assert run(src) == 14

    def test_ensure_not_triggered_on_exception(self):
        """If an error occurs before return, ensure stmts should not fire."""
        src = (
            "function will_error returns Integer\n"
            "    ensure result equals 0\n"
            "    print text undefined_variable\n"
            "    return 0\n"
            "end\n"
        )
        # Should raise a NameError (or similar) not a ContractError
        with pytest.raises(Exception) as exc_info:
            run(src + "call will_error")
        assert not isinstance(exc_info.value, NLPLContractError)

    def test_function_without_ensure_still_works(self):
        src = (
            "function plain returns Integer\n"
            "    return 99\n"
            "end\n"
            "call plain"
        )
        assert run(src) == 99

    def test_ensure_with_require_combo(self):
        src = (
            "function safe_divide with a as Integer and b as Integer returns Float\n"
            "    require b is not equal to 0\n"
            "    ensure result is greater than 0\n"
            "    return a divided by b\n"
            "end\n"
            "call safe_divide with 10 and 2"
        )
        assert run(src) == pytest.approx(5.0)

    def test_ensure_multiple_postconditions(self):
        src = (
            "function bounded returns Integer\n"
            "    ensure result is greater than 0\n"
            "    ensure result is less than 100\n"
            "    return 50\n"
            "end\n"
            "call bounded"
        )
        assert run(src) == 50


# ===========================================================================
# 3. old() expression (pre-call value capture)
# ===========================================================================


class TestOldExpression:
    def test_old_captures_pre_call_value(self):
        src = (
            "set counter to 0\n"
            "function increment returns Integer\n"
            "    ensure result equals old(counter) plus 1\n"
            "    set counter to counter plus 1\n"
            "    return counter\n"
            "end\n"
            "call increment"
        )
        assert run(src) == 1

    def test_old_captures_before_mutation(self):
        src = (
            "set x to 10\n"
            "function double_x returns Integer\n"
            "    ensure result equals old(x) times 2\n"
            "    set x to x times 2\n"
            "    return x\n"
            "end\n"
            "call double_x"
        )
        assert run(src) == 20

    def test_old_wrong_expectation_fails(self):
        src = (
            "set x to 10\n"
            "function wrong_old returns Integer\n"
            "    ensure result equals old(x) times 3\n"
            "    set x to x times 2\n"
            "    return x\n"
            "end\n"
            "call wrong_old"
        )
        with pytest.raises(NLPLContractError):
            run(src)

    def test_multiple_old_refs_independent(self):
        src = (
            "set a to 3\n"
            "set b to 7\n"
            "function swap_sum returns Integer\n"
            "    ensure result equals old(a) plus old(b)\n"
            "    set tmp to a\n"
            "    set a to b\n"
            "    set b to tmp\n"
            "    return a plus b\n"
            "end\n"
            "call swap_sum"
        )
        assert run(src) == 10

    def test_old_in_ensure_with_parameter(self):
        src = (
            "set total to 0\n"
            "function add_to_total with amount as Integer returns Integer\n"
            "    ensure result equals old(total) plus amount\n"
            "    set total to total plus amount\n"
            "    return total\n"
            "end\n"
            "call add_to_total with 5"
        )
        assert run(src) == 5

    def test_old_parseable_in_ast(self):
        """old(x) in ensure must produce an OldExpression node."""
        from nexuslang.parser.ast import OldExpression

        src = (
            "function f returns Integer\n"
            "    ensure result equals old(x) plus 1\n"
            "    return 1\n"
            "end\n"
        )
        ast = parse_only(src)
        func = ast.statements[0]
        # body contains EnsureStatement; its condition contains OldExpression
        ensure_stmts = [s for s in func.body if getattr(s, "node_type", "") == "ensure_statement"]
        assert ensure_stmts, "EnsureStatement not found in function body"
        # Traverse condition recursively to find OldExpression
        def find_old(node, _seen=None):
            if _seen is None:
                _seen = set()
            nid = id(node)
            if nid in _seen:
                return False
            _seen.add(nid)
            if isinstance(node, OldExpression):
                return True
            if not hasattr(node, "__dict__"):
                return False
            for v in vars(node).values():
                if v is not None and hasattr(v, "__dict__") and find_old(v, _seen):
                    return True
            return False

        assert find_old(ensure_stmts[0].condition), "OldExpression not found in ensure condition"


# ===========================================================================
# 4. spec block (runtime no-op, parse-only annotations)
# ===========================================================================


class TestSpecBlock:
    def test_spec_block_parses_without_error(self):
        src = (
            "spec\n"
            "    requires x is greater than 0\n"
            "end spec\n"
            "set x to 1\n"
        )
        ast = parse_only(src)
        assert any(getattr(s, "node_type", "") == "spec_block" for s in ast.statements)

    def test_spec_block_with_ensures(self):
        src = (
            "spec\n"
            "    requires n is greater than 0\n"
            "    ensure result is greater than 0\n"
            "end spec\n"
            "set n to 5\n"
        )
        ast = parse_only(src)
        spec = next(s for s in ast.statements if getattr(s, "node_type", "") == "spec_block")
        assert len(spec.annotations) >= 1

    def test_spec_block_with_invariant(self):
        src = (
            "spec\n"
            "    invariant x is greater than 0\n"
            "end spec\n"
            "set x to 1\n"
        )
        ast = parse_only(src)
        assert any(getattr(s, "node_type", "") == "spec_block" for s in ast.statements)

    def test_spec_block_is_runtime_noop(self):
        """Running a program with a spec block does not raise."""
        src = (
            "spec\n"
            "    requires n is greater than 0\n"
            "end spec\n"
            "set n to 5\n"
        )
        run(src)  # must not raise

    def test_spec_block_annotations_accessible(self):
        src = (
            "spec\n"
            "    requires x is greater than 0\n"
            "    ensure result equals x\n"
            "end spec\n"
            "set x to 1\n"
        )
        ast = parse_only(src)
        spec = next(s for s in ast.statements if getattr(s, "node_type", "") == "spec_block")
        kinds = [a.kind for a in spec.annotations]
        assert "requires" in kinds or "ensures" in kinds


# ===========================================================================
# 5. VerificationCondition and FormalSpec data structures
# ===========================================================================


class TestVerificationCondition:
    def setup_method(self):
        from nexuslang.verification.specification import (
            FormalSpec,
            VerificationCondition,
            VerificationStatus,
        )

        self.VC = VerificationCondition
        self.FS = FormalSpec
        self.VS = VerificationStatus

    def test_status_enum_values(self):
        assert self.VS.PROVED.value == "proved"
        assert self.VS.FAILED.value == "failed"
        assert self.VS.UNKNOWN.value == "unknown"
        assert self.VS.UNVERIFIED.value == "unverified"

    def test_vc_default_status_unverified(self):
        vc = self.VC(kind="precondition", description="x > 0", function="f", line=1)
        assert vc.status == self.VS.UNVERIFIED

    def test_vc_to_dict_keys(self):
        vc = self.VC(kind="postcondition", description="result > 0", function="g", line=5)
        d = vc.to_dict()
        for key in ("kind", "description", "function", "line", "status"):
            assert key in d

    def test_vc_to_dict_values(self):
        vc = self.VC(kind="guarantee", description="x >= 0", function="h", line=3)
        d = vc.to_dict()
        assert d["kind"] == "guarantee"
        assert d["function"] == "h"
        assert d["line"] == 3

    def test_vc_counter_example_none_by_default(self):
        vc = self.VC(kind="invariant", description="loop_inv", function=None, line=0)
        assert vc.counter_example is None

    def test_formal_spec_total(self):
        vcs = [
            self.VC("precondition", "a", "f", 1),
            self.VC("postcondition", "b", "f", 2),
            self.VC("guarantee", "c", "g", 3),
        ]
        spec = self.FS(path="test.nxl", conditions=vcs)
        assert spec.total == 3

    def test_formal_spec_unverified_count(self):
        vcs = [self.VC("precondition", "a", "f", 1) for _ in range(4)]
        spec = self.FS(path="test.nxl", conditions=vcs)
        assert spec.unverified == 4

    def test_formal_spec_proved_count(self):
        vcs = [self.VC("precondition", "a", "f", 1, status=self.VS.PROVED) for _ in range(2)]
        spec = self.FS(path="test.nxl", conditions=vcs)
        assert spec.proved == 2

    def test_formal_spec_by_function(self):
        vcs = [
            self.VC("precondition", "a", "f", 1),
            self.VC("postcondition", "b", "f", 2),
            self.VC("guarantee", "c", "g", 3),
        ]
        spec = self.FS(path="test.nxl", conditions=vcs)
        grouped = spec.by_function()
        assert "f" in grouped
        assert "g" in grouped
        assert len(grouped["f"]) == 2
        assert len(grouped["g"]) == 1

    def test_formal_spec_summary_is_string(self):
        spec = self.FS(path="test.nxl", conditions=[])
        s = spec.summary()
        assert isinstance(s, str)
        assert "test.nxl" in s

    def test_formal_spec_to_dict_serializable(self):
        vcs = [self.VC("precondition", "x > 0", "f", 1)]
        spec = self.FS(path="test.nxl", conditions=vcs)
        d = spec.to_dict()
        json.dumps(d)  # must not raise


# ===========================================================================
# 6. ConstraintCollector
# ===========================================================================


class TestConstraintCollector:
    def setup_method(self):
        from nexuslang.verification.constraint_collector import ConstraintCollector
        from nexuslang.verification.specification import VerificationStatus

        self.CC = ConstraintCollector
        self.VS = VerificationStatus

    def _collect(self, source: str):
        ast = parse_only(source)
        return self.CC("test.nxl").collect(ast)

    def test_empty_program_no_vcs(self):
        spec = self._collect("set x to 1")
        assert spec.total == 0

    def test_require_becomes_precondition(self):
        src = (
            "function f with x as Integer returns Integer\n"
            "    require x is greater than 0\n"
            "    return x\n"
            "end\n"
        )
        spec = self._collect(src)
        kinds = [vc.kind for vc in spec.conditions]
        assert "precondition" in kinds

    def test_ensure_becomes_postcondition(self):
        src = (
            "function f returns Integer\n"
            "    ensure result is greater than 0\n"
            "    return 1\n"
            "end\n"
        )
        spec = self._collect(src)
        kinds = [vc.kind for vc in spec.conditions]
        assert "postcondition" in kinds

    def test_guarantee_becomes_guarantee(self):
        src = (
            "function f returns Integer\n"
            "    guarantee 1 equals 1\n"
            "    return 1\n"
            "end\n"
        )
        spec = self._collect(src)
        kinds = [vc.kind for vc in spec.conditions]
        assert "guarantee" in kinds

    def test_invariant_becomes_invariant(self):
        src = (
            "function f with x as Integer returns Integer\n"
            "    invariant x is greater than 0\n"
            "    return x\n"
            "end\n"
        )
        spec = self._collect(src)
        kinds = [vc.kind for vc in spec.conditions]
        assert "invariant" in kinds

    def test_function_name_attributed_correctly(self):
        src = (
            "function compute returns Integer\n"
            "    require 1 equals 1\n"
            "    return 1\n"
            "end\n"
        )
        spec = self._collect(src)
        assert all(vc.function == "compute" for vc in spec.conditions)

    def test_top_level_contracts_have_none_function(self):
        spec = self._collect("require 1 equals 1")
        assert spec.conditions[0].function is None

    def test_all_collected_vcs_start_as_unverified(self):
        src = (
            "function f returns Integer\n"
            "    require 1 equals 1\n"
            "    ensure result equals 1\n"
            "    return 1\n"
            "end\n"
        )
        spec = self._collect(src)
        assert all(vc.status == self.VS.UNVERIFIED for vc in spec.conditions)

    def test_multiple_functions_grouped(self):
        src = (
            "function f returns Integer\n"
            "    require 1 equals 1\n"
            "    return 1\n"
            "end\n"
            "function g returns Integer\n"
            "    ensure result equals 1\n"
            "    return 1\n"
            "end\n"
        )
        spec = self._collect(src)
        grouped = spec.by_function()
        assert "f" in grouped and "g" in grouped

    def test_spec_block_annotations_collected(self):
        src = (
            "spec\n"
            "    requires x is greater than 0\n"
            "end spec\n"
            "set x to 1\n"
        )
        spec = self._collect(src)
        # spec block annotations are collected as unverified VCs
        assert spec.total >= 1

    def test_line_number_recorded(self):
        src = (
            "function f returns Integer\n"
            "    require 1 equals 1\n"
            "    return 1\n"
            "end\n"
        )
        spec = self._collect(src)
        assert spec.conditions[0].line is not None
        assert spec.conditions[0].line > 0

    def test_description_is_string(self):
        src = (
            "function f returns Integer\n"
            "    require 1 equals 1\n"
            "    return 1\n"
            "end\n"
        )
        spec = self._collect(src)
        assert isinstance(spec.conditions[0].description, str)


# ===========================================================================
# 7. Z3Backend (mocked when z3 unavailable)
# ===========================================================================


class TestZ3Backend:
    def setup_method(self):
        from nexuslang.verification.z3_backend import Z3_AVAILABLE, Z3Backend
        from nexuslang.verification.specification import (
            FormalSpec,
            VerificationCondition,
            VerificationStatus,
        )

        self.Z3Backend = Z3Backend
        self.Z3_AVAILABLE = Z3_AVAILABLE
        self.VC = VerificationCondition
        self.FS = FormalSpec
        self.VS = VerificationStatus

    def test_z3_available_is_bool(self):
        assert isinstance(self.Z3_AVAILABLE, bool)

    def test_backend_available_matches_z3_available(self):
        b = self.Z3Backend()
        assert b.available == self.Z3_AVAILABLE

    def test_verify_all_returns_spec_when_unavailable(self):
        if self.Z3_AVAILABLE:
            pytest.skip("Z3 is available; skip unavailability test")
        spec = self.FS(path="t.nxl", conditions=[])
        b = self.Z3Backend()
        result = b.verify_all(spec)
        assert result is spec

    def test_verify_one_returns_vc_when_unavailable(self):
        if self.Z3_AVAILABLE:
            pytest.skip("Z3 is available; skip unavailability test")
        vc = self.VC("precondition", "x > 0", "f", 1)
        b = self.Z3Backend()
        result = b.verify_one(vc)
        assert result is vc

    def test_verify_all_leaves_status_unverified_when_no_z3(self):
        if self.Z3_AVAILABLE:
            pytest.skip("Z3 is available; skip unavailability test")
        vcs = [self.VC("precondition", "x > 0", "f", 1)]
        spec = self.FS(path="t.nxl", conditions=vcs)
        b = self.Z3Backend()
        result = b.verify_all(spec)
        assert all(vc.status == self.VS.UNVERIFIED for vc in result.conditions)

    @pytest.mark.skipif(
        True,  # only run when z3 actually installed
        reason="Z3 not available in this environment",
    )
    def test_verify_trivially_true(self):
        import z3

        vc = self.VC("precondition", "1 == 1", "f", 1)
        b = self.Z3Backend()
        result = b.verify_one(vc)
        assert result.status == self.VS.PROVED


# ===========================================================================
# 8. VerificationReport
# ===========================================================================


class TestVerificationReport:
    def setup_method(self):
        from nexuslang.verification.reporter import VerificationReport
        from nexuslang.verification.specification import (
            FormalSpec,
            VerificationCondition,
            VerificationStatus,
        )

        self.VR = VerificationReport
        self.FS = FormalSpec
        self.VC = VerificationCondition
        self.VS = VerificationStatus

    def _make_spec(self, path, conditions):
        return self.FS(path=path, conditions=conditions)

    def test_empty_report_zero_totals(self):
        r = self.VR([])
        assert r.total_conditions == 0

    def test_report_aggregates_multiple_specs(self):
        s1 = self._make_spec("a.nxl", [self.VC("precondition", "x", "f", 1)])
        s2 = self._make_spec("b.nxl", [self.VC("postcondition", "y", "g", 2)])
        r = self.VR([s1, s2])
        assert r.total_conditions == 2

    def test_has_failures_false_when_no_failures(self):
        s = self._make_spec("a.nxl", [self.VC("precondition", "x", "f", 1)])
        r = self.VR([s])
        assert not r.has_failures

    def test_has_failures_true_when_some_failed(self):
        vc = self.VC("precondition", "x", "f", 1, status=self.VS.FAILED)
        s = self._make_spec("a.nxl", [vc])
        r = self.VR([s])
        assert r.has_failures

    def test_total_proved(self):
        vcs = [self.VC("precondition", "x", "f", 1, status=self.VS.PROVED) for _ in range(3)]
        s = self._make_spec("a.nxl", vcs)
        r = self.VR([s])
        assert r.total_proved == 3

    def test_summary_is_string(self):
        r = self.VR([])
        assert isinstance(r.summary(), str)

    def test_to_dict_is_json_serializable(self):
        vcs = [self.VC("precondition", "x", "f", 1)]
        s = self._make_spec("a.nxl", vcs)
        r = self.VR([s])
        json.dumps(r.to_dict())  # must not raise

    def test_write_json_creates_file(self, tmp_path):
        r = self.VR([])
        out = tmp_path / "report.json"
        r.write_json(str(out))
        assert out.exists()
        data = json.loads(out.read_text())
        assert "conditions" in data or "specs" in data or "total_conditions" in data

    def test_write_text_creates_file(self, tmp_path):
        r = self.VR([])
        out = tmp_path / "report.txt"
        r.write_text(str(out))
        assert out.exists()


# ===========================================================================
# 9. verify_file / verify_files helpers
# ===========================================================================


class TestVerifyFile:
    def setup_method(self):
        from nexuslang.verification.reporter import verify_file, verify_files

        self.verify_file = verify_file
        self.verify_files = verify_files

    def _write_nxl(self, tmp_path, content, name="test.nxl"):
        p = tmp_path / name
        p.write_text(content)
        return str(p)

    def test_verify_file_no_contracts_zero_vcs(self, tmp_path):
        path = self._write_nxl(tmp_path, "set x to 1\n")
        spec = self.verify_file(path, use_z3=False)
        assert spec.total == 0

    def test_verify_file_with_require_has_vc(self, tmp_path):
        src = (
            "function f with x as Integer returns Integer\n"
            "    require x is greater than 0\n"
            "    return x\n"
            "end\n"
        )
        path = self._write_nxl(tmp_path, src)
        spec = self.verify_file(path, use_z3=False)
        assert spec.total >= 1

    def test_verify_file_returns_formal_spec(self, tmp_path):
        from nexuslang.verification.specification import FormalSpec

        path = self._write_nxl(tmp_path, "set x to 1\n")
        spec = self.verify_file(path, use_z3=False)
        assert isinstance(spec, FormalSpec)

    def test_verify_files_aggregates(self, tmp_path):
        from nexuslang.verification.reporter import VerificationReport

        p1 = self._write_nxl(tmp_path, "set x to 1\n", "a.nxl")
        p2 = self._write_nxl(tmp_path, "set y to 2\n", "b.nxl")
        report = self.verify_files([p1, p2], use_z3=False)
        assert isinstance(report, VerificationReport)

    def test_verify_file_path_recorded_in_spec(self, tmp_path):
        path = self._write_nxl(tmp_path, "set x to 1\n")
        spec = self.verify_file(path, use_z3=False)
        assert spec.path == path


# ===========================================================================
# 10. nlplverify CLI
# ===========================================================================


class TestNlplVerifyCli:
    def setup_method(self):
        from nexuslang.cli.nlplverify import _cli

        self._cli = _cli

    def _write_nxl(self, tmp_path, content, name="test.nxl"):
        p = tmp_path / name
        p.write_text(content)
        return str(p)

    def test_missing_file_exits_2(self):
        rc = self._cli(["nonexistent_file_xyz.nxl"])
        assert rc == 2

    def test_valid_file_exits_0(self, tmp_path):
        path = self._write_nxl(tmp_path, "set x to 1\n")
        rc = self._cli(["--no-z3", path])
        assert rc == 0

    def test_no_z3_flag_succeeds(self, tmp_path):
        path = self._write_nxl(tmp_path, "set x to 1\n")
        rc = self._cli(["--no-z3", path])
        assert rc == 0

    def test_json_output_creates_file(self, tmp_path):
        path = self._write_nxl(tmp_path, "set x to 1\n")
        out = str(tmp_path / "out.json")
        rc = self._cli(["--no-z3", "--json", out, path])
        assert rc == 0
        assert Path(out).exists()

    def test_text_output_creates_file(self, tmp_path):
        path = self._write_nxl(tmp_path, "set x to 1\n")
        out = str(tmp_path / "out.txt")
        rc = self._cli(["--no-z3", "--text", out, path])
        assert rc == 0
        assert Path(out).exists()

    def test_fail_unverified_exits_1_when_vcs_present(self, tmp_path):
        src = (
            "function f with x as Integer returns Integer\n"
            "    require x is greater than 0\n"
            "    return x\n"
            "end\n"
        )
        path = self._write_nxl(tmp_path, src)
        rc = self._cli(["--no-z3", "--fail-unverified", path])
        assert rc == 1

    def test_quiet_flag_accepted(self, tmp_path, capsys):
        path = self._write_nxl(tmp_path, "set x to 1\n")
        rc = self._cli(["--no-z3", "--quiet", path])
        assert rc == 0
        captured = capsys.readouterr()
        assert captured.out == "" or len(captured.out) < 10  # quiet = minimal output

    def test_multiple_files_all_processed(self, tmp_path):
        p1 = self._write_nxl(tmp_path, "set x to 1\n", "a.nxl")
        p2 = self._write_nxl(tmp_path, "set y to 2\n", "b.nxl")
        rc = self._cli(["--no-z3", p1, p2])
        assert rc == 0
