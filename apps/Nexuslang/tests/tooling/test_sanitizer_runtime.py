"""Tests for sanitizer runtime parsing and combined analysis output."""

import json
import os
import sys
from pathlib import Path

_SRC = str(Path(__file__).resolve().parent.parent.parent / "src")
if _SRC not in sys.path:
    sys.path.insert(0, _SRC)

from nexuslang.tooling.sanitizer_runtime import (  # noqa: E402
    format_sanitizer_report,
    parse_sanitizer_output,
    write_combined_runtime_analysis,
    write_sanitizer_report,
)


def test_parse_asan_output_extracts_findings():
    log = """
==12345==ERROR: AddressSanitizer: heap-use-after-free on address 0x123 at pc 0xabc
#0 0xabc in main /tmp/main.c:10:5
""".strip()

    report = parse_sanitizer_output(log)

    assert report.has_findings
    assert len(report.findings) == 1
    assert report.findings[0].sanitizer == "AddressSanitizer"
    assert "heap-use-after-free" in report.findings[0].error_type
    assert report.findings[0].location == "/tmp/main.c:10:5"


def test_parse_ubsan_output_extracts_runtime_error():
    log = "runtime error: signed integer overflow: 2147483647 + 1 cannot be represented in type 'int'"
    report = parse_sanitizer_output(log)

    assert report.has_findings
    assert report.findings[0].sanitizer == "UndefinedBehaviorSanitizer"


def test_write_reports_to_json(tmp_path):
    log = "==1==ERROR: AddressSanitizer: stack-buffer-overflow"
    report = parse_sanitizer_output(log)

    sanitizer_path = write_sanitizer_report(report, str(tmp_path / "sanitizer-report.json"))
    combined_path = write_combined_runtime_analysis(
        output_dir=str(tmp_path),
        sanitizer_report=report,
        profiler_summary={"cpu_functions": 12, "memory_peak_bytes": 4096},
    )

    assert Path(sanitizer_path).exists()
    assert Path(combined_path).exists()

    payload = json.loads((tmp_path / "combined-runtime-analysis.json").read_text(encoding="utf-8"))
    assert payload["sanitizer"]["finding_count"] == 1
    assert payload["profiling"]["cpu_functions"] == 12


def test_format_report_has_header():
    report = parse_sanitizer_output("==1==ERROR: AddressSanitizer: heap-buffer-overflow")
    text = format_sanitizer_report(report)
    assert "Runtime Sanitizer Summary" in text
    assert "AddressSanitizer" in text


def test_deduplicate_keeps_distinct_locations_for_same_error_summary():
    log = "\n".join([
        "==10==ERROR: AddressSanitizer: heap-use-after-free",
        "#0 0xaaa in main /tmp/one.c:11:7",
        "==11==ERROR: AddressSanitizer: heap-use-after-free",
        "#0 0xbbb in main /tmp/two.c:22:3",
    ])

    report = parse_sanitizer_output(log)
    assert report.has_findings
    assert len(report.findings) == 2
    assert report.findings[0].location == "/tmp/one.c:11:7"
    assert report.findings[1].location == "/tmp/two.c:22:3"
