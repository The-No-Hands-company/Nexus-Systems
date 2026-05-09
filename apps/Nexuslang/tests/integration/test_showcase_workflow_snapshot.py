"""Integration checks for the hardened showcase workflow package."""

import os
import subprocess
import sys
from pathlib import Path


def test_showcase_workflow_main_matches_snapshot():
    repo_root = Path(__file__).resolve().parent.parent.parent
    source = repo_root / "examples" / "showcase_workflow" / "src" / "main.nxl"
    snapshot = repo_root / "examples" / "showcase_workflow" / "snapshots" / "expected_stdout.txt"

    assert source.exists()
    assert snapshot.exists()

    env = {**os.environ, "PYTHONPATH": "src"}
    run = subprocess.run(
        [sys.executable, "-m", "nexuslang.main", str(source.relative_to(repo_root))],
        cwd=str(repo_root),
        env=env,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        check=False,
    )

    assert run.returncode == 0, run.stderr
    assert run.stdout == snapshot.read_text(encoding="utf-8")


def test_showcase_workflow_assets_present():
    repo_root = Path(__file__).resolve().parent.parent.parent
    fixture = repo_root / "examples" / "showcase_workflow" / "fixtures" / "input_records.csv"
    baseline = repo_root / "examples" / "showcase_workflow" / "snapshots" / "benchmark_baseline.json"
    pipeline_script = repo_root / "examples" / "showcase_workflow" / "scripts" / "run_pipeline.sh"

    assert fixture.exists()
    assert baseline.exists()
    assert pipeline_script.exists()
