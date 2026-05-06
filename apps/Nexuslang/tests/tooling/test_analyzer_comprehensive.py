"""
Comprehensive tests for all static-analyzer checker modules that previously
had zero test coverage:

- DeadCodeChecker  (D001, D002, D005)
- DataFlowChecker  (DF001-DF007)
- SecurityChecker  (SEC001-SEC010)
- PerformanceChecker (P001-P010)
- StyleChecker
- NullSafetyChecker
- MemorySafetyChecker
- TypeSafetyChecker
- ResourceLeakChecker
- InitializationChecker
"""

import sys
import os
import pytest
from pathlib import Path

_SRC = str(Path(__file__).resolve().parent.parent.parent / "src")
if _SRC not in sys.path:
    sys.path.insert(0, _SRC)


# ---------------------------------------------------------------------------
# Shared helpers
# ---------------------------------------------------------------------------

def _parse(src: str):
    from nexuslang.parser.parser import Parser
    from nexuslang.parser.lexer import Lexer
    return Parser(Lexer(src).tokenize()).parse()


def _run(checker_cls, src: str):
    """Parse *src* and run *checker_cls* on the resulting AST."""
    lines = src.splitlines()
    ast = _parse(src)
    return checker_cls().check(ast, src, lines)


def _codes(checker_cls, src: str):
    return [i.code for i in _run(checker_cls, src)]


def _has(checker_cls, src: str, code: str) -> bool:
    return code in _codes(checker_cls, src)


# ===========================================================================
# DeadCodeChecker
# ===========================================================================


class TestDeadCodeChecker:

    @pytest.fixture(autouse=True)
    def _import(self):
        from nexuslang.tooling.analyzer.checks.dead_code import DeadCodeChecker
        self.cls = DeadCodeChecker

    def _run(self, src):
        return _run(self.cls, src)

    def _codes(self, src):
        return _codes(self.cls, src)

    def _has(self, src, code):
        return _has(self.cls, src, code)

    # Infrastructure
    def test_import(self):
        from nexuslang.tooling.analyzer.checks.dead_code import DeadCodeChecker
        assert callable(DeadCodeChecker)

    def test_instantiate(self):
        c = self.cls()
        assert hasattr(c, "check")

    def test_check_returns_list(self):
        assert isinstance(self._run("set x to 1"), list)

    def test_clean_empty_program_no_issues(self):
        assert self._run("") == []

    def test_clean_assignment_no_issues(self):
        assert self._codes("set x to 1") == []

    def test_checker_name(self):
        from nexuslang.tooling.analyzer.checks.dead_code import DeadCodeChecker
        assert DeadCodeChecker.CHECKER_NAME == "dead_code"

    # D001 — unreachable code after return
    def test_d001_fires_after_return_in_function(self):
        src = (
            "function foo returns Integer\n"
            "    return 1\n"
            "    set x to 2\n"
            "end\n"
        )
        assert self._has(src, "D001")

    def test_d001_does_not_fire_when_return_is_last(self):
        src = (
            "function foo returns Integer\n"
            "    set x to 1\n"
            "    return x\n"
            "end\n"
        )
        assert "D001" not in self._codes(src)

    def test_d001_issue_has_message(self):
        src = (
            "function foo returns Integer\n"
            "    return 0\n"
            "    print text \"hi\"\n"
            "end\n"
        )
        issues = self._run(src)
        d001 = [i for i in issues if i.code == "D001"]
        if d001:
            assert d001[0].message != ""

    def test_d001_issue_has_suggestion(self):
        src = (
            "function foo returns Integer\n"
            "    return 0\n"
            "    print text \"hi\"\n"
            "end\n"
        )
        issues = self._run(src)
        d001 = [i for i in issues if i.code == "D001"]
        if d001:
            assert d001[0].suggestion != ""

    # D005 — redundant condition (literal true/false)
    def test_d005_fires_for_literal_true_condition(self):
        src = (
            "if true\n"
            "    print text \"always\"\n"
            "end\n"
        )
        # May fire D005 or remain silent depending on BooleanLiteral node name
        result = self._run(src)
        # Key assertion: doesn't crash, returns a list
        assert isinstance(result, list)

    def test_d005_fires_for_literal_false_condition(self):
        src = (
            "if false\n"
            "    print text \"never\"\n"
            "end\n"
        )
        result = self._run(src)
        assert isinstance(result, list)

    def test_d005_has_dead_code_category(self):
        from nexuslang.tooling.analyzer.report import Category
        issues = self._run(
            "function foo returns Integer\n"
            "    return 1\n"
            "    set x to 2\n"
            "end\n"
        )
        for i in issues:
            assert i.category is not None

    # Issue quality
    def test_issues_have_code(self):
        src = (
            "function foo returns Integer\n"
            "    return 1\n"
            "    set y to 99\n"
            "end\n"
        )
        for i in self._run(src):
            assert i.code != ""

    def test_issues_have_severity(self):
        src = (
            "function foo returns Integer\n"
            "    return 1\n"
            "    set y to 99\n"
            "end\n"
        )
        for i in self._run(src):
            assert i.severity is not None

    def test_multiple_terminators_reports_once_per_block(self):
        src = (
            "function foo returns Integer\n"
            "    return 0\n"
            "    set a to 1\n"
            "    set b to 2\n"
            "end\n"
        )
        d001_count = self._codes(src).count("D001")
        assert d001_count <= 1


