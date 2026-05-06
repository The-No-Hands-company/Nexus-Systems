"""
FFI Debugging Tools for NexusLang.

Provides:
- Call tracing: Log every FFI call with arguments, return values, and timing
- GDB/LLDB integration: Generate debug scripts for FFI boundary inspection
- Call trace recording and replay for regression testing
- Performance profiling of FFI call overhead
- Boundary assertion hooks (pre/post conditions on FFI calls)
"""

import os
import time
import json
import threading
import traceback
import platform as _platform
from dataclasses import dataclass, field, asdict
from enum import Enum, auto
from typing import Any, Callable, Dict, List, Optional, Tuple


# ---------------------------------------------------------------------------
# Trace record types
# ---------------------------------------------------------------------------

class FFICallStatus(Enum):
    SUCCESS = "success"
    EXCEPTION = "exception"
    NULL_RETURN = "null_return"
    TIMEOUT = "timeout"


@dataclass
class FFICallRecord:
    """A single recorded FFI call."""
    call_id: int
    function_name: str
    library: Optional[str]
    arguments: List[Any]
    return_value: Any
    status: FFICallStatus
    elapsed_ns: int              # wall-clock time for the call in nanoseconds
    thread_id: int
    thread_name: str
    stack_depth: int
    pre_assertions_passed: bool  # all pre-condition checks passed
    post_assertions_passed: bool # all post-condition checks passed
    exception_info: Optional[str] = None  # traceback if status == EXCEPTION
    timestamp: float = field(default_factory=time.time)

    def to_dict(self) -> Dict[str, Any]:
        d = asdict(self)
        d["status"] = self.status.value
        # Serialize non-JSON-serializable argument types gracefully
        d["arguments"] = [_safe_repr(a) for a in self.arguments]
        d["return_value"] = _safe_repr(self.return_value)
        return d


def _safe_repr(value: Any) -> Any:
    """Convert a value to a JSON-serializable representation."""
    if value is None:
        return None
    if isinstance(value, (bool, int, float, str)):
        return value
    if isinstance(value, (bytes, bytearray)):
        return {"__bytes__": value.hex()}
    if isinstance(value, (list, tuple)):
        return [_safe_repr(v) for v in value]
    if isinstance(value, dict):
        return {str(k): _safe_repr(v) for k, v in value.items()}
    # Pointer-like int: format as hex
    if hasattr(value, "__index__"):
        try:
            return hex(int(value))
        except (TypeError, ValueError):
            pass
    return repr(value)


# ---------------------------------------------------------------------------
# Assertion hooks
# ---------------------------------------------------------------------------

AssertionFn = Callable[[str, List[Any]], Optional[str]]
"""
An assertion function takes (function_name, arguments) before the call,
or (function_name, return_value) after the call, and returns None if the
assertion passes, or an error message string if it fails.
"""


@dataclass
class AssertionHook:
    """A named pre- or post-condition assertion."""
    name: str
    function_name: str   # which FFI function this applies to ('*' = all)
    phase: str           # 'pre' or 'post'
    check: AssertionFn

    def run(self, func_name: str, values: List[Any]) -> Optional[str]:
        if self.function_name not in ("*", func_name):
            return None
        try:
            return self.check(func_name, values)
        except Exception as exc:  # noqa: BLE001
            return f"Assertion '{self.name}' raised {type(exc).__name__}: {exc}"


# ---------------------------------------------------------------------------
# Call tracer
# ---------------------------------------------------------------------------

