"""
Runtime Type Feedback Collector
================================

Records the concrete types seen at each call site during interpreter
execution.  This data drives speculative optimizations in the Tier-2 JIT:

- **Monomorphic call sites** (one type observed) are compiled with direct
  dispatch and can be inlined.
- **Polymorphic call sites** (2-4 types) get an inline cache.
- **Megamorphic call sites** (5+ types) are compiled with a generic dispatch
  that avoids per-type overhead.

The feedback is also consumed by ``TypeSpecializationPass`` to choose which
function variants to create and by ``DispatchOptimizationPass`` to generate
accurate dispatch hints.

Usage
-----
    from nexuslang.jit.type_feedback import TypeFeedbackCollector

    collector = TypeFeedbackCollector()
    collector.attach(interpreter)

    # After execution:
    hints = collector.get_hints("my_function")
    # hints = {"param_0": "Integer", "param_1": "String", ...}

    # Or get the full record:
    record = collector.get_record("my_function")
    print(record.polymorphism)  # "monomorphic" / "polymorphic" / "megamorphic"
"""

from __future__ import annotations

import threading
from collections import Counter
from dataclasses import dataclass, field
from enum import Enum
from typing import Any, Dict, FrozenSet, List, Optional, Set, Tuple, Counter


# ---------------------------------------------------------------------------
# Polymorphism classification
# ---------------------------------------------------------------------------

class Polymorphism(str, Enum):
    MONOMORPHIC = "monomorphic"    # Single type seen at this site
    POLYMORPHIC = "polymorphic"    # 2-4 types seen
    MEGAMORPHIC = "megamorphic"    # 5+ types seen (give up on specialization)


_POLYMORPHIC_THRESHOLD = 2
_MEGAMORPHIC_THRESHOLD = 5


# ---------------------------------------------------------------------------
# Per-parameter feedback
# ---------------------------------------------------------------------------

@dataclass
class ParameterFeedback:
    """Type observations for a single function parameter position."""
    param_index: int
    # type_name -> count of observations
    observed_types: Counter = field(default_factory=Counter)

    @property
    def dominant_type(self) -> Optional[str]:
        """The most-frequently observed type, or None if no data."""
        if not self.observed_types:
            return None
        return self.observed_types.most_common(1)[0][0]

    @property
    def type_set(self) -> FrozenSet[str]:
        return frozenset(self.observed_types.keys())

    @property
    def polymorphism(self) -> Polymorphism:
        n = len(self.observed_types)
        if n <= 1:
            return Polymorphism.MONOMORPHIC
        if n < _MEGAMORPHIC_THRESHOLD:
            return Polymorphism.POLYMORPHIC
        return Polymorphism.MEGAMORPHIC

    @property
    def total_observations(self) -> int:
        return sum(self.observed_types.values())

    def record(self, type_name: str) -> None:
        self.observed_types[type_name] += 1


# ---------------------------------------------------------------------------
# Per-function feedback record
# ---------------------------------------------------------------------------

@dataclass
class FunctionFeedback:
    """Aggregated type feedback for a function."""
    func_name: str
    call_count: int = 0
    # param_index -> ParameterFeedback
    parameters: Dict[int, ParameterFeedback] = field(default_factory=dict)
    # Return type observations
    return_types: Counter = field(default_factory=Counter)

    def record_call(
        self,
        arg_types: List[str],
        return_type: Optional[str] = None,
    ) -> None:
        self.call_count += 1
        for idx, type_name in enumerate(arg_types):
            if idx not in self.parameters:
                self.parameters[idx] = ParameterFeedback(param_index=idx)
            self.parameters[idx].record(type_name)
        if return_type is not None:
            self.return_types[return_type] += 1

    @property
    def polymorphism(self) -> Polymorphism:
        """Overall polymorphism: the worst of all parameters."""
        if not self.parameters:
            return Polymorphism.MONOMORPHIC
        worsts = [p.polymorphism for p in self.parameters.values()]
        if Polymorphism.MEGAMORPHIC in worsts:
            return Polymorphism.MEGAMORPHIC
        if Polymorphism.POLYMORPHIC in worsts:
            return Polymorphism.POLYMORPHIC
        return Polymorphism.MONOMORPHIC

    def get_type_hints(self) -> Dict[str, str]:
        """
        Return a mapping ``param_N -> dominant_type`` for all parameters
        that have a dominant type.
        """
        hints: Dict[str, str] = {}
        for idx, fb in sorted(self.parameters.items()):
            dominant = fb.dominant_type
            if dominant and fb.polymorphism == Polymorphism.MONOMORPHIC:
                hints[f"param_{idx}"] = dominant
        dominant_ret = (
            self.return_types.most_common(1)[0][0] if self.return_types else None
        )
        if dominant_ret:
            hints["return"] = dominant_ret
        return hints

    def is_specialization_candidate(self) -> bool:
        """True if the function is worth specializing (monomorphic + hot enough)."""
        return (
            self.call_count >= 10
            and self.polymorphism == Polymorphism.MONOMORPHIC
        )


