"""
Auto-Fix Engine
===============

Applies structured source-code fixes for static analysis issues.

Architecture
------------

A ``TextEdit`` describes a single replacement at a precise source range.
A ``FixSuggestion`` bundles one or more ``TextEdit`` objects with the
``Issue`` they resolve and a human-readable description.

``AutoFixer`` has two public entry points:

* ``generate_fixes(issues, source_lines)``
  Inspect each ``Issue`` and try to produce a structured ``FixSuggestion``
  using the built-in fix generators registered in ``_FIX_GENERATORS``.

* ``apply(source, suggestions)``
  Apply a list of ``FixSuggestion`` objects to source text, returning a
  ``FixResult`` that contains the modified source, counts, and a human-
  readable change log.

* ``preview(source, suggestions)``
  Like ``apply`` but returns the modified source without writing any files.

Built-in fix generators
-----------------------

Generator functions are registered in ``_FIX_GENERATORS`` keyed by issue
code string.  Each generator receives ``(issue, source_lines)`` and must
return either a ``FixSuggestion`` or ``None`` if no structured fix is
available.

Currently implemented generators:

  W-STYLE-TRAIL   Remove trailing whitespace from a line.
  W-STYLE-EOF     Ensure file ends with a single newline.
  W-DEAD-ASSIGN   Delete a provably dead assignment statement.
  W-UNINIT-USE   Insert a default initializer before first use.
  P003            Replace string concatenation with list-join pattern.
  P005            Cache ``len()`` call before a loop.
  S-INJECT        Wrap tainted string in a sanitize call.
"""
from __future__ import annotations

from dataclasses import dataclass, field
from typing import Callable, Dict, List, Optional, Tuple

from .report import Issue, Severity, Category, SourceLocation

__all__ = [
    "TextEdit",
    "FixSuggestion",
    "FixResult",
    "AutoFixer",
]


# ---------------------------------------------------------------------------
# Data structures
# ---------------------------------------------------------------------------

@dataclass
class TextEdit:
    """
    A single source-text replacement.

    The range ``(line, col) .. (end_line, end_col)`` is 1-based and
    *inclusive* on the start boundary, *exclusive* on the end boundary
    (matching the LSP convention).

    Setting ``new_text`` to the empty string deletes the range.
    Setting the range start == end inserts text without deleting anything.
    """
    line: int
    col: int
    end_line: int
    end_col: int
    new_text: str

    def __post_init__(self) -> None:
        if self.line <= 0 or self.col <= 0:
            raise ValueError(
                f"TextEdit line/col are 1-based; got line={self.line} col={self.col}"
            )
        if (self.end_line, self.end_col) < (self.line, self.col):
            raise ValueError(
                f"TextEdit end ({self.end_line},{self.end_col}) is before "
                f"start ({self.line},{self.col})"
            )


@dataclass
class FixSuggestion:
    """
    A structured fix for a single ``Issue``.

    ``edits`` is an ordered list of non-overlapping ``TextEdit`` objects.
    They are applied in reverse document order (last edit first) so that
    earlier line numbers stay valid as the source changes.
    """
    issue: Issue
    description: str
    edits: List[TextEdit] = field(default_factory=list)

    def is_applicable(self) -> bool:
        """Return True if there is at least one edit to apply."""
        return len(self.edits) > 0


@dataclass
class FixResult:
    """Result of applying a batch of ``FixSuggestion`` objects."""
    new_source: str
    applied: int
    skipped: int
    changes: List[str] = field(default_factory=list)

    @property
    def total(self) -> int:
        return self.applied + self.skipped


# ---------------------------------------------------------------------------
# Internal helpers
# ---------------------------------------------------------------------------

def _line_range_of(lines: List[str], line_number: int) -> Tuple[int, int]:
    """Return (col_start=1, col_end) of the text on ``line_number`` (1-based)."""
    idx = line_number - 1
    if 0 <= idx < len(lines):
        return 1, len(lines[idx]) + 1  # end_col is exclusive
    return 1, 1


