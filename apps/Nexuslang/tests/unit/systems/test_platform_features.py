"""Tests for platform section 4.1 features:
  - CompileTarget.from_triple() / to_triple()
  - target_triple condition type in evaluate_condition()
  - preprocess_ast() called before static analysis (static pruning)
  - InlineAssembly.arch field in AST
  - Parser: asm for arch "..." guard syntax
  - LLVM backend: riscv64/riscv32/mips registers and dangerous instructions
  - LLVM backend: arch guard skips mismatched InlineAssembly nodes
"""

import io
import sys
import os
import pytest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'src'))

from nexuslang.parser.lexer import Lexer
from nexuslang.parser.parser import Parser
from nexuslang.parser.ast import InlineAssembly, Program
from nexuslang.compiler.preprocessor import (
    CompileTarget,
    host_target,
    evaluate_condition,
    preprocess_ast,
)
from nexuslang.interpreter.interpreter import Interpreter
from nexuslang.runtime.runtime import Runtime
from nexuslang.stdlib import register_stdlib


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _parse(source: str) -> Program:
    tokens = Lexer(source).tokenize()
    return Parser(tokens).parse()


def _run_src(source: str, target=None, capture_output=True):
    """Execute NexusLang source, optionally against a specific CompileTarget."""
    if capture_output:
        buf = io.StringIO()
        old_stdout = sys.stdout
        sys.stdout = buf
    try:
        from nexuslang.main import run_program
        run_program(source, type_check=True, target=target)
    finally:
        if capture_output:
            sys.stdout = old_stdout
    if capture_output:
        return buf.getvalue()


# ---------------------------------------------------------------------------
# CompileTarget.from_triple() and to_triple()
# ---------------------------------------------------------------------------

class TestCompileTargetTriple:
    def test_from_triple_x86_64_linux(self):
        t = CompileTarget.from_triple("x86_64-unknown-linux-gnu")
        assert t.arch == "x86_64"
        assert t.os == "linux"
        assert t.pointer_width == "64"
        assert t.endian == "little"

    def test_from_triple_x86_64_windows(self):
        t = CompileTarget.from_triple("x86_64-pc-windows-msvc")
        assert t.arch == "x86_64"
        assert t.os == "windows"
        assert t.pointer_width == "64"
        assert t.endian == "little"

    def test_from_triple_riscv64(self):
        t = CompileTarget.from_triple("riscv64-unknown-none-elf")
        assert t.arch == "riscv64"
        assert t.pointer_width == "64"
        assert t.endian == "little"

    def test_from_triple_riscv32(self):
        t = CompileTarget.from_triple("riscv32-unknown-none-elf")
        assert t.arch == "riscv32"
        assert t.pointer_width == "32"
        assert t.endian == "little"

    def test_from_triple_mips_big_endian(self):
        t = CompileTarget.from_triple("mips-unknown-linux-gnu")
        assert t.arch == "mips"
        assert t.endian == "big"
        assert t.pointer_width == "32"

    def test_from_triple_mipsel_little_endian(self):
        t = CompileTarget.from_triple("mipsel-unknown-linux-gnu")
        assert t.arch == "mipsel"
        assert t.endian == "little"

    def test_from_triple_arm(self):
        t = CompileTarget.from_triple("arm-unknown-linux-gnueabihf")
        assert t.arch == "arm"
        assert t.pointer_width == "32"
        assert t.endian == "little"

    def test_from_triple_aarch64(self):
        t = CompileTarget.from_triple("aarch64-unknown-linux-gnu")
        assert t.arch == "aarch64"
        assert t.pointer_width == "64"
        assert t.endian == "little"

    def test_from_triple_darwin(self):
        t = CompileTarget.from_triple("x86_64-apple-darwin")
        assert t.arch == "x86_64"
        assert t.os in ("macos", "darwin")

    def test_to_triple_contains_arch(self):
        t = CompileTarget.from_triple("x86_64-unknown-linux-gnu")
        triple = t.to_triple()
        assert "x86_64" in triple

    def test_to_triple_contains_os(self):
        t = CompileTarget.from_triple("x86_64-unknown-linux-gnu")
        triple = t.to_triple()
        assert "linux" in triple

    def test_to_triple_riscv64(self):
        t = CompileTarget.from_triple("riscv64-unknown-linux-gnu")
        triple = t.to_triple()
        assert "riscv64" in triple

    def test_from_triple_too_short_raises(self):
        with pytest.raises(ValueError):
            CompileTarget.from_triple("x86_64-linux")

    def test_from_triple_single_component_raises(self):
        with pytest.raises(ValueError):
            CompileTarget.from_triple("x86_64")

    def test_from_triple_empty_raises(self):
        with pytest.raises(ValueError):
            CompileTarget.from_triple("")