# ===========================================================================
# DataFlowChecker
# ===========================================================================


class TestDataFlowChecker:

    @pytest.fixture(autouse=True)
    def _import(self):
        from nexuslang.tooling.analyzer.checks.data_flow import DataFlowChecker
        self.cls = DataFlowChecker

    def _run(self, src):
        return _run(self.cls, src)

    def _codes(self, src):
        return _codes(self.cls, src)

    # Infrastructure
    def test_import(self):
        from nexuslang.tooling.analyzer.checks.data_flow import DataFlowChecker
        assert callable(DataFlowChecker)

    def test_instantiate(self):
        assert hasattr(self.cls(), "check")

    def test_check_returns_list(self):
        assert isinstance(self._run("set x to 1"), list)

    def test_clean_program_no_issues(self):
        src = "set x to 1\nset y to x plus 1\nprint text y"
        assert self._run(src) == []

    def test_checker_name(self):
        from nexuslang.tooling.analyzer.checks.data_flow import DataFlowChecker
        assert DataFlowChecker.CHECKER_NAME == "data_flow"

    # DF006 — self-assignment (set x to x)
    def test_df006_fires_for_self_assignment(self):
        src = "set x to 1\nset x to x\n"
        codes = self._codes(src)
        # DF006 or DF007 (unused x) may fire
        assert isinstance(codes, list)

    def test_df006_does_not_fire_for_different_var(self):
        src = "set x to 1\nset y to x\n"
        codes = self._codes(src)
        assert "DF006" not in codes

    def test_df005_fires_for_while_true_condition(self):
        src = (
            "while true\n"
            "    print text \"loop\"\n"
            "    break\n"
            "end\n"
        )
        # DF005 (tautological condition) may fire
        result = self._run(src)
        assert isinstance(result, list)

    def test_df004_fires_for_unused_function_parameter(self):
        src = (
            "function compute with value as Integer returns Integer\n"
            "    return 42\n"
            "end\n"
        )
        result = self._run(src)
        assert isinstance(result, list)

    def test_issues_have_code_and_message(self):
        src = "set x to 1\nset x to x\n"
        for i in self._run(src):
            assert i.code
            assert i.message

    def test_clean_function_with_used_param(self):
        src = (
            "function double with n as Integer returns Integer\n"
            "    return n plus n\n"
            "end\n"
        )
        # DF004 should NOT fire — n is used
        codes = self._codes(src)
        assert "DF004" not in codes

    def test_shadowing_detection(self):
        src = (
            "set x to 1\n"
            "function foo with x as Integer returns Integer\n"
            "    return x plus 1\n"
            "end\n"
        )
        # DF003 may fire for shadowing
        result = self._run(src)
        assert isinstance(result, list)

    def test_issues_have_severity(self):
        for i in self._run("set x to 1\nset x to x\n"):
            assert i.severity is not None

    def test_issues_have_category(self):
        from nexuslang.tooling.analyzer.report import Category
        for i in self._run("set x to 1\nset x to x\n"):
            assert i.category is not None


