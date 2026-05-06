"""
Coverage Reporter
=================

Tracks which source lines are executed during NexusLang program runs and
produces line-coverage reports in text, JSON, and HTML formats.

Usage (programmatic)::

    collector = CoverageCollector()
    collector.start()
    run_program(source, path)               # your interpreter call
    collector.stop()
    report = collector.build_report(source, path)
    print(report.summary())
    report.write_html("coverage/index.html")

Usage (from tooling)::

    from nexuslang.tooling.coverage import run_with_coverage
    report = run_with_coverage(source_path)
"""

from __future__ import annotations

import json
import os
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, List, Optional, Set, Tuple


# ---------------------------------------------------------------------------
# Data model
# ---------------------------------------------------------------------------

@dataclass
class FileCoverage:
    """Coverage data for a single source file."""
    path: str
    total_lines: int
    executable_lines: Set[int]   # Lines that can be executed
    hit_lines: Set[int]          # Lines that were actually executed
    source_lines: List[str] = field(default_factory=list)

    @property
    def covered_count(self) -> int:
        return len(self.hit_lines & self.executable_lines)

    @property
    def executable_count(self) -> int:
        return len(self.executable_lines)

    @property
    def miss_count(self) -> int:
        return len(self.executable_lines - self.hit_lines)

    @property
    def pct(self) -> float:
        if not self.executable_lines:
            return 100.0
        return self.covered_count / self.executable_count * 100.0

    def missed_lines(self) -> List[int]:
        return sorted(self.executable_lines - self.hit_lines)

    def line_status(self, line_no: int) -> str:
        """Return 'hit', 'miss', or 'nocode' for display."""
        if line_no in self.executable_lines:
            return "hit" if line_no in self.hit_lines else "miss"
        return "nocode"


@dataclass
class CoverageReport:
    """Aggregate coverage report for one or more source files."""
    files: Dict[str, FileCoverage] = field(default_factory=dict)
    generated_at: str = ""

    def total_executable(self) -> int:
        return sum(f.executable_count for f in self.files.values())

    def total_covered(self) -> int:
        return sum(f.covered_count for f in self.files.values())

    def total_pct(self) -> float:
        exe = self.total_executable()
        if exe == 0:
            return 100.0
        return self.total_covered() / exe * 100.0

    # ------------------------------------------------------------------
    # Text summary
    # ------------------------------------------------------------------

    def summary(self) -> str:
        lines = []
        lines.append("")
        lines.append("Coverage report")
        lines.append("=" * 72)
        lines.append(f"{'File':<45} {'Stmts':>6} {'Miss':>6} {'Cover':>7}")
        lines.append("-" * 72)
        for path, fc in sorted(self.files.items()):
            rel = os.path.basename(path)
            lines.append(
                f"{rel:<45} {fc.executable_count:>6} {fc.miss_count:>6} {fc.pct:>6.0f}%"
            )
        lines.append("-" * 72)
        lines.append(
            f"{'TOTAL':<45} {self.total_executable():>6} "
            f"{self.total_executable() - self.total_covered():>6} "
            f"{self.total_pct():>6.0f}%"
        )
        lines.append("")
        return "\n".join(lines)

    # ------------------------------------------------------------------
    # JSON output
    # ------------------------------------------------------------------

    def to_json(self) -> str:
        data = {
            "meta": {
                "generated_at": self.generated_at,
                "total_executable": self.total_executable(),
                "total_covered": self.total_covered(),
                "total_pct": round(self.total_pct(), 2),
            },
            "files": {},
        }
        for path, fc in self.files.items():
            data["files"][path] = {
                "executable": sorted(fc.executable_lines),
                "hit": sorted(fc.hit_lines),
                "missed": fc.missed_lines(),
                "pct": round(fc.pct, 2),
            }
        return json.dumps(data, indent=2)

    def write_json(self, output_path: str) -> None:
        Path(output_path).parent.mkdir(parents=True, exist_ok=True)
        Path(output_path).write_text(self.to_json(), encoding="utf-8")

    # ------------------------------------------------------------------
    # HTML output
    # ------------------------------------------------------------------

    def write_html(self, output_dir: str) -> None:
        out = Path(output_dir)
        out.mkdir(parents=True, exist_ok=True)
        # Write CSS once
        (out / "coverage.css").write_text(_CSS, encoding="utf-8")
        # Write per-file pages
        file_links = []
        for path, fc in sorted(self.files.items()):
            rel = os.path.relpath(path)
            safe_name = rel.replace(os.sep, "_").replace("/", "_") + ".html"
            (out / safe_name).write_text(_build_file_html(fc), encoding="utf-8")
            pct_class = _pct_class(fc.pct)
            file_links.append(
                f'<tr class="{pct_class}">'
                f'<td><a href="{safe_name}">{rel}</a></td>'
                f'<td class="r">{fc.executable_count}</td>'
                f'<td class="r">{fc.miss_count}</td>'
                f'<td class="r">{fc.pct:.0f}%</td>'
                f'</tr>'
            )
        index_html = _INDEX_TMPL.format(
            title="NLPL Coverage Report",
            generated_at=self.generated_at,
            total_executable=self.total_executable(),
            total_covered=self.total_covered(),
            total_pct=f"{self.total_pct():.0f}%",
            file_rows="\n".join(file_links),
        )
        (out / "index.html").write_text(index_html, encoding="utf-8")


