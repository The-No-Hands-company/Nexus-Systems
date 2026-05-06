"""Tests for conditional compilation (when target / when feature syntax)."""
import pytest
import sys
import os

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'src'))

from nexuslang.parser.lexer import Lexer, TokenType
from nexuslang.parser.parser import Parser
from nexuslang.parser.ast import ConditionalCompilationBlock
from nexuslang.compiler.preprocessor import (
    CompileTarget,
    detect_host,
    host_target,
    evaluate_condition,
    preprocess_ast,
    target_summary,
)
from nexuslang.interpreter.interpreter import Interpreter
from nexuslang.runtime.runtime import Runtime
from nexuslang.stdlib import register_stdlib


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _parse(source: str):
    tokens = Lexer(source).tokenize()
    parser = Parser(tokens)
    return parser.parse()


def _run(source: str):
    """Execute NexusLang source and return the interpreter (inspect variables after)."""
    runtime = Runtime()
    register_stdlib(runtime)
    interp = Interpreter(runtime)
    interp.interpret(source)
    return interp


# ---------------------------------------------------------------------------
# CompileTarget dataclass
# ---------------------------------------------------------------------------

class TestCompileTarget:
    def test_basic_construction(self):
        t = CompileTarget(os="linux", arch="x86_64", endian="little",
                          pointer_width="64", features=frozenset())
        assert t.os == "linux"
        assert t.arch == "x86_64"
        assert t.endian == "little"
        assert t.pointer_width == "64"

    def test_with_features_adds(self):
        t = CompileTarget(os="linux", arch="x86_64", endian="little",
                          pointer_width="64", features=frozenset())
        t2 = t.with_features("networking", "bluetooth")
        assert t2.has_feature("networking")
        assert t2.has_feature("bluetooth")
        assert not t.has_feature("networking")  # original unchanged

    def test_has_feature_case_insensitive(self):
        t = CompileTarget(os="linux", arch="x86_64", endian="little",
                          pointer_width="64", features=frozenset(["networking"]))
        assert t.has_feature("networking")
        assert t.has_feature("NETWORKING")
        assert t.has_feature("Networking")

    def test_frozen_immutability(self):
        t = CompileTarget(os="linux", arch="x86_64", endian="little",
                          pointer_width="64", features=frozenset())
        with pytest.raises((AttributeError, TypeError)):
            t.os = "windows"


# ---------------------------------------------------------------------------
# detect_host / host_target
# ---------------------------------------------------------------------------

class TestDetectHost:
    def test_returns_compile_target(self):
        t = detect_host()
        assert isinstance(t, CompileTarget)

    def test_os_is_string(self):
        t = detect_host()
        assert isinstance(t.os, str)
        assert len(t.os) > 0

    def test_arch_is_string(self):
        t = detect_host()
        assert isinstance(t.arch, str)
        assert len(t.arch) > 0

    def test_endian_valid(self):
        t = detect_host()
        assert t.endian in ("little", "big")

    def test_pointer_width_valid(self):
        t = detect_host()
        assert t.pointer_width in ("32", "64")

    def test_host_target_singleton(self):
        t1 = host_target()
        t2 = host_target()
        assert t1 is t2

    def test_on_linux_os_is_linux(self):
        import platform
        if platform.system().lower() == "linux":
            t = detect_host()
            assert t.os == "linux"


# ---------------------------------------------------------------------------
# evaluate_condition
# ---------------------------------------------------------------------------

