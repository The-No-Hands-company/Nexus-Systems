#!/usr/bin/env python3
"""Quick parser performance smoke test for CI."""

import argparse
import json
import sys
import time
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
SRC_ROOT = REPO_ROOT / "src"
if str(SRC_ROOT) not in sys.path:
    sys.path.insert(0, str(SRC_ROOT))

from nexuslang.parser.ast_cache import get_global_cache
from nexuslang.parser.cached_parser import parse_with_cache
from nexuslang.errors import NxlSyntaxError


def _read_sample_text(file_path: Path) -> tuple[Path, str]:
    file_path = file_path.resolve()
    return file_path, file_path.read_text(encoding="utf-8")


def _parse_sample(file_path: Path, text: str) -> None:
    cache = get_global_cache()
    cache.clear()
    parse_with_cache(file_path.as_uri(), text, debug=False)


def validate_parseable_sample(file_path: Path) -> str | None:
    """Return a targeted validation error for an invalid sample, else ``None``."""
    resolved_path, text = _read_sample_text(file_path)
    try:
        _parse_sample(resolved_path, text)
    except NxlSyntaxError as exc:
        line = getattr(exc, "line", None)
        column = getattr(exc, "column", None)
        location = ""
        if line is not None and column is not None:
            location = f" at line {line}, column {column}"
        elif line is not None:
            location = f" at line {line}"
        first_line = str(exc).splitlines()[0].strip() if str(exc).strip() else "syntax error"
        return f"Invalid perf smoke sample {resolved_path}{location}: {first_line}"
    except Exception as exc:  # noqa: BLE001
        first_line = str(exc).splitlines()[0].strip() if str(exc).strip() else exc.__class__.__name__
        return f"Invalid perf smoke sample {resolved_path}: {exc.__class__.__name__}: {first_line}"
    return None


def validate_parseable_samples(samples: list[Path]) -> list[str]:
    """Validate all fixture samples before timing begins."""
    errors: list[str] = []
    for sample in samples:
        error = validate_parseable_sample(sample)
        if error is not None:
            errors.append(error)
    return errors


def measure_parse_ms(file_path: Path) -> float:
    """Measure cold parse time for a file in milliseconds."""
    file_path, text = _read_sample_text(file_path)
    start = time.perf_counter()
    _parse_sample(file_path, text)
    return (time.perf_counter() - start) * 1000


def load_baseline(path: Path) -> dict[str, float]:
    if not path.exists():
        return {}
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
        return {k: float(v) for k, v in data.get("results", {}).items()}
    except Exception:  # noqa: BLE001
        return {}


def main() -> int:
    parser = argparse.ArgumentParser(description="Run NexusLang perf smoke")
    parser.add_argument("--sample", action="append", default=[], help="Sample NexusLang file (repeatable)")
    parser.add_argument("--threshold-ms", type=float, default=1500.0, help="Maximum allowed parse time per sample")
    parser.add_argument("--baseline", type=Path, default=Path("perf-baseline.json"), help="Optional baseline JSON to compare")
    parser.add_argument("--tolerance", type=float, default=0.15, help="Allowed regression vs baseline (fraction)")
    parser.add_argument("--output", type=Path, default=Path("perf-smoke.json"), help="Output report path")
    parser.add_argument(
        "--summary-markdown",
        type=Path,
        default=None,
        help="Optional path to write markdown summary for CI step summaries",
    )
    args = parser.parse_args()

    samples = [Path(p) for p in (args.sample or [])] or [
        Path("examples/build_test_hello.nxl"),
        Path("examples/25_inline_assembly.nxl"),
    ]

    missing = [str(p) for p in samples if not p.exists()]
    if missing:
        print(f"Missing samples: {', '.join(missing)}", file=sys.stderr)
        return 1

    sample_errors = validate_parseable_samples(samples)
    if sample_errors:
        for item in sample_errors:
            print(f"FAIL: {item}", file=sys.stderr)
        return 1

    baseline = load_baseline(args.baseline)
    results: dict[str, float] = {}
    failures: list[str] = []

    for sample in samples:
        elapsed_ms = measure_parse_ms(sample)
        results[str(sample.resolve())] = round(elapsed_ms, 2)
        if elapsed_ms > args.threshold_ms:
            failures.append(
                f"{sample.name}: {elapsed_ms:.1f}ms > threshold {args.threshold_ms:.1f}ms"
            )
        if baseline:
            base = baseline.get(str(sample.resolve()), baseline.get(sample.name))
            if base:
                allowed = base * (1 + args.tolerance)
                if elapsed_ms > allowed:
                    failures.append(
                        f"{sample.name}: {elapsed_ms:.1f}ms > baseline {base:.1f}ms * (1+{args.tolerance:.2f})"
                    )

    report = {
        "threshold_ms": args.threshold_ms,
        "tolerance": args.tolerance,
        "baseline": baseline,
        "results": results,
        "failures": failures,
        "status": "pass" if not failures else "fail",
    }
    args.output.write_text(json.dumps(report, indent=2), encoding="utf-8")

    if args.summary_markdown is not None:
        lines: list[str] = []
        lines.append("## Performance Smoke Summary")
        lines.append("")
        lines.append(f"- Threshold per sample: {args.threshold_ms:.1f}ms")
        lines.append(f"- Baseline tolerance: {args.tolerance:.2f}")
        lines.append(f"- Baseline file: `{args.baseline}`")
        lines.append(f"- Status: {'FAIL' if failures else 'PASS'}")
        lines.append("")
        lines.append("| Sample | Parse time (ms) |")
        lines.append("|---|---:|")
        for sample_name, elapsed in results.items():
            lines.append(f"| {Path(sample_name).name} | {elapsed:.2f} |")

        if failures:
            lines.append("")
            lines.append("### Failures")
            for item in failures:
                lines.append(f"- {item}")
            lines.append("")
            lines.append("### Suggested next commands")
            lines.append(
                "- `python scripts/ci_perf_smoke.py --output perf-smoke.json --threshold-ms 1500 --baseline perf-baseline.json --tolerance 0.15`"
            )
            lines.append("- `python dev_tools/profile_interpreter.py examples/build_test_hello.nxl`")
            lines.append("- `python dev_tools/profile_compiler.py examples/build_test_hello.nxl`")

        args.summary_markdown.write_text("\n".join(lines) + "\n", encoding="utf-8")

    if failures:
        for item in failures:
            print(f"FAIL: {item}", file=sys.stderr)
        return 1

    print("Perf smoke passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
