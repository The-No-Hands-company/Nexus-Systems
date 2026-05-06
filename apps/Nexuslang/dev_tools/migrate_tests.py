#!/usr/bin/env python3
"""
Restructure tests/ into a professional subdirectory layout.

New structure:
  tests/
    conftest.py                        -- shared sys.path fixture
    unit/
      compiler/   interpreter/   type_system/   language/
      stdlib/     memory/        errors/        runtime/   systems/
    integration/
    tooling/
    fixtures/                          -- .nlpl test fixtures
    helpers/                           -- test utilities (not test files)
    runners/                           -- kept as-is
"""

import os
import re
import shutil
import sys
import textwrap
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TESTS = ROOT / "tests"
BACKUP = ROOT / "tests_backup"


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def mkdir(path: Path):
    path.mkdir(parents=True, exist_ok=True)
    init = path / "__init__.py"
    if not init.exists():
        init.write_text("")


def move(src_name: str, dst_rel: str):
    src = TESTS / src_name
    dst = TESTS / dst_rel
    dst.parent.mkdir(parents=True, exist_ok=True)
    if not (dst.parent / "__init__.py").exists():
        (dst.parent / "__init__.py").write_text("")
    if src.exists():
        shutil.move(str(src), str(dst))
        print(f"  mv  {src_name:<55}  ->  {dst_rel}")
    else:
        print(f"  MISSING: {src_name}")


def delete(name: str):
    p = TESTS / name
    if p.exists():
        p.unlink()
        print(f"  del {name}")
    else:
        print(f"  MISSING (delete): {name}")


# ---------------------------------------------------------------------------
# Conftest + pytest.ini
# ---------------------------------------------------------------------------

CONFTEST = '''\
import sys
from pathlib import Path

# Make the src/ package importable regardless of test subdirectory depth.
_SRC = str(Path(__file__).resolve().parent.parent / "src")
if _SRC not in sys.path:
    sys.path.insert(0, _SRC)
'''

PYTEST_INI = '''\
[pytest]
testpaths = tests
python_files = test_*.py
python_classes = Test*
python_functions = test_*
'''

print("\n=== pytest.ini + conftest.py ===")
(ROOT / "pytest.ini").write_text(PYTEST_INI)
print("  wrote pytest.ini")
(TESTS / "conftest.py").write_text(CONFTEST)
print("  wrote tests/conftest.py")


# ---------------------------------------------------------------------------
# Directory skeleton
# ---------------------------------------------------------------------------

print("\n=== directory skeleton ===")
for d in [
    "unit/compiler",
    "unit/interpreter",
    "unit/type_system",
    "unit/language",
    "unit/stdlib",
    "unit/memory",
    "unit/errors",
    "unit/runtime",
    "unit/systems",
    "integration",
    "tooling",
    "fixtures",
    "helpers",
]:
    mkdir(TESTS / d)
    print(f"  mkdir tests/{d}")


# ---------------------------------------------------------------------------
# Simple moves: unit/compiler
# ---------------------------------------------------------------------------

print("\n=== unit/compiler ===")
move("test_lexer.py",                  "unit/compiler/test_lexer.py")
move("test_parser.py",                 "unit/compiler/test_parser.py")
move("test_c_generation.py",           "unit/compiler/test_c_codegen.py")
move("test_docgen.py",                 "unit/compiler/test_docgen.py")
move("test_conditional_compilation.py","unit/compiler/test_conditional_compilation.py")

# f-strings: merge 3 → 1
print("\n=== unit/compiler (f-strings merge) ===")

FSTR_HDR = """\
\"\"\"
F-string tests: lexer tokens, AST nodes, and parser integration.
Merged from test_fstring_lexer.py, test_fstring_ast.py, test_fstring_parser.py.
\"\"\"

import sys
import os
import pytest

_REPO_ROOT = str((Path(__file__).resolve().parent.parent.parent.parent))
_SRC = os.path.join(_REPO_ROOT, "src")
if _SRC not in sys.path:
    sys.path.insert(0, _SRC)

"""

