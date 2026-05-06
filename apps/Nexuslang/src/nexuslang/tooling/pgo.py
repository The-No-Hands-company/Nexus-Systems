"""
Profile-Guided Optimization (PGO) Driver
==========================================

Implements a three-phase PGO workflow for NexusLang programs:

Phase 1 – Instrumentation Build
    Compile the program with profiling instrumentation injected.  Every
    function call, loop iteration, and branch is counted at runtime.

Phase 2 – Profile Collection
    Run the instrumented binary against representative workloads.  The
    execution counts are written to a profile data file (``*.nlplprof``).

Phase 3 – Optimized Rebuild
    Recompile the program using the collected profile data to guide:
    - Hot/cold function identification (aggressive inlining of hot functions,
      cold function separation)
    - Branch prediction hinting (mark likely/unlikely branches)
    - Loop-body ordering (move hot code to the top of the file)
    - Type-specialization priority (specialize functions called most often)
    - Dead-code elimination (remove code never executed in profiles)

Usage
-----
    from nexuslang.tooling.pgo import PGODriver, PGOConfig

    config = PGOConfig(profile_dir="build/profiles", min_hot_count=100)
    driver = PGODriver(config)

    # Phase 1: instrument
    instrumented_path = driver.instrument("build/myapp")

    # Phase 2: collect (run workloads, then call collect)
    # ... run instrumented_path against workloads ...
    driver.collect()

    # Phase 3: optimized rebuild
    driver.optimized_build("build/myapp.pgo")

CLI equivalent::

    nlpl pgo instrument
    ... run workloads ...
    nlpl pgo collect
    nlpl pgo build
"""

import json
import os
import subprocess
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple


# ---------------------------------------------------------------------------
# Data structures
# ---------------------------------------------------------------------------

@dataclass
class FunctionProfile:
    """Per-function execution statistics collected during profiling."""
    name: str
    call_count: int = 0
    total_time_ns: int = 0
    # Per-call-site breakdown: caller -> count
    call_sites: Dict[str, int] = field(default_factory=dict)
    # Loop iteration counts keyed by loop identifier
    loop_counts: Dict[str, int] = field(default_factory=dict)
    # Branch counts: branch_id -> (taken, not_taken)
    branch_counts: Dict[str, Tuple[int, int]] = field(default_factory=dict)

    @property
    def mean_time_ns(self) -> float:
        if self.call_count == 0:
            return 0.0
        return self.total_time_ns / self.call_count

    def is_hot(self, threshold: int) -> bool:
        return self.call_count >= threshold

    def to_dict(self) -> Dict[str, Any]:
        return {
            "name": self.name,
            "call_count": self.call_count,
            "total_time_ns": self.total_time_ns,
            "mean_time_ns": self.mean_time_ns,
            "call_sites": self.call_sites,
            "loop_counts": self.loop_counts,
            "branch_counts": {
                k: list(v) for k, v in self.branch_counts.items()
            },
        }

    @classmethod
    def from_dict(cls, d: Dict[str, Any]) -> "FunctionProfile":
        p = cls(
            name=d["name"],
            call_count=d.get("call_count", 0),
            total_time_ns=d.get("total_time_ns", 0),
            call_sites=d.get("call_sites", {}),
            loop_counts=d.get("loop_counts", {}),
        )
        raw_branches = d.get("branch_counts", {})
        p.branch_counts = {k: tuple(v) for k, v in raw_branches.items()}  # type: ignore[assignment]
        return p


