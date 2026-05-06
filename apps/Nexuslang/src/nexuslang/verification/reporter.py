"""
nlpl.verification.reporter
============================

VerificationReport: aggregates one or more FormalSpec objects and
provides text, JSON, and summary output.

``verify_file(path, ...)`` is the high-level single-file entry point.
"""

from __future__ import annotations

import json
import os
import time
from typing import List, Optional, Dict, Any

from nexuslang.verification.specification import FormalSpec, VerificationStatus, VerificationCondition
from nexuslang.verification.constraint_collector import ConstraintCollector
from nexuslang.verification.z3_backend import Z3Backend, Z3_AVAILABLE


class VerificationReport:
    """Aggregate report for one or more verified NexusLang source files.

    Args:
        specs (List[FormalSpec]): The specs to aggregate.
    """

    def __init__(self, specs: Optional[List[FormalSpec]] = None) -> None:
        self.specs: List[FormalSpec] = specs or []
        self.generated_at: str = time.strftime("%Y-%m-%dT%H:%M:%S")

    # ------------------------------------------------------------------
    # Aggregate statistics
    # ------------------------------------------------------------------

    @property
    def total_conditions(self) -> int:
        """Total VCs across all specs."""
        return sum(s.total for s in self.specs)

    @property
    def total_proved(self) -> int:
        return sum(s.proved for s in self.specs)

    @property
    def total_failed(self) -> int:
        return sum(s.failed for s in self.specs)

    @property
    def total_unverified(self) -> int:
        return sum(s.unverified for s in self.specs)

    @property
    def has_failures(self) -> bool:
        return self.total_failed > 0

    # ------------------------------------------------------------------
    # Output
    # ------------------------------------------------------------------

    def summary(self) -> str:
        """Return a multi-line text summary of all specs."""
        lines = [
            "Verification Report",
            "=" * 40,
            f"Generated: {self.generated_at}",
            f"Files: {len(self.specs)}",
            f"Total VCs: {self.total_conditions}",
            f"  Proved:     {self.total_proved}",
            f"  Failed:     {self.total_failed}",
            f"  Unverified: {self.total_unverified}",
        ]

        if not self.specs:
            lines.append("\nNo source files to report.")
            return "\n".join(lines)

        lines.append("")
        for spec in self.specs:
            lines.append(f"[{spec.path}]")
            lines.append(
                f"  VCs: {spec.total}  proved={spec.proved}  "
                f"failed={spec.failed}  unverified={spec.unverified}"
            )
            # List any failures with detail
            for vc in spec.conditions:
                if vc.status == VerificationStatus.FAILED:
                    lines.append(f"  FAIL  line {vc.line}: {vc.description}")
                    if vc.counter_example is not None:
                        lines.append(f"         counter-example: {vc.counter_example}")
                elif vc.status == VerificationStatus.PROVED:
                    lines.append(f"  PASS  line {vc.line}: {vc.description}")
                else:
                    lines.append(
                        f"  ????  line {vc.line}: {vc.description} "
                        f"[{vc.status.value}]"
                    )

        return "\n".join(lines)

    def to_dict(self) -> Dict[str, Any]:
        """Serialise to a JSON-compatible dict."""
        return {
            "generated_at": self.generated_at,
            "total_conditions": self.total_conditions,
            "total_proved": self.total_proved,
            "total_failed": self.total_failed,
            "total_unverified": self.total_unverified,
            "files": [s.to_dict() for s in self.specs],
        }

    def write_json(self, path: str) -> None:
        """Write the report as JSON to *path*, creating parent dirs as needed."""
        os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
        with open(path, "w", encoding="utf-8") as fh:
            json.dump(self.to_dict(), fh, indent=2)

    def write_text(self, path: str) -> None:
        """Write the human-readable summary to *path*."""
        os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
        with open(path, "w", encoding="utf-8") as fh:
            fh.write(self.summary())
            fh.write("\n")


# ---------------------------------------------------------------------------
# High-level entry points
# ---------------------------------------------------------------------------


def verify_file(
    source_path: str,
    *,
    use_z3: bool = True,
    z3_timeout_ms: int = 5_000,
) -> FormalSpec:
    """Parse *source_path*, collect VCs, and optionally discharge via Z3.

    Args:
        source_path   (str): Path to the ``.nlpl`` source file.
        use_z3        (bool): Use Z3 if available.  Default True.
        z3_timeout_ms (int): Per-VC solver timeout in milliseconds.

    Returns:
        FormalSpec: The specification with VC statuses filled in.

    Raises:
        FileNotFoundError: If *source_path* does not exist.
        NLPLParseError:    If the source file has syntax errors.
    """
    from nexuslang.parser.lexer import Lexer
    from nexuslang.parser.parser import Parser

    with open(source_path, encoding="utf-8") as fh:
        source = fh.read()

    ast = Parser(Lexer(source).tokenize()).parse()
    collector = ConstraintCollector(path=source_path)
    spec = collector.collect(ast)

    if use_z3 and Z3_AVAILABLE:
        backend = Z3Backend(timeout_ms=z3_timeout_ms)
        backend.verify_all(spec)

    return spec


def verify_files(
    source_paths: List[str],
    *,
    use_z3: bool = True,
    z3_timeout_ms: int = 5_000,
) -> VerificationReport:
    """Verify multiple NexusLang source files and return an aggregate report.

    Args:
        source_paths  (List[str]): Paths to ``.nlpl`` source files.
        use_z3        (bool): Use Z3 if available.
        z3_timeout_ms (int): Per-VC solver timeout.

    Returns:
        VerificationReport: Aggregate report of all files.
    """
    specs = []
    for path in source_paths:
        try:
            spec = verify_file(path, use_z3=use_z3, z3_timeout_ms=z3_timeout_ms)
            specs.append(spec)
        except Exception as exc:  # pragma: no cover
            # Create an empty spec with an error indicator
            err_spec = FormalSpec(path=path)
            from nexuslang.verification.specification import VerificationCondition
            err_spec.conditions.append(
                VerificationCondition(
                    kind="error",
                    description=f"Failed to analyse: {exc}",
                    line=0,
                    status=VerificationStatus.UNKNOWN,
                )
            )
            specs.append(err_spec)
    return VerificationReport(specs=specs)