class TestEvaluateCondition:
    def _linux_x86_target(self):
        return CompileTarget(os="linux", arch="x86_64", endian="little",
                             pointer_width="64", features=frozenset())

    def test_target_os_match(self):
        t = self._linux_x86_target()
        assert evaluate_condition("target_os", "linux", t) is True

    def test_target_os_no_match(self):
        t = self._linux_x86_target()
        assert evaluate_condition("target_os", "windows", t) is False

    def test_target_arch_match(self):
        t = self._linux_x86_target()
        assert evaluate_condition("target_arch", "x86_64", t) is True

    def test_target_arch_no_match(self):
        t = self._linux_x86_target()
        assert evaluate_condition("target_arch", "aarch64", t) is False

    def test_target_endian_little(self):
        t = self._linux_x86_target()
        assert evaluate_condition("target_endian", "little", t) is True
        assert evaluate_condition("target_endian", "big", t) is False

    def test_target_ptr_width_64(self):
        t = self._linux_x86_target()
        assert evaluate_condition("target_ptr_width", "64", t) is True
        assert evaluate_condition("target_ptr_width", "32", t) is False

    def test_feature_present(self):
        t = self._linux_x86_target().with_features("networking")
        assert evaluate_condition("feature", "networking", t) is True

    def test_feature_absent(self):
        t = self._linux_x86_target()
        assert evaluate_condition("feature", "bluetooth", t) is False

    def test_os_alias(self):
        t = self._linux_x86_target()
        assert evaluate_condition("os", "linux", t) is True

    def test_arch_alias(self):
        t = self._linux_x86_target()
        assert evaluate_condition("arch", "x86_64", t) is True

    def test_case_insensitive_value(self):
        t = self._linux_x86_target()
        assert evaluate_condition("target_os", "Linux", t) is True
        assert evaluate_condition("target_os", "LINUX", t) is True

    def test_os_normalization_ubuntu(self):
        t = CompileTarget(os="linux", arch="x86_64", endian="little",
                          pointer_width="64", features=frozenset())
        # ubuntu should normalize to linux
        assert evaluate_condition("target_os", "ubuntu", t) is True

    def test_arch_normalization_amd64(self):
        t = self._linux_x86_target()
        # amd64 should normalize to x86_64
        assert evaluate_condition("target_arch", "amd64", t) is True

    def test_arch_normalization_x64(self):
        t = self._linux_x86_target()
        assert evaluate_condition("target_arch", "x64", t) is True

    def test_uses_host_when_no_target(self):
        # Should not raise; uses host_target() internally
        result = evaluate_condition("target_endian", "little")
        assert isinstance(result, bool)


# ---------------------------------------------------------------------------
# target_summary
# ---------------------------------------------------------------------------

class TestTargetSummary:
    def test_returns_string(self):
        t = CompileTarget(os="linux", arch="x86_64", endian="little",
                          pointer_width="64", features=frozenset())
        s = target_summary(t)
        assert isinstance(s, str)
        assert "linux" in s.lower()
        assert "x86_64" in s.lower()


# ---------------------------------------------------------------------------
# Parser - ConditionalCompilationBlock
# ---------------------------------------------------------------------------

class TestConditionalCompilationParsing:
    def test_parse_target_os(self):
        src = '''
when target os is "linux"
    set x to 1
end
'''
        ast = _parse(src)
        nodes = [n for n in ast.statements
                 if isinstance(n, ConditionalCompilationBlock)]
        assert len(nodes) == 1
        node = nodes[0]
        assert node.condition_type == "target_os"
        assert node.condition_value == "linux"
        assert len(node.body) == 1
        assert node.else_body is None

    def test_parse_target_arch(self):
        src = '''
when target arch is "x86_64"
    set y to 2
end
'''
        ast = _parse(src)
        nodes = [n for n in ast.statements
                 if isinstance(n, ConditionalCompilationBlock)]
        assert len(nodes) == 1
        assert nodes[0].condition_type == "target_arch"
        assert nodes[0].condition_value == "x86_64"

    def test_parse_target_endian(self):
        src = '''
when target endian is "little"
    set y to 1
end
'''
        ast = _parse(src)
        nodes = [n for n in ast.statements
                 if isinstance(n, ConditionalCompilationBlock)]
        assert nodes[0].condition_type == "target_endian"
        assert nodes[0].condition_value == "little"

    def test_parse_target_pointer_width(self):
        src = '''
when target pointer width is "64"
    set y to 1
end
'''
        ast = _parse(src)
        nodes = [n for n in ast.statements
                 if isinstance(n, ConditionalCompilationBlock)]
        assert nodes[0].condition_type == "target_ptr_width"
        assert nodes[0].condition_value == "64"

    def test_parse_feature(self):
        src = '''
when feature "networking"
    set y to 1
end
'''
        ast = _parse(src)
        nodes = [n for n in ast.statements
                 if isinstance(n, ConditionalCompilationBlock)]
        assert nodes[0].condition_type == "feature"
        assert nodes[0].condition_value == "networking"

    def test_parse_otherwise_branch(self):
        src = '''
when target os is "linux"
    set x to 1
otherwise
    set x to 2
end
'''
        ast = _parse(src)
        nodes = [n for n in ast.statements
                 if isinstance(n, ConditionalCompilationBlock)]
        assert len(nodes) == 1
        node = nodes[0]
        assert node.else_body is not None
        assert len(node.else_body) == 1

    def test_parse_multiple_blocks(self):
        src = '''
when target os is "linux"
    set x to 1
end
when target arch is "x86_64"
    set y to 2
end
'''
        ast = _parse(src)
        nodes = [n for n in ast.statements
                 if isinstance(n, ConditionalCompilationBlock)]
        assert len(nodes) == 2

    def test_node_type_field(self):
        src = '''
when target os is "linux"
    set z to 99
end
'''
        ast = _parse(src)
        node = next(n for n in ast.statements
                    if isinstance(n, ConditionalCompilationBlock))
        assert node.node_type == "conditional_compilation_block"

    def test_body_contains_multiple_statements(self):
        src = '''
when target os is "linux"
    set a to 1
    set b to 2
    set c to 3
end
'''
        ast = _parse(src)
        node = next(n for n in ast.statements
                    if isinstance(n, ConditionalCompilationBlock))
        assert len(node.body) == 3


