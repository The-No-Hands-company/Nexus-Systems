"""
Profiler
========

CPU-time and memory-allocation profiler for NexusLang programs.

Two profilers are provided:

* ``CPUProfiler``  -- wall-clock + CPU-time per function, call counts,
  call graph, flame graph JSON.
* ``MemoryProfiler`` -- allocation tracking per site (file:line),
  peak heap size, top allocators.

Both hook into the NexusLang interpreter via the ``_profiler`` slot on the
interpreter instance.  A combined ``Profiler`` facade wraps both.

Quick start::

    profiler = Profiler()
    profiler.attach(interpreter)
    run_program(source, path)
    profiler.detach()
    profiler.print_report()
    profiler.write_html("profile/")
"""

from __future__ import annotations

import json
import os
import sys
import time
import tracemalloc
from collections import defaultdict
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple


# ==========================================================================
# CPU Profiler
# ==========================================================================

@dataclass
class FunctionProfile:
    """Timing data for a single function."""
    name: str
    qualified_name: str    # module::name
    call_count: int = 0
    total_time: float = 0.0   # inclusive wall-clock seconds
    self_time: float = 0.0    # exclusive time (total - children)
    callers: Dict[str, int] = field(default_factory=dict)  # caller -> call count
    callees: Dict[str, int] = field(default_factory=dict)  # callee -> call count

    def avg_time(self) -> float:
        return self.total_time / self.call_count if self.call_count else 0.0

    def avg_self(self) -> float:
        return self.self_time / self.call_count if self.call_count else 0.0


class CPUProfiler:
    """
    Sampling / tracing CPU profiler.

    Hooks into interpreter call/return events to accumulate per-function
    wall-clock times and call graphs.
    """

    def __init__(self) -> None:
        self._profiles: Dict[str, FunctionProfile] = {}
        self._call_stack: List[Tuple[str, float]] = []  # (name, start_time)
        self._active: bool = False

    # ------------------------------------------------------------------
    # Lifecycle
    # ------------------------------------------------------------------

    def start(self) -> None:
        self._active = True
        self._call_stack = []
        self._profiles = {}

    def stop(self) -> None:
        self._active = False

    def reset(self) -> None:
        self._profiles = {}
        self._call_stack = []

    # ------------------------------------------------------------------
    # Interpreter hooks (called by interpreter)
    # ------------------------------------------------------------------

    def on_call(self, func_name: str, module: str = "") -> None:
        if not self._active:
            return
        qualified = f"{module}::{func_name}" if module else func_name
        self._call_stack.append((qualified, time.perf_counter()))

        if qualified not in self._profiles:
            self._profiles[qualified] = FunctionProfile(
                name=func_name, qualified_name=qualified
            )
        self._profiles[qualified].call_count += 1

        # Record caller relationship
        if len(self._call_stack) >= 2:
            caller = self._call_stack[-2][0]
            self._profiles[qualified].callers[caller] = (
                self._profiles[qualified].callers.get(caller, 0) + 1
            )
            if caller in self._profiles:
                self._profiles[caller].callees[qualified] = (
                    self._profiles[caller].callees.get(qualified, 0) + 1
                )

    def on_return(self) -> None:
        if not self._active or not self._call_stack:
            return
        qualified, start = self._call_stack.pop()
        elapsed = time.perf_counter() - start

        if qualified in self._profiles:
            self._profiles[qualified].total_time += elapsed

        # Deduct from parent's self_time (if parent exists)
        if self._call_stack:
            parent = self._call_stack[-1][0]
            if parent in self._profiles:
                # We track self_time separately
                self._profiles[parent].self_time -= elapsed

        # Add elapsed to self_time of current function
        if qualified in self._profiles:
            self._profiles[qualified].self_time += elapsed

    # ------------------------------------------------------------------
    # Report
    # ------------------------------------------------------------------

    def top_functions(
        self, n: int = 20, sort_by: str = "total"
    ) -> List[FunctionProfile]:
        """Return top N functions sorted by total or self time."""
        key = (lambda p: p.total_time) if sort_by == "total" else (lambda p: p.self_time)
        return sorted(self._profiles.values(), key=key, reverse=True)[:n]

    def text_report(self, n: int = 20) -> str:
        lines = []
        lines.append("")
        lines.append("CPU Profile")
        lines.append("=" * 80)
        lines.append(
            f"{'Function':<45} {'Calls':>6} {'Total(s)':>9} {'Self(s)':>9} {'Avg(ms)':>9}"
        )
        lines.append("-" * 80)
        for p in self.top_functions(n, sort_by="total"):
            lines.append(
                f"{p.qualified_name:<45} {p.call_count:>6} "
                f"{p.total_time:>9.4f} {p.self_time:>9.4f} "
                f"{p.avg_time()*1000:>9.3f}"
            )
        lines.append("")
        return "\n".join(lines)

    def to_flame_json(self) -> str:
        """
        Output Brendan Gregg-style folded stacks JSON, suitable for
        speedscope or chrome://tracing import.
        """
        frames = []
        for p in self._profiles.values():
            frames.append({
                "name": p.qualified_name,
                "totalTime": round(p.total_time * 1000, 3),
                "selfTime": round(p.self_time * 1000, 3),
                "callCount": p.call_count,
            })
        return json.dumps({"frames": frames}, indent=2)