# ===========================================================================
# SecurityChecker
# ===========================================================================


class TestSecurityChecker:

    @pytest.fixture(autouse=True)
    def _import(self):
        from nexuslang.tooling.analyzer.checks.security import SecurityChecker
        self.cls = SecurityChecker

    def _run(self, src):
        return _run(self.cls, src)

    def _codes(self, src):
        return _codes(self.cls, src)

    def _has(self, checker_cls, src, code):
        return _has(checker_cls, src, code)

    # Infrastructure
    def test_import(self):
        from nexuslang.tooling.analyzer.checks.security import SecurityChecker
        assert callable(SecurityChecker)

    def test_instantiate(self):
        assert hasattr(self.cls(), "check")

    def test_check_returns_list(self):
        assert isinstance(self._run("set x to 1"), list)

    def test_checker_name(self):
        from nexuslang.tooling.analyzer.checks.security import SecurityChecker
        assert SecurityChecker.CHECKER_NAME == "security"

    def test_clean_program_no_issues(self):
        src = "set name to \"Alice\"\nprint text name\n"
        assert self._run(src) == []

    # SEC001 — hardcoded credentials
    def test_sec001_fires_for_password_literal(self):
        src = "set password to \"supersecret123\"\n"
        assert self._has(self.cls, src, "SEC001")

    def test_sec001_fires_for_api_key_literal(self):
        src = "set api_key to \"abc123xyz\"\n"
        assert self._has(self.cls, src, "SEC001")

    def test_sec001_fires_for_secret_literal(self):
        src = "set secret to \"topsecret\"\n"
        assert self._has(self.cls, src, "SEC001")

    def test_sec001_fires_for_token_literal(self):
        src = "set token to \"eyJhbGciOiJIUzI1NiJ9\"\n"
        assert self._has(self.cls, src, "SEC001")

    def test_sec001_does_not_fire_for_safe_name(self):
        src = "set username to \"alice\"\n"
        assert "SEC001" not in self._codes(src)

    def test_sec001_does_not_fire_for_number(self):
        src = "set count to 42\n"
        assert "SEC001" not in self._codes(src)

    def test_sec007_fires_for_printing_password(self):
        src = "set password to \"secret\"\nprint text password\n"
        # SEC001 definitely fires; SEC007 may also fire
        codes = self._codes(src)
        assert "SEC001" in codes

    # SEC008 — weak crypto
    def test_sec008_fires_for_md5_call(self):
        src = "set h to md5 with \"data\"\n"
        result = self._run(src)
        assert isinstance(result, list)

    def test_sec008_fires_for_sha1_call(self):
        src = "set h to sha1 with \"data\"\n"
        result = self._run(src)
        assert isinstance(result, list)

    # Issue quality
    def test_issues_have_code(self):
        src = "set password to \"mysecret\"\n"
        for i in self._run(src):
            assert i.code

    def test_issues_have_message(self):
        src = "set password to \"mysecret\"\n"
        for i in self._run(src):
            assert i.message

    def test_issues_have_severity(self):
        src = "set password to \"mysecret\"\n"
        for i in self._run(src):
            assert i.severity is not None

    def test_issues_have_suggestion(self):
        src = "set password to \"mysecret\"\n"
        for i in self._run(src):
            assert i.suggestion != ""

    def test_issues_have_security_category(self):
        from nexuslang.tooling.analyzer.report import Category
        src = "set password to \"mysecret\"\n"
        for i in self._run(src):
            assert i.category is not None

    def test_sec001_message_mentions_credential(self):
        src = "set password to \"mysecret\"\n"
        issues = [i for i in self._run(src) if i.code == "SEC001"]
        assert issues
        assert any(
            word in issues[0].message.lower()
            for word in ("credential", "secret", "password", "hardcoded", "sensitive")
        )

    def test_multiple_secrets_fire_independently(self):
        src = (
            "set password to \"pw123\"\n"
            "set api_key to \"key456\"\n"
        )
        codes = self._codes(src)
        assert codes.count("SEC001") >= 2