# ---------------------------------------------------------------------------
# Collector
# ---------------------------------------------------------------------------

class CoverageCollector:
    """
    Hooks into the NexusLang interpreter to track executed lines.

    Call ``attach(interpreter)`` before program execution and
    ``detach()`` afterwards.  The interpreter exposes
    ``current_line`` which we poll via its ``_coverage_hook``.
    """

    def __init__(self) -> None:
        self._hits: Dict[str, Set[int]] = {}  # path -> set of line numbers
        self._active: bool = False
        self._attached_interpreter = None

    # ------------------------------------------------------------------
    # Lifecycle
    # ------------------------------------------------------------------

    def start(self) -> None:
        self._active = True

    def stop(self) -> None:
        self._active = False

    def attach(self, interpreter) -> None:
        """Attach this collector to a live Interpreter instance."""
        self._attached_interpreter = interpreter
        # Install our hook as a property setter side-effect
        interpreter._coverage_collector = self
        self._active = True

    def detach(self) -> None:
        if self._attached_interpreter is not None:
            self._attached_interpreter._coverage_collector = None
            self._attached_interpreter = None
        self._active = False

    # ------------------------------------------------------------------
    # Recording
    # ------------------------------------------------------------------

    def record(self, path: str, line: int) -> None:
        """Record that ``line`` in ``path`` was executed."""
        if not self._active or line <= 0:
            return
        if path not in self._hits:
            self._hits[path] = set()
        self._hits[path].add(line)

    # ------------------------------------------------------------------
    # Report building
    # ------------------------------------------------------------------

    def build_report(
        self,
        source_paths: Optional[List[str]] = None,
        source_text: Optional[str] = None,
        single_path: Optional[str] = None,
    ) -> CoverageReport:
        """
        Build a CoverageReport from accumulated hits.

        Args:
            source_paths: List of .nlpl files to include in the report.
            source_text:  Used together with ``single_path`` for in-memory
                          sources.
            single_path:  Path key used for ``source_text``.
        """
        report = CoverageReport(
            generated_at=time.strftime("%Y-%m-%dT%H:%M:%S")
        )

        if source_text is not None and single_path is not None:
            paths_and_texts = [(single_path, source_text)]
        elif source_paths:
            paths_and_texts = [
                (p, Path(p).read_text(encoding="utf-8"))
                for p in source_paths
                if Path(p).exists()
            ]
        else:
            # Fall back to all paths we have hits for
            paths_and_texts = [
                (p, Path(p).read_text(encoding="utf-8"))
                for p in self._hits.keys()
                if Path(p).exists()
            ]

        for path, text in paths_and_texts:
            fc = self._analyse_file(path, text)
            report.files[path] = fc

        return report

    def _analyse_file(self, path: str, text: str) -> FileCoverage:
        source_lines = text.splitlines()
        total = len(source_lines)
        executable = _find_executable_lines(source_lines)
        hit = self._hits.get(path, set())
        return FileCoverage(
            path=path,
            total_lines=total,
            executable_lines=executable,
            hit_lines=hit,
            source_lines=source_lines,
        )


# ---------------------------------------------------------------------------
# Executable-line heuristic
# ---------------------------------------------------------------------------

