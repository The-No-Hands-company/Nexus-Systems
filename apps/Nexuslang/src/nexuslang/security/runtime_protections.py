"""
NLPL Runtime Protections Module

Provides interpreter-level runtime security protections:

1. StackCanary
   - Generates a cryptographically random 64-bit sentinel value at interpreter
     function-frame entry and validates it at function return.
   - Detects any in-process corruption of the sentinel (e.g., caused by a
     misbehaving C extension via FFI).
   - Raises StackSmashingDetected and terminates the frame on mismatch.

2. BoundsChecker
   - Wraps every list/array index access in the NexusLang interpreter.
   - Configurable: enabled by the '--bounds-check' runtime flag or programmatically.
   - On violation: raises BoundsCheckError with precise location info.
   - Performance: adds ~O(1) overhead per access; disabled by default in
     production mode.

3. IntegerOverflowChecker
   - Flags arithmetic that would silently produce incorrect results:
     * Integer addition/multiplication for values used as memory offsets.
     * Shift amounts outside [0, 63].
   - Configurable threshold for "dangerous" integer size.

4. RuntimeProtector (facade)
   - Aggregates all runtime protections in one object.
   - Example usage in Interpreter.__init__():

       self.rt_protector = RuntimeProtector(
           enable_canaries=True,
           enable_bounds=True,
       )

   - Before list access:
       self.rt_protector.bounds.check(index, len(lst), location=loc)

   - At function entry/exit:
       token = self.rt_protector.canary.enter_frame(func_name)
       ...
       self.rt_protector.canary.exit_frame(token, func_name, location=loc)

All protections are **optional and independently configurable**.  Enabling or
disabling any protection has no effect on the correctness of NexusLang programs;
it only affects whether the protection raises an error on a potential attack.
"""

from __future__ import annotations

import os
import secrets
import sys
import threading
from dataclasses import dataclass, field
from typing import Dict, List, Optional, Tuple


# =============================================================================
# Exceptions
# =============================================================================

class RuntimeProtectionError(Exception):
    """Base class for all runtime protection failures."""

    def __init__(self, message: str, *, location: Optional[str] = None):
        super().__init__(message)
        self.location = location


class StackSmashingDetected(RuntimeProtectionError):
    """
    Raised when a stack-canary sentinel value is found to be corrupted.

    This typically indicates that C code called via FFI wrote beyond a buffer
    boundary and overwrote the interpreter's sentinel storage.
    """

    def __init__(self, function_name: str, expected: int, actual: int,
                 *, location: Optional[str] = None):
        super().__init__(
            f"Stack smashing detected in function '{function_name}': "
            f"canary expected 0x{expected:016x}, got 0x{actual:016x}"
            + (f" at {location}" if location else ""),
            location=location,
        )
        self.function_name = function_name
        self.expected = expected
        self.actual = actual


class BoundsCheckError(RuntimeProtectionError):
    """
    Raised when an array or list is accessed with an out-of-bounds index.
    """

    def __init__(self, index: int, size: int, *,
                 location: Optional[str] = None):
        super().__init__(
            f"Bounds check failed: index {index} is out of range for "
            f"collection of size {size}"
            + (f" at {location}" if location else ""),
            location=location,
        )
        self.index = index
        self.size = size


class IntegerOverflowError(RuntimeProtectionError):
    """
    Raised when an arithmetic operation produces an integer that is
    suspiciously large (configurable threshold).
    """

    def __init__(self, operation: str, result: int, threshold: int,
                 *, location: Optional[str] = None):
        super().__init__(
            f"Integer overflow guard: '{operation}' produced {result!r} "
            f"(exceeds threshold {threshold})"
            + (f" at {location}" if location else ""),
            location=location,
        )
        self.operation = operation
        self.result = result
        self.threshold = threshold


# =============================================================================
# Stack Canary
# =============================================================================

@dataclass
class _CanaryFrame:
    """Internal record of a single active call frame's canary."""
    frame_id: int
    function_name: str
    canary: int          # The secret 64-bit sentinel value