def _apply_single_edit(lines: List[str], edit: TextEdit) -> List[str]:
    """
    Apply one ``TextEdit`` to a list of lines and return the modified list.

    Lines are 1-based.  If the edit spans multiple lines, all affected
    lines are merged before replacement.
    """
    # Convert to 0-based indices for list access
    s_line = edit.line - 1
    e_line = edit.end_line - 1
    s_col = edit.col - 1
    e_col = edit.end_col - 1

    # Clamp to valid ranges
    if s_line < 0 or s_line >= len(lines):
        return lines  # Out-of-range: skip

    # Build prefix (before the edit) and suffix (after the edit)
    start_text = lines[s_line][:s_col]
    if e_line < len(lines):
        end_text = lines[e_line][e_col:]
    else:
        end_text = ""

    replacement = start_text + edit.new_text + end_text

    new_lines = lines[:s_line] + replacement.split("\n") + lines[e_line + 1:]
    return new_lines


def _sort_edits_reverse(edits: List[TextEdit]) -> List[TextEdit]:
    """Sort edits in reverse document order so earlier edits stay valid."""
    return sorted(edits, key=lambda e: (e.line, e.col), reverse=True)


# ---------------------------------------------------------------------------
# Built-in fix generators
# ---------------------------------------------------------------------------

# Type alias for generator callables
_Generator = Callable[[Issue, List[str]], Optional[FixSuggestion]]

_FIX_GENERATORS: Dict[str, _Generator] = {}


def _register(code: str):
    """Decorator to register a generator for a given issue code."""
    def decorator(fn: _Generator) -> _Generator:
        _FIX_GENERATORS[code] = fn
        return fn
    return decorator


@_register("W-STYLE-TRAIL")
def _fix_trailing_whitespace(issue: Issue, lines: List[str]) -> Optional[FixSuggestion]:
    """Strip trailing whitespace from the offending line."""
    ln = issue.location.line
    if ln < 1 or ln > len(lines):
        return None
    original = lines[ln - 1]
    stripped = original.rstrip()
    if stripped == original:
        return None  # Nothing to remove
    _, end_col = _line_range_of(lines, ln)
    edit = TextEdit(
        line=ln, col=len(stripped) + 1,
        end_line=ln, end_col=end_col,
        new_text="",
    )
    return FixSuggestion(
        issue=issue,
        description=f"Remove trailing whitespace on line {ln}",
        edits=[edit],
    )


@_register("W-STYLE-EOF")
def _fix_missing_newline(issue: Issue, lines: List[str]) -> Optional[FixSuggestion]:
    """Ensure the file ends with exactly one newline."""
    if not lines:
        return None
    last_line = len(lines)
    _, end_col = _line_range_of(lines, last_line)
    edit = TextEdit(
        line=last_line, col=end_col,
        end_line=last_line, end_col=end_col,
        new_text="\n",
    )
    return FixSuggestion(
        issue=issue,
        description="Add missing newline at end of file",
        edits=[edit],
    )


@_register("W-DEAD-ASSIGN")
def _fix_dead_assignment(issue: Issue, lines: List[str]) -> Optional[FixSuggestion]:
    """Delete a dead assignment line entirely."""
    ln = issue.location.line
    if ln < 1 or ln > len(lines):
        return None
    # Delete the entire line (including the newline character that joins it)
    if ln < len(lines):
        edit = TextEdit(
            line=ln, col=1,
            end_line=ln + 1, end_col=1,
            new_text="",
        )
    else:
        # Last line: just blank it out
        _, end_col = _line_range_of(lines, ln)
        edit = TextEdit(line=ln, col=1, end_line=ln, end_col=end_col, new_text="")
    return FixSuggestion(
        issue=issue,
        description=f"Remove dead assignment on line {ln}",
        edits=[edit],
    )


@_register("W-UNINIT-USE")
def _fix_uninitialized(issue: Issue, lines: List[str]) -> Optional[FixSuggestion]:
    """
    Insert a default-zero initializer for an uninitialized variable.
    The insertion appears on the line *before* the reported location.
    """
    ln = issue.location.line
    if ln < 1:
        return None
    # Try to extract the variable name from the issue message
    # Expected format: "Variable '...' may be used before initialization"
    var_name = _extract_quoted_name(issue.message) or "variable"
    # Insert new line before the offending line
    insert_line = max(1, ln)
    insert_col, _ = 1, 1
    # Detect indentation of the offending line to match it
    if ln <= len(lines):
        indent = len(lines[ln - 1]) - len(lines[ln - 1].lstrip())
        indent_str = " " * indent
    else:
        indent_str = ""
    new_stmt = f"{indent_str}set {var_name} to 0\n"
    edit = TextEdit(
        line=insert_line, col=insert_col,
        end_line=insert_line, end_col=insert_col,
        new_text=new_stmt,
    )
    return FixSuggestion(
        issue=issue,
        description=f"Initialize '{var_name}' to 0 before use",
        edits=[edit],
    )


