"""Runtime sanitizer output parsing and reporting utilities."""

from __future__ import annotations

from dataclasses import asdict, dataclass, field
from pathlib import Path
from typing import Any, Dict, List, Optional
import json
import re


@dataclass
class SanitizerFinding:
    sanitizer: str
    error_type: str
    summary: str
    location: Optional[str] = None
    raw_line: Optional[str] = None


@dataclass
class SanitizerReport:
    findings: List[SanitizerFinding] = field(default_factory=list)

    @property
    def has_findings(self) -> bool:
        return bool(self.findings)

    def to_dict(self) -> Dict[str, Any]:
        return {
            "finding_count": len(self.findings),
            "findings": [asdict(finding) for finding in self.findings],
        }


_SANITIZER_ERROR_PATTERNS = [
    ("AddressSanitizer", re.compile(r"AddressSanitizer:\s*(?P<kind>[a-zA-Z0-9_-]+)", re.IGNORECASE)),
    ("ThreadSanitizer", re.compile(r"ThreadSanitizer:\s*(?P<kind>[a-zA-Z0-9_\-\s]+)", re.IGNORECASE)),
    ("MemorySanitizer", re.compile(r"MemorySanitizer:\s*(?P<kind>[a-zA-Z0-9_\-\s]+)", re.IGNORECASE)),
    ("UndefinedBehaviorSanitizer", re.compile(r"runtime error:\s*(?P<kind>.+)$", re.IGNORECASE)),
    ("UndefinedBehaviorSanitizer", re.compile(r"UndefinedBehaviorSanitizer:\s*(?P<kind>.+)$", re.IGNORECASE)),
]

_LOCATION_PATTERN = re.compile(r"(?P<file>[\w./\\-]+):(?P<line>\d+)(?::(?P<column>\d+))?")


def parse_sanitizer_output(output_text: str) -> SanitizerReport:
    """Parse sanitizer stderr/stdout and return structured findings."""
    report = SanitizerReport()
    if not output_text:
        return report

    lines = output_text.splitlines()
    for index, raw_line in enumerate(lines):
        line = raw_line.strip()
        if not line:
            continue

        for sanitizer_name, pattern in _SANITIZER_ERROR_PATTERNS:
            match = pattern.search(line)
            if not match:
                continue

            error_type = (match.group("kind") or "unknown").strip()
            location_match = _LOCATION_PATTERN.search(line)
            location = location_match.group(0) if location_match else _find_nearby_location(lines, index)
            report.findings.append(
                SanitizerFinding(
                    sanitizer=sanitizer_name,
                    error_type=error_type,
                    summary=line,
                    location=location,
                    raw_line=raw_line,
                )
            )
            break

    return _deduplicate_report(report)


def format_sanitizer_report(report: SanitizerReport) -> str:
    """Create a readable terminal summary for sanitizer findings."""
    if not report.findings:
        return "No sanitizer findings detected."

    lines = []
    lines.append("Runtime Sanitizer Summary")
    lines.append("=" * 80)
    lines.append(f"Total findings: {len(report.findings)}")
    lines.append("")

    for index, finding in enumerate(report.findings, start=1):
        lines.append(f"[{index}] {finding.sanitizer} - {finding.error_type}")
        if finding.location:
            lines.append(f"    Location: {finding.location}")
        lines.append(f"    {finding.summary}")
        lines.append("")

    return "\n".join(lines).rstrip()


def write_sanitizer_report(report: SanitizerReport, output_path: str) -> str:
    """Write structured sanitizer report JSON and return path."""
    path = Path(output_path)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(report.to_dict(), indent=2), encoding="utf-8")
    return str(path)


def write_combined_runtime_analysis(
    output_dir: str,
    sanitizer_report: Optional[SanitizerReport] = None,
    profiler_summary: Optional[Dict[str, Any]] = None,
) -> str:
    """Write a combined runtime analysis artifact (sanitizer + profiling)."""
    path = Path(output_dir)
    path.mkdir(parents=True, exist_ok=True)

    payload = {
        "sanitizer": sanitizer_report.to_dict() if sanitizer_report else {"finding_count": 0, "findings": []},
        "profiling": profiler_summary or {},
    }

    output_file = path / "combined-runtime-analysis.json"
    output_file.write_text(json.dumps(payload, indent=2), encoding="utf-8")
    return str(output_file)


def _deduplicate_report(report: SanitizerReport) -> SanitizerReport:
    seen = set()
    unique_findings: List[SanitizerFinding] = []

    for finding in report.findings:
        key = (finding.sanitizer, finding.error_type, finding.summary, finding.location)
        if key in seen:
            continue
        seen.add(key)
        unique_findings.append(finding)

    return SanitizerReport(findings=unique_findings)


def _find_nearby_location(lines: List[str], start_index: int, max_scan: int = 8) -> Optional[str]:
    """Find source location from nearby stack lines after a sanitizer summary line."""
    upper = min(len(lines), start_index + max_scan + 1)
    for idx in range(start_index + 1, upper):
        match = _LOCATION_PATTERN.search(lines[idx])
        if match:
            return match.group(0)
    return None
