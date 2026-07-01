#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../../.." && pwd)"
cd "$ROOT_DIR"

ITERATIONS="${1:-15}"
TARGET="examples/showcase_workflow/src/main.nxl"
OUT="examples/showcase_workflow/snapshots/benchmark_current.json"

python - <<'PY' "$ITERATIONS" "$TARGET" "$OUT"
import json
import os
import statistics
import subprocess
import sys
import time

iterations = int(sys.argv[1])
target = sys.argv[2]
out_file = sys.argv[3]

samples_ms = []
for _ in range(iterations):
    t0 = time.perf_counter()
    result = subprocess.run(
        [sys.executable, "-m", "nexuslang.main", target],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        env={**os.environ, "PYTHONPATH": "src"},
    )
    if result.returncode != 0:
        raise SystemExit(result.stderr)
    elapsed_ms = (time.perf_counter() - t0) * 1000.0
    samples_ms.append(elapsed_ms)

samples_ms.sort()
report = {
    "benchmark_name": "showcase_workflow_main",
    "command": f"PYTHONPATH=src python -m nexuslang.main {target}",
    "iterations": iterations,
    "unit": "ms",
    "median_ms": round(statistics.median(samples_ms), 3),
    "p95_ms": round(samples_ms[int(0.95 * (len(samples_ms) - 1))], 3),
    "snapshot_version": "1",
}

with open(out_file, "w", encoding="utf-8") as f:
    json.dump(report, f, indent=2)
    f.write("\n")

print(json.dumps(report, indent=2))
PY