@_register("P003")
def _fix_string_concat_in_loop(issue: Issue, lines: List[str]) -> Optional[FixSuggestion]:
    """
    Replace string concatenation pattern with list-accumulation + join.

    This is a *guidance fix*: inserts a comment near the offending line
    showing the recommended refactoring rather than making a potentially
    incorrect automated change.
    """
    ln = issue.location.line
    if ln < 1 or ln > len(lines):
        return None
    if ln <= len(lines):
        indent = len(lines[ln - 1]) - len(lines[ln - 1].lstrip())
        indent_str = " " * indent
    else:
        indent_str = ""
    comment = (
        f"{indent_str}# PERF: replace string concatenation with list + join:\n"
        f"{indent_str}# set parts to []\n"
        f"{indent_str}# ... inside loop: add item to parts ...\n"
        f"{indent_str}# set result to join parts with ''\n"
    )
    edit = TextEdit(
        line=ln, col=1, end_line=ln, end_col=1,
        new_text=comment,
    )
    return FixSuggestion(
        issue=issue,
        description="Insert refactoring guidance for string concatenation in loop",
        edits=[edit],
    )


@_register("P005")
def _fix_cache_len_call(issue: Issue, lines: List[str]) -> Optional[FixSuggestion]:
    """
    Cache the ``len()`` call into a local variable before the loop.
    Heuristic: insert ``set _len_<name> to len(...)`` one line above the loop.
    """
    ln = issue.location.line
    if ln < 1 or ln > len(lines):
        return None
    if ln <= len(lines):
        indent = len(lines[ln - 1]) - len(lines[ln - 1].lstrip())
        indent_str = " " * indent
    else:
        indent_str = ""
    comment = (
        f"{indent_str}# OPT: cache collection length before this loop\n"
        f"{indent_str}# set _total to len(your_collection)\n"
    )
    edit = TextEdit(
        line=ln, col=1, end_line=ln, end_col=1,
        new_text=comment,
    )
    return FixSuggestion(
        issue=issue,
        description="Insert len-caching guidance before loop",
        edits=[edit],
    )


@_register("S-INJECT")
def _fix_injection(issue: Issue, lines: List[str]) -> Optional[FixSuggestion]:
    """
    Wrap the tainted value in a ``sanitize()`` call.
    This is a guidance fix (comment) to avoid incorrect automated changes
    in security-critical code.
    """
    ln = issue.location.line
    if ln < 1 or ln > len(lines):
        return None
    if ln <= len(lines):
        indent = len(lines[ln - 1]) - len(lines[ln - 1].lstrip())
        indent_str = " " * indent
    else:
        indent_str = ""
    comment = (
        f"{indent_str}# SECURITY: sanitize user input before use:\n"
        f"{indent_str}# set safe_value to sanitize(user_input)\n"
    )
    edit = TextEdit(
        line=ln, col=1, end_line=ln, end_col=1,
        new_text=comment,
    )
    return FixSuggestion(
        issue=issue,
        description="Insert sanitization guidance for tainted input",
        edits=[edit],
    )


# ---------------------------------------------------------------------------
# Helper: extract variable name from message
# ---------------------------------------------------------------------------

def _extract_quoted_name(message: str) -> Optional[str]:
    """Extract the first single-quoted name from a message string."""
    import re
    m = re.search(r"'([^']+)'", message)
    return m.group(1) if m else None


# ---------------------------------------------------------------------------
# AutoFixer
# ---------------------------------------------------------------------------