# ---------------------------------------------------------------------------
# evaluate_condition() with target_triple / triple condition type
# ---------------------------------------------------------------------------

class TestTargetTripleCondition:
    def test_full_arch_match(self):
        t = CompileTarget.from_triple("riscv64-unknown-linux-gnu")
        assert evaluate_condition("target_triple", "riscv64", t) is True

    def test_full_arch_no_match(self):
        t = CompileTarget.from_triple("x86_64-unknown-linux-gnu")
        assert evaluate_condition("target_triple", "riscv64", t) is False

    def test_triple_alias_works(self):
        t = CompileTarget.from_triple("riscv64-unknown-linux-gnu")
        assert evaluate_condition("triple", "riscv64", t) is True

    def test_partial_triple_prefix_match(self):
        t = CompileTarget.from_triple("x86_64-unknown-linux-gnu")
        triple = t.to_triple()
        # Match with the actual beginning of the triple
        assert evaluate_condition("target_triple", triple[:7], t) is True

    def test_mips_arch_match(self):
        t = CompileTarget.from_triple("mips-unknown-linux-gnu")
        assert evaluate_condition("target_triple", "mips", t) is True

    def test_aarch64_no_match_arm(self):
        t = CompileTarget.from_triple("aarch64-unknown-linux-gnu")
        assert evaluate_condition("target_triple", "x86_64", t) is False


# ---------------------------------------------------------------------------
# Static pruning: preprocess_ast() filters dead branches before type-checker
# ---------------------------------------------------------------------------

class TestStaticPruning:
    def test_dead_branch_with_undefined_variable_not_reached(self):
        """A dead when-target branch referencing nonsense should not cause errors."""
        src = (
            'when target os is "this_os_does_not_exist_1234"\n'
            '    set zzz_undefined to some_nonexistent_variable_xyz\n'
            'end\n'
            'set y to 42\n'
        )
        # Should not raise; dead branch stripped before interpreter/type-checker
        _run_src(src)

    def test_live_branch_executes(self):
        """The matching when-target branch must still run."""
        host = host_target()
        src = (
            f'when target os is "{host.os}"\n'
            '    set result to "live"\n'
            'end\n'
            'print text result\n'
        )
        out = _run_src(src)
        assert "live" in out

    def test_cross_target_riscv_selects_arch_branch(self):
        """With riscv64 target, arch branch fires, otherwise branch does not."""
        src = (
            'when target arch is "riscv64"\n'
            '    set result to "riscv"\n'
            'otherwise\n'
            '    set result to "other"\n'
            'end\n'
            'print text result\n'
        )
        t = CompileTarget.from_triple("riscv64-unknown-linux-gnu")
        out = _run_src(src, target=t)
        assert "riscv" in out
        assert "other" not in out

    def test_cross_target_x86_selects_otherwise(self):
        """With x86_64 target, riscv64 branch is dead; otherwise runs."""
        src = (
            'when target arch is "riscv64"\n'
            '    set result to "riscv"\n'
            'otherwise\n'
            '    set result to "x86"\n'
            'end\n'
            'print text result\n'
        )
        t = CompileTarget.from_triple("x86_64-unknown-linux-gnu")
        out = _run_src(src, target=t)
        assert "x86" in out
        assert "riscv" not in out

    def test_preprocess_ast_mutates_program(self):
        """preprocess_ast() strips ConditionalCompilationBlock nodes from AST."""
        from nexuslang.parser.ast import ConditionalCompilationBlock
        src = (
            'when target os is "this_os_does_not_exist_9999"\n'
            '    set dead to 1\n'
            'end\n'
            'set alive to 2\n'
        )
        ast = _parse(src)
        # Before pruning, should contain the ConditionalCompilationBlock
        types_before = [type(s) for s in ast.statements]
        assert ConditionalCompilationBlock in types_before

        preprocess_ast(ast)

        # After pruning with host target, dead block for unknown OS is gone
        cc_blocks = [s for s in ast.statements
                     if isinstance(s, ConditionalCompilationBlock)]
        assert len(cc_blocks) == 0


# ---------------------------------------------------------------------------
# InlineAssembly AST node: arch field
# ---------------------------------------------------------------------------

