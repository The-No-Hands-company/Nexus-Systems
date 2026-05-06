"""
NLPL Standard Library - Coverage Utilities Module
Provides line, branch, and function coverage measurement using sys.settrace.

A coverage session tracks which source lines and functions were executed.
Multiple named sessions can be active simultaneously (each uses its own tracer
and accumulates data independently).

Session result dict (from coverage_stop / coverage_get):
    {
        'name': str,
        'active': bool,
        'files': {
            filepath: {
                'lines_hit':   set of int,  -- covered line numbers
                'lines_total': set of int,  -- all known executable lines
                'functions':   {func_name: hit_count},
                'branches':    {(from_line, to_line): count},  -- basic branch tracking
            }
        },
        'call_count': int,   -- total function call events
        'line_count': int,   -- total line execution events
    }

All coverage_* functions that take a 'session' parameter default to the most
recently started session when session=None.
"""

from __future__ import annotations

import fnmatch
import os
import sys
import threading
import time
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from nexuslang.runtime.runtime import Runtime


# ---------------------------------------------------------------------------
# Internal state
# ---------------------------------------------------------------------------

class _SessionData:
    """Mutable state for one named coverage session."""

    __slots__ = (
        "name", "active", "files", "call_count", "line_count",
        "include_patterns", "exclude_patterns",
        "_prev_tracer", "_prev_line", "_lock",
    )

    def __init__(self, name: str, include_patterns=None, exclude_patterns=None):
        self.name = name
        self.active = False
        self.files: dict[str, dict] = {}
        self.call_count = 0
        self.line_count = 0
        self.include_patterns = include_patterns or []
        self.exclude_patterns = exclude_patterns or []
        self._prev_tracer = None
        self._prev_line: dict[int, int] = {}  # thread_id -> last line
        self._lock = threading.Lock()

    def _file_rec(self, filepath: str) -> dict:
        if filepath not in self.files:
            self.files[filepath] = {
                "lines_hit": set(),
                "lines_total": set(),
                "functions": {},
                "branches": {},
            }
        return self.files[filepath]

    def _should_track(self, filepath: str) -> bool:
        if not filepath or filepath == "<string>" or filepath.startswith("<"):
            return False
        # normalise
        fp = os.path.normpath(filepath)
        if self.include_patterns:
            if not any(fnmatch.fnmatch(fp, p) for p in self.include_patterns):
                return False
        if self.exclude_patterns:
            if any(fnmatch.fnmatch(fp, p) for p in self.exclude_patterns):
                return False
        return True

    def _tracer(self, frame, event, arg):
        filepath = frame.f_code.co_filename
        if not self._should_track(filepath):
            return self._tracer  # still need to return self to keep tracing

        lineno = frame.f_lineno

        with self._lock:
            rec = self._file_rec(filepath)

            if event == "call":
                self.call_count += 1
                fname = frame.f_code.co_name
                rec["functions"][fname] = rec["functions"].get(fname, 0) + 1
                rec["lines_hit"].add(lineno)
                rec["lines_total"].add(lineno)
                tid = threading.get_ident()
                self._prev_line[tid] = lineno

            elif event == "line":
                self.line_count += 1
                rec["lines_hit"].add(lineno)
                rec["lines_total"].add(lineno)
                tid = threading.get_ident()
                prev = self._prev_line.get(tid)
                if prev is not None and prev != lineno:
                    branch_key = (prev, lineno)
                    rec["branches"][branch_key] = rec["branches"].get(branch_key, 0) + 1
                self._prev_line[threading.get_ident()] = lineno

            elif event == "return":
                rec["lines_hit"].add(lineno)
                rec["lines_total"].add(lineno)

        return self._tracer

    def start(self):
        if self.active:
            return
        self._prev_tracer = sys.gettrace()
        sys.settrace(self._tracer)
        self.active = True

    def stop(self):
        if not self.active:
            return
        sys.settrace(self._prev_tracer)
        self._prev_tracer = None
        self.active = False

    def reset(self):
        was_active = self.active
        self.stop()
        self.files.clear()
        self.call_count = 0
        self.line_count = 0
        self._prev_line.clear()
        if was_active:
            self.start()

    def to_dict(self) -> dict:
        return {
            "name": self.name,
            "active": self.active,
            "files": {
                fp: {
                    "lines_hit": sorted(rec["lines_hit"]),
                    "lines_total": sorted(rec["lines_total"]),
                    "functions": dict(rec["functions"]),
                    "branches": {
                        f"{k[0]}->{k[1]}": v for k, v in rec["branches"].items()
                    },
                }
                for fp, rec in self.files.items()
            },
            "call_count": self.call_count,
            "line_count": self.line_count,
        }