# ==========================================================================
# Memory Profiler
# ==========================================================================

@dataclass
class AllocationSite:
    """Aggregated allocation data for one call site."""
    key: str            # "file:line"
    file: str
    line: int
    alloc_count: int = 0
    total_bytes: int = 0
    current_bytes: int = 0
    peak_bytes: int = 0

    def avg_bytes(self) -> float:
        return self.total_bytes / self.alloc_count if self.alloc_count else 0.0


class MemoryProfiler:
    """
    Memory profiler that tracks allocation events.

    For the interpreter mode it hooks the allocator via
    ``_memory_profiler`` on the Runtime instance.

    It also uses Python's built-in ``tracemalloc`` to track
    Python-level allocations during interpretation.
    """

    def __init__(self) -> None:
        self._sites: Dict[str, AllocationSite] = {}
        self._peak_bytes: int = 0
        self._current_bytes: int = 0
        self._active: bool = False
        self._tracemalloc_started: bool = False

    # ------------------------------------------------------------------
    # Lifecycle
    # ------------------------------------------------------------------

    def start(self) -> None:
        self._active = True
        if not tracemalloc.is_tracing():
            tracemalloc.start(25)
            self._tracemalloc_started = True

    def stop(self) -> None:
        self._active = False

    def snapshot(self) -> None:
        """Take a tracemalloc snapshot and merge into site data."""
        if not tracemalloc.is_tracing():
            return
        snap = tracemalloc.take_snapshot()
        stats = snap.statistics("lineno")
        for stat in stats[:50]:
            key = f"{stat.traceback[0].filename}:{stat.traceback[0].lineno}"
            if key not in self._sites:
                self._sites[key] = AllocationSite(
                    key=key,
                    file=stat.traceback[0].filename,
                    line=stat.traceback[0].lineno,
                )
            site = self._sites[key]
            site.current_bytes = stat.size
            site.peak_bytes = max(site.peak_bytes, stat.size)
            site.total_bytes = max(site.total_bytes, stat.size)

    def stop_and_collect(self) -> None:
        self.snapshot()
        if self._tracemalloc_started and tracemalloc.is_tracing():
            tracemalloc.stop()
            self._tracemalloc_started = False
        self._active = False

    # ------------------------------------------------------------------
    # Interpreter hook
    # ------------------------------------------------------------------

    def record_allocation(self, size: int, site_key: str) -> None:
        """Called by the allocator stub when an allocation occurs."""
        if not self._active:
            return
        if site_key not in self._sites:
            parts = site_key.rsplit(":", 1)
            f, ln = (parts[0], int(parts[1])) if len(parts) == 2 else (site_key, 0)
            self._sites[site_key] = AllocationSite(key=site_key, file=f, line=ln)
        site = self._sites[site_key]
        site.alloc_count += 1
        site.total_bytes += size
        self._current_bytes += size
        self._peak_bytes = max(self._peak_bytes, self._current_bytes)

    def record_deallocation(self, size: int) -> None:
        if self._active:
            self._current_bytes = max(0, self._current_bytes - size)

    # ------------------------------------------------------------------
    # Report
    # ------------------------------------------------------------------

    @property
    def peak_bytes(self) -> int:
        return self._peak_bytes

    def top_sites(self, n: int = 20) -> List[AllocationSite]:
        return sorted(
            self._sites.values(), key=lambda s: s.peak_bytes, reverse=True
        )[:n]

    def text_report(self, n: int = 20) -> str:
        lines = []
        lines.append("")
        lines.append("Memory Profile")
        lines.append("=" * 80)
        lines.append(f"  Peak memory: {_fmt_bytes(self.peak_bytes)}")
        lines.append("")
        lines.append(f"{'Site':<55} {'Count':>6} {'Peak':>10} {'Total':>10}")
        lines.append("-" * 80)
        for site in self.top_sites(n):
            filename = os.path.basename(site.file)
            if filename.endswith(".nlpl"):
                filename = filename[:-5] + ".nxl"
            label = f"{filename}:{site.line}"
            lines.append(
                f"{label:<55} {site.alloc_count:>6} "
                f"{_fmt_bytes(site.peak_bytes):>10} "
                f"{_fmt_bytes(site.total_bytes):>10}"
            )
        lines.append("")
        return "\n".join(lines)


