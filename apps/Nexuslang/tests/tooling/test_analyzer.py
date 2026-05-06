"""
Linter / static-analyzer checks and control-flow checker tests.
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


class TestLinterChecks:
    def test_performance_check_import(self):
        from nexuslang.tooling.analyzer.checks.performance import PerformanceChecker
        assert PerformanceChecker is not None

    def test_security_check_import(self):
        from nexuslang.tooling.analyzer.checks.security import SecurityChecker
        assert SecurityChecker is not None

    def test_data_flow_check_import(self):
        from nexuslang.tooling.analyzer.checks.data_flow import DataFlowChecker
        assert DataFlowChecker is not None

    def test_performance_check_instantiate(self):
        from nexuslang.tooling.analyzer.checks.performance import PerformanceChecker
        c = PerformanceChecker()
        assert hasattr(c, "check") or hasattr(c, "run") or hasattr(c, "analyze")

    def test_security_check_instantiate(self):
        from nexuslang.tooling.analyzer.checks.security import SecurityChecker
        c = SecurityChecker()
        assert c is not None

    def test_data_flow_check_instantiate(self):
        from nexuslang.tooling.analyzer.checks.data_flow import DataFlowChecker
        c = DataFlowChecker()
        assert c is not None

    def test_analyzer_includes_checks(self):
        from nexuslang.tooling.analyzer.analyzer import StaticAnalyzer
        a = StaticAnalyzer()
        assert a is not None


class TestControlFlowChecker:
    """Comprehensive tests for ControlFlowChecker (CF001/CF002/CF003)."""

    def _check(self, src):
        from nexuslang.tooling.analyzer.checks.control_flow import ControlFlowChecker
        from nexuslang.parser.parser import Parser
        from nexuslang.parser.lexer import Lexer
        prog = Parser(Lexer(src).tokenize()).parse()
        return ControlFlowChecker().check(prog, src, src.splitlines())

    def _codes(self, src):
        """Return list of issue codes emitted for *src*."""
        return [i.code for i in self._check(src)]

    def _has(self, src, code):
        return code in self._codes(src)

    def _none(self, src, code):
        return code not in self._codes(src)

    # ------------------------------------------------------------------
    # Infrastructure
    # ------------------------------------------------------------------

    def test_import(self):
        from nexuslang.tooling.analyzer.checks.control_flow import ControlFlowChecker
        assert callable(ControlFlowChecker)

    def test_instantiate(self):
        from nexuslang.tooling.analyzer.checks.control_flow import ControlFlowChecker
        c = ControlFlowChecker()
        assert hasattr(c, "check")

    def test_check_returns_list(self):
        assert isinstance(self._check("set x to 1"), list)

    def test_control_flow_category_importable(self):
        from nexuslang.tooling.analyzer.report import Category
        assert hasattr(Category, "CONTROL_FLOW")

    def test_checker_registered_in_init(self):
        from nexuslang.tooling.analyzer.checks import ControlFlowChecker
        assert ControlFlowChecker is not None

    def test_analyzer_includes_control_flow(self):
        from nexuslang.tooling.analyzer.analyzer import StaticAnalyzer
        analyzer = StaticAnalyzer(enable_all=True)
        checker_types = [type(c).__name__ for c in analyzer.checkers]
        assert "ControlFlowChecker" in checker_types

    def test_checker_name(self):
        from nexuslang.tooling.analyzer.checks.control_flow import ControlFlowChecker
        assert ControlFlowChecker.CHECKER_NAME == "control_flow"

    # ------------------------------------------------------------------
    # Clean programs — nothing should fire
    # ------------------------------------------------------------------

    def test_clean_empty_program(self):
        assert self._check("") == []

    def test_clean_assignment(self):
        assert self._codes("set x to 5") == []

    def test_clean_multiple_assignments(self):
        src = "set x to 1\nset y to 2\nset z to x plus y"
        assert self._codes(src) == []

    def test_clean_function_always_returns(self):
        src = (
            "function square with n as Integer returns Integer\n"
            "    return n times n\n"
            "end"
        )
        assert self._none(src, "CF001")

    def test_clean_void_function_no_return(self):
        src = (
            "function greet with name as String\n"
            "    print text name\n"
            "end"
        )
        assert self._none(src, "CF001")

    def test_clean_if_else_both_return(self):
        src = (
            "function abs_val with n as Integer returns Integer\n"
            "    if n is greater than 0\n"
            "        return n\n"
            "    else\n"
            "        return 0 minus n\n"
            "    end\n"
            "end"
        )
        assert self._none(src, "CF001")

    def test_clean_while_true_with_break(self):
        src = (
            "while true\n"
            "    break\n"
            "end"
        )
        assert self._none(src, "CF002")

    def test_clean_while_true_with_conditional_break(self):
        src = (
            "set x to 0\n"
            "while true\n"
            "    if x is greater than 10\n"
            "        break\n"
            "    end\n"
            "    set x to x plus 1\n"
            "end"
        )
        assert self._none(src, "CF002")

    def test_clean_while_true_with_return_inside_function(self):
        src = (
            "function count_to with limit as Integer returns Integer\n"
            "    set x to 0\n"
            "    while true\n"
            "        set x to x plus 1\n"
            "        if x is greater than or equal to limit\n"
            "            return x\n"
            "        end\n"
            "    end\n"
            "end"
        )
        assert self._none(src, "CF002")

    def test_clean_regular_while_no_cf002(self):
        src = (
            "set x to 0\n"
            "while x is less than 5\n"
            "    set x to x plus 1\n"
            "end"
        )
        assert self._none(src, "CF002")

    def test_clean_normal_sequence_no_cf003(self):
        src = (
            "set a to 1\n"
            "set b to 2\n"
            "set c to a plus b"
        )
        assert self._none(src, "CF003")

    # ------------------------------------------------------------------
    # CF001 — Missing return in typed function
    # ------------------------------------------------------------------

    def test_cf001_fires_when_body_has_no_return(self):
        src = (
            "function compute returns Integer\n"
            "    set result to 42\n"
            "end"
        )
        assert self._has(src, "CF001"), "Expected CF001 for function with no return"

    def test_cf001_fires_when_body_is_empty(self):
        src = "function todo returns Integer\nend"
        assert self._has(src, "CF001"), "Expected CF001 for empty typed function"

    def test_cf001_fires_for_string_return_type(self):
        src = (
            "function greet with name as String returns String\n"
            "    print text name\n"
            "end"
        )
        assert self._has(src, "CF001")

    def test_cf001_fires_for_float_return_type(self):
        src = (
            "function pi_approx returns Float\n"
            "    set x to 3\n"
            "end"
        )
        assert self._has(src, "CF001")

    def test_cf001_fires_when_if_has_no_else(self):
        src = (
            "function maybe_positive with n as Integer returns Integer\n"
            "    if n is greater than 0\n"
            "        return n\n"
            "    end\n"
            "end"
        )
        assert self._has(src, "CF001"), "CF001 expected: else branch is missing"

    def test_cf001_does_not_fire_for_void_function(self):
        src = (
            "function log_value with x as Integer\n"
            "    print text x\n"
            "end"
        )
        assert self._none(src, "CF001")

    def test_cf001_does_not_fire_when_always_returns(self):
        src = (
            "function double with n as Integer returns Integer\n"
            "    return n times 2\n"
            "end"
        )
        assert self._none(src, "CF001")

    def test_cf001_does_not_fire_for_if_else_both_returning(self):
        src = (
            "function sign with n as Integer returns Integer\n"
            "    if n is greater than 0\n"
            "        return 1\n"
            "    else\n"
            "        return 0 minus 1\n"
            "    end\n"
            "end"
        )
        assert self._none(src, "CF001")

    def test_cf001_message_includes_function_name(self):
        src = (
            "function my_func returns Integer\n"
            "    set x to 1\n"
            "end"
        )
        issues = self._check(src)
        cf001 = [i for i in issues if i.code == "CF001"]
        assert cf001, "Expected at least one CF001 issue"
        assert "my_func" in cf001[0].message

    def test_cf001_message_includes_return_type(self):
        src = (
            "function my_func returns Integer\n"
            "    set x to 1\n"
            "end"
        )
        issues = self._check(src)
        cf001 = [i for i in issues if i.code == "CF001"]
        assert cf001
        assert "Integer" in cf001[0].message

    def test_cf001_issue_has_suggestion(self):
        src = (
            "function placeholder returns Integer\n"
            "    set x to 0\n"
            "end"
        )
        issues = self._check(src)
        cf001 = [i for i in issues if i.code == "CF001"]
        assert cf001
        assert cf001[0].suggestion is not None and len(cf001[0].suggestion) > 0

    # ------------------------------------------------------------------
    # CF002 — Potential infinite loop
    # ------------------------------------------------------------------

    def test_cf002_fires_for_while_true_no_break(self):
        src = (
            "while true\n"
            "    set x to 1\n"
            "end"
        )
        assert self._has(src, "CF002"), "CF002 expected for while true without break"

    def test_cf002_fires_for_while_true_no_escape(self):
        src = (
            "set i to 0\n"
            "while true\n"
            "    set i to i plus 1\n"
            "    print text i\n"
            "end"
        )
        assert self._has(src, "CF002")

    def test_cf002_does_not_fire_when_break_in_if(self):
        src = (
            "set i to 0\n"
            "while true\n"
            "    set i to i plus 1\n"
            "    if i is equal to 5\n"
            "        break\n"
            "    end\n"
            "end"
        )
        assert self._none(src, "CF002")

    def test_cf002_does_not_fire_when_break_present(self):
        src = (
            "while true\n"
            "    set x to 1\n"
            "    break\n"
            "end"
        )
        assert self._none(src, "CF002")

    def test_cf002_does_not_fire_for_conditional_while(self):
        src = (
            "set x to 0\n"
            "while x is less than 10\n"
            "    set x to x plus 1\n"
            "end"
        )
        assert self._none(src, "CF002")

    def test_cf002_does_not_fire_for_regular_while_with_variable(self):
        src = (
            "set running to true\n"
            "set counter to 0\n"
            "while running\n"
            "    set counter to counter plus 1\n"
            "end"
        )
        assert self._none(src, "CF002")

    def test_cf002_issue_message_mentions_break_or_return(self):
        src = (
            "while true\n"
            "    set x to 0\n"
            "end"
        )
        issues = self._check(src)
        cf002 = [i for i in issues if i.code == "CF002"]
        assert cf002
        msg = cf002[0].message.lower()
        assert "break" in msg or "return" in msg

    def test_cf002_issue_has_suggestion(self):
        src = (
            "while true\n"
            "    set x to 0\n"
            "end"
        )
        issues = self._check(src)
        cf002 = [i for i in issues if i.code == "CF002"]
        assert cf002
        assert cf002[0].suggestion is not None and len(cf002[0].suggestion) > 0

    # ------------------------------------------------------------------
    # CF003 — Unreachable code after unconditional jump
    # ------------------------------------------------------------------

    def test_cf003_fires_for_return_then_statement_in_function(self):
        src = (
            "function foo returns Integer\n"
            "    return 1\n"
            "    set x to 2\n"
            "end"
        )
        assert self._has(src, "CF003"), "CF003 expected after return"

    def test_cf003_fires_for_break_then_statement_in_while(self):
        src = (
            "set x to 0\n"
            "while x is less than 5\n"
            "    break\n"
            "    set x to x plus 1\n"
            "end"
        )
        assert self._has(src, "CF003"), "CF003 expected after break"

    def test_cf003_fires_for_continue_then_statement_in_for_each(self):
        src = (
            "set items to [1, 2, 3]\n"
            "for each item in items\n"
            "    continue\n"
            "    print text item\n"
            "end"
        )
        assert self._has(src, "CF003"), "CF003 expected after continue"

    def test_cf003_does_not_fire_for_normal_sequence(self):
        src = (
            "function add with a as Integer and b as Integer returns Integer\n"
            "    set total to a plus b\n"
            "    return total\n"
            "end"
        )
        assert self._none(src, "CF003")

    def test_cf003_does_not_fire_when_return_is_last(self):
        src = (
            "function foo returns Integer\n"
            "    set x to 42\n"
            "    return x\n"
            "end"
        )
        assert self._none(src, "CF003")

    def test_cf003_reports_only_first_unreachable_per_block(self):
        src = (
            "function foo returns Integer\n"
            "    return 1\n"
            "    set a to 2\n"
            "    set b to 3\n"
            "end"
        )
        cf003 = [i for i in self._check(src) if i.code == "CF003"]
        assert len(cf003) == 1, "Only first unreachable statement per block"

    def test_cf003_issue_has_suggestion(self):
        src = (
            "function foo returns Integer\n"
            "    return 1\n"
            "    set x to 2\n"
            "end"
        )
        issues = self._check(src)
        cf003 = [i for i in issues if i.code == "CF003"]
        assert cf003
        assert cf003[0].suggestion is not None and len(cf003[0].suggestion) > 0

    # ------------------------------------------------------------------
    # Issue structure validation
    # ------------------------------------------------------------------

    def test_issues_have_code_attribute(self):
        src = (
            "function bad returns Integer\n"
            "    set x to 1\n"
            "end"
        )
        issues = self._check(src)
        assert all(hasattr(i, "code") for i in issues)

    def test_issues_have_message_attribute(self):
        src = (
            "function bad returns Integer\n"
            "    set x to 1\n"
            "end"
        )
        issues = self._check(src)
        assert all(hasattr(i, "message") for i in issues)

    def test_issues_have_severity_attribute(self):
        src = (
            "function bad returns Integer\n"
            "    set x to 1\n"
            "end"
        )
        issues = self._check(src)
        assert all(hasattr(i, "severity") for i in issues)

    def test_issues_have_category_attribute(self):
        src = (
            "function bad returns Integer\n"
            "    set x to 1\n"
            "end"
        )
        issues = self._check(src)
        assert all(hasattr(i, "category") for i in issues)

    def test_cf001_issue_category_is_control_flow(self):
        from nexuslang.tooling.analyzer.report import Category
        src = (
            "function bad returns Integer\n"
            "    set x to 1\n"
            "end"
        )
        issues = self._check(src)
        cf001 = [i for i in issues if i.code == "CF001"]
        assert cf001
        assert cf001[0].category == Category.CONTROL_FLOW

    def test_cf002_issue_category_is_control_flow(self):
        from nexuslang.tooling.analyzer.report import Category
        src = (
            "while true\n"
            "    set x to 0\n"
            "end"
        )
        issues = self._check(src)
        cf002 = [i for i in issues if i.code == "CF002"]
        assert cf002
        assert cf002[0].category == Category.CONTROL_FLOW

    def test_cf003_issue_category_is_control_flow(self):
        from nexuslang.tooling.analyzer.report import Category
        src = (
            "function foo returns Integer\n"
            "    return 1\n"
            "    set x to 2\n"
            "end"
        )
        issues = self._check(src)
        cf003 = [i for i in issues if i.code == "CF003"]
        assert cf003
        assert cf003[0].category == Category.CONTROL_FLOW

    # ------------------------------------------------------------------
    # StaticAnalyzer integration
    # ------------------------------------------------------------------

    def test_static_analyzer_emits_cf001_via_analyze_file(self):
        import tempfile, os
        from nexuslang.tooling.analyzer.analyzer import StaticAnalyzer
        src = (
            "function broken returns Integer\n"
            "    set x to 0\n"
            "end"
        )
        with tempfile.NamedTemporaryFile(suffix=".nxl", mode="w",
                                         delete=False) as f:
            f.write(src)
            path = f.name
        try:
            analyzer = StaticAnalyzer(enable_all=True)
            report = analyzer.analyze_file(path)
            codes = [i.code for i in report.issues]
            assert "CF001" in codes
        finally:
            os.unlink(path)

    def test_static_analyzer_emits_cf002_via_analyze_file(self):
        import tempfile, os
        from nexuslang.tooling.analyzer.analyzer import StaticAnalyzer
        src = (
            "while true\n"
            "    set x to 1\n"
            "end"
        )
        with tempfile.NamedTemporaryFile(suffix=".nxl", mode="w",
                                         delete=False) as f:
            f.write(src)
            path = f.name
        try:
            analyzer = StaticAnalyzer(enable_all=True)
            report = analyzer.analyze_file(path)
            codes = [i.code for i in report.issues]
            assert "CF002" in codes
        finally:
            os.unlink(path)