# Global registry of sessions: {name: _SessionData}
_sessions: dict[str, _SessionData] = {}
_default_session_name: str | None = None
_lock = threading.Lock()


def _get_session(name=None) -> _SessionData:
    with _lock:
        if name is None:
            if _default_session_name is None:
                raise RuntimeError(
                    "coverage_utils: no active session. Call coverage_start() first."
                )
            name = _default_session_name
        if name not in _sessions:
            raise RuntimeError(f"coverage_utils: session not found: {name!r}")
        return _sessions[name]


# ---------------------------------------------------------------------------
# Session management
# ---------------------------------------------------------------------------

def coverage_start(name="default", include=None, exclude=None):
    """Start a new named coverage session.

    Creates a session if it does not exist, then starts tracing.

    Args:
        name: session name (default 'default')
        include: list of glob patterns — only files matching are tracked
        exclude: list of glob patterns — matching files are excluded

    Returns:
        name (str)
    """
    global _default_session_name
    name = str(name)
    with _lock:
        if name not in _sessions:
            _sessions[name] = _SessionData(
                name,
                include_patterns=list(include) if include else [],
                exclude_patterns=list(exclude) if exclude else [],
            )
        _default_session_name = name
    _sessions[name].start()
    return name


def coverage_stop(name=None):
    """Stop a coverage session and return its result dict.

    Args:
        name: session name (default: most recently started)

    Returns:
        session result dict
    """
    sess = _get_session(name)
    sess.stop()
    return sess.to_dict()


def coverage_pause(name=None):
    """Temporarily pause tracing without discarding collected data.

    Args:
        name: session name (default: most recently started)
    """
    sess = _get_session(name)
    if sess.active:
        sys.settrace(sess._prev_tracer)
        sess.active = False
    return sess.name


def coverage_resume(name=None):
    """Resume a paused session.

    Args:
        name: session name (default: most recently started)
    """
    sess = _get_session(name)
    if not sess.active:
        sess._prev_tracer = sys.gettrace()
        sys.settrace(sess._tracer)
        sess.active = True
    return sess.name


def coverage_reset(name=None):
    """Clear all collected data for a session (keeps session active if it was running).

    Args:
        name: session name (default: most recently started)
    """
    sess = _get_session(name)
    sess.reset()
    return sess.name


def coverage_destroy(name=None):
    """Stop and remove a session entirely.

    Args:
        name: session name (default: most recently started)
    """
    global _default_session_name
    sess = _get_session(name)
    sess.stop()
    with _lock:
        _sessions.pop(sess.name, None)
        if _default_session_name == sess.name:
            _default_session_name = next(iter(_sessions), None)
    return sess.name


def coverage_is_active(name=None):
    """Return True if the session is currently tracing.

    Args:
        name: session name (default: most recently started)
    """
    try:
        return _get_session(name).active
    except RuntimeError:
        return False


def coverage_list_sessions():
    """Return a list of all session names."""
    with _lock:
        return list(_sessions.keys())


# ---------------------------------------------------------------------------
# Data retrieval
# ---------------------------------------------------------------------------

def coverage_get(name=None):
    """Return the full session result dict without stopping.

    Args:
        name: session name (default: most recently started)

    Returns:
        session result dict
    """
    return _get_session(name).to_dict()


def coverage_files(name=None):
    """Return a list of all tracked file paths in a session.

    Args:
        name: session name

    Returns:
        list of str
    """
    sess = _get_session(name)
    return list(sess.files.keys())


def coverage_lines_hit(filepath, name=None):
    """Return sorted list of hit line numbers for a file.

    Args:
        filepath: source file path
        name: session name

    Returns:
        list of int
    """
    sess = _get_session(name)
    rec = sess.files.get(os.path.normpath(filepath)) or sess.files.get(filepath, {})
    return sorted(rec.get("lines_hit", set()))


def coverage_lines_total(filepath, name=None):
    """Return sorted list of all known executable line numbers for a file.

    Args:
        filepath: source file path
        name: session name

    Returns:
        list of int
    """
    sess = _get_session(name)
    rec = sess.files.get(os.path.normpath(filepath)) or sess.files.get(filepath, {})
    return sorted(rec.get("lines_total", set()))


