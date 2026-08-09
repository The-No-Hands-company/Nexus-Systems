"""Tests for ecosystem-stack-control-and-updater / stack_control.py"""

import json
from pathlib import Path

import pytest

import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from stack_control import (
    DepRecord,
    build_markdown,
    compare_version_tuples,
    extract_exact_version,
    is_version_less_than,
    is_version_greater_or_equal,
    major_lag,
    parse_package_json,
    parse_requirements,
    parse_version_tuple,
    resolve_enforcement_profile,
    seems_outdated,
    split_python_requirement,
)


# ── version helpers ───────────────────────────────────────────────────


def test_parse_version_tuple():
    assert parse_version_tuple("1.2.3") == (1, 2, 3)
    assert parse_version_tuple("v10.20.30") == (10, 20, 30)
    assert parse_version_tuple("") == ()
    assert parse_version_tuple("abc") == ()


def test_compare_version_tuples():
    assert compare_version_tuples((1, 0, 0), (2, 0, 0)) == -1
    assert compare_version_tuples((2, 0, 0), (1, 0, 0)) == 1
    assert compare_version_tuples((1, 2, 3), (1, 2, 3)) == 0
    assert compare_version_tuples((1, 2), (1, 2, 3)) == -1  # shorter treated as having trailing zeros


def test_extract_exact_version():
    assert extract_exact_version("1.2.3") == "1.2.3"
    assert extract_exact_version("==1.2.3") == "1.2.3"
    assert extract_exact_version(">=1.0") is None
    assert extract_exact_version("^1.2") is None
    assert extract_exact_version("*") is None
    assert extract_exact_version("") is None


def test_is_version_less_than():
    assert is_version_less_than("1.0.0", "2.0.0") is True
    assert is_version_less_than("2.0.0", "1.0.0") is False
    assert is_version_less_than("1.0.0", "1.0.0") is False


def test_is_version_greater_or_equal():
    assert is_version_greater_or_equal("2.0.0", "1.0.0") is True
    assert is_version_greater_or_equal("1.0.0", "1.0.0") is True
    assert is_version_greater_or_equal("1.0.0", "2.0.0") is False


def test_seems_outdated():
    assert seems_outdated("1.0.0", "2.0.0") is True
    assert seems_outdated("2.0.0", "1.0.0") is False
    assert seems_outdated("^2.0", "3.0") is False  # range spec can't be compared
    assert seems_outdated("1.0.0", None) is False


def test_major_lag():
    assert major_lag("1.0.0", "3.0.0") == 2
    assert major_lag("2.0.0", "2.5.0") == 0
    assert major_lag("^2.0", "3.0") == 0  # range spec


# ── requirement parsing ──────────────────────────────────────────────


def test_split_python_requirement():
    assert split_python_requirement("requests>=2.28") == ("requests", ">=2.28")
    assert split_python_requirement("flask==3.0.0") == ("flask", "==3.0.0")
    assert split_python_requirement("django") == ("django", "")


def test_parse_requirements(tmp_path):
    req_file = tmp_path / "requirements.txt"
    req_file.write_text("flask==3.0.0\n# comment\nrequests>=2.28\n")
    deps = parse_requirements(req_file, tmp_path)
    assert len(deps) == 2
    assert deps[0].name == "flask"
    assert deps[0].spec == "==3.0.0"
    assert deps[1].name == "requests"


# ── package.json parsing ─────────────────────────────────────────────


def test_parse_package_json(tmp_path):
    pkg = tmp_path / "package.json"
    content = json.dumps({"name": "test-pkg", "dependencies": {"express": "^4.18"}, "devDependencies": {"jest": "^29.0"}})
    pkg.write_text(content)
    deps, meta = parse_package_json(pkg, tmp_path)
    assert len(deps) == 2
    assert deps[0].ecosystem == "npm"
    assert deps[0].name == "express"
    assert meta["name"] == "test-pkg"


# ── enforcement profile ──────────────────────────────────────────────


def test_resolve_enforcement_profile_balanced():
    profile = resolve_enforcement_profile({"report": {}}, "balanced")
    assert profile["mode"] == "balanced"
    assert profile["failOnCritical"] is True
    assert profile["majorLagThreshold"] == 1


def test_resolve_enforcement_profile_strict():
    profile = resolve_enforcement_profile({}, "strict")
    assert profile["mode"] == "strict"
    assert profile["failOnOutdated"] is True
    assert profile["maxTotalOutdated"] == 0


def test_resolve_enforcement_profile_conservative():
    profile = resolve_enforcement_profile({}, "conservative")
    assert profile["mode"] == "conservative"
    assert profile["majorLagThreshold"] == 2


# ── markdown report ──────────────────────────────────────────────────


