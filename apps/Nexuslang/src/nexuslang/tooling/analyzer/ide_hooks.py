"""
IDE Integration Hooks
=====================

Converts NexusLang static analysis results into Language Server Protocol (LSP)
compatible data structures so that any LSP-aware IDE (VS Code, Emacs,
Neovim, etc.) can consume NexusLang diagnostics and code actions natively.

Specification references
------------------------
* LSP 3.17 ``textDocument/publishDiagnostics``
  https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#textDocument_publishDiagnostics
* LSP 3.17 ``textDocument/codeAction``
  https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#textDocument_codeAction

Usage
-----

::

    from nexuslang.tooling.analyzer.ide_hooks import IDEHooks, LspFormatter
    from nexuslang.tooling.analyzer.autofix import AutoFixer

    hooks = IDEHooks()
    fixer = AutoFixer()

    # After running the analyzer:
    diagnostics = hooks.issues_to_diagnostics(report.issues, file_uri)
    # Push diagnostics to the client:
    # server.publish_diagnostics(file_uri, diagnostics)

    # On a code-action request:
    suggestions = fixer.generate_fixes(report.issues, source_lines)
    actions = hooks.fixes_to_code_actions(suggestions, file_uri)

JSON output format
------------------

``diagnostic`` (matches LSP ``Diagnostic``)::

    {
      "range": {
        "start": {"line": 0, "character": 0},   # 0-based
        "end":   {"line": 0, "character": 10}
      },
      "severity": 1,            # 1=Error 2=Warn 3=Info 4=Hint
      "code": "E001",
    "source": "nlpl-analyze",
      "message": "Human-readable message",
      "relatedInformation": [   # optional
        {
          "location": {"uri": "file:///...", "range": {...}},
          "message": "..."
        }
      ],
      "data": {                 # NexusLang extension
        "category": "memory",
        "suggestion": "...",
        "fix_hint": "..."
      }
    }

``codeAction`` (matches LSP ``CodeAction``)::

    {
      "title": "Fix: description",
      "kind":  "quickfix",
      "diagnostics": [...],
      "isPreferred": true,
      "edit": {
        "changes": {
          "file:///path/to/file.nxl": [
            {
              "range": {
                "start": {"line": 0, "character": 0},
                "end":   {"line": 1, "character": 0}
              },
              "newText": "replacement text"
            }
          ]
        }
      }
    }
"""
from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Dict, List, Optional

from .report import Issue, Severity, AnalysisReport, SourceLocation
from .autofix import FixSuggestion, TextEdit

__all__ = [
    "IDEHooks",
    "LspFormatter",
    "lsp_position",
    "lsp_range",
    "severity_to_lsp",
]

# ---------------------------------------------------------------------------
# LSP severity mapping
# ---------------------------------------------------------------------------

# LSP DiagnosticSeverity values
_LSP_ERROR = 1
_LSP_WARNING = 2
_LSP_INFORMATION = 3
_LSP_HINT = 4

_SEVERITY_MAP: Dict[Severity, int] = {
    Severity.ERROR: _LSP_ERROR,
    Severity.WARNING: _LSP_WARNING,
    Severity.INFO: _LSP_INFORMATION,
    Severity.HINT: _LSP_HINT,
}


def severity_to_lsp(severity: Severity) -> int:
    """Convert a NexusLang ``Severity`` to an LSP ``DiagnosticSeverity`` integer."""
    return _SEVERITY_MAP.get(severity, _LSP_INFORMATION)


# ---------------------------------------------------------------------------
# LSP primitive constructors
# ---------------------------------------------------------------------------

def lsp_position(line: int, character: int) -> Dict[str, int]:
    """
    Build an LSP ``Position`` dict.

    Args:
        line:      0-based line number.
        character: 0-based character offset within the line (UTF-16 units).
    """
    return {"line": line, "character": character}


def lsp_range(
    start_line: int,
    start_char: int,
    end_line: int,
    end_char: int,
) -> Dict[str, Dict[str, int]]:
    """
    Build an LSP ``Range`` dict from 0-based positions.

    All four parameters use the 0-based LSP coordinate system.
    """
    return {
        "start": lsp_position(start_line, start_char),
        "end": lsp_position(end_line, end_char),
    }


def _location_to_lsp_range(loc: SourceLocation) -> Dict[str, Any]:
    """
    Convert a NexusLang ``SourceLocation`` (1-based) to an LSP ``Range`` (0-based).
    """
    start_line = max(0, loc.line - 1)
    start_char = max(0, loc.column - 1)
    end_line = max(0, (loc.end_line or loc.line) - 1)
    end_char = max(0, (loc.end_column or loc.column) - 1)
    # Ensure end >= start
    if (end_line, end_char) < (start_line, start_char):
        end_line, end_char = start_line, start_char
    return lsp_range(start_line, start_char, end_line, end_char)


def _path_to_uri(path: str) -> str:
    """Convert a file system path to a ``file://`` URI."""
    return Path(path).resolve().as_uri()