def coverage_line_rate(filepath=None, name=None):
    """Return line coverage rate (0.0-1.0) for a file or overall.

    Args:
        filepath: if None, computes across all files in the session
        name: session name

    Returns:
        float in [0.0, 1.0]
    """
    sess = _get_session(name)
    if filepath is not None:
        rec = sess.files.get(os.path.normpath(filepath)) or sess.files.get(filepath, {})
        total = len(rec.get("lines_total", set()))
        hit = len(rec.get("lines_hit", set()))
        return hit / total if total > 0 else 1.0
    # Overall
    total = sum(len(r["lines_total"]) for r in sess.files.values())
    hit = sum(len(r["lines_hit"]) for r in sess.files.values())
    return hit / total if total > 0 else 1.0


def coverage_functions(filepath, name=None):
    """Return dict of {function_name: hit_count} for a file.

    Args:
        filepath: source file path
        name: session name

    Returns:
        dict
    """
    sess = _get_session(name)
    rec = sess.files.get(os.path.normpath(filepath)) or sess.files.get(filepath, {})
    return dict(rec.get("functions", {}))


def coverage_function_rate(filepath=None, name=None):
    """Return function coverage rate (0.0-1.0): fraction of known functions that were called.

    Since we only know functions that *were* called (sys.settrace sees 'call' events),
    this returns 1.0 when any functions were recorded.  For meaningful function rate
    analysis, combine with a static analysis pass or provide total_functions.

    Args:
        filepath: source file path (None = all files)
        name: session name

    Returns:
        float — number of distinct functions recorded / total known functions
    """
    sess = _get_session(name)
    if filepath is not None:
        rec = sess.files.get(os.path.normpath(filepath)) or sess.files.get(filepath, {})
        return 1.0 if rec.get("functions") else 0.0
    called = sum(len(r["functions"]) for r in sess.files.values())
    return 1.0 if called > 0 else 0.0


def coverage_branches(filepath, name=None):
    """Return dict of {"from->to": count} branch transitions for a file.

    Args:
        filepath: source file path
        name: session name

    Returns:
        dict
    """
    sess = _get_session(name)
    rec = sess.files.get(os.path.normpath(filepath)) or sess.files.get(filepath, {})
    return {
        f"{k[0]}->{k[1]}": v
        for k, v in rec.get("branches", {}).items()
    }


def coverage_branch_count(filepath=None, name=None):
    """Return total number of distinct branch transitions observed.

    Args:
        filepath: file path (None = all files)
        name: session name

    Returns:
        int
    """
    sess = _get_session(name)
    if filepath is not None:
        rec = sess.files.get(os.path.normpath(filepath)) or sess.files.get(filepath, {})
        return len(rec.get("branches", {}))
    return sum(len(r["branches"]) for r in sess.files.values())


def coverage_call_count(name=None):
    """Return total function call events recorded in a session.

    Args:
        name: session name

    Returns:
        int
    """
    return _get_session(name).call_count


def coverage_line_count(name=None):
    """Return total line execution events recorded in a session.

    Args:
        name: session name

    Returns:
        int
    """
    return _get_session(name).line_count


# ---------------------------------------------------------------------------
# Reporting
# ---------------------------------------------------------------------------

def coverage_summary(name=None):
    """Return a summary dict for a session.

    Returns:
        dict with:
            name, active, file_count, total_lines, hit_lines, line_rate (float),
            total_branches, total_functions, call_count, line_count
    """
    sess = _get_session(name)
    total_lines = sum(len(r["lines_total"]) for r in sess.files.values())
    hit_lines = sum(len(r["lines_hit"]) for r in sess.files.values())
    total_branches = sum(len(r["branches"]) for r in sess.files.values())
    total_functions = sum(len(r["functions"]) for r in sess.files.values())
    return {
        "name": sess.name,
        "active": sess.active,
        "file_count": len(sess.files),
        "total_lines": total_lines,
        "hit_lines": hit_lines,
        "line_rate": hit_lines / total_lines if total_lines > 0 else 1.0,
        "total_branches": total_branches,
        "total_functions": total_functions,
        "call_count": sess.call_count,
        "line_count": sess.line_count,
    }


def coverage_report(name=None, show_lines=False):
    """Return a list of per-file report dicts.

    Each entry:
        {filename, hit_lines, total_lines, line_rate, functions, branch_count}

    Args:
        name: session name
        show_lines: if True, include the lists of hit/miss line numbers

    Returns:
        list of dicts, sorted by filename
    """
    sess = _get_session(name)
    rows = []
    for filepath, rec in sorted(sess.files.items()):
        total = len(rec["lines_total"])
        hit = len(rec["lines_hit"])
        entry = {
            "filename": filepath,
            "hit_lines": hit,
            "total_lines": total,
            "line_rate": hit / total if total > 0 else 1.0,
            "functions": len(rec["functions"]),
            "branch_count": len(rec["branches"]),
        }
        if show_lines:
            miss = sorted(rec["lines_total"] - rec["lines_hit"])
            entry["hit_line_list"] = sorted(rec["lines_hit"])
            entry["miss_line_list"] = miss
        rows.append(entry)
    return rows


