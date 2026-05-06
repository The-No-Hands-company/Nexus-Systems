"""
NLPL Security Analysis Module

Static and dynamic analysis tools for detecting security issues in NexusLang programs:

1. Taint Analysis
   - Track values originating from untrusted sources (user input, env vars,
     network data, FFI return values) through the execution graph.
   - Flag tainted values that flow into dangerous sinks (shell execution,
     file writes, format strings, SQL queries).

2. Control Flow Integrity (CFI)
   - Build a valid call graph during interpretation.
   - Verify that function-pointer style invocations only target registered
     callable objects.
   - Detect unexpected return-address corruption in interpreter frames.

3. Memory Safety Validation
   - Validate bounds on all array/buffer accesses.
   - Detect use-after-free patterns in the managed memory layer.
   - Report off-by-one errors at access time.

All analysers are **non-fatal by default**: violations produce
AnalysisViolation exceptions (or warnings depending on policy) rather than
crashing the interpreter outright.  The execution policy is configurable via
AnalysisPolicy.
"""

from __future__ import annotations

import sys
import enum
import functools
import threading
import weakref
from dataclasses import dataclass, field
from typing import Any, Callable, Dict, FrozenSet, List, Optional, Set, Tuple


# =============================================================================
# Shared exceptions
# =============================================================================

class AnalysisViolation(Exception):
    """Base class for all analysis-detected security violations."""

    def __init__(self, message: str, *, kind: str = "violation",
                 location: Optional[str] = None):
        super().__init__(message)
        self.kind = kind
        self.location = location


class TaintViolation(AnalysisViolation):
    """Raised when a tainted value flows into a dangerous sink."""

    def __init__(self, message: str, source_label: "TaintLabel",
                 sink: str, *, location: Optional[str] = None):
        super().__init__(message, kind="taint", location=location)
        self.source_label = source_label
        self.sink = sink


class CFIViolation(AnalysisViolation):
    """Raised when a call targets an unregistered or unexpected destination."""

    def __init__(self, message: str, target: Any, *,
                 location: Optional[str] = None):
        super().__init__(message, kind="cfi", location=location)
        self.target = target


class MemorySafetyViolation(AnalysisViolation):
    """Raised when a memory-safety invariant is broken."""

    def __init__(self, message: str, *, kind: str = "memory_safety",
                 location: Optional[str] = None):
        super().__init__(message, kind=kind, location=location)


# =============================================================================
# Analysis Policy
# =============================================================================

class ViolationPolicy(enum.Enum):
    """
    How to handle detected violations.

    RAISE  -- raise AnalysisViolation immediately (strict mode, default).
    WARN   -- print a warning to stderr and continue execution.
    LOG    -- record the violation in the analyser log and continue silently.
    IGNORE -- completely ignore violations (analysis disabled).
    """
    RAISE = "raise"
    WARN = "warn"
    LOG = "log"
    IGNORE = "ignore"


@dataclass
class AnalysisPolicy:
    """Global configuration for all analysers."""

    taint_policy: ViolationPolicy = ViolationPolicy.RAISE
    cfi_policy: ViolationPolicy = ViolationPolicy.RAISE
    memory_policy: ViolationPolicy = ViolationPolicy.RAISE

    #: If True, log is populated regardless of policy.
    always_log: bool = False


_DEFAULT_POLICY: AnalysisPolicy = AnalysisPolicy()
_policy_lock = threading.Lock()


def get_analysis_policy() -> AnalysisPolicy:
    """Return the current global analysis policy."""
    return _DEFAULT_POLICY


def set_analysis_policy(policy: AnalysisPolicy) -> None:
    """Replace the global analysis policy."""
    global _DEFAULT_POLICY
    with _policy_lock:
        _DEFAULT_POLICY = policy


def _handle_violation(violation: AnalysisViolation, policy: ViolationPolicy,
                      log: List[AnalysisViolation]) -> None:
    """Apply the configured violation policy."""
    if get_analysis_policy().always_log:
        log.append(violation)

    if policy == ViolationPolicy.RAISE:
        raise violation
    elif policy == ViolationPolicy.WARN:
        print(f"[NLPL security warning] {violation}", file=sys.stderr)
    elif policy == ViolationPolicy.LOG:
        log.append(violation)
    # IGNORE: do nothing


