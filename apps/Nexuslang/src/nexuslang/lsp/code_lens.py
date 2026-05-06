"""
Code Lens Provider
==================

Provides code lens annotations above function, class, struct, and test
definitions.  Each lens shows actionable inline metadata:

  - Functions / methods : "N references"
  - Classes / structs   : "N references"
  - test / describe blocks : "Run test" / "Run N tests"

Code lenses are computed from lightweight regex scanning of the document
text so they are fast and do not require a full parse.  Reference counts
are derived by counting non-definition occurrences of the symbol name in
the current document.
"""

from __future__ import annotations

import re
from typing import Dict, List, Optional


# LSP SymbolKind values referenced in code lens data
_KIND_FUNCTION = 12
_KIND_CLASS = 5
_KIND_STRUCT = 23


class CodeLensProvider:
    """
    Provides code lens items for NexusLang documents.

    Usage:
        provider = CodeLensProvider(server)
        lenses = provider.get_code_lenses(uri, text)
        # Each lens is fully resolved (command included)
    """

    def __init__(self, server):
        self.server = server

    # ------------------------------------------------------------------
    # Public API
    # ------------------------------------------------------------------

    def get_code_lenses(self, uri: str, text: str) -> List[Dict]:
        """
        Return all code lenses for the document.

        Args:
            uri:  Document URI (used in command arguments so the editor
                  knows which file to act on).
            text: Full document text.

        Returns:
            List of LSP CodeLens objects, each with range and command
            already filled in.
        """
        lenses: List[Dict] = []
        lines = text.split("\n")

        for line_num, line in enumerate(lines):
            stripped = line.strip()
            if not stripped:
                continue

            # Function definition -------------------------------------------
            m = re.match(r"^function\s+(\w+)", stripped, re.IGNORECASE)
            if m:
                name = m.group(1)
                col = _find_col(line, m.group(0))
                ref_count = self._count_references(text, name, line_num)
                lenses.append(
                    _make_lens(
                        line_num, col, col + len(m.group(0)),
                        title=_refs_label(ref_count),
                        command="nexuslang.findReferences",
                        args=[uri, {"line": line_num, "character": col}],
                    )
                )
                continue

            # Class definition ----------------------------------------------
            m = re.match(r"^class\s+(\w+)", stripped, re.IGNORECASE)
            if m:
                name = m.group(1)
                m2 = re.search(r"\bclass\s+\w+", line, re.IGNORECASE)
                col = m2.start() if m2 else 0
                ref_count = self._count_references(text, name, line_num)
                lenses.append(
                    _make_lens(
                        line_num, col, col + len(m.group(0)),
                        title=_refs_label(ref_count),
                        command="nexuslang.findReferences",
                        args=[uri, {"line": line_num, "character": col}],
                    )
                )
                continue

            # Struct definition ---------------------------------------------
            m = re.match(r"^struct\s+(\w+)", stripped, re.IGNORECASE)
            if m:
                name = m.group(1)
                m2 = re.search(r"\bstruct\s+\w+", line, re.IGNORECASE)
                col = m2.start() if m2 else 0
                ref_count = self._count_references(text, name, line_num)
                lenses.append(
                    _make_lens(
                        line_num, col, col + len(m.group(0)),
                        title=_refs_label(ref_count),
                        command="nexuslang.findReferences",
                        args=[uri, {"line": line_num, "character": col}],
                    )
                )
                continue

            # Enum definition -----------------------------------------------
            m = re.match(r"^enum\s+(\w+)", stripped, re.IGNORECASE)
            if m:
                name = m.group(1)
                m2 = re.search(r"\benum\s+\w+", line, re.IGNORECASE)
                col = m2.start() if m2 else 0
                ref_count = self._count_references(text, name, line_num)
                lenses.append(
                    _make_lens(
                        line_num, col, col + len(m.group(0)),
                        title=_refs_label(ref_count),
                        command="nexuslang.findReferences",
                        args=[uri, {"line": line_num, "character": col}],
                    )
                )
                continue

            # describe block ------------------------------------------------
            m = re.match(r'^describe\s+"([^"]+)"', stripped, re.IGNORECASE)
            if m:
                suite_name = m.group(1)
                m2 = re.search(r"\bdescribe\b", line, re.IGNORECASE)
                col = m2.start() if m2 else 0
                test_count = self._count_tests_in_block(lines, line_num)
                lenses.append(
                    _make_lens(
                        line_num, col, col + len(m.group(0)),
                        title=f"Run {test_count} test{'s' if test_count != 1 else ''}",
                        command="nexuslang.runTestSuite",
                        args=[uri, suite_name],
                    )
                )
                continue

            # Standalone test block -----------------------------------------
            m = re.match(r'^test\s+"([^"]+)"', stripped, re.IGNORECASE)
            if m:
                test_name = m.group(1)
                m2 = re.search(r"\btest\b", line, re.IGNORECASE)
                col = m2.start() if m2 else 0
                lenses.append(
                    _make_lens(
                        line_num, col, col + len(m.group(0)),
                        title="Run test",
                        command="nexuslang.runTest",
                        args=[uri, test_name],
                    )
                )
                continue

            # it block inside describe --------------------------------------
            m = re.match(r'^it\s+"([^"]+)"', stripped, re.IGNORECASE)
            if m:
                it_name = m.group(1)
                m2 = re.search(r"\bit\b", line, re.IGNORECASE)
                col = m2.start() if m2 else 0
                lenses.append(
                    _make_lens(
                        line_num, col, col + len(m.group(0)),
                        title="Run",
                        command="nexuslang.runTest",
                        args=[uri, it_name],
                    )
                )

        return lenses

    def resolve_code_lens(self, lens: Dict) -> Dict:
        """
        Resolve a previously returned code lens.

        NexusLang code lenses are always fully resolved, so this is a pass-through.
        """
        return lens

    # ------------------------------------------------------------------
    # Internal helpers
    # ------------------------------------------------------------------

    def _count_references(self, text: str, name: str, definition_line: int) -> int:
        """
        Count non-definition lines that contain `name` as a whole word.

        Args:
            text:            Full document text.
            name:            Symbol name to count.
            definition_line: Line index of the definition (skipped).

        Returns:
            Number of reference occurrences.
        """
        pattern = re.compile(rf"\b{re.escape(name)}\b")
        count = 0
        for i, line in enumerate(text.split("\n")):
            if i == definition_line:
                continue
            count += len(pattern.findall(line))
        return count

    def _count_tests_in_block(self, lines: List[str], start_line: int) -> int:
        """
        Count `it` or `test` entries inside a describe block.

        Scans forward from start_line + 1 until a bare `end` is found.

        Args:
            lines:      All document lines.
            start_line: Line index of the `describe` header.

        Returns:
            Count of test cases inside the block.
        """
        depth = 0
        count = 0
        for line in lines[start_line + 1 :]:
            stripped = line.strip()
            if not stripped:
                continue
            # Track nesting for nested describe / function blocks
            if re.match(
                r"^(describe|function|class|struct|if|while|for|match)\b",
                stripped,
                re.IGNORECASE,
            ):
                depth += 1
            elif re.match(r"^end\b", stripped, re.IGNORECASE):
                if depth == 0:
                    break
                depth -= 1
            elif re.match(r'^(it|test)\s+"', stripped, re.IGNORECASE):
                # Count this test entry; also push depth so its own 'end'
                # is consumed rather than terminating the describe block.
                if depth == 0:
                    count += 1
                depth += 1
        return count


# ------------------------------------------------------------------
# Module-level helpers
# ------------------------------------------------------------------


def _find_col(line: str, substring: str) -> int:
    """Return the start column of `substring` in `line`, or 0 if not found."""
    idx = line.find(substring)
    return idx if idx >= 0 else 0


def _refs_label(count: int) -> str:
    return f"{count} reference{'s' if count != 1 else ''}"


def _make_lens(
    line: int,
    start_char: int,
    end_char: int,
    title: str,
    command: str,
    args: list,
) -> Dict:
    """Build a fully-resolved LSP CodeLens object."""
    return {
        "range": {
            "start": {"line": line, "character": start_char},
            "end": {"line": line, "character": end_char},
        },
        "command": {
            "title": title,
            "command": command,
            "arguments": args,
        },
    }