# ---------------------------------------------------------------------------
# Collector
# ---------------------------------------------------------------------------

class TypeFeedbackCollector:
    """
    Attaches to an interpreter and records runtime type feedback.

    The collector is thread-safe (uses a lock for updates).  Reads are
    lock-free snapshots.
    """

    def __init__(self):
        self._records: Dict[str, FunctionFeedback] = {}
        self._lock = threading.Lock()
        self._attached_interpreter: Optional[Any] = None
        self._enabled = False

    # ------------------------------------------------------------------
    # Lifecycle
    # ------------------------------------------------------------------

    def attach(self, interpreter: Any) -> None:
        """Hook into an interpreter instance to start collecting feedback."""
        self._attached_interpreter = interpreter
        interpreter._type_feedback_collector = self
        self._enabled = True

        # Wrap the interpreter's function-call hook if it exists
        original_hook = getattr(interpreter, "_on_function_call", None)

        def feedback_hook(func_name: str, func_def: Any, args: list) -> Any:
            self._record_call(func_name, args)
            if original_hook is not None:
                return original_hook(func_name, func_def, args)
            return None

        interpreter._on_function_call = feedback_hook

    def detach(self) -> None:
        self._enabled = False
        if self._attached_interpreter is not None:
            self._attached_interpreter._type_feedback_collector = None
            self._attached_interpreter = None

    def enable(self) -> None:
        self._enabled = True

    def disable(self) -> None:
        self._enabled = False

    # ------------------------------------------------------------------
    # Recording
    # ------------------------------------------------------------------

    def _record_call(self, func_name: str, args: list) -> None:
        if not self._enabled:
            return
        arg_types = [self._type_name_of(a) for a in args]
        with self._lock:
            if func_name not in self._records:
                self._records[func_name] = FunctionFeedback(func_name=func_name)
            self._records[func_name].record_call(arg_types)

    def record_return(self, func_name: str, return_value: Any) -> None:
        """Record the return type of a function call (optional)."""
        if not self._enabled:
            return
        type_name = self._type_name_of(return_value)
        with self._lock:
            rec = self._records.get(func_name)
            if rec is not None:
                rec.return_types[type_name] += 1

    def _type_name_of(self, value: Any) -> str:
        """Derive an NexusLang type name from a Python runtime value."""
        if value is None:
            return "None"
        if isinstance(value, bool):
            return "Boolean"
        if isinstance(value, int):
            return "Integer"
        if isinstance(value, float):
            return "Float"
        if isinstance(value, str):
            return "String"
        if isinstance(value, list):
            return "List"
        if isinstance(value, dict):
            return "Dictionary"

        # Check for NexusLang runtime objects
        type_tag = getattr(value, "_nxl_type", None)
        if type_tag is not None:
            return str(type_tag)

        # Check for objects with a type_name attribute
        class_name = getattr(value, "__class__", None)
        if class_name is not None:
            return type(value).__name__

        return "Unknown"

    # ------------------------------------------------------------------
    # Querying
    # ------------------------------------------------------------------

    def get_record(self, func_name: str) -> Optional[FunctionFeedback]:
        return self._records.get(func_name)

    def get_hints(self, func_name: str) -> Dict[str, str]:
        """
        Return type hints for use by the Tier-2 JIT optimizer.
        Keys: ``"param_0"``, ``"param_1"``, …, ``"return"``
        Values: dominant type name (only included if site is monomorphic).
        """
        rec = self._records.get(func_name)
        if rec is None:
            return {}
        return rec.get_type_hints()

    def specialization_candidates(self) -> List[FunctionFeedback]:
        """Return all functions that are worth type-specializing."""
        return [r for r in self._records.values() if r.is_specialization_candidate()]

    def all_records(self) -> Dict[str, FunctionFeedback]:
        return dict(self._records)

    # ------------------------------------------------------------------
    # Reporting
    # ------------------------------------------------------------------

    def print_report(self) -> None:
        print("Type Feedback Report")
        print(f"  Functions tracked: {len(self._records)}")
        candidates = self.specialization_candidates()
        print(f"  Specialization candidates: {len(candidates)}")

        for rec in sorted(self._records.values(), key=lambda r: r.call_count, reverse=True)[:20]:
            print(
                f"  {rec.func_name:40s}  {rec.call_count:8d} calls  "
                f"{rec.polymorphism.value}"
            )
            hints = rec.get_type_hints()
            if hints:
                hint_str = ", ".join(f"{k}={v}" for k, v in hints.items())
                print(f"    type hints: {hint_str}")

    def to_dict(self) -> Dict[str, Any]:
        return {
            name: {
                "call_count": rec.call_count,
                "polymorphism": rec.polymorphism.value,
                "hints": rec.get_type_hints(),
            }
            for name, rec in self._records.items()
        }