class FFICallTracer:
    """
    Runtime call tracer that wraps FFI call sites.

    Usage::

        tracer = FFICallTracer(max_records=10_000)

        # Register a null-return assertion on malloc
        tracer.add_post_assertion("check_malloc_null", "malloc",
            lambda fn, rv: "malloc returned NULL" if rv == 0 else None)

        # Wrap a ctypes function
        wrapped_malloc = tracer.wrap_ctypes_function("malloc", libc.malloc)
        ptr = wrapped_malloc(1024)

        # Print trace
        tracer.print_summary()

        # Export
        tracer.export_json("/tmp/nxl_ffi_trace.json")
    """

    def __init__(
        self,
        max_records: int = 100_000,
        capture_stacks: bool = False,
        enabled: bool = True,
    ):
        self._lock = threading.Lock()
        self._records: List[FFICallRecord] = []
        self._call_counter = 0
        self.max_records = max_records
        self.capture_stacks = capture_stacks
        self.enabled = enabled
        self._pre_assertions: List[AssertionHook] = []
        self._post_assertions: List[AssertionHook] = []
        self._listeners: List[Callable[[FFICallRecord], None]] = []

    # ------------------------------------------------------------------
    # Configuration
    # ------------------------------------------------------------------

    def enable(self) -> None:
        self.enabled = True

    def disable(self) -> None:
        self.enabled = False

    def clear(self) -> None:
        with self._lock:
            self._records.clear()
            self._call_counter = 0

    def add_listener(self, fn: Callable[["FFICallRecord"], None]) -> None:
        """Register a callback invoked after every recorded call."""
        self._listeners.append(fn)

    def add_pre_assertion(
        self, name: str, function_name: str, check: AssertionFn
    ) -> None:
        """Add a pre-call assertion (runs before the FFI call)."""
        self._pre_assertions.append(
            AssertionHook(name=name, function_name=function_name, phase="pre", check=check)
        )

    def add_post_assertion(
        self, name: str, function_name: str, check: AssertionFn
    ) -> None:
        """Add a post-call assertion (runs after the FFI call)."""
        self._post_assertions.append(
            AssertionHook(name=name, function_name=function_name, phase="post", check=check)
        )

    # ------------------------------------------------------------------
    # Core tracing logic
    # ------------------------------------------------------------------

    def record(
        self,
        function_name: str,
        library: Optional[str],
        arguments: List[Any],
        return_value: Any,
        status: FFICallStatus,
        elapsed_ns: int,
        pre_ok: bool = True,
        post_ok: bool = True,
        exception_info: Optional[str] = None,
    ) -> FFICallRecord:
        """Record a completed FFI call."""
        thread = threading.current_thread()
        try:
            depth = len(traceback.extract_stack()) if self.capture_stacks else 0
        except Exception:  # noqa: BLE001
            depth = 0

        with self._lock:
            self._call_counter += 1
            rec = FFICallRecord(
                call_id=self._call_counter,
                function_name=function_name,
                library=library,
                arguments=arguments,
                return_value=return_value,
                status=status,
                elapsed_ns=elapsed_ns,
                thread_id=thread.ident or 0,
                thread_name=thread.name,
                stack_depth=depth,
                pre_assertions_passed=pre_ok,
                post_assertions_passed=post_ok,
                exception_info=exception_info,
            )
            if len(self._records) < self.max_records:
                self._records.append(rec)

        for listener in self._listeners:
            try:
                listener(rec)
            except Exception:  # noqa: BLE001
                pass

        return rec

    def wrap_ctypes_function(
        self,
        function_name: str,
        func: Callable[..., Any],
        library: Optional[str] = None,
    ) -> Callable[..., Any]:
        """
        Wrap a ctypes function with call tracing.

        Returns a new callable that records every invocation.
        """
        tracer = self

        def traced(*args: Any, **kwargs: Any) -> Any:
            if not tracer.enabled:
                return func(*args, **kwargs)

            arg_list = list(args) + list(kwargs.values())

            # Pre-assertions
            pre_ok = True
            for hook in tracer._pre_assertions:
                msg = hook.run(function_name, arg_list)
                if msg is not None:
                    pre_ok = False
                    tracer._warn_assertion(function_name, "pre", hook.name, msg)

            start = time.perf_counter_ns()
            status = FFICallStatus.SUCCESS
            return_value: Any = None
            exc_info: Optional[str] = None

            try:
                return_value = func(*args, **kwargs)

                # Check for NULL pointer returns (represented as 0 or None in ctypes)
                if return_value is None or return_value == 0:
                    status = FFICallStatus.NULL_RETURN

            except Exception as exc:  # noqa: BLE001
                status = FFICallStatus.EXCEPTION
                exc_info = traceback.format_exc()
                raise  # re-raise after recording
            finally:
                elapsed = time.perf_counter_ns() - start

                # Post-assertions
                post_ok = True
                for hook in tracer._post_assertions:
                    msg = hook.run(function_name, [return_value])
                    if msg is not None:
                        post_ok = False
                        tracer._warn_assertion(function_name, "post", hook.name, msg)

                tracer.record(
                    function_name=function_name,
                    library=library,
                    arguments=arg_list,
                    return_value=return_value,
                    status=status,
                    elapsed_ns=elapsed,
                    pre_ok=pre_ok,
                    post_ok=post_ok,
                    exception_info=exc_info,
                )

            return return_value

        traced.__name__ = f"traced_{function_name}"
        traced.__qualname__ = f"traced_{function_name}"
        return traced

    def _warn_assertion(
        self, func_name: str, phase: str, hook_name: str, message: str
    ) -> None:
        import warnings
        warnings.warn(
            f"FFI assertion '{hook_name}' ({phase}) failed on '{func_name}': {message}",
            stacklevel=4,
        )

    # ------------------------------------------------------------------
    # Query
    # ------------------------------------------------------------------

    @property
    def records(self) -> List[FFICallRecord]:
        with self._lock:
            return list(self._records)

    def calls_to(self, function_name: str) -> List[FFICallRecord]:
        return [r for r in self.records if r.function_name == function_name]

    def failed_calls(self) -> List[FFICallRecord]:
        return [
            r for r in self.records
            if r.status not in (FFICallStatus.SUCCESS, FFICallStatus.NULL_RETURN)
            or not r.pre_assertions_passed or not r.post_assertions_passed
        ]

    def null_returns(self) -> List[FFICallRecord]:
        return [r for r in self.records if r.status == FFICallStatus.NULL_RETURN]

    def slowest_calls(self, top_n: int = 10) -> List[FFICallRecord]:
        return sorted(self.records, key=lambda r: r.elapsed_ns, reverse=True)[:top_n]

    # ------------------------------------------------------------------
    # Reporting
    # ------------------------------------------------------------------

    def print_summary(self) -> None:
        records = self.records
        if not records:
            print("FFI trace: no calls recorded.")
            return

        total = len(records)
        failed = len(self.failed_calls())
        null_ret = len(self.null_returns())
        total_ns = sum(r.elapsed_ns for r in records)

        print(f"=== FFI Call Trace Summary ===")
        print(f"  Total calls     : {total}")
        print(f"  Failed calls    : {failed}")
        print(f"  NULL returns    : {null_ret}")
        print(f"  Total wall time : {total_ns / 1_000_000:.3f} ms")
        print()

        # Per-function stats
        by_func: Dict[str, List[FFICallRecord]] = {}
        for rec in records:
            by_func.setdefault(rec.function_name, []).append(rec)

        print(f"  {'Function':<30} {'Calls':>6} {'Total ms':>10} {'Avg us':>10} {'Errors':>7}")
        print(f"  {'-'*30} {'-'*6} {'-'*10} {'-'*10} {'-'*7}")
        for fname, recs in sorted(by_func.items()):
            n = len(recs)
            total_f_ns = sum(r.elapsed_ns for r in recs)
            avg_us = (total_f_ns / n) / 1000
            errors = sum(
                1 for r in recs
                if r.status == FFICallStatus.EXCEPTION
                or not r.pre_assertions_passed
                or not r.post_assertions_passed
            )
            print(f"  {fname:<30} {n:>6} {total_f_ns / 1_000_000:>10.3f} {avg_us:>10.2f} {errors:>7}")

    def export_json(self, path: str) -> None:
        """Write the full trace to a JSON file."""
        data = [r.to_dict() for r in self.records]
        with open(path, "w", encoding="utf-8") as fh:
            json.dump(data, fh, indent=2)
        print(f"FFI trace exported to {path} ({len(data)} records)")

    def import_json(self, path: str) -> None:
        """Load a previously exported trace (for comparison / regression)."""
        with open(path, encoding="utf-8") as fh:
            data = json.load(fh)

        with self._lock:
            for d in data:
                d["status"] = FFICallStatus(d["status"])
                rec = FFICallRecord(**d)
                self._records.append(rec)