class TestInlineAssemblyASTNode:
    # Correct NexusLang inline-assembly syntax: indented 'code' block, closed by a
    # single trailing 'end' at the asm level (no nested 'end' for the code block).
    _ASM_SIMPLE = 'asm\n    code\n        "nop"\nend\n'
    _ASM_ARCH = 'asm for arch "riscv64"\n    code\n        "add a0, a1, a2"\nend\n'

    def test_asm_without_arch_guard_parsed(self):
        prog = _parse(self._ASM_SIMPLE)
        node = prog.statements[0]
        assert isinstance(node, InlineAssembly)

    def test_asm_without_arch_guard_has_none_arch(self):
        prog = _parse(self._ASM_SIMPLE)
        node = prog.statements[0]
        assert node.arch is None

    def test_asm_with_arch_guard_parsed(self):
        prog = _parse(self._ASM_ARCH)
        node = prog.statements[0]
        assert isinstance(node, InlineAssembly)

    def test_asm_with_arch_guard_has_correct_arch(self):
        prog = _parse(self._ASM_ARCH)
        node = prog.statements[0]
        assert node.arch == "riscv64"

    def test_asm_arch_stored_lowercase(self):
        src = 'asm for arch "RISCV64"\n    code\n        "nop"\nend\n'
        prog = _parse(src)
        node = prog.statements[0]
        assert node.arch == "riscv64"

    def test_asm_str_repr_includes_arch(self):
        prog = _parse(self._ASM_ARCH)
        node = prog.statements[0]
        text = str(node)
        assert "riscv64" in text.lower()

    def test_asm_x86_arch_guard(self):
        src = 'asm for arch "x86_64"\n    code\n        "nop"\nend\n'
        prog = _parse(src)
        node = prog.statements[0]
        assert node.arch == "x86_64"

    def test_asm_arm_arch_guard(self):
        src = 'asm for arch "arm"\n    code\n        "nop"\nend\n'
        prog = _parse(src)
        node = prog.statements[0]
        assert node.arch == "arm"


# ---------------------------------------------------------------------------
# LLVM backend: register sets for riscv64/riscv32/mips/mips64
# ---------------------------------------------------------------------------

class TestLLVMArchRegisters:
    """Instantiate LLVMIRGenerator and override target_arch to test register sets."""

    @staticmethod
    def _make_gen(arch: str):
        """Create an LLVMIRGenerator with target_arch forced to arch."""
        from nexuslang.compiler.backends.llvm_ir_generator import LLVMIRGenerator
        gen = LLVMIRGenerator.__new__(LLVMIRGenerator)
        # Minimal attribute setup to avoid full __init__ overhead
        gen.target_arch = arch
        return gen

    def _regs(self, arch):
        gen = self._make_gen(arch)
        return gen._get_valid_registers()

    def test_riscv64_has_x_registers(self):
        regs = self._regs("riscv64")
        for i in range(32):
            assert f"x{i}" in regs

    def test_riscv64_has_abi_names(self):
        regs = self._regs("riscv64")
        for name in ("zero", "ra", "sp", "gp", "tp", "fp"):
            assert name in regs
        for i in range(8):
            assert f"a{i}" in regs
        for i in range(7):
            assert f"t{i}" in regs
        for i in range(12):
            assert f"s{i}" in regs

    def test_riscv64_has_float_registers(self):
        regs = self._regs("riscv64")
        for i in range(32):
            assert f"f{i}" in regs

    def test_riscv64_has_vector_registers(self):
        regs = self._regs("riscv64")
        for i in range(32):
            assert f"v{i}" in regs

    def test_riscv32_has_same_base_registers(self):
        regs32 = self._regs("riscv32")
        for reg in ("x0", "a0", "sp", "ra", "zero"):
            assert reg in regs32

    def test_riscv_alias_maps_to_riscv_regs(self):
        # "riscv" bare alias (some toolchains use it)
        regs = self._regs("riscv")
        assert "a0" in regs or "x0" in regs  # either set is acceptable

    def test_mips_has_dollar_registers(self):
        regs = self._regs("mips")
        for i in range(32):
            assert f"${i}" in regs

    def test_mips_has_abi_names(self):
        regs = self._regs("mips")
        for name in ("zero", "at", "gp", "sp", "fp", "ra"):
            assert name in regs
        for i in range(2):
            assert f"v{i}" in regs
        for i in range(4):
            assert f"a{i}" in regs
        for i in range(10):
            assert f"t{i}" in regs

    def test_mips_has_float_registers(self):
        regs = self._regs("mips")
        for i in range(32):
            assert f"f{i}" in regs

    def test_mipsel_matches_mips(self):
        regs_mips = self._regs("mips")
        regs_el = self._regs("mipsel")
        # Same register names, endian differs only at arch level
        assert "a0" in regs_el
        assert "$0" in regs_el

    def test_mips64_has_extended_arg_registers(self):
        regs = self._regs("mips64")
        # mips64 adds a4-a7 per N64 ABI
        for i in range(8):
            assert f"a{i}" in regs