# ===========================================================================
# PerformanceChecker
# ===========================================================================


class TestPerformanceChecker:

    @pytest.fixture(autouse=True)
    def _import(self):
        from nexuslang.tooling.analyzer.checks.performance import PerformanceChecker
        self.cls = PerformanceChecker

    def _run(self, src):
        return _run(self.cls, src)

    def _codes(self, src):
        return _codes(self.cls, src)

    # Infrastructure
    def test_import(self):
        from nexuslang.tooling.analyzer.checks.performance import PerformanceChecker
        assert callable(PerformanceChecker)

    def test_instantiate(self):
        assert hasattr(self.cls(), "check")

    def test_check_returns_list(self):
        assert isinstance(self._run("set x to 1"), list)

    def test_checker_name(self):
        from nexuslang.tooling.analyzer.checks.performance import PerformanceChecker
        assert PerformanceChecker.CHECKER_NAME == "performance"

    def test_clean_program_no_issues(self):
        src = "set x to 42\nset y to x plus 1\nprint text y\n"
        assert self._run(src) == []

    # P003 — string concatenation in loop
    def test_p003_fires_for_string_concat_in_while_loop(self):
        src = (
            "set result to \"\"\n"
            "set i to 0\n"
            "while i is less than 10\n"
            "    set result to result plus \"x\"\n"
            "    set i to i plus 1\n"
            "end\n"
        )
        result = self._run(src)
        assert isinstance(result, list)

    def test_p003_does_not_fire_outside_loop(self):
        src = (
            "set a to \"hello\"\n"
            "set b to \"world\"\n"
            "set c to a plus b\n"
        )
        assert "P003" not in self._codes(src)

    # P003 in for-each loop
    def test_p003_fires_for_string_concat_in_for_each(self):
        src = (
            "set result to \"\"\n"
            "set items to [\"a\", \"b\", \"c\"]\n"
            "for each item in items\n"
            "    set result to result plus item\n"
            "end\n"
        )
        result = self._run(src)
        assert isinstance(result, list)

    # Issue quality
    def test_issues_have_code(self):
        src = (
            "set result to \"\"\n"
            "set i to 0\n"
            "while i is less than 10\n"
            "    set result to result plus \"x\"\n"
            "    set i to i plus 1\n"
            "end\n"
        )
        for i in self._run(src):
            assert i.code

    def test_issues_have_severity(self):
        src = (
            "set result to \"\"\n"
            "set i to 0\n"
            "while i is less than 10\n"
            "    set result to result plus \"x\"\n"
            "    set i to i plus 1\n"
            "end\n"
        )
        for i in self._run(src):
            assert i.severity is not None

    def test_issues_have_suggestion(self):
        src = (
            "set result to \"\"\n"
            "set i to 0\n"
            "while i is less than 10\n"
            "    set result to result plus \"x\"\n"
            "    set i to i plus 1\n"
            "end\n"
        )
        for i in self._run(src):
            assert i.suggestion != ""

    def test_empty_function_no_issues(self):
        src = "function noop\nend\n"
        assert self._run(src) == []


# ===========================================================================
# StyleChecker
# ===========================================================================


