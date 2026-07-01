#!/usr/bin/env python3
"""LSP smoke test for CI with simple latency checks."""

import argparse
import json
import sys
import time
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
SRC_ROOT = REPO_ROOT / "src"
if str(SRC_ROOT) not in sys.path:
    sys.path.insert(0, str(SRC_ROOT))

from nexuslang.lsp.server import NLPLLanguageServer, Position


def find_symbol_position(text: str, symbol: str) -> Position | None:
    """Find the first occurrence of a symbol in the given text."""
    for idx, line in enumerate(text.splitlines()):
        col = line.find(symbol)
        if col != -1:
            return Position(line=idx, character=col)
    return None


def main() -> int:
    parser = argparse.ArgumentParser(description="Run NexusLang LSP smoke checks")
    parser.add_argument(
        "--sample",
        default="examples/24_comprehensive_data_processing.nlpl",
        help="Sample NexusLang file to load",
    )
    parser.add_argument(
        "--symbol",
        default="compute_statistics",
        help="Symbol to query for nav features",
    )
    parser.add_argument("--max-latency-ms", type=float, default=500.0, help="Maximum allowed latency per check in ms")
    parser.add_argument("--output", default="lsp-smoke.json", help="Path to write JSON report")
    parser.add_argument(
        "--summary-markdown",
        default=None,
        help="Optional path to write markdown summary for CI step summaries",
    )
    args = parser.parse_args()

    sample_path = Path(args.sample).resolve()
    if not sample_path.exists():
        report: dict[str, object] = {
            "sample": str(sample_path),
            "symbol": args.symbol,
            "max_latency_ms": args.max_latency_ms,
            "results": {},
            "failures": [f"sample file not found: {sample_path}"],
            "status": "fail",
        }
        Path(args.output).write_text(json.dumps(report, indent=2), encoding="utf-8")
        if args.summary_markdown:
            lines = [
                "## LSP Smoke Summary",
                "",
                f"- Sample: `{sample_path}`",
                f"- Symbol: `{args.symbol}`",
                f"- Max latency per check: {args.max_latency_ms:.1f}ms",
                "- Status: FAIL",
                "",
                "### Failures",
                f"- sample file not found: {sample_path}",
                "",
                "### Suggested next commands",
                "- `python scripts/ci_lsp_smoke.py --sample examples/24_comprehensive_data_processing.nlpl --symbol compute_statistics --max-latency-ms 500 --output lsp-smoke.json`",
            ]
            Path(args.summary_markdown).write_text("\n".join(lines) + "\n", encoding="utf-8")
        print(f"Sample file not found: {sample_path}", file=sys.stderr)
        return 1

    text = sample_path.read_text(encoding="utf-8")
    uri = sample_path.as_uri()

    server = NLPLLanguageServer()
    server.documents[uri] = text

    report: dict[str, object] = {
        "sample": str(sample_path),
        "symbol": args.symbol,
        "max_latency_ms": args.max_latency_ms,
        "results": {},
        "failures": [],
        "status": "unknown",
    }

    failures: list[str] = []

    def timed(label: str, func):
        start = time.perf_counter()
        result = func()
        elapsed_ms = (time.perf_counter() - start) * 1000
        report["results"][label] = {"elapsed_ms": round(elapsed_ms, 2)}
        if elapsed_ms > args.max_latency_ms:
            failures.append(f"{label}: {elapsed_ms:.1f}ms > {args.max_latency_ms:.1f}ms")
        return result

    try:
        timed("diagnostics", lambda: server.diagnostics_provider.get_diagnostics(uri, text, check_imports=False))
    except Exception as exc:  # noqa: BLE001
        failures.append(f"diagnostics exception: {exc}")

    position = find_symbol_position(text, args.symbol)
    if position is None:
        failures.append(f"symbol '{args.symbol}' not found")
    else:
        try:
            hover = timed("hover", lambda: server.hover_provider.get_hover(text, position))
            if hover is None:
                failures.append("hover: no result")
        except Exception as exc:  # noqa: BLE001
            failures.append(f"hover exception: {exc}")

        try:
            definition = timed("definition", lambda: server.definition_provider.get_definition(text, position, uri))
            if definition is None:
                failures.append("definition: no result")
        except Exception as exc:  # noqa: BLE001
            failures.append(f"definition exception: {exc}")

        try:
            references = timed("references", lambda: server.references_provider.find_references(text, position, uri))
            count = len(references) if references else 0
            report["results"]["references"]["count"] = count
            if count == 0:
                failures.append("references: empty result")
        except Exception as exc:  # noqa: BLE001
            failures.append(f"references exception: {exc}")

    Path(args.output).write_text(json.dumps(report, indent=2), encoding="utf-8")

    if failures:
        report["status"] = "fail"
    else:
        report["status"] = "pass"
    report["failures"] = failures
    Path(args.output).write_text(json.dumps(report, indent=2), encoding="utf-8")

    if args.summary_markdown:
        lines: list[str] = []
        lines.append("## LSP Smoke Summary")
        lines.append("")
        lines.append(f"- Sample: `{sample_path}`")
        lines.append(f"- Symbol: `{args.symbol}`")
        lines.append(f"- Max latency per check: {args.max_latency_ms:.1f}ms")
        lines.append(f"- Status: {'FAIL' if failures else 'PASS'}")
        lines.append("")
        lines.append("| Check | Elapsed (ms) |")
        lines.append("|---|---:|")
        for check_name, payload in report["results"].items():
            elapsed_ms = payload.get("elapsed_ms", 0.0)
            lines.append(f"| {check_name} | {float(elapsed_ms):.2f} |")

        if failures:
            lines.append("")
            lines.append("### Failures")
            for item in failures:
                lines.append(f"- {item}")
            lines.append("")
            lines.append("### Suggested next commands")
            lines.append(
                "- `python scripts/ci_lsp_smoke.py --sample examples/24_comprehensive_data_processing.nlpl --symbol compute_statistics --max-latency-ms 500 --output lsp-smoke.json`"
            )
            lines.append("- `pytest tests/tooling/test_lsp_document_features.py -q`")
            lines.append("- `pytest tests/tooling/test_lsp_goto_definition.py tests/tooling/test_cross_file_navigation.py -q`")

        Path(args.summary_markdown).write_text("\n".join(lines) + "\n", encoding="utf-8")

    if failures:
        for item in failures:
            print(f"FAIL: {item}", file=sys.stderr)
        return 1

    print("LSP smoke passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