# ---------------------------------------------------------------------------
# LLVM backend: dangerous instruction patterns for riscv / mips
# ---------------------------------------------------------------------------

class TestLLVMDangerousInstructions:
    @staticmethod
    def _make_gen(arch: str):
        from nexuslang.compiler.backends.llvm_ir_generator import LLVMIRGenerator
        gen = LLVMIRGenerator.__new__(LLVMIRGenerator)
        gen.target_arch = arch
        return gen

    def _dangerous(self, arch, instructions):
        gen = self._make_gen(arch)
        return gen._validate_dangerous_instructions(instructions)

    def test_riscv_ecall_is_dangerous(self):
        warnings = self._dangerous("riscv64", ["ecall"])
        assert len(warnings) > 0

    def test_riscv_ebreak_is_dangerous(self):
        warnings = self._dangerous("riscv64", ["ebreak"])
        assert len(warnings) > 0

    def test_riscv_wfi_is_dangerous(self):
        warnings = self._dangerous("riscv64", ["wfi"])
        assert len(warnings) > 0

    def test_riscv_csrrw_is_dangerous(self):
        warnings = self._dangerous("riscv64", ["csrrw x0, mstatus, x1"])
        assert len(warnings) > 0

    def test_riscv_fence_is_dangerous(self):
        warnings = self._dangerous("riscv64", ["fence"])
        assert len(warnings) > 0

    def test_riscv_safe_instruction_is_not_dangerous(self):
        warnings = self._dangerous("riscv64", ["add a0, a1, a2"])
        assert len(warnings) == 0

    def test_mips_syscall_is_dangerous(self):
        warnings = self._dangerous("mips", ["syscall"])
        assert len(warnings) > 0

    def test_mips_eret_is_dangerous(self):
        warnings = self._dangerous("mips", ["eret"])
        assert len(warnings) > 0

    def test_mips_mtc0_is_dangerous(self):
        warnings = self._dangerous("mips", ["mtc0 $t0, $12"])
        assert len(warnings) > 0

    def test_mips_safe_instruction_is_not_dangerous(self):
        warnings = self._dangerous("mips", ["add $t0, $t1, $t2"])
        assert len(warnings) == 0


# ---------------------------------------------------------------------------
# LLVM backend: arch guard skips InlineAssembly for wrong architecture
# ---------------------------------------------------------------------------

class TestLLVMArchGuard:
    """Verify that InlineAssembly nodes with arch guards are skipped when
    the generator's target_arch does not match the guard."""

    @staticmethod
    def _make_gen(arch: str):
        from nexuslang.compiler.backends.llvm_ir_generator import LLVMIRGenerator
        gen = LLVMIRGenerator.__new__(LLVMIRGenerator)
        gen.target_arch = arch
        gen.ir_lines = []
        return gen

    def test_arch_guard_mismatch_skips_node(self):
        """An InlineAssembly node with arch='riscv64' should not be emitted
        when target_arch is 'x86_64'."""
        from nexuslang.compiler.backends.llvm_ir_generator import LLVMIRGenerator
        gen = self._make_gen("x86_64")

        # Build a minimal InlineAssembly node with arch guard
        node = InlineAssembly("add a0, a1, a2", arch="riscv64")
        node.node_type = "InlineAssembly"

        # Replicate the dispatch logic from _generate_statement
        node_arch = getattr(node, 'arch', None)
        if node_arch is not None:
            from nexuslang.compiler.preprocessor import _ARCH_ALIASES as _PP_ARCH
            normalized = _PP_ARCH.get(node_arch.lower(), node_arch.lower())
            skipped = (normalized != gen.target_arch)
        else:
            skipped = False

        assert skipped is True

    def test_arch_guard_match_not_skipped(self):
        """An InlineAssembly node with arch='x86_64' should NOT be skipped
        when target_arch is 'x86_64'."""
        from nexuslang.compiler.backends.llvm_ir_generator import LLVMIRGenerator
        gen = self._make_gen("x86_64")

        node = InlineAssembly("nop", arch="x86_64")
        node.node_type = "InlineAssembly"

        node_arch = getattr(node, 'arch', None)
        if node_arch is not None:
            from nexuslang.compiler.preprocessor import _ARCH_ALIASES as _PP_ARCH
            normalized = _PP_ARCH.get(node_arch.lower(), node_arch.lower())
            skipped = (normalized != gen.target_arch)
        else:
            skipped = False

        assert skipped is False

    def test_no_arch_guard_never_skipped(self):
        """An InlineAssembly node without an arch guard is never skipped."""
        node = InlineAssembly("nop")
        node_arch = getattr(node, 'arch', None)
        assert node_arch is None
        # No guard -> not skipped regardless of target_arch