# =============================================================================
# Taint Analysis
# =============================================================================

class TaintLabel(enum.Enum):
    """
    Labels describing where a tainted value originated.

    Higher numeric values represent more dangerous (less trusted) sources.
    """
    TRUSTED = 0          # Internal, developer-controlled value
    FILE_READ = 10       # Data read from an external file
    ENV_VAR = 20         # Value from os.environ or NexusLang env access
    FFI_RETURN = 30      # Value returned by a C/foreign function
    NETWORK = 40         # Data received from a network socket
    USER_INPUT = 50      # Direct user input (stdin, argv, prompts)

    @property
    def is_untrusted(self) -> bool:
        """True for any label that may carry attacker-controlled data."""
        return self.value >= TaintLabel.FILE_READ.value

    @classmethod
    def most_dangerous(cls, labels: "FrozenSet[TaintLabel]") -> "TaintLabel":
        """Return the label with the highest danger level from a set."""
        if not labels:
            return cls.TRUSTED
        return max(labels, key=lambda l: l.value)


@dataclass
class TaintedValue:
    """
    A wrapper that associates a runtime value with one or more taint labels.

    Propagation rules (conservative):
    - Any operation on a TaintedValue produces a TaintedValue.
    - The result inherits the union of all input labels.
    - TRUSTED values are not wrapped; only untrusted sources create wrappers.

    Attributes:
        value:    The underlying runtime value.
        labels:   The set of taint labels attached to this value.
        sources:  Human-readable descriptions of where the taint came from.
    """
    value: Any
    labels: FrozenSet[TaintLabel] = field(
        default_factory=lambda: frozenset({TaintLabel.TRUSTED})
    )
    sources: Tuple[str, ...] = field(default_factory=tuple)

    @classmethod
    def from_source(cls, value: Any, label: TaintLabel,
                    source: str = "") -> "TaintedValue":
        """Create a tainted value from a specific source."""
        return cls(
            value=value,
            labels=frozenset({label}),
            sources=(source,) if source else (),
        )

    def propagate(self, new_value: Any,
                  *others: "TaintedValue") -> "TaintedValue":
        """
        Propagate taint from this value (and optionally others) to new_value.

        Use this when computing derived values:
            result = a.propagate(a.value + b.value, b)
        """
        combined_labels: Set[TaintLabel] = set(self.labels)
        combined_sources: List[str] = list(self.sources)

        for other in others:
            if isinstance(other, TaintedValue):
                combined_labels.update(other.labels)
                combined_sources.extend(other.sources)

        return TaintedValue(
            value=new_value,
            labels=frozenset(combined_labels),
            sources=tuple(dict.fromkeys(combined_sources)),  # deduplicate
        )

    @property
    def dominant_label(self) -> TaintLabel:
        """The most dangerous label on this value."""
        return TaintLabel.most_dangerous(self.labels)

    @property
    def is_tainted(self) -> bool:
        """True if any label is untrusted."""
        return any(lbl.is_untrusted for lbl in self.labels)

    def __repr__(self) -> str:
        return (
            f"TaintedValue({self.value!r}, "
            f"labels={{{', '.join(l.name for l in self.labels)}}}, "
            f"sources={self.sources})"
        )


def unwrap(value: Any) -> Any:
    """Return the underlying value, stripping any TaintedValue wrapper."""
    if isinstance(value, TaintedValue):
        return value.value
    return value


def taint_label_of(value: Any) -> TaintLabel:
    """Return the dominant taint label of a value (TRUSTED if not tainted)."""
    if isinstance(value, TaintedValue):
        return value.dominant_label
    return TaintLabel.TRUSTED


def is_tainted(value: Any) -> bool:
    """Return True if value carries any untrusted taint."""
    return isinstance(value, TaintedValue) and value.is_tainted


class TaintSink(enum.Enum):
    """Categories of dangerous sinks where tainted data must not flow."""
    SHELL_EXEC = "shell_exec"
    FILE_WRITE = "file_write"
    FORMAT_STRING = "format_string"
    SQL_QUERY = "sql_query"
    DYNAMIC_IMPORT = "dynamic_import"
    FFI_CALL_ARG = "ffi_call_arg"
    EVAL = "eval"
    NETWORK_SEND = "network_send"