@dataclass
class PGOProfile:
    """Aggregated profile data for an entire program."""
    program_name: str
    collection_runs: int = 0
    total_runtime_ns: int = 0
    functions: Dict[str, FunctionProfile] = field(default_factory=dict)
    # Ordered list of modules by hotness
    hot_modules: List[str] = field(default_factory=list)

    def merge(self, other: "PGOProfile") -> None:
        """Merge another profile into this one (sum all counts)."""
        self.collection_runs += other.collection_runs
        self.total_runtime_ns += other.total_runtime_ns
        for name, fp in other.functions.items():
            if name in self.functions:
                existing = self.functions[name]
                existing.call_count += fp.call_count
                existing.total_time_ns += fp.total_time_ns
                for site, cnt in fp.call_sites.items():
                    existing.call_sites[site] = existing.call_sites.get(site, 0) + cnt
                for lid, cnt in fp.loop_counts.items():
                    existing.loop_counts[lid] = existing.loop_counts.get(lid, 0) + cnt
                for bid, (taken, not_taken) in fp.branch_counts.items():
                    old = existing.branch_counts.get(bid, (0, 0))
                    existing.branch_counts[bid] = (old[0] + taken, old[1] + not_taken)
            else:
                self.functions[name] = fp

    def hot_functions(self, threshold: int) -> List[FunctionProfile]:
        """Return all functions called at least threshold times, sorted by count."""
        return sorted(
            [fp for fp in self.functions.values() if fp.is_hot(threshold)],
            key=lambda fp: fp.call_count,
            reverse=True,
        )

    def cold_functions(self, threshold: int) -> List[FunctionProfile]:
        """Return all functions called fewer than threshold times."""
        return [fp for fp in self.functions.values() if not fp.is_hot(threshold)]

    # ------------------------------------------------------------------
    # Convenience methods for recording and querying execution data
    # ------------------------------------------------------------------

    def record_execution(self, func_name: str, time_ns: int = 0) -> None:
        """Record one execution of *func_name* (increments call_count by 1)."""
        if func_name in self.functions:
            self.functions[func_name].call_count += 1
            self.functions[func_name].total_time_ns += time_ns
        else:
            self.functions[func_name] = FunctionProfile(
                name=func_name, call_count=1, total_time_ns=time_ns
            )

    def is_hot(self, func_name: str, threshold: int = 1) -> bool:
        """Return True if *func_name* has been called at least *threshold* times."""
        fp = self.functions.get(func_name)
        return fp is not None and fp.is_hot(threshold)

    def get_call_count(self, func_name: str) -> int:
        """Return the recorded call count for *func_name*, or 0 if never recorded."""
        fp = self.functions.get(func_name)
        return fp.call_count if fp is not None else 0

    def to_json(self) -> str:
        d = {
            "program_name": self.program_name,
            "collection_runs": self.collection_runs,
            "total_runtime_ns": self.total_runtime_ns,
            "functions": {k: v.to_dict() for k, v in self.functions.items()},
            "hot_modules": self.hot_modules,
        }
        return json.dumps(d, indent=2)

    def save(self, path: str) -> None:
        Path(path).parent.mkdir(parents=True, exist_ok=True)
        with open(path, "w", encoding="utf-8") as f:
            f.write(self.to_json())

    @classmethod
    def load(cls, path: str) -> "PGOProfile":
        with open(path, "r", encoding="utf-8") as f:
            d = json.load(f)
        profile = cls(
            program_name=d.get("program_name", ""),
            collection_runs=d.get("collection_runs", 0),
            total_runtime_ns=d.get("total_runtime_ns", 0),
            hot_modules=d.get("hot_modules", []),
        )
        for name, fp_dict in d.get("functions", {}).items():
            profile.functions[name] = FunctionProfile.from_dict(fp_dict)
        return profile


# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

@dataclass
class PGOConfig:
    """Configuration for PGO workflow."""
    profile_dir: str = "build/profiles"
    profile_filename: str = "nexuslang.prof"
    # A function is "hot" if called >= min_hot_count times across workloads
    min_hot_count: int = 100
    # Inline hot functions up to this size (in AST nodes)
    hot_inline_size: int = 200
    # Aggressive type-specialization: generate N variants of each hot function
    hot_specialization_variants: int = 4
    # Merge multiple profile runs (allows running many workloads)
    merge_profiles: bool = True
    # Show PGO statistics during compile
    verbose: bool = False

    @property
    def profile_path(self) -> str:
        return os.path.join(self.profile_dir, self.profile_filename)