# ==========================================================================
# Combined Profiler facade
# ==========================================================================

class Profiler:
    """
    Combined CPU + memory profiler.

    Attach to an interpreter / runtime before execution, detach after.
    """

    def __init__(self, cpu: bool = True, memory: bool = True) -> None:
        self.cpu = CPUProfiler() if cpu else None
        self.memory = MemoryProfiler() if memory else None
        self._attached_interpreter = None

    # ------------------------------------------------------------------
    # Lifecycle
    # ------------------------------------------------------------------

    def start(self) -> None:
        if self.cpu:
            self.cpu.start()
        if self.memory:
            self.memory.start()

    def stop(self) -> None:
        if self.cpu:
            self.cpu.stop()
        if self.memory:
            self.memory.stop_and_collect()

    def attach(self, interpreter) -> None:
        """Inject profiler hooks into a live Interpreter instance."""
        self._attached_interpreter = interpreter
        interpreter._profiler = self
        self.start()

    def detach(self) -> None:
        if self._attached_interpreter is not None:
            self._attached_interpreter._profiler = None
            self._attached_interpreter = None
        self.stop()

    # ------------------------------------------------------------------
    # Report
    # ------------------------------------------------------------------

    def print_report(self, top: int = 20, file=None) -> None:
        out = file or sys.stdout
        if self.cpu:
            print(self.cpu.text_report(top), file=out)
        if self.memory:
            print(self.memory.text_report(top), file=out)

    def write_json(self, output_path: str) -> None:
        data: Dict[str, Any] = {}
        if self.cpu:
            data["cpu"] = {
                "functions": [
                    {
                        "name": p.qualified_name,
                        "calls": p.call_count,
                        "total_ms": round(p.total_time * 1000, 3),
                        "self_ms": round(p.self_time * 1000, 3),
                        "callers": p.callers,
                        "callees": p.callees,
                    }
                    for p in self.cpu.top_functions(200)
                ]
            }
        if self.memory:
            data["memory"] = {
                "peak_bytes": self.memory.peak_bytes,
                "peak_human": _fmt_bytes(self.memory.peak_bytes),
                "top_sites": [
                    {
                        "site": s.key,
                        "alloc_count": s.alloc_count,
                        "peak_bytes": s.peak_bytes,
                        "total_bytes": s.total_bytes,
                    }
                    for s in self.memory.top_sites(100)
                ],
            }
        Path(output_path).parent.mkdir(parents=True, exist_ok=True)
        Path(output_path).write_text(json.dumps(data, indent=2), encoding="utf-8")

    def write_html(self, output_dir: str) -> None:
        out = Path(output_dir)
        out.mkdir(parents=True, exist_ok=True)

        cpu_rows = ""
        if self.cpu:
            rows = []
            for p in self.cpu.top_functions(50):
                rows.append(
                    f"<tr>"
                    f"<td>{p.qualified_name}</td>"
                    f"<td class='r'>{p.call_count}</td>"
                    f"<td class='r'>{p.total_time*1000:.3f}</td>"
                    f"<td class='r'>{p.self_time*1000:.3f}</td>"
                    f"<td class='r'>{p.avg_time()*1000:.3f}</td>"
                    f"</tr>"
                )
            cpu_rows = "\n".join(rows)

        mem_rows = ""
        if self.memory:
            rows = []
            for s in self.memory.top_sites(50):
                rows.append(
                    f"<tr>"
                    f"<td>{os.path.basename(s.file)}:{s.line}</td>"
                    f"<td class='r'>{s.alloc_count}</td>"
                    f"<td class='r'>{_fmt_bytes(s.peak_bytes)}</td>"
                    f"<td class='r'>{_fmt_bytes(s.total_bytes)}</td>"
                    f"</tr>"
                )
            mem_rows = "\n".join(rows)

        html = _PROFILE_HTML_TMPL.format(
            cpu_rows=cpu_rows,
            mem_rows=mem_rows,
            peak_bytes=_fmt_bytes(self.memory.peak_bytes) if self.memory else "N/A",
            generated_at=time.strftime("%Y-%m-%dT%H:%M:%S"),
        )
        (out / "profile.html").write_text(html, encoding="utf-8")