class TaintTracker:
    """
    Runtime taint tracker.

    Usage in the interpreter:

        tracker = TaintTracker()

        # Mark a value as tainted when reading from stdin:
        user_val = tracker.taint(input(), TaintLabel.USER_INPUT, "stdin")

        # Check before passing to a dangerous sink:
        tracker.check_sink(user_val, TaintSink.SHELL_EXEC, location="line 42")

        # Propagate taint through computations:
        result = tracker.propagate(user_val + "_suffix", user_val)
    """

    def __init__(self, policy: Optional[AnalysisPolicy] = None) -> None:
        self._policy = policy
        self._violations: List[TaintViolation] = []
        # Weak-key dict: tracks which objects have been tainted in this session
        self._taint_map: Dict[int, TaintedValue] = {}

    def _current_policy(self) -> AnalysisPolicy:
        return self._policy if self._policy is not None else get_analysis_policy()

    def taint(self, value: Any, label: TaintLabel,
              source: str = "") -> TaintedValue:
        """
        Mark a value as tainted and return a TaintedValue wrapper.

        Args:
            value:  The runtime value to taint.
            label:  The taint label describing the source.
            source: Optional human-readable description of the source.

        Returns:
            A TaintedValue wrapping the original value.
        """
        tv = TaintedValue.from_source(value, label, source)
        self._taint_map[id(tv)] = tv
        return tv

    def propagate(self, new_value: Any, *parents: Any) -> Any:
        """
        Propagate taint from one or more parent values to new_value.

        If none of the parents are tainted, new_value is returned as-is.
        Otherwise, returns a TaintedValue with the union of parent labels.

        Args:
            new_value: The computed result value.
            *parents:  The input values (may or may not be TaintedValues).

        Returns:
            new_value wrapped in TaintedValue if any parent is tainted,
            otherwise new_value unchanged.
        """
        tainted_parents = [p for p in parents if isinstance(p, TaintedValue)]
        if not tainted_parents:
            return new_value

        first = tainted_parents[0]
        return first.propagate(new_value, *tainted_parents[1:])

    def check_sink(self, value: Any, sink: TaintSink,
                   location: Optional[str] = None) -> None:
        """
        Assert that value is clean before it flows into sink.

        Args:
            value:    The value being passed to the sink.
            sink:     The category of the dangerous sink.
            location: Human-readable location (e.g., "line 42").

        Raises:
            TaintViolation: If the value is tainted and policy is RAISE.
        """
        if not isinstance(value, TaintedValue) or not value.is_tainted:
            return

        label = value.dominant_label
        sources_desc = ", ".join(value.sources) if value.sources else "unknown"
        message = (
            f"Tainted value ({label.name} from {sources_desc}) "
            f"flowed into sink '{sink.value}'"
            + (f" at {location}" if location else "")
        )
        violation = TaintViolation(
            message, source_label=label, sink=sink.value, location=location
        )
        _handle_violation(
            violation,
            self._current_policy().taint_policy,
            self._violations,
        )

    def check_sinks(self, values: List[Any], sink: TaintSink,
                    location: Optional[str] = None) -> None:
        """Check multiple values against the same sink."""
        for v in values:
            self.check_sink(v, sink, location=location)

    @property
    def violations(self) -> List[TaintViolation]:
        """All recorded taint violations (even if not raised)."""
        return list(self._violations)

    def reset(self) -> None:
        """Clear accumulated violation log and taint state."""
        self._violations.clear()
        self._taint_map.clear()


# =============================================================================
# Control Flow Integrity (CFI)
# =============================================================================

@dataclass
class CallSite:
    """Metadata about a registered call site."""
    name: str                          # Symbolic name (function name / var name)
    valid_targets: Set[int]            # Set of id() of allowed callables
    location: Optional[str] = None    # Source location for diagnostics


