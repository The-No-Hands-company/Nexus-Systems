"""
Dead Code Provider
==================

Detects dead / unreachable code patterns in NexusLang documents and returns
LSP Diagnostic objects that editors can display as warnings.

Three categories are detected:

1. **Unreachable statements** — any statement that follows a `return`,
   `break`, or `continue` at the same nesting depth inside a block.

2. **Empty function bodies** — functions whose body contains nothing but
   a comment or nothing at all (before the closing `end`).

3. **Constant-condition branches** — `if true` / `if false` / `if 1`
   constructs where the branch outcome is statically obvious.

Each diagnostic carries a ``data.fixes`` list so the editor can offer a
"Remove dead code" quick-fix via the CodeActionsProvider.
"""

from __future__ import annotations

import re
from typing import Dict, List, Optional


# LSP DiagnosticSeverity
_HINT = 4       # Hint  — lightweight, barely visible
_INFORMATION = 3
_WARNING = 2


class DeadCodeProvider:
    """
    Detects dead code in a single NexusLang document.

    Usage::

        provider = DeadCodeProvider(server)
        diags = provider.get_diagnostics(uri, text)
        # Returns [] or a list of LSP Diagnostic dicts.
    """

    def __init__(self, server):
        self.server = server

    # ------------------------------------------------------------------
    # Public API
    # ------------------------------------------------------------------

    def get_diagnostics(self, uri: str, text: str) -> List[Dict]:
        """Return dead-code diagnostics for the document."""
        if not text.strip():
            return []

        lines = text.split("\n")
        diags: List[Dict] = []

        diags.extend(self._find_unreachable_statements(lines))
        diags.extend(self._find_empty_function_bodies(lines))
        diags.extend(self._find_constant_conditions(lines))

        return diags

    # ------------------------------------------------------------------
    # 1. Unreachable statements
    # ------------------------------------------------------------------

    def _find_unreachable_statements(self, lines: List[str]) -> List[Dict]:
        """
        Find statements that appear after an unconditional return / break /
        continue inside the same logical block.

        The algorithm uses indentation-based scope tracking: when we see a
        return/break/continue, we record the indent level.  Every subsequent
        non-empty, non-comment line at the *same or lesser* indent that is
        NOT a block-closing keyword is flagged as unreachable.  The "dead
        zone" is reset when we leave the scope (encounter `end`, `else`,
        `elif`, or a dedented line).
        """
        diags: List[Dict] = []
        dead_indent: Optional[int] = None  # indent level that triggered death

        for line_num, line in enumerate(lines):
            stripped = line.strip()
            indent = len(line) - len(line.lstrip())

            if not stripped or stripped.startswith("#"):
                continue

            # --- Scope exit: reset dead zone ---
            if dead_indent is not None:
                # A block-closer at any indent resets the zone
                if re.match(r"^(end|else|else if|elif)\b", stripped, re.IGNORECASE):
                    dead_indent = None
                    continue
                # A dedent past the triggering level (new sibling or parent)
                if indent < dead_indent:
                    dead_indent = None
                    # Don't skip — this line is live code at a higher scope

            # --- Trigger: unconditional exit ---
            if dead_indent is None and re.match(
                r"^(return|break|continue)\b", stripped, re.IGNORECASE
            ):
                dead_indent = indent
                continue

            # --- Inside dead zone ---
            if dead_indent is not None and indent >= dead_indent:
                diags.append(
                    _make_diag(
                        line=line_num,
                        start_char=indent,
                        end_char=len(line.rstrip()),
                        message="Unreachable code: this statement can never execute",
                        code="dead-unreachable",
                        severity=_WARNING,
                        fixes=["Remove unreachable code"],
                    )
                )

        return diags

    # ------------------------------------------------------------------
    # 2. Empty function bodies
    # ------------------------------------------------------------------

    def _find_empty_function_bodies(self, lines: List[str]) -> List[Dict]:
        """
        Flag function definitions whose body is empty (contains no
        executable statement between the header and its closing `end`).
        """
        diags: List[Dict] = []
        i = 0
        while i < len(lines):
            line = lines[i]
            stripped = line.strip()
            func_m = re.match(
                r"^(function|method)\s+(\w+)", stripped, re.IGNORECASE
            )
            if func_m:
                func_name = func_m.group(2)
                func_indent = len(line) - len(line.lstrip())
                body_start = i + 1
                j = body_start
                has_executable = False

                while j < len(lines):
                    body_line = lines[j].strip()
                    if not body_line or body_line.startswith("#"):
                        j += 1
                        continue
                    # Reached the closing `end` at the same indent level
                    if body_line.lower().startswith("end") and (
                        len(lines[j]) - len(lines[j].lstrip()) <= func_indent
                    ):
                        break
                    has_executable = True
                    break

                if not has_executable:
                    diags.append(
                        _make_diag(
                            line=i,
                            start_char=len(line) - len(line.lstrip()),
                            end_char=len(line.rstrip()),
                            message=f"Function '{func_name}' has an empty body",
                            code="dead-empty-function",
                            severity=_HINT,
                            fixes=["Add a return statement", "Remove empty function"],
                        )
                    )
            i += 1
        return diags

    # ------------------------------------------------------------------
    # 3. Constant conditions
    # ------------------------------------------------------------------

    def _find_constant_conditions(self, lines: List[str]) -> List[Dict]:
        """
        Flag `if true` / `if false` / `if 0` / `if 1` where the branch
        outcome is statically known.
        """
        diags: List[Dict] = []

        # Matches: if <literal_bool_or_int>  (with no further operands)
        _CONST_COND = re.compile(
            r"^\s*if\s+(true|false|0|1)\s*$", re.IGNORECASE
        )

        for line_num, line in enumerate(lines):
            m = _CONST_COND.match(line)
            if m:
                value = m.group(1).lower()
                always_taken = value in ("true", "1")
                note = "always taken" if always_taken else "never taken"
                diags.append(
                    _make_diag(
                        line=line_num,
                        start_char=len(line) - len(line.lstrip()),
                        end_char=len(line.rstrip()),
                        message=f"Constant condition: branch is {note}",
                        code="dead-constant-condition",
                        severity=_WARNING,
                        fixes=["Simplify constant condition"],
                    )
                )
        return diags


# ---------------------------------------------------------------------------
# Module-level helpers
# ---------------------------------------------------------------------------


def _make_diag(
    line: int,
    start_char: int,
    end_char: int,
    message: str,
    code: str,
    severity: int,
    fixes: List[str],
) -> Dict:
    return {
        "range": {
            "start": {"line": line, "character": start_char},
            "end": {"line": line, "character": end_char},
        },
        "severity": severity,
        "code": code,
        "source": "nlpl-dead-code",
        "message": message,
        "data": {"fixes": fixes},
    }