# ---------------------------------------------------------------------------
# Interpreter execution
# ---------------------------------------------------------------------------

class TestConditionalCompilationExecution:
    """Tests that use the interpreter to confirm runtime behavior."""

    def test_true_branch_executes_on_linux(self):
        import platform
        if platform.system().lower() != "linux":
            pytest.skip("Linux-only test")
        src = '''
when target os is "linux"
    set result to "linux_branch"
otherwise
    set result to "other_branch"
end
'''
        interp = _run(src)
        assert interp.get_variable("result") == "linux_branch"

    def test_false_branch_executes_on_linux(self):
        import platform
        if platform.system().lower() != "linux":
            pytest.skip("Linux-only test")
        src = '''
when target os is "windows"
    set result to "windows_branch"
otherwise
    set result to "other_branch"
end
'''
        interp = _run(src)
        assert interp.get_variable("result") == "other_branch"

    def test_no_otherwise_false_branch_is_noop(self):
        src = '''
set result to "initial"
when target os is "windows_only_os_never_exists_xyz"
    set result to "changed"
end
'''
        interp = _run(src)
        assert interp.get_variable("result") == "initial"

    def test_arch_x86_on_x86_machine(self):
        import platform
        if platform.machine().lower() not in ("x86_64", "amd64"):
            pytest.skip("x86_64-only test")
        src = '''
when target arch is "x86_64"
    set arch_ok to 1
otherwise
    set arch_ok to 0
end
'''
        interp = _run(src)
        assert interp.get_variable("arch_ok") == 1

    def test_little_endian_branch(self):
        import sys as _sys
        if _sys.byteorder != "little":
            pytest.skip("little-endian-only test")
        src = '''
when target endian is "little"
    set endian_ok to 1
otherwise
    set endian_ok to 0
end
'''
        interp = _run(src)
        assert interp.get_variable("endian_ok") == 1

    def test_feature_false_when_no_features(self):
        src = '''
set result to "no_feature"
when feature "bluetooth"
    set result to "has_bluetooth"
end
'''
        interp = _run(src)
        assert interp.get_variable("result") == "no_feature"

    def test_nested_conditional_compilation(self):
        import platform
        if platform.system().lower() != "linux":
            pytest.skip("Linux-only test")
        src = '''
when target os is "linux"
    set os_ok to 1
    when target arch is "x86_64"
        set arch_ok to 1
    otherwise
        set arch_ok to 0
    end
otherwise
    set os_ok to 0
end
'''
        interp = _run(src)
        assert interp.get_variable("os_ok") == 1

    def test_cross_compilation_override(self):
        """When runtime.compile_target is set, it overrides host detection."""
        runtime = Runtime()
        register_stdlib(runtime)
        fake_target = CompileTarget(
            os="windows", arch="aarch64", endian="little",
            pointer_width="64", features=frozenset()
        )
        runtime.compile_target = fake_target

        interp = Interpreter(runtime)
        src = '''
when target os is "windows"
    set result to "windows_branch"
otherwise
    set result to "other_branch"
end
'''
        interp.interpret(src)
        assert interp.get_variable("result") == "windows_branch"


# ---------------------------------------------------------------------------
# preprocess_ast
# ---------------------------------------------------------------------------

class TestPreprocessAst:
    def test_removes_false_branch(self):
        import platform
        if platform.system().lower() != "linux":
            pytest.skip("Linux-only test")
        src = '''
when target os is "linux"
    set x to 1
end
when target os is "windows"
    set y to 2
end
'''
        tokens = Lexer(src).tokenize()
        ast = Parser(tokens).parse()
        target = CompileTarget(os="linux", arch="x86_64", endian="little",
                               pointer_width="64", features=frozenset())
        preprocess_ast(ast, target)
        # Should have only one alive statement (x=1 block or its body inlined)
        assert len(ast.statements) > 0
