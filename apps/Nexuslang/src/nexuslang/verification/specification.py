"""
nlpl.verification.specification
================================

Data model for formal verification: verification conditions, their
status, and the overall FormalSpec for a single source file.
"""

from __future__ import annotations

import enum
import time
from dataclasses import dataclass, field
from typing import List, Optional, Any, Dict, Counter


class VerificationStatus(enum.Enum):
    """Result of attempting to discharge a single verification condition."""

    PROVED = "proved"
    """The condition was proved UNSAT by the SMT solver (always true)."""

    FAILED = "failed"
    """The solver found a counter-example (condition can be violated)."""

    UNKNOWN = "unknown"
    """The solver did not determine the outcome (timeout, not supported)."""

    UNVERIFIED = "unverified"
    """No solver was available or the condition was not submitted."""


@dataclass
class VerificationCondition:
    """A single contract assertion that can be formally verified.

    Attributes:
        kind        (str): "precondition", "postcondition", "invariant",
                           "guarantee", or "spec_requires"/"spec_ensures".
        description (str): Human-readable description of the condition.
        function    (str|None): Enclosing function/method name, if any.
        line        (int): Source line number where the contract appears.
        ast_node    (Any): Reference to the AST node (RequireStatement,
                    EnsureStatement, InvariantStatement, SpecAnnotation,
                    or GuaranteeStatement).
        status      (VerificationStatus): Verification outcome.
        counter_example (Any|None): Counter-example from the solver, or None.
        solver_output   (str|None): Raw solver output for diagnostics.
    """

    kind: str
    description: str
    function: Optional[str] = None
    line: int = 0
    ast_node: Any = field(default=None, repr=False)
    status: VerificationStatus = VerificationStatus.UNVERIFIED
    counter_example: Optional[Any] = None
    solver_output: Optional[str] = None

    def to_dict(self) -> Dict[str, Any]:
        """Serialise to a JSON-compatible dict (excludes AST node reference)."""
        return {
            "kind": self.kind,
            "description": self.description,
            "function": self.function,
            "line": self.line,
            "status": self.status.value,
            "counter_example": str(self.counter_example)
            if self.counter_example is not None
            else None,
            "solver_output": self.solver_output,
        }


@dataclass
class FormalSpec:
    """Collection of verification conditions extracted from one source file.

    Attributes:
        path          (str): Path of the NexusLang source file.
        generated_at  (str): ISO-8601 timestamp of analysis.
        conditions    (List[VerificationCondition]): All extracted VCs.
    """

    path: str
    generated_at: str = field(default_factory=lambda: time.strftime("%Y-%m-%dT%H:%M:%S"))
    conditions: List[VerificationCondition] = field(default_factory=list)

    # ------------------------------------------------------------------
    # Derived properties
    # ------------------------------------------------------------------

    @property
    def total(self) -> int:
        """Total number of verification conditions."""
        return len(self.conditions)

    @property
    def proved(self) -> int:
        """Number of conditions proved correct."""
        return sum(1 for c in self.conditions if c.status == VerificationStatus.PROVED)

    @property
    def failed(self) -> int:
        """Number of conditions with counter-examples."""
        return sum(1 for c in self.conditions if c.status == VerificationStatus.FAILED)

    @property
    def unverified(self) -> int:
        """Number of conditions that were not submitted to a solver."""
        return sum(
            1
            for c in self.conditions
            if c.status in (VerificationStatus.UNVERIFIED, VerificationStatus.UNKNOWN)
        )

    def by_function(self) -> Dict[str, List[VerificationCondition]]:
        """Return conditions grouped by function name (None = top-level)."""
        groups: Dict[str, List[VerificationCondition]] = {}
        for vc in self.conditions:
            key = vc.function or "<top-level>"
            groups.setdefault(key, []).append(vc)
        return groups

    def summary(self) -> str:
        """Return a one-line text summary of the spec."""
        return (
            f"[{self.path}] {self.total} VCs: "
            f"{self.proved} proved, "
            f"{self.failed} failed, "
            f"{self.unverified} unverified"
        )

    def to_dict(self) -> Dict[str, Any]:
        """Serialise to a JSON-compatible dict."""
        return {
            "path": self.path,
            "generated_at": self.generated_at,
            "total": self.total,
            "proved": self.proved,
            "failed": self.failed,
            "unverified": self.unverified,
            "conditions": [c.to_dict() for c in self.conditions],
        }