def _text_edit_to_lsp(edit: TextEdit, file_uri: str) -> Dict[str, Any]:
    """Convert a ``TextEdit`` (1-based) to an LSP ``TextEdit`` dict (0-based)."""
    range_dict = lsp_range(
        edit.line - 1, edit.col - 1,
        edit.end_line - 1, edit.end_col - 1,
    )
    return {
        "range": range_dict,
        "newText": edit.new_text,
    }


# ---------------------------------------------------------------------------
# IDEHooks
# ---------------------------------------------------------------------------

class IDEHooks:
    """
    Converts NexusLang analysis results to LSP-compatible JSON structures.

    All methods return plain Python ``dict`` / ``list`` objects that are
    directly JSON-serializable via ``json.dumps()``.

    Args:
        source_name: Value used for the ``"source"`` field in diagnostics
                     (default: ``"nlpl-analyze```).
    """

    def __init__(self, source_name: str = "nlpl-analyze") -> None:
        self.source_name = source_name

    # ------------------------------------------------------------------
    # Diagnostics
    # ------------------------------------------------------------------

    def issue_to_diagnostic(
        self,
        issue: Issue,
        file_uri: Optional[str] = None,
    ) -> Dict[str, Any]:
        """
        Convert a single ``Issue`` to an LSP ``Diagnostic`` dict.

        Args:
            issue:    The analysis issue.
            file_uri: URI of the file containing the issue (used for related
                      information).  If omitted, the issue's location path is
                      converted to a URI automatically.

        Returns:
            LSP ``Diagnostic`` dict.
        """
        uri = file_uri or _path_to_uri(issue.location.file)

        diag: Dict[str, Any] = {
            "range": _location_to_lsp_range(issue.location),
            "severity": severity_to_lsp(issue.severity),
            "code": issue.code,
            "source": self.source_name,
            "message": issue.message,
        }

        # Related locations
        if issue.related_locations:
            related: List[Dict[str, Any]] = []
            for rel_loc in issue.related_locations:
                rel_uri = _path_to_uri(rel_loc.file)
                related.append({
                    "location": {
                        "uri": rel_uri,
                        "range": _location_to_lsp_range(rel_loc),
                    },
                    "message": f"Related: {issue.message}",
                })
            diag["relatedInformation"] = related

        # NLPL-specific extension data
        data: Dict[str, Any] = {"category": issue.category.value}
        if issue.suggestion:
            data["suggestion"] = issue.suggestion
        if issue.fix:
            data["fix_hint"] = issue.fix
        if issue.help_url:
            data["help_url"] = issue.help_url
        diag["data"] = data

        return diag

    def issues_to_diagnostics(
        self,
        issues: List[Issue],
        file_uri: Optional[str] = None,
    ) -> List[Dict[str, Any]]:
        """
        Convert a list of ``Issue`` objects to a list of LSP diagnostics.

        Args:
            issues:   Issues from an ``AnalysisReport``.
            file_uri: URI of the file (passed to ``issue_to_diagnostic``).

        Returns:
            List of LSP ``Diagnostic`` dicts.
        """
        return [self.issue_to_diagnostic(i, file_uri) for i in issues]

    def report_to_publish_params(
        self,
        report: AnalysisReport,
    ) -> Dict[str, Any]:
        """
        Convert an ``AnalysisReport`` to LSP ``PublishDiagnosticsParams``.

        Returns a dict ready to pass as the ``params`` of a
        ``textDocument/publishDiagnostics`` notification.
        """
        uri = _path_to_uri(report.file_path)
        return {
            "uri": uri,
            "diagnostics": self.issues_to_diagnostics(report.issues, uri),
        }

    # ------------------------------------------------------------------
    # Code Actions
    # ------------------------------------------------------------------

    def fix_to_code_action(
        self,
        suggestion: FixSuggestion,
        file_uri: str,
        is_preferred: bool = True,
    ) -> Dict[str, Any]:
        """
        Convert a ``FixSuggestion`` to an LSP ``CodeAction`` dict.

        Args:
            suggestion:   The fix suggestion.
            file_uri:     URI of the file to modify.
            is_preferred: Whether this is the preferred fix for the diagnostic.

        Returns:
            LSP ``CodeAction`` dict with ``"kind": "quickfix"``.
        """
        lsp_edits = [
            _text_edit_to_lsp(edit, file_uri) for edit in suggestion.edits
        ]

        action: Dict[str, Any] = {
            "title": f"Fix: {suggestion.description}",
            "kind": "quickfix",
            "isPreferred": is_preferred,
            "edit": {
                "changes": {
                    file_uri: lsp_edits,
                }
            },
        }

        # Attach the originating diagnostic so the IDE can correlate them
        action["diagnostics"] = [
            self.issue_to_diagnostic(suggestion.issue, file_uri)
        ]

        return action

    def fixes_to_code_actions(
        self,
        suggestions: List[FixSuggestion],
        file_uri: str,
    ) -> List[Dict[str, Any]]:
        """
        Convert a list of ``FixSuggestion`` objects to LSP ``CodeAction`` dicts.

        Args:
            suggestions: Suggestions from ``AutoFixer.generate_fixes()``.
            file_uri:    URI of the file to modify.

        Returns:
            List of LSP ``CodeAction`` dicts.
        """
        return [
            self.fix_to_code_action(s, file_uri, is_preferred=(i == 0))
            for i, s in enumerate(suggestions)
        ]

    def get_code_actions_for_range(
        self,
        issues: List[Issue],
        suggestions: List[FixSuggestion],
        file_uri: str,
        start_line: int,
        end_line: int,
    ) -> List[Dict[str, Any]]:
        """
        Return code actions for issues that fall within the given line range.

        This mirrors the LSP ``textDocument/codeAction`` request semantics
        where the client sends a range and the server returns applicable
        actions.

        Args:
            issues:      All issues for the file.
            suggestions: All fix suggestions for the file.
            file_uri:    URI of the file.
            start_line:  First line of the requested range (1-based, inclusive).
            end_line:    Last line of the requested range (1-based, inclusive).

        Returns:
            List of LSP ``CodeAction`` dicts for the range.
        """
        # Determine which issue codes are within the range
        in_range_codes = {
            (i.code, i.location.line)
            for i in issues
            if start_line <= i.location.line <= end_line
        }

        # Keep only suggestions whose issue is within the range
        relevant = [
            s for s in suggestions
            if (s.issue.code, s.issue.location.line) in in_range_codes
        ]

        return self.fixes_to_code_actions(relevant, file_uri)

    # ------------------------------------------------------------------
    # Workspace-level helpers
    # ------------------------------------------------------------------

    def reports_to_workspace_diagnostics(
        self,
        reports: List[AnalysisReport],
    ) -> Dict[str, List[Dict[str, Any]]]:
        """
        Convert multiple ``AnalysisReport`` objects to a mapping from
        file URI to list of LSP diagnostics.

        Returns:
            ``{"file:///path/to/file.nxl": [<diagnostic>, ...], ...}``
        """
        result: Dict[str, List[Dict[str, Any]]] = {}
        for report in reports:
            uri = _path_to_uri(report.file_path)
            result[uri] = self.issues_to_diagnostics(report.issues, uri)
        return result