class CallGraph:
    """
    Records the valid call graph during interpretation.

    The interpreter must register callable objects (functions, closures,
    lambdas) at definition time.  Before dispatching any indirect call,
    it verifies the callee against the registered set.

    Thread-safety: a per-interpreter lock protects the registry.
    """

    def __init__(self) -> None:
        self._lock = threading.Lock()
        # id(callable) -> human-readable name
        self._callables: Dict[int, str] = {}
        # call site name -> CallSite
        self._call_sites: Dict[str, CallSite] = {}

    def register_callable(self, func: Callable, name: str = "") -> None:
        """
        Record a callable as a legitimate invocation target.

        Args:
            func: Any callable (function, method, built-in).
            name: Human-readable name for diagnostics.
        """
        with self._lock:
            self._callables[id(func)] = name or repr(func)

    def unregister_callable(self, func: Callable) -> None:
        """Remove a callable from the valid set (e.g. after function goes out of scope)."""
        with self._lock:
            self._callables.pop(id(func), None)

    def register_call_site(self, site_name: str,
                           valid_targets: List[Callable],
                           location: Optional[str] = None) -> None:
        """
        Register a call site with its explicitly allowed targets.

        Args:
            site_name:     Name identifying the call site (e.g. the NexusLang variable name).
            valid_targets: The callables that are authorized at this site.
            location:      Source location for diagnostics.
        """
        with self._lock:
            target_ids = {id(t) for t in valid_targets}
            for t in valid_targets:
                if id(t) not in self._callables:
                    self._callables[id(t)] = repr(t)
            self._call_sites[site_name] = CallSite(
                name=site_name,
                valid_targets=target_ids,
                location=location,
            )

    def is_registered(self, func: Callable) -> bool:
        """Return True if func has been registered as a valid callable."""
        with self._lock:
            return id(func) in self._callables

    def get_name(self, func: Callable) -> str:
        """Return the registered name for func, or '<unknown>'."""
        with self._lock:
            return self._callables.get(id(func), "<unknown>")

    def all_registered_ids(self) -> FrozenSet[int]:
        """Return a snapshot of all registered callable IDs."""
        with self._lock:
            return frozenset(self._callables.keys())


class CFIChecker:
    """
    Control flow integrity checker.

    Wraps a CallGraph and enforces CFI policy at call dispatch time.

    Example usage inside the interpreter:

        cfi = CFIChecker()
        cfi.call_graph.register_callable(my_func, "my_func")
        ...
        cfi.check_call(callee, site_name="callback", location="line 10")
    """

    def __init__(self, call_graph: Optional[CallGraph] = None,
                 policy: Optional[AnalysisPolicy] = None) -> None:
        self.call_graph = call_graph or CallGraph()
        self._policy = policy
        self._violations: List[CFIViolation] = []
        # Return-address tracking: stack of (frame_id, expected_return_name)
        self._call_stack: List[Tuple[int, str]] = []
        self._frame_counter = 0

    def _current_policy(self) -> AnalysisPolicy:
        return self._policy if self._policy is not None else get_analysis_policy()

    def check_call(self, target: Any, site_name: Optional[str] = None,
                   location: Optional[str] = None) -> None:
        """
        Verify that target is a registered, expected callable.

        Args:
            target:    The object being called.
            site_name: Symbolic name of the call site (for per-site policy).
            location:  Human-readable source location.

        Raises:
            CFIViolation: If target is not registered and policy is RAISE.
        """
        if not callable(target):
            violation = CFIViolation(
                f"CFI: attempt to call non-callable object {target!r}"
                + (f" at {location}" if location else ""),
                target=target,
                location=location,
            )
            _handle_violation(
                violation,
                self._current_policy().cfi_policy,
                self._violations,
            )
            return

        # If a specific call site is registered, check against its target set
        if site_name and site_name in self.call_graph._call_sites:
            site = self.call_graph._call_sites[site_name]
            if id(target) not in site.valid_targets:
                violation = CFIViolation(
                    f"CFI: call at site '{site_name}' targets unexpected function "
                    f"{self.call_graph.get_name(target)!r}"
                    + (f" at {location}" if location else ""),
                    target=target,
                    location=location,
                )
                _handle_violation(
                    violation,
                    self._current_policy().cfi_policy,
                    self._violations,
                )
                return

        # General check: target must be in the global callable registry
        if not self.call_graph.is_registered(target):
            violation = CFIViolation(
                f"CFI: call targets unregistered callable {target!r}"
                + (f" at {location}" if location else ""),
                target=target,
                location=location,
            )
            _handle_violation(
                violation,
                self._current_policy().cfi_policy,
                self._violations,
            )

    def enter_frame(self, function_name: str) -> int:
        """
        Record entry into a new interpreter call frame.

        Returns:
            frame_id: An integer token the caller must pass to exit_frame().
        """
        self._frame_counter += 1
        frame_id = self._frame_counter
        self._call_stack.append((frame_id, function_name))
        return frame_id

    def exit_frame(self, frame_id: int, function_name: str,
                   location: Optional[str] = None) -> None:
        """
        Verify that the frame being exited matches the expected entry point.

        Detects return-address corruption (where a frame exits as a different
        function than it entered as).

        Args:
            frame_id:      Token returned by enter_frame().
            function_name: Name of the function currently returning.
            location:      Human-readable source location.
        """
        if not self._call_stack:
            violation = CFIViolation(
                "CFI: exit_frame called on empty call stack"
                + (f" at {location}" if location else ""),
                target=function_name,
                location=location,
            )
            _handle_violation(
                violation,
                self._current_policy().cfi_policy,
                self._violations,
            )
            return

        top_id, top_name = self._call_stack[-1]
        if top_id != frame_id or top_name != function_name:
            violation = CFIViolation(
                f"CFI: return-address mismatch — expected frame {top_id}/{top_name!r}, "
                f"got {frame_id}/{function_name!r}"
                + (f" at {location}" if location else ""),
                target=function_name,
                location=location,
            )
            _handle_violation(
                violation,
                self._current_policy().cfi_policy,
                self._violations,
            )
        else:
            self._call_stack.pop()

    @property
    def violations(self) -> List[CFIViolation]:
        """All recorded CFI violations."""
        return list(self._violations)

    def reset(self) -> None:
        """Clear accumulated violation log and frame stack."""
        self._violations.clear()
        self._call_stack.clear()