# ---------------------------------------------------------------------------
# GDB/LLDB script generator
# ---------------------------------------------------------------------------

class GDBScriptGenerator:
    """
    Generates GDB command scripts for FFI boundary debugging.

    Produced scripts can be loaded with:   gdb -x script.gdb ./program
    """

    def __init__(self, symbol_prefix: str = "nxl_ffi_"):
        self.symbol_prefix = symbol_prefix
        self._breakpoints: List[Tuple[str, str]] = []   # (symbol, command)
        self._watchpoints: List[Tuple[str, str]] = []   # (expr, command)

    def add_function_breakpoint(
        self,
        function_name: str,
        log_args: bool = True,
        log_return: bool = True,
        condition: Optional[str] = None,
    ) -> None:
        """
        Add a GDB breakpoint on a C function entry/exit with auto-logging.

        Args:
            function_name: Symbol name to break on.
            log_args: Whether to log function arguments.
            log_return: Whether to log the return value.
            condition: Optional GDB condition expression.
        """
        cmds = [f'echo [FFI] Entering {function_name}\\n']
        if log_args:
            cmds.append('info args')
        if condition:
            entry_bp = (
                f"break {function_name}\n"
                f"commands\n"
                f"  {'\\n  '.join(cmds)}\n"
                f"  continue\n"
                f"end\n"
                f"condition $bpnum {condition}"
            )
        else:
            entry_bp = (
                f"break {function_name}\n"
                f"commands\n"
                f"  {'\\n  '.join(cmds)}\n"
                f"  continue\n"
                f"end"
            )

        self._breakpoints.append((function_name, entry_bp))

        if log_return:
            return_bp = (
                f"# Return breakpoint for {function_name}\n"
                f"break {function_name}\n"
                f"commands\n"
                f"  finish\n"
                f"  echo [FFI] Returned from {function_name}: \\n\n"
                f"  print $retval\n"
                f"  continue\n"
                f"end"
            )
            self._breakpoints.append((f"{function_name}_ret", return_bp))

    def add_watchpoint(self, expression: str, condition: Optional[str] = None) -> None:
        """Add a GDB watchpoint on a variable/expression."""
        cmd = f"watch {expression}"
        if condition:
            cmd += f"\ncondition $bpnum {condition}"
        self._watchpoints.append((expression, cmd))

    def generate(self, program_path: str) -> str:
        """
        Generate the full GDB script content.

        Args:
            program_path: Path to the NLPL-compiled program binary.

        Returns:
            GDB script as a string.
        """
        lines = [
            f"# GDB script generated by NexusLang FFI debugger",
            f"# Load with: gdb -x <this_file> {program_path}",
            f"",
            f"set pagination off",
            f"set logging on nxl_ffi_debug.gdb.log",
            f"set logging overwrite on",
            f"",
            f"file {program_path}",
            f"",
            f"# ---- Watchpoints ----",
        ]

        for expr, cmd in self._watchpoints:
            lines.append(f"# Watch: {expr}")
            lines.append(cmd)
            lines.append("")

        lines.append("# ---- Breakpoints ----")
        for symbol, cmd in self._breakpoints:
            lines.append(f"# Break: {symbol}")
            lines.append(cmd)
            lines.append("")

        lines += [
            "# ---- Run ----",
            "run",
            "",
            "# ---- Post-mortem ----",
            "echo \\n=== NexusLang FFI Debug Session Complete ===\\n",
            "info breakpoints",
            "quit",
        ]

        return "\n".join(lines)

    def write(self, path: str, program_path: str) -> None:
        """Write GDB script to a file."""
        content = self.generate(program_path)
        with open(path, "w", encoding="utf-8") as fh:
            fh.write(content)
        print(f"GDB script written to {path}")