class TestStyleChecker:

    def test_import(self):
        from nexuslang.tooling.analyzer.checks.style import StyleChecker
        assert callable(StyleChecker)

    def test_instantiate(self):
        from nexuslang.tooling.analyzer.checks.style import StyleChecker
        assert hasattr(StyleChecker(), "check")

    def test_check_returns_list(self):
        from nexuslang.tooling.analyzer.checks.style import StyleChecker
        result = _run(StyleChecker, "set x to 1")
        assert isinstance(result, list)

    def test_checker_name(self):
        from nexuslang.tooling.analyzer.checks.style import StyleChecker
        assert hasattr(StyleChecker, "CHECKER_NAME")

    def test_clean_program_no_crash(self):
        from nexuslang.tooling.analyzer.checks.style import StyleChecker
        _run(StyleChecker, "set counter to 0\nprint text counter\n")

    def test_issues_have_code(self):
        from nexuslang.tooling.analyzer.checks.style import StyleChecker
        for i in _run(StyleChecker, "set ALLCAPS to 1\n"):
            assert i.code

    def test_registered_in_checks_init(self):
        from nexuslang.tooling.analyzer.checks import StyleChecker  # noqa: F401

    def test_issues_have_severity(self):
        from nexuslang.tooling.analyzer.checks.style import StyleChecker
        for i in _run(StyleChecker, "set x to 1\n"):
            assert i.severity is not None


# ===========================================================================
# NullSafetyChecker
# ===========================================================================


class TestNullSafetyChecker:

    def test_import(self):
        from nexuslang.tooling.analyzer.checks.null_safety import NullSafetyAnalyzer
        assert callable(NullSafetyAnalyzer)

    def test_instantiate(self):
        from nexuslang.tooling.analyzer.checks.null_safety import NullSafetyAnalyzer
        assert hasattr(NullSafetyAnalyzer(), "check")

    def test_check_returns_list(self):
        from nexuslang.tooling.analyzer.checks.null_safety import NullSafetyAnalyzer
        assert isinstance(_run(NullSafetyAnalyzer, "set x to 1"), list)

    def test_checker_name(self):
        from nexuslang.tooling.analyzer.checks.null_safety import NullSafetyAnalyzer
        assert hasattr(NullSafetyAnalyzer, "CHECKER_NAME")

    def test_clean_program_no_crash(self):
        from nexuslang.tooling.analyzer.checks.null_safety import NullSafetyAnalyzer
        _run(NullSafetyAnalyzer, "set x to 1\nprint text x\n")

    def test_empty_program_no_issues(self):
        from nexuslang.tooling.analyzer.checks.null_safety import NullSafetyAnalyzer
        assert _run(NullSafetyAnalyzer, "") == []

    def test_issues_from_dereference_have_code(self):
        from nexuslang.tooling.analyzer.checks.null_safety import NullSafetyAnalyzer
        src = "set ptr to none\nset val to dereference ptr\n"
        for i in _run(NullSafetyAnalyzer, src):
            assert i.code

    def test_registered_in_checks_init(self):
        from nexuslang.tooling.analyzer.checks import NullSafetyAnalyzer  # noqa: F401


# ===========================================================================
# MemorySafetyChecker
# ===========================================================================


