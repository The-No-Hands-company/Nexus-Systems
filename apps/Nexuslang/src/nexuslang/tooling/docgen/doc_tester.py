"""Documentation test runner for NexusLang.

Finds all ``@example`` blocks in documented items and executes them through the
NLPL interpreter.  Each block must run without raising an exception to pass.

Usage from a project root::

    from nexuslang.tooling.docgen.doc_tester import run_doc_tests, DocTestResult
    from nexuslang.tooling.docgen.extractor import extract_from_directory

    items_by_file = extract_from_directory("src/")
    all_items = [item for lst in items_by_file.values() for item in lst]
    result = run_doc_tests(all_items)
    result.print_summary()
    if not result.all_passed:
        sys.exit(1)
"""

from __future__ import annotations

import io
import textwrap
import traceback
from dataclasses import dataclass, field
from typing import List, Optional, Tuple

from .extractor import DocItem


# ── result types ─────────────────────────────────────────────────────────────

@dataclass
class DocTestFailure:
    item_name: str
    source_file: str
    example_index: int    # 0-based
    code: str
    error: str            # exception type + message

    @property
    def label(self) -> str:
        return f"{self.source_file}::{self.item_name}[example {self.example_index + 1}]"


@dataclass
class DocTestResult:
    passed: int = 0
    failed: int = 0
    skipped: int = 0
    failures: List[DocTestFailure] = field(default_factory=list)

    @property
    def total(self) -> int:
        return self.passed + self.failed + self.skipped

    @property
    def all_passed(self) -> bool:
        return self.failed == 0

    def print_summary(self, verbose: bool = False) -> None:
        """Print a human-readable summary to stdout."""
        if verbose or self.failures:
            for f in self.failures:
                print(f"FAIL  {f.label}")
                print(textwrap.indent(f"Code:\n{f.code}", "  "))
                print(textwrap.indent(f"Error: {f.error}", "  "))
                print()

        status = "OK" if self.all_passed else "FAILED"
        parts = [f"{self.passed} passed"]
        if self.failed:
            parts.append(f"{self.failed} failed")
        if self.skipped:
            parts.append(f"{self.skipped} skipped")
        print(f"doc tests: {status} ({', '.join(parts)})")


# ── runner ───────────────────────────────────────────────────────────────────

def _run_single_example(code: str) -> Optional[str]:
    """Execute a single NexusLang code snippet.

    Returns:
        ``None`` on success, or an error string on failure.
    """
    # Import lazily to avoid slow startup cost when the module is merely imported.
    try:
        from nexuslang.interpreter.interpreter import Interpreter
        from nexuslang.parser.lexer import Lexer
        from nexuslang.parser.parser import Parser
        from nexuslang.runtime.runtime import Runtime
        from nexuslang.stdlib import register_stdlib
    except ImportError as exc:
        return f"ImportError: {exc}"

    try:
        tokens = Lexer(code).tokenize()
        parser = Parser(tokens)
        program = parser.parse()

        runtime = Runtime()
        register_stdlib(runtime)
        interpreter = Interpreter(runtime)

        # Suppress stdout from the example so it doesn't clutter test output.
        import sys
        old_stdout = sys.stdout
        sys.stdout = io.StringIO()
        try:
            interpreter.interpret(program)
        finally:
            sys.stdout = old_stdout

    except Exception as exc:
        return f"{type(exc).__name__}: {exc}"

    return None


def run_doc_tests(
    items: List[DocItem],
    verbose: bool = False,
    stop_on_first_failure: bool = False,
) -> DocTestResult:
    """Execute all ``@example`` blocks in *items*.

    Args:
        items:                  List of :class:`~.extractor.DocItem` instances.
        verbose:                Print each test as it runs.
        stop_on_first_failure:  Abort iteration after the first failure.

    Returns:
        A :class:`DocTestResult` summarising pass/fail counts.
    """
    result = DocTestResult()

    for item in items:
        if not item.examples:
            continue

        for idx, code in enumerate(item.examples):
            code = code.strip()
            if not code:
                result.skipped += 1
                continue

            if verbose:
                label = f"{item.source_file}::{item.name}[example {idx + 1}]"
                print(f"running  {label} ... ", end="", flush=True)

            error = _run_single_example(code)
            if error is None:
                result.passed += 1
                if verbose:
                    print("ok")
            else:
                result.failed += 1
                failure = DocTestFailure(
                    item_name=item.name,
                    source_file=item.source_file,
                    example_index=idx,
                    code=code,
                    error=error,
                )
                result.failures.append(failure)
                if verbose:
                    print(f"FAIL\n  {error}")
                if stop_on_first_failure:
                    return result

    return result