# =============================================================================
# Memory Safety Validation
# =============================================================================

class BoundsError(MemorySafetyViolation):
    """Raised on out-of-bounds array/buffer access."""

    def __init__(self, index: int, size: int, *,
                 location: Optional[str] = None):
        super().__init__(
            f"Out-of-bounds access: index {index} on buffer of size {size}"
            + (f" at {location}" if location else ""),
            kind="bounds_error",
            location=location,
        )
        self.index = index
        self.size = size


class UseAfterFreeError(MemorySafetyViolation):
    """Raised when a freed memory address is accessed."""

    def __init__(self, address: int, *, location: Optional[str] = None):
        super().__init__(
            f"Use-after-free: address 0x{address:016x} has already been freed"
            + (f" at {location}" if location else ""),
            kind="use_after_free",
            location=location,
        )
        self.address = address


class MemorySafetyValidator:
    """
    Validates memory safety invariants at interpreter runtime.

    This validator wraps the NexusLang interpreter's managed-memory layer
    (MemoryManager / MemoryAddress from runtime/memory.py) and enforces:

    - Bounds checking on all list/buffer index accesses.
    - Use-after-free detection for manually allocated blocks.
    - Integer overflow detection for index computations.

    Integration point: before any managed-array read/write, call:

        validator.check_bounds(index, len(array), location="line N")
        validator.check_not_freed(address, location="line N")
    """

    def __init__(self, policy: Optional[AnalysisPolicy] = None) -> None:
        self._policy = policy
        self._violations: List[MemorySafetyViolation] = []
        self._freed_addresses: Set[int] = set()

    def _current_policy(self) -> AnalysisPolicy:
        return self._policy if self._policy is not None else get_analysis_policy()

    # ------------------------------------------------------------------
    # Bounds checking
    # ------------------------------------------------------------------

    def check_bounds(self, index: int, size: int,
                     location: Optional[str] = None) -> None:
        """
        Assert that index is within [0, size).

        Args:
            index:    The integer index being accessed.
            size:     The size of the buffer / list / array.
            location: Human-readable source location.

        Raises:
            BoundsError: If index is out of bounds and policy is RAISE.
        """
        # Accept negative Python-style indices
        if size > 0 and -size <= index < size:
            return

        # Empty buffer: any access is OOB
        if size == 0:
            violation = BoundsError(index, size, location=location)
            _handle_violation(
                violation,
                self._current_policy().memory_policy,
                self._violations,
            )
            return

        if index < -size or index >= size:
            violation = BoundsError(index, size, location=location)
            _handle_violation(
                violation,
                self._current_policy().memory_policy,
                self._violations,
            )

    # ------------------------------------------------------------------
    # Use-after-free detection
    # ------------------------------------------------------------------

    def record_free(self, address: int) -> None:
        """
        Record that a memory address has been freed.

        Subsequent calls to check_not_freed(address) will raise UseAfterFreeError.
        """
        self._freed_addresses.add(address)

    def record_alloc(self, address: int) -> None:
        """
        Record a new allocation, clearing any previous free record for
        this address (handles re-allocation of the same address).
        """
        self._freed_addresses.discard(address)

    def check_not_freed(self, address: int,
                        location: Optional[str] = None) -> None:
        """
        Assert that address has not been freed.

        Args:
            address:  The integer memory address being accessed.
            location: Human-readable source location.

        Raises:
            UseAfterFreeError: If the address was freed and policy is RAISE.
        """
        if address in self._freed_addresses:
            violation = UseAfterFreeError(address, location=location)
            _handle_violation(
                violation,
                self._current_policy().memory_policy,
                self._violations,
            )

    # ------------------------------------------------------------------
    # Integer overflow (index computation guard)
    # ------------------------------------------------------------------

    def check_index_no_overflow(self, index: Any,
                                 location: Optional[str] = None) -> None:
        """
        Warn if the computed index is not an integer (type confusion guard).

        Args:
            index:    The computed index value.
            location: Human-readable source location.
        """
        if not isinstance(index, int):
            violation = MemorySafetyViolation(
                f"Array index must be an integer, got {type(index).__name__}: {index!r}"
                + (f" at {location}" if location else ""),
                kind="type_confusion",
                location=location,
            )
            _handle_violation(
                violation,
                self._current_policy().memory_policy,
                self._violations,
            )

    @property
    def violations(self) -> List[MemorySafetyViolation]:
        """All recorded memory safety violations."""
        return list(self._violations)

    def reset(self) -> None:
        """Clear accumulated violation log and freed-address set."""
        self._violations.clear()
        self._freed_addresses.clear()