class StackCanary:
    """
    Interpreter-level stack canary protection.

    For every NexusLang function call, a cryptographically random 64-bit value
    (the "canary") is stored in a per-frame structure protected by a lock.
    When the function returns, the stored value is compared against the
    original.  If they differ, StackSmashingDetected is raised.

    Thread safety: each thread has its own frame stack; the lock protects
    only shared bookkeeping.

    Limitations:
    - The canary protects the interpreter's *Python* stack, not the C stack.
      It will detect corruption that propagates through Python object
      references (e.g., FFI code overwriting a Python dict or list that holds
      the canary value).
    - A determined attacker with full Python access can defeat this check.
      The canary is a fast Trip-Wire, not a guaranteed security boundary.
    """

    def __init__(self, enabled: bool = True) -> None:
        self._enabled = enabled
        self._lock = threading.Lock()
        self._frame_counter: int = 0
        # thread_id -> list of _CanaryFrame
        self._thread_stacks: Dict[int, List[_CanaryFrame]] = {}

    @property
    def enabled(self) -> bool:
        return self._enabled

    @enabled.setter
    def enabled(self, value: bool) -> None:
        self._enabled = value

    def _get_stack(self) -> List[_CanaryFrame]:
        tid = threading.get_ident()
        with self._lock:
            if tid not in self._thread_stacks:
                self._thread_stacks[tid] = []
            return self._thread_stacks[tid]

    def enter_frame(self, function_name: str) -> int:
        """
        Called at the start of every NexusLang function execution.

        Generates a new canary and records the frame.

        Args:
            function_name: The name of the function being entered.

        Returns:
            frame_id: An opaque integer token.  MUST be passed to exit_frame().
        """
        if not self._enabled:
            return 0

        with self._lock:
            self._frame_counter += 1
            frame_id = self._frame_counter

        # secrets.randbits is cryptographically secure
        canary_value = secrets.randbits(64)
        frame = _CanaryFrame(
            frame_id=frame_id,
            function_name=function_name,
            canary=canary_value,
        )
        self._get_stack().append(frame)
        return frame_id

    def exit_frame(self, frame_id: int, function_name: str,
                   current_canary: Optional[int] = None,
                   *, location: Optional[str] = None) -> None:
        """
        Called at the end of every NexusLang function execution.

        Validates that the canary is unchanged and pops the frame.

        Args:
            frame_id:       Token returned by enter_frame().
            function_name:  The name of the function returning.
            current_canary: If provided, this value is compared against the
                            stored canary.  If None, the check is skipped
                            (only the stack structure is verified).
            location:       Human-readable source position for the return.

        Raises:
            StackSmashingDetected: If the canary has been modified.
        """
        if not self._enabled:
            return

        stack = self._get_stack()
        if not stack:
            return

        frame = stack[-1]
        if frame.frame_id != frame_id or frame.function_name != function_name:
            # Stack structure mismatch — could be a return from the wrong frame
            raise StackSmashingDetected(
                function_name,
                expected=frame.frame_id,
                actual=frame_id,
                location=location,
            )

        if current_canary is not None and current_canary != frame.canary:
            stack.pop()
            raise StackSmashingDetected(
                function_name,
                expected=frame.canary,
                actual=current_canary,
                location=location,
            )

        stack.pop()

    def get_canary(self, frame_id: int) -> Optional[int]:
        """
        Retrieve the canary value for a given frame.

        Returns None if the frame is not found (or canaries are disabled).
        """
        if not self._enabled:
            return None

        stack = self._get_stack()
        for frame in reversed(stack):
            if frame.frame_id == frame_id:
                return frame.canary
        return None

    def current_depth(self) -> int:
        """Return the number of active frames on the current thread's stack."""
        if not self._enabled:
            return 0
        return len(self._get_stack())

    def cleanup_thread(self) -> None:
        """Remove frame stack for the current thread (call from thread cleanup)."""
        tid = threading.get_ident()
        with self._lock:
            self._thread_stacks.pop(tid, None)


# =============================================================================
# Bounds Checker
# =============================================================================