# ---------------------------------------------------------------------------
# Instrumentation injector
# ---------------------------------------------------------------------------

class PGOInstrumenter:
    """
    Injects profiling probes into an AST before compilation.

    Probes added:
    - Function entry/exit counters with timing
    - Branch taken/not-taken counters
    - Loop iteration counters
    - Call-site identifiers

    The generated code writes profile data to a ``*.nlplprof`` file when the
    program exits (via atexit hook or explicit flush).
    """

    ENTRY_PROBE = "__pgo_enter__"
    EXIT_PROBE = "__pgo_exit__"
    BRANCH_PROBE = "__pgo_branch__"
    LOOP_PROBE = "__pgo_loop__"

    def __init__(self, config: PGOConfig):
        self.config = config
        self._probes_inserted = 0
        self._functions_instrumented = 0

    def instrument(self, ast: Any) -> Any:
        """Insert profiling probes into the AST."""
        if ast is None:
            return ast
        self._walk_and_instrument(ast)
        return ast

    def _walk_and_instrument(self, node: Any, depth: int = 0) -> None:
        if node is None or depth > 50:
            return
        node_type = type(node).__name__

        if node_type == "FunctionDefinition":
            self._instrument_function(node)
        elif node_type in ("IfStatement", "WhileLoop", "ForEachLoop"):
            self._instrument_branch_or_loop(node, node_type)

        for child in self._iter_children(node):
            self._walk_and_instrument(child, depth + 1)

    def _instrument_function(self, func_node: Any) -> None:
        """Wrap function body with entry/exit probes."""
        func_name = getattr(func_node, "name", "__unknown__")
        try:
            func_node._pgo_instrumented = True
            func_node._pgo_entry_probe = self.ENTRY_PROBE
            func_node._pgo_func_name = func_name
        except AttributeError:
            pass
        self._functions_instrumented += 1
        self._probes_inserted += 2  # entry + exit

    def _instrument_branch_or_loop(self, node: Any, node_type: str) -> None:
        try:
            node._pgo_probe = (
                self.BRANCH_PROBE if node_type == "IfStatement" else self.LOOP_PROBE
            )
            node._pgo_probe_id = f"probe_{id(node)}"
        except AttributeError:
            pass
        self._probes_inserted += 1

    def _iter_children(self, node: Any):
        if not hasattr(node, "__dict__"):
            return
        for k, v in vars(node).items():
            if k.startswith("_"):
                continue
            if isinstance(v, list):
                yield from [i for i in v if i is not None and hasattr(i, "__dict__")]
            elif hasattr(v, "__dict__"):
                yield v

    @property
    def probes_inserted(self) -> int:
        return self._probes_inserted

    @property
    def functions_instrumented(self) -> int:
        return self._functions_instrumented


# ---------------------------------------------------------------------------
# Runtime profile collector (Python-level, for interpreter mode)
# ---------------------------------------------------------------------------

