#!/usr/bin/env python3
"""Focused benchmark for LSP definition lookup in large workspaces.

This benchmark synthesizes a workspace with many nested modules, indexes it,
then measures DefinitionProvider lookup latency for cross-file imported symbols.
"""

from __future__ import annotations

import argparse
import json
import random
import tempfile
import time
from dataclasses import dataclass
from pathlib import Path
from statistics import mean
import sys

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "src"
if str(SRC) not in sys.path:
    sys.path.insert(0, str(SRC))

from nexuslang.lsp.definitions import DefinitionProvider
from nexuslang.lsp.server import Position
from nexuslang.lsp.workspace_index import WorkspaceIndex


@dataclass
class MockServer:
    documents: dict
    workspace_index: WorkspaceIndex


def _percentile(values: list[float], p: float) -> float:
    if not values:
        return 0.0
    if p <= 0:
        return min(values)
    if p >= 100:
        return max(values)
    ordered = sorted(values)
    idx = int(round((p / 100.0) * (len(ordered) - 1)))
    return ordered[idx]


def _write_module(module_file: Path, symbol_count: int, module_id: int) -> list[str]:
    symbols = []
    lines = []
    for i in range(symbol_count):
        sym = f"fn_{module_id}_{i}"
        symbols.append(sym)
        lines.append(f"function {sym} with x as Integer returns Integer")
        lines.append("    return x")
        lines.append("end")
        lines.append("")
    module_file.parent.mkdir(parents=True, exist_ok=True)
    module_file.write_text("\n".join(lines), encoding="utf-8")
    return symbols


def _build_workspace(root: Path, module_count: int, symbols_per_module: int) -> tuple[Path, str, list[tuple[int, int, str, str]]]:
    """Create nested package workspace and return main file + lookups metadata.

    Returns:
      main_file_path, main_file_content, lookups list of
      (line, character, symbol_name, expected_module_file)
    """
    lookups: list[tuple[int, int, str, str]] = []
    import_lines = []
    call_lines = []

    for module_id in range(module_count):
        pkg = f"pkg{module_id % 20}"
        subpkg = f"sub{module_id % 10}"
        mod = f"mod{module_id}"
        module_file = root / pkg / subpkg / f"{mod}.nxl"
        symbols = _write_module(module_file, symbols_per_module, module_id)

        picked = symbols[0]
        import_lines.append(f"from {pkg}.{subpkg}.{mod} import {picked}")
        call_lines.append(f"set result_{module_id} to {picked} with 1")

    main_lines = import_lines + [""] + call_lines + [""]
    main_text = "\n".join(main_lines)
    main_file = root / "apps" / "service" / "main.nxl"
    main_file.parent.mkdir(parents=True, exist_ok=True)
    main_file.write_text(main_text, encoding="utf-8")

    # Build lookup metadata based on call line positions.
    call_start_line = len(import_lines) + 1
    for module_id in range(module_count):
        sym = f"fn_{module_id}_0"
        line = call_start_line + module_id
        char = main_lines[line].find(sym)
        expected = str(root / f"pkg{module_id % 20}" / f"sub{module_id % 10}" / f"mod{module_id}.nxl")
        lookups.append((line, char, sym, expected))

    return main_file, main_text, lookups


def run_benchmark(module_count: int, symbols_per_module: int, lookups: int, seed: int) -> dict:
    random.seed(seed)

    with tempfile.TemporaryDirectory(prefix="nxl_lsp_bench_") as tmp:
        ws_root = Path(tmp)
        main_file, main_text, lookup_meta = _build_workspace(ws_root, module_count, symbols_per_module)

        index = WorkspaceIndex(str(ws_root))
        t0 = time.perf_counter()
        index.scan_workspace()
        index_time_ms = (time.perf_counter() - t0) * 1000.0

        server = MockServer(documents={}, workspace_index=index)
        main_uri = f"file://{main_file}"
        server.documents[main_uri] = main_text
        provider = DefinitionProvider(server)

        timings_us = []
        misses = 0

        for _ in range(lookups):
            line, char, _sym, expected_path = random.choice(lookup_meta)
            pos = Position(line, char)

            t1 = time.perf_counter()
            loc = provider.get_definition(main_text, pos, main_uri)
            dt_us = (time.perf_counter() - t1) * 1_000_000.0
            timings_us.append(dt_us)

            if not loc or not loc.uri.startswith("file://"):
                misses += 1
                continue
            got_path = loc.uri.replace("file://", "")
            if Path(got_path).resolve() != Path(expected_path).resolve():
                misses += 1

        return {
            "workspace": {
                "modules": module_count,
                "symbols_per_module": symbols_per_module,
                "total_symbols_approx": module_count * symbols_per_module,
            },
            "indexing": {
                "indexed_files": len(index.indexed_files),
                "index_time_ms": round(index_time_ms, 3),
            },
            "lookup": {
                "lookups": lookups,
                "misses": misses,
                "hit_rate": round((lookups - misses) / max(1, lookups), 6),
                "mean_us": round(mean(timings_us), 3) if timings_us else 0.0,
                "p50_us": round(_percentile(timings_us, 50), 3),
                "p95_us": round(_percentile(timings_us, 95), 3),
                "p99_us": round(_percentile(timings_us, 99), 3),
                "max_us": round(max(timings_us) if timings_us else 0.0, 3),
            },
        }


def main() -> int:
    parser = argparse.ArgumentParser(description="Benchmark LSP definition lookup in large workspaces")
    parser.add_argument("--modules", type=int, default=500, help="Number of generated module files")
    parser.add_argument("--symbols-per-module", type=int, default=8, help="Function symbols generated per module")
    parser.add_argument("--lookups", type=int, default=5000, help="Definition lookup queries to execute")
    parser.add_argument("--seed", type=int, default=1337, help="Random seed")
    args = parser.parse_args()

    result = run_benchmark(
        module_count=args.modules,
        symbols_per_module=args.symbols_per_module,
        lookups=args.lookups,
        seed=args.seed,
    )
    print(json.dumps(result, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