# ---------------------------------------------------------------------------
# Convenience runner
# ---------------------------------------------------------------------------

def run_with_profiling(
    source_path: str,
    output_dir: str = "profile",
    cpu: bool = True,
    memory: bool = True,
) -> Profiler:
    """
    Run an NexusLang source file with profiling enabled.

    Returns the Profiler; writes HTML + JSON to output_dir.
    """
    from ..main import run_program

    profiler = Profiler(cpu=cpu, memory=memory)
    source = Path(source_path).read_text(encoding="utf-8")
    profiler.start()
    try:
        run_program(source, source_path, profiler=profiler)
    except Exception:
        pass
    finally:
        profiler.stop()

    Path(output_dir).mkdir(parents=True, exist_ok=True)
    profiler.write_json(os.path.join(output_dir, "profile.json"))
    profiler.write_html(output_dir)
    profiler.print_report()
    return profiler


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _fmt_bytes(n: int) -> str:
    if n < 1024:
        return f"{n} B"
    if n < 1024 ** 2:
        return f"{n / 1024:.1f} KB"
    if n < 1024 ** 3:
        return f"{n / 1024**2:.1f} MB"
    return f"{n / 1024**3:.1f} GB"


_PROFILE_HTML_TMPL = """\
<!DOCTYPE html>
<html><head><meta charset="utf-8">
<title>NLPL Profile Report</title>
<style>
body {{ font-family: monospace; background: #1e1e2e; color: #cdd6f4; margin: 2rem; }}
h1, h2 {{ color: #89b4fa; }}
table {{ border-collapse: collapse; width: 100%; margin-bottom: 2rem; }}
th {{ background: #313244; padding: .4rem .8rem; text-align: left; }}
td {{ padding: .2rem .8rem; }}
.r {{ text-align: right; }}
tr:nth-child(even) {{ background: #24243e; }}
</style>
</head><body>
<h1>NLPL Profile Report</h1>
<p>Generated: {generated_at} &mdash; Peak memory: {peak_bytes}</p>

<h2>CPU Hotspots (top 50 by total time)</h2>
<table>
<tr><th>Function</th><th class="r">Calls</th><th class="r">Total (ms)</th>
    <th class="r">Self (ms)</th><th class="r">Avg (ms)</th></tr>
{cpu_rows}
</table>

<h2>Memory Hotspots (top 50 allocators)</h2>
<table>
<tr><th>Site</th><th class="r">Allocs</th><th class="r">Peak</th>
    <th class="r">Total</th></tr>
{mem_rows}
</table>
</body></html>
"""

__all__ = [
    "Profiler",
    "CPUProfiler",
    "MemoryProfiler",
    "FunctionProfile",
    "AllocationSite",
    "run_with_profiling",
]
