"""
JIT Tiered Compilation
========================

Implements a three-tier execution model:

    Tier 0 – Interpreter
        All functions start here.  No compilation overhead, maximum
        flexibility for deoptimization.

    Tier 1 – Baseline JIT (warm code)
        After ``tier1_threshold`` calls the function is compiled with
        minimal optimization (O1).  Compilation is fast; code is slightly
        faster than the interpreter.

    Tier 2 – Optimizing JIT (hot code)
        After ``tier2_threshold`` calls (or ``tier1_threshold`` extended
        calls) the function is recompiled at O3 with full inlining and
        type-specialization hints from the TypeFeedbackCollector.

    Deoptimization
        If a compiled guard fails at runtime the function is transparently
        re-entered in the interpreter and its tier counter is reset.

The actual LLVM code-generation is performed by the existing
``JITCompiler`` in ``nlpl.jit.jit_compiler``.  The ``TieredCompiler``
wraps it and adds the tier-promotion/deoptimization bookkeeping.
"""

from __future__ import annotations

import threading
import time
from dataclasses import dataclass, field
from enum import Enum, auto
from typing import Any, Callable, Dict, List, Optional, Tuple


# ---------------------------------------------------------------------------
# Tier definitions
# ---------------------------------------------------------------------------

class ExecutionTier(Enum):
    INTERPRETER = 0
    BASELINE_JIT = 1
    OPTIMIZING_JIT = 2


@dataclass
class FunctionTierState:
    """Per-function tier-promotion bookkeeping."""
    name: str
    tier: ExecutionTier = ExecutionTier.INTERPRETER
    call_count: int = 0
    deopt_count: int = 0
    baseline_compiled_at: Optional[int] = None   # call_count when compiled
    optimized_compiled_at: Optional[int] = None
    last_deopt_at: Optional[int] = None
    # Cached compiled callables per tier
    baseline_callable: Optional[Callable] = None
    optimized_callable: Optional[Callable] = None
    # Guard failure tracking (for deopt heuristics)
    guard_failures: int = 0


# ---------------------------------------------------------------------------
# Tiered compiler
# ---------------------------------------------------------------------------