# ---------------------------------------------------------------------------
# LspFormatter  — a convenience façade that bundles IDEHooks + AutoFixer
# ---------------------------------------------------------------------------

class LspFormatter:
    """
    High-level façade that combines ``IDEHooks`` and ``AutoFixer`` into a
    single object for use inside an LSP server implementation.

    Example::

        from nexuslang.tooling.analyzer import StaticAnalyzer
        from nexuslang.tooling.analyzer.ide_hooks import LspFormatter

        formatter = LspFormatter()
        analyzer = StaticAnalyzer()

        report = analyzer.analyze_file("program.nxl")
        with open("program.nxl") as fh:
            source = fh.read()

        # For publishDiagnostics notification:
        params = formatter.diagnostics_for_file(report, source)

        # For codeAction response:
        actions = formatter.code_actions_for_range(report, source,
                                                    start_line=5, end_line=10)
    """

    def __init__(self, source_name: str = "nlpl-analyze") -> None:
        from .autofix import AutoFixer
        self._hooks = IDEHooks(source_name=source_name)
        self._fixer = AutoFixer()

    @property
    def hooks(self) -> IDEHooks:
        """The underlying ``IDEHooks`` instance."""
        return self._hooks

    def diagnostics_for_file(
        self,
        report: AnalysisReport,
        source: Optional[str] = None,
    ) -> Dict[str, Any]:
        """
        Return LSP ``PublishDiagnosticsParams`` for the given report.

        Args:
            report: ``AnalysisReport`` from the analyzer.
            source: Source text (unused, kept for API symmetry).
        """
        return self._hooks.report_to_publish_params(report)

    def code_actions_for_range(
        self,
        report: AnalysisReport,
        source: str,
        start_line: int,
        end_line: int,
    ) -> List[Dict[str, Any]]:
        """
        Return LSP ``CodeAction`` dicts for issues in the given line range.

        Args:
            report:      ``AnalysisReport`` containing the issues.
            source:      Original source text of the file.
            start_line:  First line of range (1-based).
            end_line:    Last line of range (1-based).
        """
        uri = _path_to_uri(report.file_path)
        lines = source.splitlines(keepends=False)
        suggestions = self._fixer.generate_fixes(report.issues, lines)
        return self._hooks.get_code_actions_for_range(
            report.issues, suggestions, uri, start_line, end_line
        )

    def apply_fixes(
        self,
        report: AnalysisReport,
        source: str,
        dry_run: bool = False,
    ) -> str:
        """
        Generate and apply fixes, returning the modified source.

        Args:
            report:  ``AnalysisReport`` with the issues to fix.
            source:  Original source text.
            dry_run: If True, return modified source without writing to disk.
        """
        lines = source.splitlines(keepends=False)
        suggestions = self._fixer.generate_fixes(report.issues, lines)
        return self._fixer.preview(source, suggestions)