def _strip_header(text: str) -> str:
    """Remove the standard sys.path boilerplate and docstring from file text."""
    lines = text.splitlines(keepends=True)
    out = []
    in_docstring = False
    skip_blank_after_import = True
    i = 0
    # Strip leading docstring
    if lines and lines[0].startswith('"""'):
        while i < len(lines):
            if i == 0 and lines[i].startswith('"""'):
                i += 1
                while i < len(lines) and '"""' not in lines[i]:
                    i += 1
                i += 1  # skip closing """
                break
            i += 1
    # Skip sys.path boilerplate
    stripped = []
    for line in lines[i:]:
        if re.match(r'^import sys$', line.rstrip()):
            continue
        if re.match(r'^import os$', line.rstrip()):
            continue
        if re.match(r'^import tempfile$', line.rstrip()):
            pass  # keep tempfile, may be needed
        if re.match(r'^_REPO_ROOT\s*=', line):
            continue
        if re.match(r'^if os\.path\.join\(_REPO_ROOT', line):
            continue
        if re.match(r"^\s+sys\.path\.insert\(0", line):
            continue
        if re.match(r'^_SRC\s*=', line):
            continue
        if re.match(r'^if _SRC not in sys\.path', line):
            continue
        if re.match(r"^\s+sys\.path\.insert\(0, _SRC", line):
            continue
        stripped.append(line)
    # Remove leading blank lines
    while stripped and stripped[0].strip() == "":
        stripped.pop(0)
    return "".join(stripped)


fstr_parts = []
for fname in ["test_fstring_lexer.py", "test_fstring_ast.py", "test_fstring_parser.py"]:
    p = TESTS / fname
    if p.exists():
        content = _strip_header(p.read_text())
        fstr_parts.append(f"# --- from {fname} ---\n\n" + content)
        p.unlink()
        print(f"  consumed {fname}")

if fstr_parts:
    out_path = TESTS / "unit/compiler/test_fstrings.py"
    out_path.write_text(FSTR_HDR + "\n\n".join(fstr_parts))
    print(f"  wrote unit/compiler/test_fstrings.py")


# ---------------------------------------------------------------------------
# unit/interpreter
# ---------------------------------------------------------------------------

print("\n=== unit/interpreter ===")
move("test_interpreter.py",            "unit/interpreter/test_interpreter.py")


# ---------------------------------------------------------------------------
# unit/type_system
# ---------------------------------------------------------------------------

print("\n=== unit/type_system ===")
move("test_advanced_types.py",         "unit/type_system/test_type_system_features.py")
move("test_type_inference.py",         "unit/type_system/test_type_inference.py")
move("test_type_inference_simple.py",  "unit/type_system/test_type_inference_simple.py")
move("test_type_aliases.py",           "unit/type_system/test_type_aliases.py")
move("test_generics.py",               "unit/type_system/test_generics.py")
move("test_generic_constraints.py",    "unit/type_system/test_generic_constraints.py")
move("test_generic_inference.py",      "unit/type_system/test_generic_inference.py")
move("test_traits.py",                 "unit/type_system/test_traits.py")
move("test_variance.py",               "unit/type_system/test_variance.py")
move("test_bidirectional_inference.py","unit/type_system/test_bidirectional_inference.py")
move("test_pattern_type_inference.py", "unit/type_system/test_pattern_type_inference.py")


# ---------------------------------------------------------------------------
# unit/language
# ---------------------------------------------------------------------------

print("\n=== unit/language ===")
move("test_control_flow.py",           "unit/language/test_control_flow.py")
move("test_break_continue.py",         "unit/language/test_break_continue.py")
move("test_while_loops.py",            "unit/language/test_while_loops.py")
move("test_for_each.py",               "unit/language/test_for_each.py")
move("test_operators.py",              "unit/language/test_operators.py")
move("test_pattern_matching.py",       "unit/language/test_pattern_matching.py")


# ---------------------------------------------------------------------------
# unit/stdlib
# ---------------------------------------------------------------------------

print("\n=== unit/stdlib ===")
move("test_strings.py",                "unit/stdlib/test_strings.py")
move("test_collections.py",            "unit/stdlib/test_collections.py")
move("test_array_feature.py",          "unit/stdlib/test_arrays.py")
move("test_dictionaries.py",           "unit/stdlib/test_dictionaries.py")
move("test_indexing.py",               "unit/stdlib/test_indexing.py")
move("test_nested_indexing.py",        "unit/stdlib/test_nested_indexing.py")
move("test_stdlib.py",                 "unit/stdlib/test_stdlib.py")
move("test_stdlib_enhancements.py",    "unit/stdlib/test_stdlib_enhancements.py")
move("test_option_result.py",          "unit/stdlib/test_option_result.py")
move("test_math3d.py",                 "unit/stdlib/test_math3d.py")
move("test_iterators.py",              "unit/stdlib/test_iterators.py")
move("test_parallel_stdlib.py",        "unit/stdlib/test_parallel.py")


# ---------------------------------------------------------------------------
# unit/memory
# ---------------------------------------------------------------------------