def coverage_report_text(name=None):
    """Return a human-readable text coverage report string.

    Args:
        name: session name

    Returns:
        str — multi-line report
    """
    rows = coverage_report(name, show_lines=True)
    summ = coverage_summary(name)

    lines = []
    lines.append(f"Coverage Report: {summ['name']}")
    lines.append("-" * 70)
    lines.append(f"{'File':<40} {'Lines':>8} {'Hit':>6} {'Rate':>7}  {'Branches':>8}")
    lines.append("-" * 70)

    for row in rows:
        fname = os.path.basename(row["filename"])
        if len(fname) > 38:
            fname = "..." + fname[-35:]
        rate_pct = row["line_rate"] * 100
        lines.append(
            f"{fname:<40} {row['total_lines']:>8} {row['hit_lines']:>6} "
            f"{rate_pct:>6.1f}%  {row['branch_count']:>8}"
        )
        if row.get("miss_line_list"):
            miss_str = ", ".join(str(ln) for ln in row["miss_line_list"][:10])
            if len(row["miss_line_list"]) > 10:
                miss_str += f", ... ({len(row['miss_line_list']) - 10} more)"
            lines.append(f"  {'Missing lines:':<38} {miss_str}")

    lines.append("-" * 70)
    rate_pct = summ["line_rate"] * 100
    lines.append(
        f"{'TOTAL':<40} {summ['total_lines']:>8} {summ['hit_lines']:>6} "
        f"{rate_pct:>6.1f}%  {summ['total_branches']:>8}"
    )
    lines.append(f"Functions recorded: {summ['total_functions']}")
    lines.append(f"Call events:        {summ['call_count']}")
    lines.append(f"Line events:        {summ['line_count']}")
    return "\n".join(lines)


def coverage_to_dict(name=None):
    """Return the full session data as a serialisable dict.

    Args:
        name: session name

    Returns:
        dict (all sets converted to sorted lists)
    """
    return _get_session(name).to_dict()


def coverage_merge(name_a, name_b, result_name="merged"):
    """Merge two sessions into a new session.

    Creates a new session called result_name containing the union of hits from
    both sessions. Neither source session is affected.

    Args:
        name_a: first session name
        name_b: second session name
        result_name: name for the new merged session (default 'merged')

    Returns:
        result_name (str)
    """
    a = _get_session(name_a)
    b = _get_session(name_b)

    with _lock:
        merged = _SessionData(result_name)
        merged.call_count = a.call_count + b.call_count
        merged.line_count = a.line_count + b.line_count
        _sessions[result_name] = merged

    # Merge file records
    all_files = set(a.files) | set(b.files)
    for fp in all_files:
        rec = merged._file_rec(fp)
        ra = a.files.get(fp, {})
        rb = b.files.get(fp, {})
        rec["lines_hit"] = set(ra.get("lines_hit", set())) | set(rb.get("lines_hit", set()))
        rec["lines_total"] = set(ra.get("lines_total", set())) | set(rb.get("lines_total", set()))
        # Merge function counts
        for fn, cnt in ra.get("functions", {}).items():
            rec["functions"][fn] = rec["functions"].get(fn, 0) + cnt
        for fn, cnt in rb.get("functions", {}).items():
            rec["functions"][fn] = rec["functions"].get(fn, 0) + cnt
        # Merge branch counts
        for bk, cnt in ra.get("branches", {}).items():
            # bk is (int, int) in internal format
            rec["branches"][bk] = rec["branches"].get(bk, 0) + cnt
        for bk, cnt in rb.get("branches", {}).items():
            rec["branches"][bk] = rec["branches"].get(bk, 0) + cnt

    return result_name