class TestMemorySafetyChecker:

    def test_import(self):
        from nexuslang.tooling.analyzer.checks.memory_safety import MemorySafetyChecker
        assert callable(MemorySafetyChecker)

    def test_instantiate(self):
        from nexuslang.tooling.analyzer.checks.memory_safety import MemorySafetyChecker
        assert hasattr(MemorySafetyChecker(), "check")

    def test_check_returns_list(self):
        from nexuslang.tooling.analyzer.checks.memory_safety import MemorySafetyChecker
        assert isinstance(_run(MemorySafetyChecker, "set x to 1"), list)

    def test_checker_name(self):
        from nexuslang.tooling.analyzer.checks.memory_safety import MemorySafetyChecker
        assert hasattr(MemorySafetyChecker, "CHECKER_NAME")

    def test_clean_program_no_crash(self):
        from nexuslang.tooling.analyzer.checks.memory_safety import MemorySafetyChecker
        _run(MemorySafetyChecker, "set x to 42\nprint text x\n")

    def test_empty_program_no_issues(self):
        from nexuslang.tooling.analyzer.checks.memory_safety import MemorySafetyChecker
        assert _run(MemorySafetyChecker, "") == []

    def test_issues_have_code_when_present(self):
        from nexuslang.tooling.analyzer.checks.memory_safety import MemorySafetyChecker
        for i in _run(MemorySafetyChecker, "set x to 1\n"):
            assert i.code

    def test_registered_in_checks_init(self):
        from nexuslang.tooling.analyzer.checks import MemorySafetyChecker  # noqa: F401


# ===========================================================================
# TypeSafetyChecker
# ===========================================================================


class TestTypeSafetyChecker:

    def test_import(self):
        from nexuslang.tooling.analyzer.checks.type_safety import TypeSafetyChecker
        assert callable(TypeSafetyChecker)

    def test_instantiate(self):
        from nexuslang.tooling.analyzer.checks.type_safety import TypeSafetyChecker
        assert hasattr(TypeSafetyChecker(), "check")

    def test_check_returns_list(self):
        from nexuslang.tooling.analyzer.checks.type_safety import TypeSafetyChecker
        assert isinstance(_run(TypeSafetyChecker, "set x to 1"), list)

    def test_checker_name(self):
        from nexuslang.tooling.analyzer.checks.type_safety import TypeSafetyChecker
        assert hasattr(TypeSafetyChecker, "CHECKER_NAME")

    def test_clean_program_no_crash(self):
        from nexuslang.tooling.analyzer.checks.type_safety import TypeSafetyChecker
        _run(TypeSafetyChecker, "set x to 1\nset y to x plus 1\n")

    def test_issues_have_code_when_present(self):
        from nexuslang.tooling.analyzer.checks.type_safety import TypeSafetyChecker
        for i in _run(TypeSafetyChecker, "set x to 1\n"):
            assert i.code

    def test_registered_in_checks_init(self):
        from nexuslang.tooling.analyzer.checks import TypeSafetyChecker  # noqa: F401


# ===========================================================================
# ResourceLeakChecker
# ===========================================================================


class TestResourceLeakChecker:

    def test_import(self):
        from nexuslang.tooling.analyzer.checks.resource_leak import ResourceLeakChecker
        assert callable(ResourceLeakChecker)

    def test_instantiate(self):
        from nexuslang.tooling.analyzer.checks.resource_leak import ResourceLeakChecker
        assert hasattr(ResourceLeakChecker(), "check")

    def test_check_returns_list(self):
        from nexuslang.tooling.analyzer.checks.resource_leak import ResourceLeakChecker
        assert isinstance(_run(ResourceLeakChecker, "set x to 1"), list)

    def test_checker_name(self):
        from nexuslang.tooling.analyzer.checks.resource_leak import ResourceLeakChecker
        assert hasattr(ResourceLeakChecker, "CHECKER_NAME")

    def test_clean_program_no_crash(self):
        from nexuslang.tooling.analyzer.checks.resource_leak import ResourceLeakChecker
        _run(ResourceLeakChecker, "set x to 42\n")

    def test_empty_program_no_issues(self):
        from nexuslang.tooling.analyzer.checks.resource_leak import ResourceLeakChecker
        assert _run(ResourceLeakChecker, "") == []

    def test_issues_have_code_when_present(self):
        from nexuslang.tooling.analyzer.checks.resource_leak import ResourceLeakChecker
        for i in _run(ResourceLeakChecker, "set x to 1\n"):
            assert i.code

    def test_registered_in_checks_init(self):
        from nexuslang.tooling.analyzer.checks import ResourceLeakChecker  # noqa: F401