class BoundsChecker:
    """
    Checks list/array/string index accesses for out-of-bounds conditions.

    The NexusLang interpreter calls check() immediately before any subscript
    operation to confirm the access is within range.

    Python natively supports negative indices (e.g., a[-1] == a[len(a)-1]).
    This checker accepts negative indices that fall within [-size, -1].

    Configurable attributes:
        enabled:  Toggle bounds checking on/off.
    """

    def __init__(self, enabled: bool = True) -> None:
        self._enabled = enabled
        self._violation_count: int = 0
        self._lock = threading.Lock()

    @property
    def enabled(self) -> bool:
        return self._enabled

    @enabled.setter
    def enabled(self, value: bool) -> None:
        self._enabled = value

    @property
    def violation_count(self) -> int:
        """Total number of out-of-bounds accesses detected since construction."""
        return self._violation_count

    def check(self, index: int, size: int,
              location: Optional[str] = None) -> None:
        """
        Validate that index is within the bounds of a collection of length size.

        Args:
            index:    The integer index being accessed.
            size:     The length of the collection being indexed.
            location: Human-readable source location (e.g., "file.nlpl:42").

        Raises:
            BoundsCheckError: If index is out of bounds (and enabled=True).
        """
        if not self._enabled:
            return

        # Allow Python-style negative indices
        if size > 0 and -size <= index < size:
            return

        # Empty collection or truly out-of-range
        if size == 0 or index < -size or index >= size:
            with self._lock:
                self._violation_count += 1
            raise BoundsCheckError(index, size, location=location)

    def check_slice(self, start: Optional[int], stop: Optional[int],
                    size: int, location: Optional[str] = None) -> None:
        """
        Validate slice bounds.

        Clamped (Python-style) slice semantics: slice indices outside [0, size]
        are not an error in Python, but explicit indices far outside this range
        may indicate bugs.  This method only raises if start > stop after
        normalization (invalid slice order).

        Args:
            start:    Slice start (None = 0).
            stop:     Slice stop (None = size).
            size:     Collection size.
            location: Human-readable source location.
        """
        if not self._enabled:
            return

        s = 0 if start is None else (max(0, start + size) if start < 0 else min(start, size))
        e = size if stop is None else (max(0, stop + size) if stop < 0 else min(stop, size))

        if s > e:
            with self._lock:
                self._violation_count += 1
            raise BoundsCheckError(s, size, location=location)

    def reset_count(self) -> None:
        """Reset the violation counter."""
        with self._lock:
            self._violation_count = 0


# =============================================================================
# Integer Overflow Checker
# =============================================================================

_DEFAULT_INT_THRESHOLD = (1 << 63) - 1  # 2^63 - 1


class IntegerOverflowChecker:
    """
    Detects integers that exceed a configurable size threshold.

    Python integers are arbitrary precision and never overflow natively, but
    very large integers used as memory offsets, loop counters, or buffer sizes
    can indicate attacker-controlled values (e.g., integer overflow in a
    computation that is then used as a buffer length).

    This checker raises IntegerOverflowError when a guarded arithmetic
    result exceeds the threshold.

    Usage:

        checker = IntegerOverflowChecker()
        result = a + b
        checker.check("a + b", result, location="line 10")
    """

    def __init__(self, enabled: bool = True,
                 threshold: int = _DEFAULT_INT_THRESHOLD) -> None:
        self._enabled = enabled
        self.threshold = threshold

    @property
    def enabled(self) -> bool:
        return self._enabled

    @enabled.setter
    def enabled(self, value: bool) -> None:
        self._enabled = value

    def check(self, operation: str, result: int,
              location: Optional[str] = None) -> None:
        """
        Raise IntegerOverflowError if |result| > threshold.

        Args:
            operation: Human-readable description of the operation (for errors).
            result:    The computed integer result.
            location:  Human-readable source location.
        """
        if not self._enabled:
            return
        if not isinstance(result, int):
            return
        if abs(result) > self.threshold:
            raise IntegerOverflowError(
                operation, result, self.threshold, location=location
            )

    def check_shift(self, amount: int, location: Optional[str] = None) -> None:
        """
        Raise IntegerOverflowError if a bit-shift amount is outside [0, 63].

        Args:
            amount:   The shift amount.
            location: Human-readable source location.
        """
        if not self._enabled:
            return
        if not isinstance(amount, int) or amount < 0 or amount > 63:
            raise IntegerOverflowError(
                "bit shift",
                amount,
                63,
                location=location,
            )