def coverage_diff(name_a, name_b):
    """Return lines hit in session_a but not in session_b, per file.

    Useful for finding what a test run covers that another doesn't.

    Args:
        name_a: first session name
        name_b: second session name

    Returns:
        dict {filepath: {'only_in_a': [lines], 'only_in_b': [lines]}}
    """
    a = _get_session(name_a)
    b = _get_session(name_b)
    all_files = set(a.files) | set(b.files)
    result = {}
    for fp in sorted(all_files):
        ra = a.files.get(fp, {})
        rb = b.files.get(fp, {})
        hit_a = set(ra.get("lines_hit", set()))
        hit_b = set(rb.get("lines_hit", set()))
        only_a = sorted(hit_a - hit_b)
        only_b = sorted(hit_b - hit_a)
        if only_a or only_b:
            result[fp] = {"only_in_a": only_a, "only_in_b": only_b}
    return result


# ---------------------------------------------------------------------------
# Convenience: measure coverage of a callable
# ---------------------------------------------------------------------------

def coverage_measure(fn, args=None, kwargs=None, name=None, include=None, exclude=None):
    """Run fn(*args, **kwargs) while measuring coverage, then return (result, session_dict).

    Creates a temporary session, runs the function, stops tracing, returns
    both the function's return value and the coverage dict.

    Args:
        fn: callable to execute
        args: positional arguments (list, default [])
        kwargs: keyword arguments (dict, default {})
        name: session name to use (default: auto-generated)
        include: include glob patterns
        exclude: exclude glob patterns

    Returns:
        tuple (return_value, session_dict)
    """
    if not callable(fn):
        raise TypeError("coverage_measure: fn must be callable")
    args = list(args) if args is not None else []
    kwargs = dict(kwargs) if kwargs is not None else {}
    session_name = name if name is not None else f"_measure_{id(fn)}_{time.monotonic_ns()}"
    coverage_start(session_name, include=include, exclude=exclude)
    try:
        result = fn(*args, **kwargs)
    finally:
        coverage_stop(session_name)
    return (result, coverage_to_dict(session_name))


def coverage_measure_line_rate(fn, args=None, kwargs=None, include=None, exclude=None):
    """Run fn and return the overall line coverage rate (0.0-1.0).

    Args:
        fn: callable to run
        args: positional arguments
        kwargs: keyword arguments
        include: include patterns
        exclude: exclude patterns

    Returns:
        float — line coverage rate across all tracked files
    """
    _, sess_dict = coverage_measure(fn, args=args, kwargs=kwargs, include=include, exclude=exclude)
    files = sess_dict["files"]
    total = sum(len(v["lines_total"]) for v in files.values())
    hit = sum(len(v["lines_hit"]) for v in files.values())
    return hit / total if total > 0 else 1.0


# ---------------------------------------------------------------------------
# Registration
# ---------------------------------------------------------------------------

def register_coverage_utils_functions(runtime: "Runtime") -> None:
    """Register all coverage utility functions with the NexusLang runtime."""

    # Session management
    runtime.register_function("coverage_start", coverage_start)
    runtime.register_function("coverage_stop", coverage_stop)
    runtime.register_function("coverage_pause", coverage_pause)
    runtime.register_function("coverage_resume", coverage_resume)
    runtime.register_function("coverage_reset", coverage_reset)
    runtime.register_function("coverage_destroy", coverage_destroy)
    runtime.register_function("coverage_is_active", coverage_is_active)
    runtime.register_function("coverage_list_sessions", coverage_list_sessions)

    # Data retrieval
    runtime.register_function("coverage_get", coverage_get)
    runtime.register_function("coverage_files", coverage_files)
    runtime.register_function("coverage_lines_hit", coverage_lines_hit)
    runtime.register_function("coverage_lines_total", coverage_lines_total)
    runtime.register_function("coverage_line_rate", coverage_line_rate)
    runtime.register_function("coverage_functions", coverage_functions)
    runtime.register_function("coverage_function_rate", coverage_function_rate)
    runtime.register_function("coverage_branches", coverage_branches)
    runtime.register_function("coverage_branch_count", coverage_branch_count)
    runtime.register_function("coverage_call_count", coverage_call_count)
    runtime.register_function("coverage_line_count", coverage_line_count)

    # Reporting
    runtime.register_function("coverage_summary", coverage_summary)
    runtime.register_function("coverage_report", coverage_report)
    runtime.register_function("coverage_report_text", coverage_report_text)
    runtime.register_function("coverage_to_dict", coverage_to_dict)
    runtime.register_function("coverage_merge", coverage_merge)
    runtime.register_function("coverage_diff", coverage_diff)

    # Measurement helpers
    runtime.register_function("coverage_measure", coverage_measure)
    runtime.register_function("coverage_measure_line_rate", coverage_measure_line_rate)

    # Module aliases
    runtime.register_module("coverage_utils")
    runtime.register_module("coverage")
