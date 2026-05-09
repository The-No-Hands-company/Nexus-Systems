"""End-to-end showcase workflow integration tests (lint + build + run)."""

import os
import stat
import sys
from pathlib import Path

_SRC = str(Path(__file__).resolve().parent.parent.parent / "src")
if _SRC not in sys.path:
    sys.path.insert(0, _SRC)

from nexuslang.tooling.analyzer.analyzer import StaticAnalyzer  # noqa: E402
from nexuslang.tooling.builder import BuildSystem  # noqa: E402
from nexuslang.tooling.config import ConfigLoader  # noqa: E402


def _write_project_scaffold(root: Path) -> None:
    (root / "src").mkdir(parents=True, exist_ok=True)
    (root / "build").mkdir(parents=True, exist_ok=True)

    (root / "nexuslang.toml").write_text(
        """
[package]
name = "showcase-e2e"
version = "0.1.0"
authors = []
description = ""

[build]
source_dir = "src"
output_dir = "build"
target = "c"
optimization = 0
headers = false

[dependencies]

[dev-dependencies]

[features]
default = []
""".strip()
        + "\n",
        encoding="utf-8",
    )


def test_showcase_lint_build_run_workflow(tmp_path, monkeypatch):
    repo_root = Path(__file__).resolve().parent.parent.parent
    showcase_source = repo_root / "examples" / "showcase_workflow_demo.nxl"
    assert showcase_source.exists()

    project_root = tmp_path / "showcase_project"
    _write_project_scaffold(project_root)
    (project_root / "src" / "main.nxl").write_text(showcase_source.read_text(encoding="utf-8"), encoding="utf-8")

    # Lint step
    analyzer = StaticAnalyzer(enable_all=True)
    reports = analyzer.analyze_file(str(project_root / "src" / "main.nxl"))
    assert reports is not None

    # Build + run step with fake executable (keeps test deterministic in CI).
    config = ConfigLoader.load(str(project_root / "nexuslang.toml"))
    builder = BuildSystem(config)

    fake_exe = project_root / "build" / "showcase-bin"
    fake_exe.write_text(
        "#!/usr/bin/env bash\n"
        "echo 'Showcase program executed'\n"
        "echo '==42==ERROR: AddressSanitizer: heap-use-after-free' 1>&2\n"
        "exit 0\n",
        encoding="utf-8",
    )
    fake_exe.chmod(fake_exe.stat().st_mode | stat.S_IXUSR)

    monkeypatch.setattr(builder, "build", lambda **kwargs: True)
    monkeypatch.setattr(builder, "_find_executable", lambda out_dir: str(fake_exe))

    exit_code = builder.run(
        sanitize=["address"],
        analyze_runtime=True,
        analysis_output=str(project_root / "build" / "runtime-analysis"),
        profile_source=str(project_root / "src" / "main.nxl"),
    )

    assert exit_code == 0
    assert (project_root / "build" / "runtime-analysis" / "sanitizer-report.json").exists()
    assert (project_root / "build" / "runtime-analysis" / "combined-runtime-analysis.json").exists()