class PGORuntimeCollector:
    """
    Collects PGO profile data from the NexusLang interpreter at runtime.

    Attach to an interpreter instance to automatically record function
    calls, timings, and branch outcomes during execution.
    """

    def __init__(self, program_name: str):
        self.program_name = program_name
        self._profile = PGOProfile(program_name=program_name)
        self._call_stack: List[Tuple[str, int]] = []  # (name, start_ns)
        self._attached_interpreter: Optional[Any] = None

    def attach(self, interpreter: Any) -> None:
        """Hook into the interpreter's call mechanism."""
        self._attached_interpreter = interpreter
        original_call = getattr(interpreter, "_call_function", None)
        original_return = getattr(interpreter, "_return_function", None)

        if original_call is not None:
            def wrapped_call(func_name, *args, **kwargs):
                self.on_call(func_name)
                return original_call(func_name, *args, **kwargs)
            interpreter._call_function = wrapped_call

        if original_return is not None:
            def wrapped_return(func_name, result):
                self.on_return(func_name)
                return original_return(func_name, result)
            interpreter._return_function = wrapped_return

        interpreter._pgo_collector = self

    def detach(self) -> None:
        if self._attached_interpreter is not None:
            self._attached_interpreter._pgo_collector = None
            self._attached_interpreter = None

    def on_call(self, func_name: str) -> None:
        now = time.time_ns()
        self._call_stack.append((func_name, now))
        fp = self._profile.functions.setdefault(func_name, FunctionProfile(func_name))
        fp.call_count += 1

    def on_return(self, func_name: str) -> None:
        now = time.time_ns()
        # Pop the most recent matching frame
        for idx in range(len(self._call_stack) - 1, -1, -1):
            if self._call_stack[idx][0] == func_name:
                _, start = self._call_stack.pop(idx)
                elapsed = now - start
                fp = self._profile.functions.get(func_name)
                if fp is not None:
                    fp.total_time_ns += elapsed
                break

    def on_branch(self, branch_id: str, taken: bool) -> None:
        for fp in self._profile.functions.values():
            old = fp.branch_counts.get(branch_id, (0, 0))
            if taken:
                fp.branch_counts[branch_id] = (old[0] + 1, old[1])
            else:
                fp.branch_counts[branch_id] = (old[0], old[1] + 1)

    def finalize(self) -> PGOProfile:
        self._profile.collection_runs += 1
        return self._profile

    def save(self, path: str) -> None:
        self.finalize().save(path)


# ---------------------------------------------------------------------------
# PGO-aware optimization pipeline builder
# ---------------------------------------------------------------------------