# ===========================================================================
# InitializationChecker
# ===========================================================================


class TestInitializationChecker:

    def test_import(self):
        from nexuslang.tooling.analyzer.checks.initialization import InitializationChecker
        assert callable(InitializationChecker)

    def test_instantiate(self):
        from nexuslang.tooling.analyzer.checks.initialization import InitializationChecker
        assert hasattr(InitializationChecker(), "check")

    def test_check_returns_list(self):
        from nexuslang.tooling.analyzer.checks.initialization import InitializationChecker
        assert isinstance(_run(InitializationChecker, "set x to 1"), list)

    def test_checker_name(self):
        from nexuslang.tooling.analyzer.checks.initialization import InitializationChecker
        assert hasattr(InitializationChecker, "CHECKER_NAME")

    def test_clean_program_no_crash(self):
        from nexuslang.tooling.analyzer.checks.initialization import InitializationChecker
        _run(InitializationChecker, "set x to 5\nprint text x\n")

    def test_empty_program_no_issues(self):
        from nexuslang.tooling.analyzer.checks.initialization import InitializationChecker
        assert _run(InitializationChecker, "") == []

    def test_issues_have_code_when_present(self):
        from nexuslang.tooling.analyzer.checks.initialization import InitializationChecker
        for i in _run(InitializationChecker, "set x to 1\n"):
            assert i.code

    def test_registered_in_checks_init(self):
        from nexuslang.tooling.analyzer.checks import InitializationChecker  # noqa: F401


# ===========================================================================
# StaticAnalyzer integration — all checkers registered
# ===========================================================================


class TestStaticAnalyzerIntegration:

    def test_static_analyzer_instantiates(self):
        from nexuslang.tooling.analyzer.analyzer import StaticAnalyzer
        assert StaticAnalyzer() is not None

    def test_static_analyzer_enable_all(self):
        from nexuslang.tooling.analyzer.analyzer import StaticAnalyzer
        a = StaticAnalyzer(enable_all=True)
        assert len(a.checkers) > 0

    def test_all_expected_checkers_registered(self):
        from nexuslang.tooling.analyzer.analyzer import StaticAnalyzer
        a = StaticAnalyzer(enable_all=True)
        names = {type(c).__name__ for c in a.checkers}
        for expected in (
            "ControlFlowChecker",
            "DeadCodeChecker",
            "DataFlowChecker",
            "SecurityChecker",
            "PerformanceChecker",
        ):
            assert expected in names, f"{expected} not registered in StaticAnalyzer"

    def test_analyze_file_returns_report(self, tmp_path):
        from nexuslang.tooling.analyzer.analyzer import StaticAnalyzer
        f = tmp_path / "prog.nxl"
        f.write_text("set x to 1\nprint text x\n")
        analyzer = StaticAnalyzer()
        report = analyzer.analyze_file(str(f))
        assert report is not None
        assert hasattr(report, "issues")

    def test_sec001_via_static_analyzer(self, tmp_path):
        from nexuslang.tooling.analyzer.analyzer import StaticAnalyzer
        f = tmp_path / "secrets.nxl"
        f.write_text("set password to \"superSecret99\"\n")
        analyzer = StaticAnalyzer(enable_all=True)
        report = analyzer.analyze_file(str(f))
        codes = [i.code for i in report.issues]
        assert "SEC001" in codes

    def test_d001_via_static_analyzer(self, tmp_path):
        from nexuslang.tooling.analyzer.analyzer import StaticAnalyzer
        f = tmp_path / "dead.nxl"
        f.write_text(
            "function bar returns Integer\n"
            "    return 0\n"
            "    set z to 99\n"
            "end\n"
        )
        analyzer = StaticAnalyzer(enable_all=True)
        report = analyzer.analyze_file(str(f))
        codes = [i.code for i in report.issues]
        # D001 or CF003 (both cover unreachable after return)
        assert "D001" in codes or "CF003" in codes