print("\n=== unit/memory ===")
move("test_borrow_checker.py",                   "unit/memory/test_borrow_checker.py")
move("test_lifetime_checker.py",                 "unit/memory/test_lifetime_checker.py")
move("test_ffi_safety.py",                       "unit/memory/test_ffi_safety.py")
move("test_ffi_advanced.py",                     "unit/memory/test_ffi_interop.py")
move("test_allocator_syntax.py",                 "unit/memory/test_allocator_syntax.py")
move("test_allocator_and_parallel_syntax.py",    "unit/memory/test_allocator_and_parallel_syntax.py")


# ---------------------------------------------------------------------------
# unit/errors
# ---------------------------------------------------------------------------

print("\n=== unit/errors ===")
move("test_errors.py",                 "unit/errors/test_errors.py")
move("test_error_reporting.py",        "unit/errors/test_error_reporting.py")
move("test_error_propagation.py",      "unit/errors/test_error_propagation.py")
move("test_comprehensive_errors.py",   "unit/errors/test_comprehensive_errors.py")


# ---------------------------------------------------------------------------
# unit/runtime
# ---------------------------------------------------------------------------

print("\n=== unit/runtime ===")
move("test_async_runtime.py",          "unit/runtime/test_async.py")
move("test_promise.py",                "unit/runtime/test_promise.py")


# ---------------------------------------------------------------------------
# unit/systems
# ---------------------------------------------------------------------------

print("\n=== unit/systems ===")
move("test_cpu_control.py",            "unit/systems/test_cpu_control.py")
move("test_kernel_primitives.py",      "unit/systems/test_kernel_primitives.py")
move("test_drivers.py",                "unit/systems/test_drivers.py")
move("test_freestanding.py",           "unit/systems/test_freestanding.py")
move("test_platform_features.py",      "unit/systems/test_platform_features.py")
move("test_security.py",               "unit/systems/test_security.py")
move("test_performance.py",            "unit/systems/test_performance.py")


# ---------------------------------------------------------------------------
# integration/
# ---------------------------------------------------------------------------

print("\n=== integration ===")
move("test_integration.py",            "integration/test_integration.py")
move("test_phase4_advanced.py",        "integration/test_phase4_advanced.py")


# ---------------------------------------------------------------------------
# tooling/
# ---------------------------------------------------------------------------

print("\n=== tooling ===")
move("test_lsp_startup.py",            "tooling/test_lsp_startup.py")
move("test_lsp_integration_real.py",   "tooling/test_lsp_integration_real.py")
move("test_lsp_enhancements.py",       "tooling/test_lsp_enhancements.py")
move("test_lsp_document_features.py",  "tooling/test_lsp_document_features.py")
move("test_lsp_diagnostic_payload.py", "tooling/test_lsp_diagnostic_payload.py")
move("test_lsp_goto_definition.py",    "tooling/test_lsp_goto_definition.py")
move("test_lsp_smoke_diagnostics.py",  "tooling/test_lsp_smoke_diagnostics.py")
move("test_lsp_features_check.py",     "tooling/test_lsp_features_check.py")
move("test_build_system.py",           "tooling/test_build_system.py")
move("test_build_advanced.py",         "tooling/test_build_extended.py")
move("test_manifest.py",               "tooling/test_manifest.py")
move("test_package_manager.py",        "tooling/test_package_manager.py")
move("test_ide_and_profiling.py",      "tooling/test_ide_and_profiling.py")
move("test_cross_file_navigation.py",  "tooling/test_cross_file_navigation.py")
move("test_symbol_extraction.py",      "tooling/test_symbol_extraction.py")
move("test_workspace_index.py",        "tooling/test_workspace_index.py")


# ---------------------------------------------------------------------------
# helpers/ (test_utils.py is a helper module, not a test file)
# ---------------------------------------------------------------------------

print("\n=== helpers ===")
src_utils = TESTS / "test_utils.py"
if src_utils.exists():
    dst_utils = TESTS / "helpers/utils.py"
    shutil.move(str(src_utils), str(dst_utils))
    print("  mv  test_utils.py -> helpers/utils.py")


# ---------------------------------------------------------------------------
# fixtures/ (move .nlpl files)
# ---------------------------------------------------------------------------

print("\n=== fixtures ===")
for nxl_file in TESTS.glob("*.nxl"):
    dst = TESTS / "fixtures" / nxl_file.name
    shutil.move(str(nxl_file), str(dst))
    print(f"  mv  {nxl_file.name} -> fixtures/{nxl_file.name}")


# ---------------------------------------------------------------------------
# Delete stale files
# ---------------------------------------------------------------------------

print("\n=== cleanup ===")
delete("test_parser.py.old")


# ---------------------------------------------------------------------------
# Split test_session_features.py
# ---------------------------------------------------------------------------