class LLDBScriptGenerator:
    """
    Generates LLDB Python scripts for FFI boundary debugging.

    Produced scripts can be loaded with:
        lldb -s script.lldb -- ./program
    """

    def __init__(self):
        self._breakpoints: List[Tuple[str, str, Optional[str]]] = []   # (name, action, condition)

    def add_function_breakpoint(
        self,
        function_name: str,
        log_args: bool = True,
        condition: Optional[str] = None,
    ) -> None:
        """Add an LLDB breakpoint on a named C function."""
        action = (
            f'br set -n "{function_name}"'
        )
        self._breakpoints.append((function_name, action, condition))

    def generate(self, program_path: str) -> str:
        """
        Generate the full LLDB script content.

        Args:
            program_path: Path to the NLPL-compiled program binary.

        Returns:
            LLDB script as a string.
        """
        lines = [
            f"# LLDB script generated by NexusLang FFI debugger",
            f"# Load with: lldb -s <this_file> -- {program_path}",
            f"",
            f"file {program_path}",
            f"",
        ]

        for func_name, action, condition in self._breakpoints:
            lines.append(f"# Breakpoint: {func_name}")
            lines.append(action)
            if condition:
                lines.append(f"br modify -c '{condition}' -G true")
            else:
                lines.append("br modify -G true")
                lines.append(f"br command add -o 'frame variable' -o 'continue'")
            lines.append("")

        lines += [
            "process launch",
            "",
            "# When execution stops at a bug:",
            "# bt        -- backtrace",
            "# frame var -- local variables",
            "# register read -- register state",
            "quit",
        ]

        return "\n".join(lines)

    def write(self, path: str, program_path: str) -> None:
        """Write LLDB script to a file."""
        content = self.generate(program_path)
        with open(path, "w", encoding="utf-8") as fh:
            fh.write(content)
        print(f"LLDB script written to {path}")