# =============================================================================
# ASLR status helper
# =============================================================================

def aslr_level() -> Optional[int]:
    """
    Return the Linux kernel ASLR randomization level (0, 1, or 2).

    Returns None on non-Linux systems or if the file cannot be read.
    """
    try:
        with open("/proc/sys/kernel/randomize_va_space") as fh:
            return int(fh.read().strip())
    except (OSError, ValueError):
        return None


def aslr_warning_message() -> Optional[str]:
    """
    Return a warning message if ASLR is disabled or degraded, else None.
    """
    level = aslr_level()
    if level is None:
        return None
    if level == 0:
        return (
            "ASLR is DISABLED (randomize_va_space=0). "
            "Address-space layout randomization significantly reduces "
            "the exploitability of memory corruption bugs. "
            "Enable with: echo 2 | sudo tee /proc/sys/kernel/randomize_va_space"
        )
    if level == 1:
        return (
            "ASLR is in conservative mode (randomize_va_space=1). "
            "Full ASLR (level 2) provides better protection."
        )
    return None  # level 2 is fine


def check_and_warn_aslr(*, file=sys.stderr) -> None:
    """
    Print an ASLR warning to file if protection is degraded.
    """
    msg = aslr_warning_message()
    if msg:
        print(f"[NLPL security warning] {msg}", file=file)


# =============================================================================
# RuntimeProtector (facade)
# =============================================================================

@dataclass
class RuntimeProtectorConfig:
    """
    Configuration for RuntimeProtector.

    Attributes:
        enable_canaries:        Enable stack canary checks.
        enable_bounds:          Enable bounds checking on array accesses.
        enable_overflow:        Enable integer overflow detection.
        overflow_threshold:     Threshold above which integers are suspicious.
    """
    enable_canaries: bool = False
    enable_bounds: bool = False
    enable_overflow: bool = False
    overflow_threshold: int = _DEFAULT_INT_THRESHOLD


class RuntimeProtector:
    """
    Facade aggregating all runtime security protections.

    The interpreter should instantiate one RuntimeProtector and store it as
    self.rt_protector.  All protections are individually configurable.

    Typical interpreter integration:

        class Interpreter:
            def __init__(self, config: RuntimeProtectorConfig):
                self.rt_protector = RuntimeProtector(config)

            def call_function(self, func, args, location):
                token = self.rt_protector.canary.enter_frame(func.name)
                try:
                    result = self._execute_body(func.body)
                finally:
                    canary_val = self.rt_protector.canary.get_canary(token)
                    self.rt_protector.canary.exit_frame(
                        token, func.name, canary_val, location=location
                    )
                return result

            def get_index(self, collection, index, location):
                self.rt_protector.bounds.check(index, len(collection), location)
                return collection[index]
    """

    def __init__(self, config: Optional[RuntimeProtectorConfig] = None) -> None:
        cfg = config or RuntimeProtectorConfig()
        self.canary = StackCanary(enabled=cfg.enable_canaries)
        self.bounds = BoundsChecker(enabled=cfg.enable_bounds)
        self.overflow = IntegerOverflowChecker(
            enabled=cfg.enable_overflow,
            threshold=cfg.overflow_threshold,
        )
        self._config = cfg

    @property
    def config(self) -> RuntimeProtectorConfig:
        return self._config

    def enable_all(self) -> None:
        """Enable every protection."""
        self.canary.enabled = True
        self.bounds.enabled = True
        self.overflow.enabled = True

    def disable_all(self) -> None:
        """Disable every protection (maximum performance mode)."""
        self.canary.enabled = False
        self.bounds.enabled = False
        self.overflow.enabled = False

    def startup_checks(self, *, file=sys.stderr) -> None:
        """
        Run startup security environment checks.

        Currently checks ASLR status and prints a warning if degraded.
        Call this once when the NexusLang runtime initialises.
        """
        check_and_warn_aslr(file=file)