print("\n=== splitting test_session_features.py ===")

SESSION_FILE = TESTS / "test_session_features.py"
if not SESSION_FILE.exists():
    print("  test_session_features.py not found, skipping split")
else:
    raw = SESSION_FILE.read_text()

    # Shared boilerplate for every generated file
    SPLIT_HDR_TPL = '''\
"""
{title}
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

'''

    def extract_classes(source: str, class_names: list) -> str:
        """Extract complete class blocks by name from source text."""
        lines = source.splitlines(keepends=True)
        # Build index: line_no -> class_name for top-level class defs
        class_starts = {}
        for i, line in enumerate(lines):
            m = re.match(r'^class (Test\w+)\b', line)
            if m:
                class_starts[i] = m.group(1)

        # All top-level class line numbers (sorted)
        all_class_lines = sorted(class_starts.keys())

        blocks = []
        for target in class_names:
            # Find where this class starts
            start_line = None
            for ln, cname in class_starts.items():
                if cname == target:
                    start_line = ln
                    break
            if start_line is None:
                print(f"    WARNING: class {target} not found")
                continue
            # Find where it ends (start of next top-level class or EOF)
            idx_in_all = all_class_lines.index(start_line)
            if idx_in_all + 1 < len(all_class_lines):
                end_line = all_class_lines[idx_in_all + 1]
            else:
                end_line = len(lines)
            block = "".join(lines[start_line:end_line]).rstrip() + "\n\n"
            blocks.append(block)
        return "".join(blocks)

    # Mapping: destination file -> (title, [class names])
    SPLIT_MAP = {
        "unit/compiler/test_optimizer.py": (
            "Optimizer, PGO, and optimization-level tests.",
            ["TestOptimizerPasses", "TestPGO", "TestOptimizationLevel"],
        ),
        "unit/runtime/test_jit.py": (
            "JIT compilation and type-feedback tests.",
            ["TestJIT", "TestTypeFeedback"],
        ),
        "tooling/test_analyzer.py": (
            "Linter / static-analyzer checks and control-flow checker tests.",
            ["TestLinterChecks", "TestControlFlowChecker"],
        ),
        "unit/stdlib/test_collections_advanced.py": (
            "Advanced collection types: BTreeMap, BTreeSet, LinkedList, VecDeque, heaps.",
            ["TestBTreeMap", "TestBTreeSet", "TestLinkedList", "TestVecDeque",
             "TestMinHeap", "TestMaxHeap"],
        ),
        "unit/stdlib/test_algorithms.py": (
            "Graph and string algorithm tests.",
            ["TestGraphAlgorithms", "TestStringAlgorithms"],
        ),
        "unit/stdlib/test_io.py": (
            "Buffered I/O tests: BufferedReader, BufferedWriter, Pipe, MemoryMappedFile.",
            ["TestBufferedIO"],
        ),
        "unit/compiler/test_test_framework.py": (
            "Native NexusLang test-framework: lexer tokens, AST nodes, parser, interpreter.",
            ["TestLexerTestTokens", "TestASTNodes",
             "TestParserTestFramework", "TestInterpreterTestFramework"],
        ),
        "unit/language/test_assertions.py": (
            "Assertion library tests: lexer, AST, parser, and interpreter integration.",
            ["TestAssertionLibraryLexer", "TestAssertionLibraryAST",
             "TestAssertionLibraryParser", "TestAssertionLibraryInterpreter"],
        ),
        "unit/language/test_contracts.py": (
            "Contract programming: require, ensure, guarantee, NLPLContractError.",
            ["TestContractProgramming"],
        ),
    }

    for dst_rel, (title, class_names) in SPLIT_MAP.items():
        body = extract_classes(raw, class_names)
        if not body.strip():
            print(f"  EMPTY split -> {dst_rel}, skipping")
            continue
        dst = TESTS / dst_rel
        dst.parent.mkdir(parents=True, exist_ok=True)
        if not (dst.parent / "__init__.py").exists():
            (dst.parent / "__init__.py").write_text("")
        hdr = SPLIT_HDR_TPL.format(title=title)
        dst.write_text(hdr + body)
        n_classes = len(class_names)
        print(f"  wrote {dst_rel}  ({n_classes} classes: {', '.join(class_names)})")

    SESSION_FILE.unlink()
    print("  deleted test_session_features.py")


# ---------------------------------------------------------------------------
# Final summary
# ---------------------------------------------------------------------------

print("\n=== done ===")
total_py = list(TESTS.rglob("test_*.py"))
print(f"  test files after migration: {len(total_py)}")
for p in sorted(total_py):
    rel = p.relative_to(TESTS)
    print(f"    {rel}")