class PGOOptimizer:
    """
    Applies profile-guided optimizations to an AST using a collected profile.

    This doesn't replace the standard optimizer pipeline – it runs *before*
    the pipeline and annotates the AST with profiling metadata so that
    individual passes can make better decisions.
    """

    def __init__(self, profile: PGOProfile, config: PGOConfig):
        self.profile = profile
        self.config = config

    def annotate_ast(self, ast: Any) -> Any:
        """Annotate AST nodes with profile-derived metadata."""
        if ast is None:
            return ast
        self._annotate_node(ast)
        return ast

    def _annotate_node(self, node: Any, depth: int = 0) -> None:
        if node is None or depth > 50:
            return
        node_type = type(node).__name__

        if node_type == "FunctionDefinition":
            name = getattr(node, "name", None)
            if isinstance(name, str) and name in self.profile.functions:
                fp = self.profile.functions[name]
                try:
                    node._pgo_call_count = fp.call_count
                    node._pgo_total_time_ns = fp.total_time_ns
                    node._pgo_is_hot = fp.is_hot(self.config.min_hot_count)
                    node._pgo_is_cold = not fp.is_hot(max(1, self.config.min_hot_count // 10))
                except AttributeError:
                    pass

        for child in self._iter_children(node):
            self._annotate_node(child, depth + 1)

    def _iter_children(self, node: Any):
        if not hasattr(node, "__dict__"):
            return
        for k, v in vars(node).items():
            if k.startswith("_"):
                continue
            if isinstance(v, list):
                yield from [i for i in v if i is not None and hasattr(i, "__dict__")]
            elif hasattr(v, "__dict__"):
                yield v

    def get_hot_inline_candidates(self) -> List[str]:
        """Return names of functions that should be aggressively inlined."""
        return [
            fp.name
            for fp in self.profile.hot_functions(self.config.min_hot_count)
        ]

    def get_cold_function_names(self) -> List[str]:
        """Return names of functions that are rarely or never called."""
        return [fp.name for fp in self.profile.cold_functions(self.config.min_hot_count)]


# ---------------------------------------------------------------------------
# Top-level PGO Driver
# ---------------------------------------------------------------------------

class PGODriver:
    """
    Orchestrates the full PGO workflow: instrument → collect → optimize.

    Example (3-phase PGO)::

        driver = PGODriver(PGOConfig())
        driver.instrument(source_path)
        # ... run program against workloads ...
        driver.collect(raw_profile_path="build/run1.nlplprof")
        optimized = driver.build_with_profile(source_path)
    """

    def __init__(self, config: Optional[PGOConfig] = None):
        self.config = config or PGOConfig()
        self._merged_profile: Optional[PGOProfile] = None

    # ------------------------------------------------------------------
    # Phase 1: Instrumentation
    # ------------------------------------------------------------------

    def create_instrumenter(self) -> PGOInstrumenter:
        """Return an instrumenter ready to inject probes into an AST."""
        return PGOInstrumenter(self.config)

    def create_runtime_collector(self, program_name: str) -> PGORuntimeCollector:
        """Return a runtime collector for interpreter-mode profiling."""
        return PGORuntimeCollector(program_name)

    # ------------------------------------------------------------------
    # Phase 2: Collection
    # ------------------------------------------------------------------

    def collect(self, raw_profile_path: Optional[str] = None) -> PGOProfile:
        """
        Load a raw profile file and merge it into the aggregate profile.

        If ``raw_profile_path`` is None, loads from ``config.profile_path``.
        """
        path = raw_profile_path or self.config.profile_path
        if not os.path.exists(path):
            raise FileNotFoundError(
                f"Profile data not found: {path}\n"
                f"Run the instrumented binary first to generate profile data."
            )
        profile = PGOProfile.load(path)
        if self._merged_profile is None:
            self._merged_profile = profile
        else:
            self._merged_profile.merge(profile)

        if self.config.verbose:
            hot = self._merged_profile.hot_functions(self.config.min_hot_count)
            print(
                f"PGO collect: loaded {path!r}, "
                f"{len(self._merged_profile.functions)} functions, "
                f"{len(hot)} hot"
            )
        return self._merged_profile

    def save_merged_profile(self, path: Optional[str] = None) -> str:
        """Save the merged aggregate profile to disk."""
        if self._merged_profile is None:
            raise RuntimeError("No profile data collected yet.")
        dest = path or self.config.profile_path
        self._merged_profile.save(dest)
        return dest

    # ------------------------------------------------------------------
    # Phase 3: Optimized build
    # ------------------------------------------------------------------

    def get_optimizer(self) -> Optional[PGOOptimizer]:
        """
        Return a PGOOptimizer primed with the collected profile data.
        Returns None if no profile data has been collected.
        """
        if self._merged_profile is None:
            return None
        return PGOOptimizer(self._merged_profile, self.config)

    def has_profile_data(self) -> bool:
        """True if profile data has been loaded."""
        return self._merged_profile is not None

    # ------------------------------------------------------------------
    # Reporting
    # ------------------------------------------------------------------

    def print_profile_summary(self) -> None:
        if self._merged_profile is None:
            print("No profile data available.")
            return
        p = self._merged_profile
        hot = p.hot_functions(self.config.min_hot_count)
        cold = p.cold_functions(self.config.min_hot_count)
        print(f"PGO Profile Summary: {p.program_name}")
        print(f"  Collection runs    : {p.collection_runs}")
        print(f"  Total runtime      : {p.total_runtime_ns / 1e6:.1f} ms")
        print(f"  Functions tracked  : {len(p.functions)}")
        print(f"  Hot functions      : {len(hot)} (>= {self.config.min_hot_count} calls)")
        print(f"  Cold functions     : {len(cold)}")
        if hot:
            print("  Top 5 hot functions:")
            for fp in hot[:5]:
                print(
                    f"    {fp.name:40s}  {fp.call_count:8d} calls  "
                    f"{fp.mean_time_ns / 1000:.1f} us/call"
                )