def test_build_markdown():
    report = {
        "generatedAt": "2025-01-01T00:00:00Z",
        "inventory": {"counts": {"packageJson": 3, "requirementsTxt": 1, "cargoToml": 0, "dockerfiles": 2}},
        "runtime": {"nodeReleaseSnapshot": {"current": "23.0.0", "latestLts": "22.11.0", "latestLtsMajor": 22}},
        "resolvedStandards": {"ecmascript": "ES2026", "node": {"minimumLtsMajor": 22}},
        "enforcement": {"mode": "balanced", "failOnCritical": True, "failOnOutdated": False, "failOnMajorLag": True, "majorLagThreshold": 1, "maxTotalOutdated": 60},
        "freshness": {"summary": {"npmOutdated": 5, "pypiOutdated": 2, "cargoOutdated": 1}, "outdatedNpm": [], "outdatedPypi": [], "outdatedCargo": []},
        "security": {"summary": {"advisoryMatches": 0}, "advisories": []},
        "codeAudit": {"enabled": True, "summary": {"filesScanned": 100, "patternCount": 3, "findings": 0, "severity": {}}, "findings": []},
        "semanticPhase": {"enabled": True, "summary": {"rulesConfigured": 3, "rulesEvaluated": 2, "unresolvedDependencyVersions": 1, "findings": 0, "byEcosystem": {}, "filesScanned": {"typescript": 10, "python": 5, "rust": 2}}, "findings": []},
    }
    md = build_markdown(report)
    assert "# Ecosystem Stack Control Report" in md
    assert "ES2026" in md
    assert "package.json files: 3" in md


# ── Informational advisories must not fail the build ─────────────────────────
#
# RustSec files "this crate is unmaintained" through the same channel as
# memory-safety bugs. bincode's RUSTSEC-2025-0141 carries severity None and
# informational="unmaintained", and it matches every 1.x release, so gating on
# it produces a permanently red build that no upgrade can clear.
#
# These drive the real functions with a stubbed transport — asserting against a
# locally recomputed value would pass no matter what the scanner did.

import stack_control as _sc


@pytest.fixture(autouse=True)
def _clear_osv_cache():
    _sc._OSV_DETAIL_CACHE.clear()
    yield
    _sc._OSV_DETAIL_CACHE.clear()


def _stub_osv(monkeypatch, *, batch_ids: list[str], informational: dict[str, str]):
    """Stub http_json for both the querybatch call and the per-advisory lookups."""
    def fake(url, method="GET", body=None):
        if url.endswith("/querybatch"):
            return {"results": [{"vulns": [{"id": i} for i in batch_ids]}]}
        vuln_id = url.rsplit("/", 1)[-1]
        kind = informational.get(vuln_id)
        return {"affected": [{"database_specific": ({"informational": kind} if kind else {})}]}
    monkeypatch.setattr(_sc, "http_json", fake)


def _run(monkeypatch, batch_ids, informational):
    _stub_osv(monkeypatch, batch_ids=batch_ids, informational=informational)
    rec = DepRecord(ecosystem="cargo", name="example", spec="1.0.0", source_file="Cargo.toml")
    findings = _sc.batch_osv_queries([rec])
    assert len(findings) == 1
    return findings[0]


def test_informational_only_finding_does_not_block(monkeypatch):
    f = _run(monkeypatch, ["RUSTSEC-X"], {"RUSTSEC-X": "unmaintained"})
    assert f["count"] == 1
    assert f["blockingCount"] == 0
    assert f["vulnerabilities"][0]["informational"] == "unmaintained"


def test_real_vulnerability_blocks(monkeypatch):
    f = _run(monkeypatch, ["GHSA-real"], {})
    assert f["blockingCount"] == 1
    assert f["vulnerabilities"][0]["informational"] is None


def test_mixed_finding_blocks_on_the_real_one(monkeypatch):
    f = _run(monkeypatch, ["A", "B", "C"], {"A": "unmaintained", "C": "unsound"})
    assert f["count"] == 3
    assert f["blockingCount"] == 1


def test_classifier_fails_open_when_the_lookup_errors(monkeypatch):
    """A network failure must not silently downgrade a genuine advisory."""
    def boom(url, method="GET", body=None):
        if url.endswith("/querybatch"):
            return {"results": [{"vulns": [{"id": "GHSA-real"}]}]}
        raise RuntimeError("network down")
    monkeypatch.setattr(_sc, "http_json", boom)
    rec = DepRecord(ecosystem="cargo", name="example", spec="1.0.0", source_file="Cargo.toml")
    f = _sc.batch_osv_queries([rec])[0]
    assert f["blockingCount"] == 1


def test_enforcement_profile_exposes_fail_on_informational_and_defaults_off():
    from stack_control import resolve_enforcement_profile

    for mode in ("strict", "balanced", "conservative"):
        resolved = resolve_enforcement_profile({"report": {"policyMode": mode}}, None)
        assert resolved["failOnInformational"] is False, mode
        assert resolved["failOnCritical"] is True, mode

    opted_in = resolve_enforcement_profile({"report": {"failOnInformational": True}}, None)
    assert opted_in["failOnInformational"] is True