class AutoFixer:
    """
    Generates and applies structured fixes for static analysis issues.

    Usage::

        fixer = AutoFixer()

        # Get structured suggestions
        suggestions = fixer.generate_fixes(report.issues, source.splitlines())

        # Preview changes (dry-run)
        preview_src = fixer.preview(source, suggestions)

        # Actually apply
        result = fixer.apply(source, suggestions)
        print(f"Applied {result.applied} fixes, skipped {result.skipped}")
    """

    def __init__(self, extra_generators: Optional[Dict[str, _Generator]] = None):
        """
        Args:
            extra_generators: Optional additional issue-code -> generator
                              mappings to extend or override the built-ins.
        """
        self._generators: Dict[str, _Generator] = dict(_FIX_GENERATORS)
        if extra_generators:
            self._generators.update(extra_generators)

    # ------------------------------------------------------------------
    # Public API
    # ------------------------------------------------------------------

    def generate_fixes(
        self,
        issues: List[Issue],
        source_lines: List[str],
    ) -> List[FixSuggestion]:
        """
        Iterate over ``issues`` and produce structured ``FixSuggestion``
        objects for every issue whose code has a registered generator.

        Issues without a structured generator are silently skipped (only the
        human-readable ``issue.fix`` hint in the ``Issue`` object will be
        available to the user in that case).

        Args:
            issues: Issues from an ``AnalysisReport``.
            source_lines: Source split by line (``source.splitlines()``).

        Returns:
            List of ``FixSuggestion`` objects, one per actionable issue.
        """
        suggestions: List[FixSuggestion] = []
        for issue in issues:
            generator = self._generators.get(issue.code)
            if generator is None:
                continue
            try:
                suggestion = generator(issue, source_lines)
            except Exception:
                suggestion = None
            if suggestion is not None and suggestion.is_applicable():
                suggestions.append(suggestion)
        return suggestions

    def preview(
        self,
        source: str,
        suggestions: List[FixSuggestion],
    ) -> str:
        """
        Return the modified source without changing any files.

        Args:
            source: Original source text.
            suggestions: Suggestions from ``generate_fixes()``.

        Returns:
            Modified source as a string.
        """
        return self._apply_edits(source, suggestions)

    def apply(
        self,
        source: str,
        suggestions: List[FixSuggestion],
    ) -> FixResult:
        """
        Apply all applicable suggestions and return the result.

        Non-overlapping edits are applied in reverse document order so that
        line numbers remain valid as each edit is made.

        Args:
            source: Original source text.
            suggestions: Suggestions from ``generate_fixes()``.

        Returns:
            ``FixResult`` containing the modified source and statistics.
        """
        applied = 0
        skipped = 0
        changes: List[str] = []

        # Mark which suggestions have conflicting edits
        applicable: List[FixSuggestion] = []
        for s in suggestions:
            if s.is_applicable():
                applicable.append(s)
            else:
                skipped += 1

        new_source = self._apply_edits(source, applicable)

        for s in applicable:
            changes.append(
                f"{s.issue.location.file}:{s.issue.location.line}: {s.description}"
            )
            applied += 1

        return FixResult(
            new_source=new_source,
            applied=applied,
            skipped=skipped,
            changes=changes,
        )

    def apply_to_file(
        self,
        file_path: str,
        issues: List[Issue],
        dry_run: bool = False,
    ) -> FixResult:
        """
        Generate fixes for ``issues`` found in ``file_path``, optionally
        writing the result back to disk.

        Args:
            file_path: Path to the source file.
            issues:    Issues produced by the analyzer for this file.
            dry_run:   If True, do not write the modified source.

        Returns:
            ``FixResult`` with statistics and the modified source.
        """
        with open(file_path, "r", encoding="utf-8") as fh:
            source = fh.read()

        lines = source.splitlines(keepends=False)
        suggestions = self.generate_fixes(issues, lines)
        result = self.apply(source, suggestions)

        if not dry_run and result.applied > 0:
            with open(file_path, "w", encoding="utf-8") as fh:
                fh.write(result.new_source)

        return result

    def register_generator(self, code: str, generator: _Generator) -> None:
        """Register a custom fix generator for the given issue code."""
        self._generators[code] = generator

    def supported_codes(self) -> List[str]:
        """Return a sorted list of issue codes that have registered generators."""
        return sorted(self._generators.keys())

    # ------------------------------------------------------------------
    # Internal helpers
    # ------------------------------------------------------------------

    def _apply_edits(
        self,
        source: str,
        suggestions: List[FixSuggestion],
    ) -> str:
        """Collect all edits, sort in reverse order, apply sequentially."""
        # Gather all edits with their suggestion context
        all_edits: List[TextEdit] = []
        for s in suggestions:
            all_edits.extend(s.edits)

        if not all_edits:
            return source

        lines = source.splitlines(keepends=False)
        for edit in _sort_edits_reverse(all_edits):
            lines = _apply_single_edit(lines, edit)

        return "\n".join(lines)