# ---------------------------------------------------------------------------
# Valgrind script helper
# ---------------------------------------------------------------------------

def generate_valgrind_command(
    program_path: str,
    program_args: Optional[List[str]] = None,
    output_xml: Optional[str] = None,
    track_origins: bool = True,
    leak_check: str = "full",
) -> str:
    """
    Generate a Valgrind memcheck command for FFI debugging.

    Args:
        program_path: Path to the compiled NexusLang program.
        program_args: Optional list of program arguments.
        output_xml: Optional path for XML output (for CI integration).
        track_origins: Whether to track uninitialised value origins.
        leak_check: Leak check level ('full', 'summary', 'no').

    Returns:
        Shell command string.
    """
    args = ["valgrind", "--tool=memcheck"]
    args.append(f"--leak-check={leak_check}")

    if track_origins:
        args.append("--track-origins=yes")

    args += [
        "--error-exitcode=1",
        "--show-error-list=yes",
        "--num-callers=20",
        "--suppressions=/dev/null",  # Replace with NLPL-specific suppressions file
    ]

    if output_xml:
        args += ["--xml=yes", f"--xml-file={output_xml}"]

    args.append(program_path)

    if program_args:
        args.extend(program_args)

    return " ".join(args)


# ---------------------------------------------------------------------------
# High-level FFI debugger facade
# ---------------------------------------------------------------------------

class FFIDebugger:
    """
    High-level FFI debugging facade.

    Combines the call tracer, GDB/LLDB generators, and assertion hooks
    into a single convenience API.

    Usage::

        dbg = FFIDebugger(program_path="/path/to/program")

        # Enable call tracing
        dbg.tracer.add_post_assertion("no_null_malloc", "malloc",
            lambda fn, rv: "malloc returned NULL" if rv == 0 else None)

        # Generate debug scripts
        dbg.write_gdb_script("/tmp/ffi_debug.gdb")
        dbg.write_lldb_script("/tmp/ffi_debug.lldb")

        # Wrap ctypes calls
        import ctypes
        libc = ctypes.CDLL(ctypes.util.find_library('c'))
        traced_malloc = dbg.trace(libc.malloc, "malloc")

        # After the program runs
        dbg.tracer.print_summary()
        dbg.tracer.export_json("/tmp/ffi_trace.json")
    """

    def __init__(
        self,
        program_path: str = "./program",
        capture_stacks: bool = False,
        enabled: bool = True,
    ):
        self.program_path = program_path
        self.tracer = FFICallTracer(capture_stacks=capture_stacks, enabled=enabled)
        self._gdb = GDBScriptGenerator()
        self._lldb = LLDBScriptGenerator()
        self._watched_functions: List[str] = []

    def watch_function(
        self,
        function_name: str,
        log_args: bool = True,
        log_return: bool = True,
    ) -> None:
        """Add a function to the GDB/LLDB watch list and the tracer."""
        self._watched_functions.append(function_name)
        self._gdb.add_function_breakpoint(function_name, log_args=log_args, log_return=log_return)
        self._lldb.add_function_breakpoint(function_name, log_args=log_args)

    def trace(
        self,
        func: Callable[..., Any],
        function_name: str,
        library: Optional[str] = None,
    ) -> Callable[..., Any]:
        """Wrap a callable for runtime call tracing."""
        return self.tracer.wrap_ctypes_function(function_name, func, library=library)

    def write_gdb_script(self, path: str) -> None:
        self._gdb.write(path, self.program_path)

    def write_lldb_script(self, path: str) -> None:
        self._lldb.write(path, self.program_path)

    def print_valgrind_command(self, output_xml: Optional[str] = None) -> None:
        cmd = generate_valgrind_command(
            self.program_path, output_xml=output_xml
        )
        print(f"Valgrind command:\n  {cmd}")

    def summary(self) -> None:
        self.tracer.print_summary()

    def export_trace(self, path: str) -> None:
        self.tracer.export_json(path)