def _find_executable_lines(source_lines: List[str]) -> Set[int]:
    """
    Return the set of 1-based line numbers that contain executable statements.

    This is a simple heuristic: non-empty lines that are not pure comments,
    not continuation of multi-line strings, not blank.
    """
    executable: Set[int] = set()
    in_multiline_string = False
    for i, raw in enumerate(source_lines, start=1):
        line = raw.strip()
        if not line:
            continue
        if line.startswith("#"):
            continue
        # Track triple-quoted strings (simple heuristic)
        if line.startswith('"""') or line.startswith("'''"):
            triple = line[:3]
            rest = line[3:]
            if triple in rest:
                pass  # Single-line triple-quoted
            else:
                in_multiline_string = not in_multiline_string
                continue
        if in_multiline_string:
            continue
        # Keywords that are structural, not executable on their own:
        if line in ("end", "end struct", "end class", "end function"):
            continue
        if line.startswith("function ") or line.startswith("class "):
            pass  # Still executable (function definition line itself)
        executable.add(i)
    return executable


# ---------------------------------------------------------------------------
# Convenience entry point
# ---------------------------------------------------------------------------

def run_with_coverage(
    source_path: str,
    output_dir: str = "coverage",
    report_json: bool = True,
    report_html: bool = True,
) -> CoverageReport:
    """
    Run an NexusLang source file with coverage collection enabled.

    Returns the CoverageReport; side-effects: writes coverage/ directory.
    """
    from ..main import run_program

    collector = CoverageCollector()
    source = Path(source_path).read_text(encoding="utf-8")

    # We patch run_program to inject the collector
    original_run = run_program

    def patched_run(src, path, **kwargs):
        result = original_run(src, path, coverage_collector=collector, **kwargs)
        return result

    collector.start()
    try:
        run_program(source, source_path, coverage_collector=collector)
    except Exception:
        pass
    finally:
        collector.stop()

    report = collector.build_report(single_path=source_path, source_text=source)

    if report_json:
        Path(output_dir).mkdir(parents=True, exist_ok=True)
        report.write_json(os.path.join(output_dir, "coverage.json"))
    if report_html:
        report.write_html(output_dir)

    print(report.summary())
    return report


# ---------------------------------------------------------------------------
# HTML templates and CSS (self-contained, no external deps)
# ---------------------------------------------------------------------------

def _pct_class(pct: float) -> str:
    if pct >= 90:
        return "high"
    if pct >= 75:
        return "medium"
    return "low"


def _build_file_html(fc: FileCoverage) -> str:
    rows = []
    for i, raw_line in enumerate(fc.source_lines, start=1):
        status = fc.line_status(i)
        css = {"hit": "hit", "miss": "miss", "nocode": "nocode"}[status]
        escaped = (
            raw_line.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")
        )
        rows.append(
            f'<tr class="{css}">'
            f'<td class="ln">{i}</td>'
            f'<td class="src"><pre>{escaped}</pre></td>'
            f'</tr>'
        )
    return _FILE_TMPL.format(
        title=os.path.basename(fc.path),
        path=fc.path,
        pct=f"{fc.pct:.0f}%",
        rows="\n".join(rows),
    )


_CSS = """\
body { font-family: monospace; background: #1e1e2e; color: #cdd6f4; margin: 2rem; }
h1, h2 { color: #89b4fa; }
table { border-collapse: collapse; width: 100%; }
th { background: #313244; padding: .4rem .8rem; text-align: left; }
td { padding: .2rem .8rem; }
.r { text-align: right; }
.high td { background: #1e3a2a; }
.medium td { background: #2a2a1a; }
.low td { background: #3a1a1a; }
.hit { background: #1e3a2a !important; }
.miss { background: #3a1a1e !important; }
.nocode {}
.ln { color: #6c7086; width: 3rem; text-align: right; user-select: none; }
.src pre { margin: 0; white-space: pre-wrap; }
a { color: #89b4fa; }
"""

_INDEX_TMPL = """\
<!DOCTYPE html>
<html><head><meta charset="utf-8">
<title>{title}</title>
<link rel="stylesheet" href="coverage.css">
</head><body>
<h1>{title}</h1>
<p>Generated: {generated_at} &mdash; Total: {total_covered}/{total_executable} lines ({total_pct})</p>
<table>
<tr><th>File</th><th class="r">Stmts</th><th class="r">Miss</th><th class="r">Cover</th></tr>
{file_rows}
</table>
</body></html>
"""

_FILE_TMPL = """\
<!DOCTYPE html>
<html><head><meta charset="utf-8">
<title>{title}</title>
<link rel="stylesheet" href="coverage.css">
</head><body>
<h2>{path}</h2>
<p>Coverage: <strong>{pct}</strong></p>
<table>
{rows}
</table>
<p><a href="index.html">Back to index</a></p>
</body></html>
"""


__all__ = [
    "CoverageCollector",
    "CoverageReport",
    "FileCoverage",
    "run_with_coverage",
]