# =============================================================================
# Convenience: combined analysis context
# =============================================================================

class SecurityAnalyser:
    """
    Aggregates TaintTracker, CFIChecker, and MemorySafetyValidator into a
    single object for use inside the interpreter.

    Usage in Interpreter.__init__():

        from nexuslang.security.analysis import SecurityAnalyser
        self.security_analyser = SecurityAnalyser()

    Then at call dispatch:
        self.security_analyser.cfi.check_call(callee, location=loc)

    When reading from stdin:
        val = self.security_analyser.taint.taint(
            raw, TaintLabel.USER_INPUT, "stdin"
        )
    """

    def __init__(self, policy: Optional[AnalysisPolicy] = None) -> None:
        self._policy = policy
        self.taint = TaintTracker(policy)
        self.cfi = CFIChecker(policy=policy)
        self.memory = MemorySafetyValidator(policy)

    def all_violations(self) -> Dict[str, List[AnalysisViolation]]:
        """Return all violations grouped by analyser type."""
        return {
            "taint": self.taint.violations,
            "cfi": self.cfi.violations,
            "memory": self.memory.violations,
        }

    def has_violations(self) -> bool:
        """True if any analyser recorded a violation."""
        return bool(
            self.taint.violations
            or self.cfi.violations
            or self.memory.violations
        )

    def reset(self) -> None:
        """Clear all accumulated state."""
        self.taint.reset()
        self.cfi.reset()
        self.memory.reset()

    def report(self, *, file=sys.stderr) -> None:
        """Print a human-readable summary of all violations to file."""
        all_v = self.all_violations()
        total = sum(len(v) for v in all_v.values())

        if total == 0:
            print("[security] No violations detected.", file=file)
            return

        print(f"[security] {total} violation(s) detected:", file=file)
        for category, violations in all_v.items():
            if violations:
                print(f"  [{category.upper()}]", file=file)
                for v in violations:
                    loc = f" @ {v.location}" if v.location else ""
                    print(f"    - {v}{loc}", file=file)