class TieredCompiler:
    """
    Manages two-tier JIT compilation on top of the existing JITCompiler.

    Attributes
    ----------
    tier1_threshold : int
        Number of calls before Tier-1 (baseline JIT) compilation.
        Default 50.
    tier2_threshold : int
        Number of calls before Tier-2 (optimizing JIT) compilation.
        Default 500.
    max_deopt_before_disable : int
        After this many deoptimizations for one function, fall back
        permanently to the interpreter.  Default 5.
    """

    def __init__(
        self,
        tier1_threshold: int = 50,
        tier2_threshold: int = 500,
        max_deopt_before_disable: int = 5,
    ):
        self.tier1_threshold = tier1_threshold
        self.tier2_threshold = tier2_threshold
        self.max_deopt_before_disable = max_deopt_before_disable

        self._function_states: Dict[str, FunctionTierState] = {}
        self._lock = threading.Lock()

        # Reference to the underlying JITCompiler (set by attach_to_interpreter)
        self._jit: Optional[Any] = None
        self._interpreter: Optional[Any] = None

        # Statistics
        self._tier1_compilations = 0
        self._tier2_compilations = 0
        self._deopts = 0

    # ------------------------------------------------------------------
    # Attachment
    # ------------------------------------------------------------------

    def attach_to_interpreter(self, interpreter: Any) -> None:
        """
        Hook the tiered compiler into an interpreter instance.
        Also attach the underlying JITCompiler.
        """
        self._interpreter = interpreter

        # Try to attach / create the underlying JIT
        try:
            from .jit_compiler import JITCompiler
            self._jit = JITCompiler(
                hot_threshold=self.tier1_threshold,
                optimization_level=1,
            )
            self._jit.attach_to_interpreter(interpreter)
        except Exception:
            self._jit = None  # JIT unavailable; tiered tracking still works

        # Replace interpreter's on_call hook
        original_on_call = getattr(interpreter, "_on_function_call", None)

        def tiered_on_call(func_name: str, func_def: Any, args: list) -> Any:
            self._record_call(func_name)
            if original_on_call is not None:
                return original_on_call(func_name, func_def, args)
            return None

        interpreter._on_function_call = tiered_on_call
        interpreter._tiered_compiler = self

    def detach(self) -> None:
        if self._interpreter is not None:
            self._interpreter._tiered_compiler = None
            self._interpreter = None
        if self._jit is not None:
            try:
                self._jit.detach_from_interpreter()
            except Exception:
                pass
            self._jit = None

    # ------------------------------------------------------------------
    # Call tracking and tier promotion
    # ------------------------------------------------------------------

    def _record_call(self, func_name: str) -> None:
        with self._lock:
            state = self._function_states.setdefault(
                func_name, FunctionTierState(name=func_name)
            )
            state.call_count += 1
            self._maybe_promote(state)

    def _maybe_promote(self, state: FunctionTierState) -> None:
        """Check if a function should be promoted to the next tier."""
        if state.tier == ExecutionTier.INTERPRETER:
            if state.call_count >= self.tier1_threshold:
                self._compile_baseline(state)

        elif state.tier == ExecutionTier.BASELINE_JIT:
            if state.call_count >= self.tier2_threshold:
                self._compile_optimized(state)

    # ------------------------------------------------------------------
    # Compilation
    # ------------------------------------------------------------------

    def _compile_baseline(self, state: FunctionTierState) -> None:
        """Promote function to Tier-1 (baseline JIT)."""
        if state.deopt_count >= self.max_deopt_before_disable:
            return  # Too many deopts; stay in interpreter

        func_def = self._get_function_def(state.name)
        if func_def is None:
            return

        try:
            if self._jit is not None:
                compiled = self._jit.compile_function(state.name, func_def, opt_level=1)
                state.baseline_callable = compiled
            state.tier = ExecutionTier.BASELINE_JIT
            state.baseline_compiled_at = state.call_count
            self._tier1_compilations += 1
        except Exception:
            pass  # Compilation failed; stay in interpreter

    def _compile_optimized(self, state: FunctionTierState) -> None:
        """Promote function to Tier-2 (optimizing JIT with type feedback)."""
        if state.deopt_count >= self.max_deopt_before_disable:
            return

        func_def = self._get_function_def(state.name)
        if func_def is None:
            return

        # Gather type-feedback annotations
        type_hints = {}
        if self._interpreter is not None:
            feedback = getattr(self._interpreter, "_type_feedback_collector", None)
            if feedback is not None:
                type_hints = feedback.get_hints(state.name)

        try:
            if self._jit is not None:
                compiled = self._jit.compile_function(
                    state.name, func_def, opt_level=3, type_hints=type_hints
                )
                state.optimized_callable = compiled
            state.tier = ExecutionTier.OPTIMIZING_JIT
            state.optimized_compiled_at = state.call_count
            self._tier2_compilations += 1
        except Exception:
            pass  # Compilation failed; stay at Tier-1

    # ------------------------------------------------------------------
    # Deoptimization
    # ------------------------------------------------------------------

    def deoptimize(self, func_name: str, reason: str = "") -> None:
        """
        Deoptimize a function back to the interpreter tier.

        Called when a guard check fails or the compiled code encounters
        an unexpected condition.
        """
        with self._lock:
            state = self._function_states.get(func_name)
            if state is None:
                return
            state.tier = ExecutionTier.INTERPRETER
            state.optimized_callable = None
            state.baseline_callable = None
            state.deopt_count += 1
            state.last_deopt_at = state.call_count
            state.guard_failures += 1
            self._deopts += 1

    # ------------------------------------------------------------------
    # Dispatch
    # ------------------------------------------------------------------

    def get_callable(self, func_name: str) -> Optional[Callable]:
        """
        Return the best compiled callable for ``func_name``, or None if
        the function should be interpreted.
        """
        state = self._function_states.get(func_name)
        if state is None:
            return None
        if state.tier == ExecutionTier.OPTIMIZING_JIT and state.optimized_callable:
            return state.optimized_callable
        if state.tier == ExecutionTier.BASELINE_JIT and state.baseline_callable:
            return state.baseline_callable
        return None

    # ------------------------------------------------------------------
    # Helpers
    # ------------------------------------------------------------------

    def _get_function_def(self, func_name: str) -> Optional[Any]:
        """Retrieve a function AST node from the interpreter."""
        if self._interpreter is None:
            return None
        try:
            scope = getattr(self._interpreter, "global_scope", None) or {}
            return scope.get(func_name)
        except Exception:
            return None

    # ------------------------------------------------------------------
    # Statistics and diagnostics
    # ------------------------------------------------------------------

    @property
    def stats(self) -> Dict[str, Any]:
        with self._lock:
            return {
                "tier1_compilations": self._tier1_compilations,
                "tier2_compilations": self._tier2_compilations,
                "total_deopts": self._deopts,
                "functions_tracked": len(self._function_states),
                "in_interpreter": sum(
                    1 for s in self._function_states.values()
                    if s.tier == ExecutionTier.INTERPRETER
                ),
                "in_baseline_jit": sum(
                    1 for s in self._function_states.values()
                    if s.tier == ExecutionTier.BASELINE_JIT
                ),
                "in_optimizing_jit": sum(
                    1 for s in self._function_states.values()
                    if s.tier == ExecutionTier.OPTIMIZING_JIT
                ),
            }

    def print_report(self) -> None:
        s = self.stats
        print("Tiered JIT Compiler Report")
        print(f"  Functions tracked     : {s['functions_tracked']}")
        print(f"  In interpreter        : {s['in_interpreter']}")
        print(f"  In baseline JIT       : {s['in_baseline_jit']}")
        print(f"  In optimizing JIT     : {s['in_optimizing_jit']}")
        print(f"  Tier-1 compilations   : {s['tier1_compilations']}")
        print(f"  Tier-2 compilations   : {s['tier2_compilations']}")
        print(f"  Deoptimizations       : {s['total_deopts']}")

        # Per-function details
        with self._lock:
            hot = [
                s2 for s2 in self._function_states.values()
                if s2.call_count >= self.tier1_threshold
            ]
        if hot:
            print("  Hot functions:")
            for fn in sorted(hot, key=lambda x: x.call_count, reverse=True)[:10]:
                print(
                    f"    {fn.name:40s}  {fn.call_count:8d} calls  "
                    f"tier={fn.tier.name:<16s}  deopts={fn.deopt_count}"
                )

    def tier_of(self, func_name: str) -> ExecutionTier:
        state = self._function_states.get(func_name)
        return state.tier if state else ExecutionTier.INTERPRETER
