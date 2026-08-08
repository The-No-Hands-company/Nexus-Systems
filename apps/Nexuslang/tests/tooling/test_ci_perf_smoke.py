"""Regression tests for perf smoke fixture validation."""

import importlib.util
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
SCRIPT_PATH = REPO_ROOT / "scripts" / "ci_perf_smoke.py"


def _load_perf_smoke_module():
    spec = importlib.util.spec_from_file_location("ci_perf_smoke", SCRIPT_PATH)
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def test_validate_parseable_sample_accepts_known_good_fixture():
    perf_smoke = _load_perf_smoke_module()
    sample = REPO_ROOT / "examples" / "build_test_hello.nxl"

    assert perf_smoke.validate_parseable_sample(sample) is None


def test_validate_parseable_sample_reports_targeted_error_for_invalid_fixture(tmp_path):
    perf_smoke = _load_perf_smoke_module()
    sample = tmp_path / "invalid_perf_fixture.nxl"
    sample.write_text(
        "function main returns Integer\n"
        "    set broken to (\n"
        "    return 0\n"
        "end\n",
        encoding="utf-8",
    )

    error = perf_smoke.validate_parseable_sample(sample)

    assert error is not None
    assert "Invalid perf smoke sample" in error
    assert str(sample.resolve()) in error
    assert "line" in error
    assert "column" in error
    assert "Traceback" not in error


def test_validate_parseable_samples_accumulates_fixture_errors(tmp_path):
    perf_smoke = _load_perf_smoke_module()
    sample_one = tmp_path / "broken_one.nxl"
    sample_two = tmp_path / "broken_two.nxl"
    sample_one.write_text("set a to (\n", encoding="utf-8")
    sample_two.write_text("function nope\n    set b to (\nend\n", encoding="utf-8")

    errors = perf_smoke.validate_parseable_samples([sample_one, sample_two])

    assert len(errors) == 2
    assert all("Invalid perf smoke sample" in error for error in errors)