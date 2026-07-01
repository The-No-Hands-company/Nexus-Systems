#!/usr/bin/env python3
"""
Ecosystem Stack Control and Updater

Scans a mixed-language monorepo and reports stack freshness + security posture.
"""

from __future__ import annotations

import argparse
import asyncio
import difflib
import json
import os
import re
import sys
import urllib.error
import urllib.parse
import urllib.request
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

try:
    import httpx
    _HTTPX_AVAILABLE = True
except ImportError:
    _HTTPX_AVAILABLE = False

try:
    import tomllib  # Python 3.11+
except ModuleNotFoundError:  # pragma: no cover
    tomllib = None

__version__ = "2.0.0"


HTTP_TIMEOUT_SECONDS = int(os.environ.get("STACK_CONTROL_HTTP_TIMEOUT_SECONDS", "12"))


@dataclass
class DepRecord:
    ecosystem: str  # npm | pypi | cargo
    name: str
    spec: str
    source_file: str


def now_iso() -> str:
    return datetime.now(timezone.utc).isoformat()


def load_json_file(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def save_text(path: Path, content: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8")


def http_json(url: str, method: str = "GET", body: dict[str, Any] | None = None) -> dict[str, Any] | list[Any]:
    data = None
    headers = {"Accept": "application/json", "User-Agent": "nexus-stack-control/1.0"}
    if body is not None:
        data = json.dumps(body).encode("utf-8")
        headers["Content-Type"] = "application/json"

    req = urllib.request.Request(url, data=data, headers=headers, method=method)
    with urllib.request.urlopen(req, timeout=HTTP_TIMEOUT_SECONDS) as resp:
        payload = resp.read().decode("utf-8", errors="replace")
    return json.loads(payload)


async def _http_json_async(url: str, method: str = "GET", body: dict[str, Any] | None = None) -> dict[str, Any] | list[Any]:
    """Async variant using httpx when available; falls back to sync urllib."""
    if _HTTPX_AVAILABLE:
        headers = {"Accept": "application/json", "User-Agent": f"nexus-stack-control/{__version__}"}
        async with httpx.AsyncClient(timeout=HTTP_TIMEOUT_SECONDS) as client:
            if method == "POST" and body:
                headers["Content-Type"] = "application/json"
                resp = await client.post(url, json=body, headers=headers)
            else:
                resp = await client.get(url, headers=headers)
            return resp.json()
    # Fallback: run the sync version in a thread
    return await asyncio.to_thread(http_json, url, method, body)


async def fetch_versions_async(deps: list) -> dict[str, dict]:
    """Batch-fetch latest versions for all deps using async HTTP."""
    npm_cache: dict[str, str] = {}
    pypi_cache: dict[str, str] = {}
    cargo_cache: dict[str, str] = {}
    results: dict[str, dict] = {}

    async def _fetch_one(dep) -> None:
        nonlocal npm_cache, pypi_cache, cargo_cache
        latest = None
        try:
            if dep.ecosystem == "npm":
                latest = await _http_json_async(f"https://registry.npmjs.org/{urllib.parse.quote(dep.name)}/latest")
                if isinstance(latest, dict):
                    latest = str(latest.get("version", "")).strip() or None
            elif dep.ecosystem == "pypi":
                latest = await _http_json_async(f"https://pypi.org/pypi/{urllib.parse.quote(dep.name)}/json")
                info = latest.get("info", {}) if isinstance(latest, dict) else {}
                latest = str(info.get("version", "")).strip() if isinstance(info, dict) else None
            elif dep.ecosystem == "cargo":
                latest = await _http_json_async(f"https://crates.io/api/v1/crates/{urllib.parse.quote(dep.name)}")
                crate = latest.get("crate", {}) if isinstance(latest, dict) else {}
                latest = str(crate.get("max_stable_version", "") or crate.get("newest_version", "")).strip() if isinstance(crate, dict) else None
        except Exception:
            latest = None
        results[dep.name] = {"latest": latest, "ecosystem": dep.ecosystem}

    # Process in batches of 8 to avoid overwhelming APIs
    batch_size = 8
    for i in range(0, len(deps), batch_size):
        batch = deps[i:i + batch_size]
        await asyncio.gather(*[_fetch_one(d) for d in batch])

    return results


def is_excluded(path: Path, excluded_dirs: set[str]) -> bool:
    for part in path.parts:
        if part in excluded_dirs:
            return True
    return False


def walk_files(
    workspace: Path,
    excluded_dirs: set[str],
    include_roots: list[str],
) -> tuple[list[Path], list[Path], list[Path], list[Path]]:
    package_jsons: list[Path] = []
    requirements: list[Path] = []
    cargo_tomls: list[Path] = []
    dockerfiles: list[Path] = []

    scan_roots: list[Path] = []
    for rel_root in include_roots:
        if rel_root in ("", "."):
            scan_roots.append(workspace)
            continue
        candidate = (workspace / rel_root).resolve()
        if candidate.exists() and candidate.is_dir():
            scan_roots.append(candidate)

    if not scan_roots:
        scan_roots = [workspace]

    seen_files: set[Path] = set()

    for base in scan_roots:
        for root, dirs, files in os.walk(base):
            root_path = Path(root)
            dirs[:] = [d for d in dirs if d not in excluded_dirs]
            if is_excluded(root_path, excluded_dirs):
                continue

            for file_name in files:
                f = root_path / file_name
                if f in seen_files:
                    continue
                seen_files.add(f)
                if file_name == "package.json":
                    package_jsons.append(f)
                elif file_name.startswith("requirements") and file_name.endswith(".txt"):
                    requirements.append(f)
                elif file_name == "Cargo.toml":
                    cargo_tomls.append(f)
                elif file_name.startswith("Dockerfile"):
                    dockerfiles.append(f)

    return package_jsons, requirements, cargo_tomls, dockerfiles


def walk_source_files(
    workspace: Path,
    excluded_dirs: set[str],
    include_roots: list[str],
    extensions: set[str],
) -> list[Path]:
    source_files: list[Path] = []
    scan_roots: list[Path] = []

    for rel_root in include_roots:
        if rel_root in ("", "."):
            scan_roots.append(workspace)
            continue
        candidate = (workspace / rel_root).resolve()
        if candidate.exists() and candidate.is_dir():
            scan_roots.append(candidate)

    if not scan_roots:
        scan_roots = [workspace]

    seen_files: set[Path] = set()
    for base in scan_roots:
        for root, dirs, files in os.walk(base):
            root_path = Path(root)
            dirs[:] = [d for d in dirs if d not in excluded_dirs]
            if is_excluded(root_path, excluded_dirs):
                continue

            for file_name in files:
                f = root_path / file_name
                if f in seen_files:
                    continue
                seen_files.add(f)
                if f.suffix.lower() in extensions:
                    source_files.append(f)

    return source_files


def run_code_audit(
    workspace: Path,
    excluded_dirs: set[str],
    include_roots: list[str],
    config: dict[str, Any],
) -> dict[str, Any]:
    exts = set(str(x).strip().lower() for x in config.get("extensions", []))
    if not exts:
        exts = {".js", ".jsx", ".ts", ".tsx", ".py", ".rs"}

    max_findings = int(config.get("maxFindings", 300))
    patterns = config.get("patterns", [])
    if not isinstance(patterns, list):
        patterns = []

    compiled: list[tuple[dict[str, Any], re.Pattern[str]]] = []
    for p in patterns:
        if not isinstance(p, dict):
            continue
        pid = str(p.get("id", "")).strip()
        pregex = str(p.get("regex", "")).strip()
        if not pid or not pregex:
            continue
        try:
            cre = re.compile(pregex)
        except re.error:
            continue
        compiled.append((p, cre))

    files = walk_source_files(workspace, excluded_dirs, include_roots, exts)
    findings: list[dict[str, Any]] = []
    severity_counts: dict[str, int] = {}

    for f in files:
        try:
            content = f.read_text(encoding="utf-8", errors="replace")
        except Exception:
            continue
        rel_file = str(f.relative_to(workspace))
        for pattern_meta, cre in compiled:
            for match in cre.finditer(content):
                line = content.count("\n", 0, match.start()) + 1
                sev = str(pattern_meta.get("severity", "warning"))
                severity_counts[sev] = severity_counts.get(sev, 0) + 1
                snippet = match.group(0)
                findings.append(
                    {
                        "id": pattern_meta.get("id"),
                        "severity": sev,
                        "description": pattern_meta.get("description"),
                        "file": rel_file,
                        "line": line,
                        "match": snippet[:160],
                    }
                )
                if len(findings) >= max_findings:
                    break
            if len(findings) >= max_findings:
                break
        if len(findings) >= max_findings:
            break

    return {
        "enabled": True,
        "summary": {
            "filesScanned": len(files),
            "patternCount": len(compiled),
            "findings": len(findings),
            "severity": severity_counts,
        },
        "findings": findings,
    }


def parse_package_json(path: Path, workspace: Path) -> tuple[list[DepRecord], dict[str, Any]]:
    data = load_json_file(path)
    records: list[DepRecord] = []

    for key in ("dependencies", "devDependencies", "optionalDependencies", "peerDependencies"):
        deps = data.get(key, {})
        if isinstance(deps, dict):
            for dep_name, dep_spec in deps.items():
                records.append(
                    DepRecord(
                        ecosystem="npm",
                        name=str(dep_name),
                        spec=str(dep_spec),
                        source_file=str(path.relative_to(workspace)),
                    )
                )

    meta = {
        "file": str(path.relative_to(workspace)),
        "name": data.get("name"),
        "engines": data.get("engines", {}),
        "packageManager": data.get("packageManager"),
    }
    return records, meta


def parse_requirements(path: Path, workspace: Path) -> list[DepRecord]:
    records: list[DepRecord] = []
    for raw in path.read_text(encoding="utf-8", errors="replace").splitlines():
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        if line.startswith("-"):
            continue

        name, spec = split_python_requirement(line)
        records.append(
            DepRecord(
                ecosystem="pypi",
                name=name,
                spec=spec,
                source_file=str(path.relative_to(workspace)),
            )
        )
    return records


def split_python_requirement(line: str) -> tuple[str, str]:
    match = re.match(r"^([A-Za-z0-9_.\-]+)(.*)$", line)
    if not match:
        return line, ""
    return match.group(1), match.group(2).strip()


def parse_cargo_toml(path: Path, workspace: Path) -> tuple[list[DepRecord], dict[str, Any]]:
    records: list[DepRecord] = []
    package_meta: dict[str, Any] = {
        "file": str(path.relative_to(workspace)),
        "rust-version": None,
        "package": None,
    }

    if tomllib is None:
        return records, package_meta

    try:
        data = tomllib.loads(path.read_text(encoding="utf-8"))
    except Exception:
        return records, package_meta

    pkg = data.get("package", {})
    if isinstance(pkg, dict):
        package_meta["package"] = pkg.get("name")
        package_meta["rust-version"] = pkg.get("rust-version")

    for block in ("dependencies", "dev-dependencies", "build-dependencies"):
        deps = data.get(block, {})
        if not isinstance(deps, dict):
            continue

        for name, value in deps.items():
            if isinstance(value, str):
                spec = value
            elif isinstance(value, dict):
                spec = str(value.get("version", ""))
            else:
                spec = ""
            records.append(
                DepRecord(
                    ecosystem="cargo",
                    name=str(name),
                    spec=spec,
                    source_file=str(path.relative_to(workspace)),
                )
            )

    return records, package_meta


def parse_dockerfile(path: Path, workspace: Path) -> list[dict[str, str]]:
    bases: list[dict[str, str]] = []
    for raw in path.read_text(encoding="utf-8", errors="replace").splitlines():
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        if line.upper().startswith("FROM "):
            parts = line.split()
            if len(parts) >= 2:
                bases.append(
                    {
                        "file": str(path.relative_to(workspace)),
                        "from": parts[1],
                    }
                )
    return bases


def normalize_npm_latest_version(dep_name: str, cache: dict[str, str]) -> str | None:
    if dep_name in cache:
        return cache[dep_name]
    url = f"https://registry.npmjs.org/{urllib.parse.quote(dep_name)}/latest"
    try:
        data = http_json(url)
    except Exception:
        cache[dep_name] = ""
        return None
    version = str(data.get("version", "")).strip() if isinstance(data, dict) else ""
    cache[dep_name] = version
    return version or None


def normalize_pypi_latest_version(dep_name: str, cache: dict[str, str]) -> str | None:
    key = dep_name.lower()
    if key in cache:
        return cache[key]
    url = f"https://pypi.org/pypi/{urllib.parse.quote(dep_name)}/json"
    try:
        data = http_json(url)
    except Exception:
        cache[key] = ""
        return None
    info = data.get("info", {}) if isinstance(data, dict) else {}
    version = str(info.get("version", "")).strip() if isinstance(info, dict) else ""
    cache[key] = version
    return version or None


def normalize_cargo_latest_version(dep_name: str, cache: dict[str, str]) -> str | None:
    if dep_name in cache:
        return cache[dep_name]
    url = f"https://crates.io/api/v1/crates/{urllib.parse.quote(dep_name)}"
    try:
        data = http_json(url)
    except Exception:
        cache[dep_name] = ""
        return None
    crate = data.get("crate", {}) if isinstance(data, dict) else {}
    version = str(crate.get("max_stable_version", "") or crate.get("newest_version", "")).strip() if isinstance(crate, dict) else ""
    cache[dep_name] = version
    return version or None


def extract_exact_version(spec: str) -> str | None:
    s = spec.strip()
    if not s:
        return None

    # Exact semantic version like 1.2.3 or ==1.2.3
    if re.fullmatch(r"==?\s*\d+(?:\.\d+){0,3}", s):
        return re.sub(r"^==?\s*", "", s)
    if re.fullmatch(r"\d+(?:\.\d+){0,3}", s):
        return s

    return None


def parse_version_tuple(version: str) -> tuple[int, ...]:
    nums = re.findall(r"\d+", version)
    if not nums:
        return tuple()
    return tuple(int(n) for n in nums)


def seems_outdated(spec: str, latest: str | None) -> bool:
    if not latest:
        return False
    pinned = extract_exact_version(spec)
    if not pinned:
        return False
    return parse_version_tuple(pinned) < parse_version_tuple(latest)


def major_lag(spec: str, latest: str | None) -> int:
    pinned = extract_exact_version(spec)
    if not pinned or not latest:
        return 0
    pinned_tuple = parse_version_tuple(pinned)
    latest_tuple = parse_version_tuple(latest)
    if not pinned_tuple or not latest_tuple:
        return 0
    return max(0, latest_tuple[0] - pinned_tuple[0])


def compare_version_tuples(a: tuple[int, ...], b: tuple[int, ...]) -> int:
    max_len = max(len(a), len(b))
    aa = a + (0,) * (max_len - len(a))
    bb = b + (0,) * (max_len - len(b))
    if aa < bb:
        return -1
    if aa > bb:
        return 1
    return 0


def is_version_less_than(version_a: str, version_b: str) -> bool:
    return compare_version_tuples(parse_version_tuple(version_a), parse_version_tuple(version_b)) < 0


def is_version_greater_or_equal(version_a: str, version_b: str) -> bool:
    return compare_version_tuples(parse_version_tuple(version_a), parse_version_tuple(version_b)) >= 0


def build_exact_dependency_index(deps: list[DepRecord]) -> dict[tuple[str, str], str]:
    """
    Build exact-version index keyed by (ecosystem, dependency-name-lower).
    If multiple exact versions exist, keep the highest one.
    """
    index: dict[tuple[str, str], str] = {}
    for dep in deps:
        ver = extract_exact_version(dep.spec)
        if not ver:
            continue
        key = (dep.ecosystem, dep.name.lower())
        current = index.get(key)
        if current is None or is_version_less_than(current, ver):
            index[key] = ver
    return index


def run_semantic_analysis(
    workspace: Path,
    excluded_dirs: set[str],
    include_roots: list[str],
    deps: list[DepRecord],
    config: dict[str, Any],
) -> dict[str, Any]:
    """
    Strict semantic phase: checks source usage against exact dependency versions.

    A finding is reported only when:
    - dependency exact version is known, and
    - rule version gate indicates usage is unsupported for that exact version, and
    - usage pattern is present in source files for the target ecosystem.
    """
    rules = config.get("rules", [])
    if not isinstance(rules, list):
        rules = []
    max_findings = int(config.get("maxFindings", 300))

    ext_cfg = config.get("languageExtensions", {}) if isinstance(config.get("languageExtensions", {}), dict) else {}
    ts_exts = set(str(x).lower() for x in ext_cfg.get("typescript", [".ts", ".tsx", ".js", ".jsx", ".mjs", ".cjs"]))
    py_exts = set(str(x).lower() for x in ext_cfg.get("python", [".py"]))
    rs_exts = set(str(x).lower() for x in ext_cfg.get("rust", [".rs"]))

    files_by_eco = {
        "typescript": walk_source_files(workspace, excluded_dirs, include_roots, ts_exts),
        "python": walk_source_files(workspace, excluded_dirs, include_roots, py_exts),
        "rust": walk_source_files(workspace, excluded_dirs, include_roots, rs_exts),
    }

    dep_index = build_exact_dependency_index(deps)
    findings: list[dict[str, Any]] = []
    unresolved_versions = 0
    rules_evaluated = 0
    by_ecosystem_counts: dict[str, int] = {"typescript": 0, "python": 0, "rust": 0}

    for raw_rule in rules:
        if not isinstance(raw_rule, dict):
            continue

        rule_id = str(raw_rule.get("id", "")).strip()
        dep_eco = str(raw_rule.get("ecosystem", "")).strip().lower()
        dep_name = str(raw_rule.get("dependency", "")).strip().lower()
        src_eco = str(raw_rule.get("sourceEcosystem", "")).strip().lower()
        requires_at_least = str(raw_rule.get("requiresAtLeast", "")).strip()
        unsupported_at_or_above = str(raw_rule.get("unsupportedAtOrAbove", "")).strip()
        usage_regex = str(raw_rule.get("usageRegex", "")).strip()
        desc = str(raw_rule.get("description", "")).strip()
        sev = str(raw_rule.get("severity", "high")).strip().lower()

        if not rule_id or not dep_eco or not dep_name or not src_eco or not usage_regex:
            continue

        if src_eco not in files_by_eco:
            continue

        try:
            pattern = re.compile(usage_regex)
        except re.error:
            continue

        dep_version = dep_index.get((dep_eco, dep_name))
        if not dep_version:
            unresolved_versions += 1
            continue

        is_unsupported = False
        if requires_at_least and is_version_less_than(dep_version, requires_at_least):
            is_unsupported = True
        if unsupported_at_or_above and is_version_greater_or_equal(dep_version, unsupported_at_or_above):
            is_unsupported = True

        rules_evaluated += 1
        if not is_unsupported:
            continue

        scan_files = files_by_eco[src_eco]
        for src in scan_files:
            try:
                content = src.read_text(encoding="utf-8", errors="replace")
            except Exception:
                continue

            rel = str(src.relative_to(workspace))
            for match in pattern.finditer(content):
                line = content.count("\n", 0, match.start()) + 1
                findings.append(
                    {
                        "ruleId": rule_id,
                        "severity": sev,
                        "description": desc,
                        "sourceEcosystem": src_eco,
                        "dependency": dep_name,
                        "dependencyEcosystem": dep_eco,
                        "dependencyVersion": dep_version,
                        "requiresAtLeast": requires_at_least or None,
                        "unsupportedAtOrAbove": unsupported_at_or_above or None,
                        "file": rel,
                        "line": line,
                        "match": match.group(0)[:180],
                    }
                )
                by_ecosystem_counts[src_eco] = by_ecosystem_counts.get(src_eco, 0) + 1
                if len(findings) >= max_findings:
                    break
            if len(findings) >= max_findings:
                break
        if len(findings) >= max_findings:
            break

    return {
        "enabled": True,
        "summary": {
            "rulesConfigured": len([r for r in rules if isinstance(r, dict)]),
            "rulesEvaluated": rules_evaluated,
            "unresolvedDependencyVersions": unresolved_versions,
            "findings": len(findings),
            "byEcosystem": by_ecosystem_counts,
            "filesScanned": {
                "typescript": len(files_by_eco["typescript"]),
                "python": len(files_by_eco["python"]),
                "rust": len(files_by_eco["rust"]),
            },
        },
        "findings": findings,
    }


def resolve_enforcement_profile(policy: dict[str, Any], cli_policy_mode: str | None) -> dict[str, Any]:
    report = policy.get("report", {}) if isinstance(policy.get("report", {}), dict) else {}
    mode = (cli_policy_mode or str(report.get("policyMode", "balanced"))).strip().lower()
    if mode not in {"strict", "balanced", "conservative"}:
        mode = "balanced"

    # Baseline behavior by mode.
    defaults = {
        "strict": {
            "failOnCritical": True,
            "failOnOutdated": True,
            "failOnMajorLag": True,
            "majorLagThreshold": 1,
            "maxTotalOutdated": 0,
        },
        "balanced": {
            "failOnCritical": True,
            "failOnOutdated": False,
            "failOnMajorLag": True,
            "majorLagThreshold": 1,
            "maxTotalOutdated": 60,
        },
        "conservative": {
            "failOnCritical": True,
            "failOnOutdated": False,
            "failOnMajorLag": True,
            "majorLagThreshold": 2,
            "maxTotalOutdated": 150,
        },
    }[mode]

    resolved = {
        "mode": mode,
        "failOnCritical": bool(report.get("failOnCritical", defaults["failOnCritical"])),
        "failOnOutdated": bool(report.get("failOnOutdated", defaults["failOnOutdated"])),
        "failOnMajorLag": bool(report.get("failOnMajorLag", defaults["failOnMajorLag"])),
        "majorLagThreshold": int(report.get("majorLagThreshold", defaults["majorLagThreshold"])),
        "maxTotalOutdated": int(report.get("maxTotalOutdated", defaults["maxTotalOutdated"])),
    }
    return resolved


def fetch_node_release_snapshot() -> dict[str, Any]:
    try:
        data = http_json("https://nodejs.org/dist/index.json")
        if not isinstance(data, list) or not data:
            return {"current": None, "latestLts": None}

        current = str(data[0].get("version", "")).lstrip("v")
        lts_candidates = [item for item in data if isinstance(item, dict) and item.get("lts")]
        latest_lts = str(lts_candidates[0].get("version", "")).lstrip("v") if lts_candidates else None
        lts_major: int | None = None
        if latest_lts:
            tup = parse_version_tuple(latest_lts)
            if tup:
                lts_major = tup[0]
        return {
            "current": current or None,
            "latestLts": latest_lts,
            "latestLtsMajor": lts_major,
        }
    except Exception:
        return {"current": None, "latestLts": None, "latestLtsMajor": None}


def derive_ecmascript_edition() -> str:
    """ECMAScript aligns to the calendar year it is ratified (ES2026 in 2026, etc.)."""
    return f"ES{datetime.now(timezone.utc).year}"


def resolve_policy_standards(
    policy: dict[str, Any], node_snapshot: dict[str, Any]
) -> dict[str, Any]:
    """Replace 'auto' sentinel values in standards config with live-detected versions."""
    raw_standards = policy.get("standards", {})
    resolved: dict[str, Any] = dict(raw_standards)

    if resolved.get("ecmascript") in (None, "", "auto"):
        resolved["ecmascript"] = derive_ecmascript_edition()

    node_cfg = dict(resolved.get("node", {}))
    if str(node_cfg.get("minimumLtsMajor", "auto")) == "auto":
        lts_major = node_snapshot.get("latestLtsMajor")
        if lts_major is not None:
            node_cfg["minimumLtsMajor"] = lts_major
    resolved["node"] = node_cfg

    return resolved


def create_github_issues(
    repo: str,
    token: str,
    advisories: list[dict[str, Any]],
    outdated_npm: list[dict[str, Any]],
    outdated_pypi: list[dict[str, Any]],
    outdated_cargo: list[dict[str, Any]],
) -> list[str]:
    """Post GitHub issues for security advisories and outdated deps. Returns created issue URLs."""
    api_base = f"https://api.github.com/repos/{repo}/issues"
    api_headers = {
        "Accept": "application/vnd.github+json",
        "Authorization": f"Bearer {token}",
        "X-GitHub-Api-Version": "2022-11-28",
        "User-Agent": "nexus-stack-control/1.0",
        "Content-Type": "application/json",
    }

    def issue_exists(title: str) -> bool:
        try:
            existing = http_json(f"{api_base}?state=open&per_page=100")
        except Exception:
            return False
        if not isinstance(existing, list):
            return False
        for item in existing:
            if isinstance(item, dict) and str(item.get("title", "")).strip() == title:
                return True
        return False

    def post_issue(title: str, body: str, labels: list[str]) -> str | None:
        if issue_exists(title):
            print(f"[stack-control] issue already exists, skipping: {title}")
            return None
        payload = {"title": title, "body": body, "labels": labels}
        data = json.dumps(payload).encode("utf-8")
        req = urllib.request.Request(api_base, data=data, headers=api_headers, method="POST")
        try:
            with urllib.request.urlopen(req, timeout=HTTP_TIMEOUT_SECONDS) as resp:
                result = json.loads(resp.read().decode("utf-8"))
            url = result.get("html_url") if isinstance(result, dict) else None
            if url:
                print(f"[stack-control] created issue: {url}")
            return url
        except Exception as exc:
            print(f"[stack-control] issue creation failed: {exc}", file=sys.stderr)
            return None

    created: list[str] = []

    if advisories:
        rows = ["| Package | Version | File | Advisory IDs |", "|---------|---------|------|--------------|"] + [
            "| {name} | {ver} | `{src}` | {ids} |".format(
                name=a["name"],
                ver=a["version"],
                src=a["sourceFile"],
                ids=", ".join(v.get("id", "") for v in a.get("vulnerabilities", [])),
            )
            for a in advisories[:60]
        ]
        body = "## Security Advisories Detected\n\n" + "\n".join(rows) + "\n\nRun `stack_control.py --apply-write` to attempt automated patches.\n"
        url = post_issue(
            f"[stack-control] \U0001f534 Security advisories: {len(advisories)} packages affected",
            body,
            ["security", "dependencies", "automated"],
        )
        if url:
            created.append(url)

    all_outdated = outdated_npm + outdated_pypi + outdated_cargo
    if all_outdated:
        rows = ["| Ecosystem | Package | Pinned | Latest | File |", "|-----------|---------|--------|--------|------|"] + [
            "| {eco} | {name} | `{spec}` | `{latest}` | `{src}` |".format(
                eco=item["ecosystem"],
                name=item["name"],
                spec=item["spec"],
                latest=item["latest"],
                src=item["sourceFile"],
            )
            for item in all_outdated[:60]
        ]
        body = f"## Outdated Dependencies ({len(all_outdated)} total)\n\n" + "\n".join(rows) + "\n\nRun `stack_control.py --apply` (dry-run) or `--apply-write` (patch files) to address these.\n"
        url = post_issue(
            f"[stack-control] \U0001f7e1 Outdated dependencies: {len(all_outdated)} packages",
            body,
            ["dependencies", "maintenance", "automated"],
        )
        if url:
            created.append(url)

    return created


def apply_upgrades(
    workspace: Path,
    outdated_npm: list[dict[str, Any]],
    outdated_pypi: list[dict[str, Any]],
    outdated_cargo: list[dict[str, Any]],
    write: bool = False,
    patch_output_dir: Path | None = None,
) -> tuple[dict[str, list[str]], dict[str, list[str]]]:
    """
    Propose (dry_run) or apply (write=True) version bumps per app directory.
    Returns tuple: ({app_dir: [change descriptions]}, {app_dir: [patch file paths]}).
    """
    changes: dict[str, list[str]] = {}
    patch_files: dict[str, list[str]] = {}
    before_content: dict[Path, str] = {}
    after_content: dict[Path, str] = {}

    def group_from_source(source_file: str) -> str:
        parts = list(Path(source_file).parts)
        if not parts:
            return source_file
        if parts[0] == "apps" and len(parts) >= 2:
            return f"apps/{parts[1]}"
        return parts[0]

    def record(source_file: str, msg: str) -> None:
        app_dir = group_from_source(source_file)
        changes.setdefault(app_dir, []).append(msg)

    # --- npm: patch package.json ---
    for item in outdated_npm:
        src = workspace / item["sourceFile"]
        if not src.exists():
            continue
        try:
            data = load_json_file(src)
            original_text = src.read_text(encoding="utf-8", errors="replace")
        except Exception:
            continue

        modified = False
        for key in ("dependencies", "devDependencies", "optionalDependencies"):
            deps = data.get(key, {})
            if not isinstance(deps, dict) or item["name"] not in deps:
                continue
            old_spec = str(deps[item["name"]])
            if not extract_exact_version(old_spec):
                continue
            record(item["sourceFile"], f"npm: {item['name']} {old_spec} \u2192 {item['latest']}")
            deps[item["name"]] = item["latest"]
            data[key] = deps
            modified = True

        if modified:
            if src not in before_content:
                before_content[src] = original_text
            updated_text = json.dumps(data, indent=2, ensure_ascii=False) + "\n"
            after_content[src] = updated_text
            if write:
                src.write_text(updated_text, encoding="utf-8")

    # --- pypi: patch requirements*.txt ---
    for item in outdated_pypi:
        src = workspace / item["sourceFile"]
        if not src.exists():
            continue
        try:
            raw_lines = src.read_text(encoding="utf-8", errors="replace").splitlines(keepends=True)
        except Exception:
            continue

        original_text = "".join(raw_lines)
        new_lines: list[str] = []
        changed = False
        for line in raw_lines:
            stripped = line.strip()
            name, spec = split_python_requirement(stripped)
            if name.lower() == item["name"].lower() and extract_exact_version(spec):
                record(item["sourceFile"], f"pypi: {name}{spec} \u2192 =={item['latest']}")
                new_lines.append(line.replace(stripped, f"{name}=={item['latest']}"))
                changed = True
                continue
            new_lines.append(line)

        if changed:
            if src not in before_content:
                before_content[src] = original_text
            updated_text = "".join(new_lines)
            after_content[src] = updated_text
            if write:
                src.write_text(updated_text, encoding="utf-8")

    # --- cargo: patch Cargo.toml (simple `name = "x.y.z"` form only) ---
    for item in outdated_cargo:
        src = workspace / item["sourceFile"]
        if not src.exists():
            continue
        pinned = extract_exact_version(item["spec"])
        if not pinned:
            continue
        try:
            content = src.read_text(encoding="utf-8")
        except Exception:
            continue

        pattern = re.compile(
            r'(^' + re.escape(item["name"]) + r'\s*=\s*")(' + re.escape(pinned) + r')(")',
            re.MULTILINE,
        )
        if pattern.search(content):
            record(item["sourceFile"], f"cargo: {item['name']} {item['spec']} \u2192 {item['latest']}")
            if src not in before_content:
                before_content[src] = content
            new_content = pattern.sub(r"\g<1>" + item["latest"] + r"\3", content)
            after_content[src] = new_content
            if write:
                src.write_text(new_content, encoding="utf-8")

    if patch_output_dir:
        patch_output_dir.mkdir(parents=True, exist_ok=True)
        patch_pairs = [(path, before_content[path], after_content[path]) for path in before_content if path in after_content]

        for file_path, before, after in patch_pairs:
            if before == after:
                continue
            rel = str(file_path.relative_to(workspace))
            group = group_from_source(rel)
            group_dir_name = group.replace("/", "__")
            group_dir = patch_output_dir / group_dir_name
            group_dir.mkdir(parents=True, exist_ok=True)
            patch_name = file_path.name + ".patch"
            patch_path = group_dir / patch_name
            diff_text = "".join(
                difflib.unified_diff(
                    before.splitlines(keepends=True),
                    after.splitlines(keepends=True),
                    fromfile=f"a/{rel}",
                    tofile=f"b/{rel}",
                )
            )
            patch_path.write_text(diff_text, encoding="utf-8")
            patch_files.setdefault(group, []).append(str(patch_path.relative_to(workspace)))

    return changes, patch_files


def batch_osv_queries(records: list[DepRecord]) -> list[dict[str, Any]]:
    # Only exact versions can be checked reliably
    queries: list[dict[str, Any]] = []
    mapped: list[tuple[DepRecord, str]] = []

    ecosystem_map = {
        "npm": "npm",
        "pypi": "PyPI",
        "cargo": "crates.io",
    }

    for rec in records:
        version = extract_exact_version(rec.spec)
        if not version:
            continue
        osv_eco = ecosystem_map.get(rec.ecosystem)
        if not osv_eco:
            continue
        queries.append({"package": {"name": rec.name, "ecosystem": osv_eco}, "version": version})
        mapped.append((rec, version))

    if not queries:
        return []

    try:
        response = http_json("https://api.osv.dev/v1/querybatch", method="POST", body={"queries": queries})
    except Exception:
        return []

    if not isinstance(response, dict):
        return []

    results = response.get("results", [])
    if not isinstance(results, list):
        return []

    findings: list[dict[str, Any]] = []
    for idx, result in enumerate(results):
        if idx >= len(mapped):
            break
        rec, version = mapped[idx]
        vulns = result.get("vulns", []) if isinstance(result, dict) else []
        if not vulns:
            continue

        vuln_items = []
        for vuln in vulns:
            if not isinstance(vuln, dict):
                continue
            vuln_items.append(
                {
                    "id": vuln.get("id"),
                    "summary": vuln.get("summary"),
                    "details": vuln.get("details"),
                }
            )

        findings.append(
            {
                "ecosystem": rec.ecosystem,
                "name": rec.name,
                "version": version,
                "sourceFile": rec.source_file,
                "count": len(vuln_items),
                "vulnerabilities": vuln_items,
            }
        )

    return findings


def build_markdown(report: dict[str, Any]) -> str:
    lines: list[str] = []
    lines.append("# Ecosystem Stack Control Report")
    lines.append("")
    lines.append(f"Generated: {report['generatedAt']}")
    lines.append("")

    inv = report["inventory"]
    lines.append("## Inventory")
    lines.append("")
    lines.append(f"- package.json files: {inv['counts']['packageJson']}")
    lines.append(f"- requirements files: {inv['counts']['requirementsTxt']}")
    lines.append(f"- Cargo.toml files: {inv['counts']['cargoToml']}")
    lines.append(f"- Dockerfiles: {inv['counts']['dockerfiles']}")
    lines.append("")

    node = report["runtime"]["nodeReleaseSnapshot"]
    resolved = report.get("resolvedStandards", report.get("policy", {}).get("standards", {}))
    lines.append("## Runtime")
    lines.append("")
    lines.append(f"- Node current: {node.get('current')}")
    lines.append(f"- Node latest LTS: {node.get('latestLts')} (major: {node.get('latestLtsMajor')})")
    lines.append(f"- ECMAScript standard (auto-resolved): {resolved.get('ecmascript')}")
    lines.append(f"- Node LTS minimum (auto-resolved): {resolved.get('node', {}).get('minimumLtsMajor')}")
    lines.append("")

    policy_profile = report.get("enforcement", {})
    lines.append("## Enforcement")
    lines.append("")
    lines.append(f"- Policy mode: {policy_profile.get('mode')}")
    lines.append(f"- Fail on critical advisories: {policy_profile.get('failOnCritical')}")
    lines.append(f"- Fail on any outdated pin: {policy_profile.get('failOnOutdated')}")
    lines.append(f"- Fail on major lag: {policy_profile.get('failOnMajorLag')} (threshold: {policy_profile.get('majorLagThreshold')})")
    lines.append(f"- Max total outdated before fail: {policy_profile.get('maxTotalOutdated')}")
    lines.append("")

    freshness = report["freshness"]
    lines.append("## Freshness")
    lines.append("")
    lines.append(f"- npm outdated direct deps: {freshness['summary']['npmOutdated']}")
    lines.append(f"- pypi outdated direct deps: {freshness['summary']['pypiOutdated']}")
    lines.append(f"- cargo outdated direct deps: {freshness['summary']['cargoOutdated']}")
    lines.append("")

    sec = report["security"]
    lines.append("## Security")
    lines.append("")
    lines.append(f"- advisories matched (OSV exact pins): {sec['summary']['advisoryMatches']}")
    lines.append("")

    code_audit = report.get("codeAudit", {})
    if code_audit.get("enabled"):
        ca_sum = code_audit.get("summary", {})
        lines.append("## Code Audit")
        lines.append("")
        lines.append(f"- source files scanned: {ca_sum.get('filesScanned')}")
        lines.append(f"- compatibility patterns: {ca_sum.get('patternCount')}")
        lines.append(f"- deprecated/unsafe matches: {ca_sum.get('findings')}")
        lines.append("")

    semantic = report.get("semanticPhase", {})
    if semantic.get("enabled"):
        ss = semantic.get("summary", {})
        files_scanned = ss.get("filesScanned", {})
        lines.append("## Semantic Phase")
        lines.append("")
        lines.append(f"- rules configured: {ss.get('rulesConfigured')}")
        lines.append(f"- rules evaluated (exact dep versions known): {ss.get('rulesEvaluated')}")
        lines.append(f"- unresolved dependency versions: {ss.get('unresolvedDependencyVersions')}")
        lines.append(f"- unsupported API findings: {ss.get('findings')}")
        lines.append(
            f"- source files scanned: ts={files_scanned.get('typescript')} py={files_scanned.get('python')} rs={files_scanned.get('rust')}"
        )
        lines.append("")

    top = (freshness["outdatedNpm"] + freshness["outdatedPypi"] + freshness["outdatedCargo"])[:30]
    if top:
        lines.append("## Top Outdated Items")
        lines.append("")
        for item in top:
            lines.append(
                f"- [{item['ecosystem']}] {item['name']} in {item['sourceFile']} :: spec={item['spec']} latest={item['latest']}"
            )
        lines.append("")

    vulns = sec["advisories"][:30]
    if vulns:
        lines.append("## Advisory Matches")
        lines.append("")

    semantic_findings = semantic.get("findings", [])[:30] if isinstance(semantic, dict) else []
    if semantic_findings:
        lines.append("## Unsupported API Usage")
        lines.append("")
        for item in semantic_findings:
            lines.append(
                "- [{eco}] {rule} in {file}:{line} :: dep={dep}@{ver} requires>={req} match={match}".format(
                    eco=item.get("sourceEcosystem"),
                    rule=item.get("ruleId"),
                    file=item.get("file"),
                    line=item.get("line"),
                    dep=item.get("dependency"),
                    ver=item.get("dependencyVersion"),
                    req=item.get("requiresAtLeast") or "n/a",
                    match=item.get("match"),
                )
            )
        lines.append("")
        for item in vulns:
            lines.append(
                f"- [{item['ecosystem']}] {item['name']}@{item['version']} in {item['sourceFile']} ({item['count']} advisories)"
            )
        lines.append("")

    lines.append("## Notes")
    lines.append("")
    lines.append("- Advisory matching requires exact dependency versions.")
    lines.append("- Range specs should be tightened for stronger exploit mapping and faster emergency patching.")
    lines.append("")

    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser(description="Nexus ecosystem stack control and updater scanner")
    parser.add_argument("--version", action="version", version=f"%(prog)s {__version__}")
    parser.add_argument("--workspace", required=True, help="Workspace root to scan")
    parser.add_argument("--config", required=True, help="Path to stack-control config JSON")
    parser.add_argument("--json-out", required=True, help="Output JSON report path")
    parser.add_argument("--md-out", required=True, help="Output Markdown report path")
    parser.add_argument(
        "--create-issues",
        action="store_true",
        help="Create GitHub issues for findings (needs GITHUB_REPO and GITHUB_TOKEN env vars)",
    )
    parser.add_argument(
        "--apply",
        action="store_true",
        help="Dry-run: print which files would be patched with latest versions",
    )
    parser.add_argument(
        "--apply-write",
        action="store_true",
        help="Actually write version bumps to package.json / requirements*.txt / Cargo.toml",
    )
    parser.add_argument(
        "--fail-on-outdated",
        action="store_true",
        help="Force exit 1 when any outdated pinned dependency is detected",
    )
    parser.add_argument(
        "--policy-mode",
        choices=["strict", "balanced", "conservative"],
        help="Override enforcement profile for this run",
    )
    parser.add_argument(
        "--fail-on-semantic-unsupported",
        action="store_true",
        help="Force exit 1 when semantic phase finds unsupported API usage",
    )
    parser.add_argument(
        "--semantic-only",
        action="store_true",
        help="Run semantic/code audit phase only (skip network freshness/security lookups)",
    )
    args = parser.parse_args()

    workspace = Path(args.workspace).resolve()
    config_path = Path(args.config).resolve()
    json_out = Path(args.json_out).resolve()
    md_out = Path(args.md_out).resolve()

    if not workspace.exists():
        print(f"workspace does not exist: {workspace}", file=sys.stderr)
        return 2
    if not config_path.exists():
        print(f"config does not exist: {config_path}", file=sys.stderr)
        return 2

    policy = load_json_file(config_path)
    excluded_dirs = set(policy.get("scan", {}).get("excludeDirectories", []))
    include_roots = list(policy.get("scan", {}).get("includeRoots", ["."]))

    package_jsons, requirements, cargo_tomls, dockerfiles = walk_files(workspace, excluded_dirs, include_roots)

    deps: list[DepRecord] = []
    node_projects: list[dict[str, Any]] = []
    cargo_projects: list[dict[str, Any]] = []
    docker_bases: list[dict[str, str]] = []

    for p in package_jsons:
        recs, meta = parse_package_json(p, workspace)
        deps.extend(recs)
        node_projects.append(meta)

    for r in requirements:
        deps.extend(parse_requirements(r, workspace))

    for c in cargo_tomls:
        recs, meta = parse_cargo_toml(c, workspace)
        deps.extend(recs)
        cargo_projects.append(meta)

    for d in dockerfiles:
        docker_bases.extend(parse_dockerfile(d, workspace))

    # Fetch live runtime info and resolve dynamic policy standards before scanning
    node_snapshot = fetch_node_release_snapshot()
    resolved_standards = resolve_policy_standards(policy, node_snapshot)
    policy = dict(policy)
    policy["standards"] = resolved_standards

    npm_cache: dict[str, str] = {}
    pypi_cache: dict[str, str] = {}
    cargo_cache: dict[str, str] = {}

    outdated_npm: list[dict[str, Any]] = []
    outdated_pypi: list[dict[str, Any]] = []
    outdated_cargo: list[dict[str, Any]] = []
    major_lag_outdated: list[dict[str, Any]] = []
    advisories: list[dict[str, Any]] = []

    if not args.semantic_only:
        for dep in deps:
            latest = None
            if dep.ecosystem == "npm":
                latest = normalize_npm_latest_version(dep.name, npm_cache)
            elif dep.ecosystem == "pypi":
                latest = normalize_pypi_latest_version(dep.name, pypi_cache)
            elif dep.ecosystem == "cargo":
                latest = normalize_cargo_latest_version(dep.name, cargo_cache)

            if seems_outdated(dep.spec, latest):
                lag = major_lag(dep.spec, latest)
                item = {
                    "ecosystem": dep.ecosystem,
                    "name": dep.name,
                    "spec": dep.spec,
                    "latest": latest,
                    "sourceFile": dep.source_file,
                    "majorLag": lag,
                }
                if dep.ecosystem == "npm":
                    outdated_npm.append(item)
                elif dep.ecosystem == "pypi":
                    outdated_pypi.append(item)
                elif dep.ecosystem == "cargo":
                    outdated_cargo.append(item)
                if lag > 0:
                    major_lag_outdated.append(item)

        advisories = batch_osv_queries(deps)
    enforcement = resolve_enforcement_profile(policy, args.policy_mode)
    code_audit_cfg = policy.get("codeAudit", {}) if isinstance(policy.get("codeAudit", {}), dict) else {}
    code_audit_result = {
        "enabled": False,
        "summary": {"filesScanned": 0, "patternCount": 0, "findings": 0, "severity": {}},
        "findings": [],
    }
    if bool(code_audit_cfg.get("enabled", False)):
        code_audit_result = run_code_audit(workspace, excluded_dirs, include_roots, code_audit_cfg)

    semantic_cfg = policy.get("semanticPhase", {}) if isinstance(policy.get("semanticPhase", {}), dict) else {}
    semantic_result = {
        "enabled": False,
        "summary": {
            "rulesConfigured": 0,
            "rulesEvaluated": 0,
            "unresolvedDependencyVersions": 0,
            "findings": 0,
            "byEcosystem": {"typescript": 0, "python": 0, "rust": 0},
            "filesScanned": {"typescript": 0, "python": 0, "rust": 0},
        },
        "findings": [],
    }
    if bool(semantic_cfg.get("enabled", False)):
        semantic_result = run_semantic_analysis(
            workspace=workspace,
            excluded_dirs=excluded_dirs,
            include_roots=include_roots,
            deps=deps,
            config=semantic_cfg,
        )

    report = {
        "generatedAt": now_iso(),
        "workspace": str(workspace),
        "policy": policy,
        "resolvedStandards": resolved_standards,
        "scanRoots": include_roots,
        "inventory": {
            "counts": {
                "packageJson": len(package_jsons),
                "requirementsTxt": len(requirements),
                "cargoToml": len(cargo_tomls),
                "dockerfiles": len(dockerfiles),
            },
            "nodeProjects": node_projects,
            "cargoProjects": cargo_projects,
            "dockerBases": docker_bases,
        },
        "runtime": {
            "nodeReleaseSnapshot": node_snapshot,
        },
        "freshness": {
            "summary": {
                "npmOutdated": len(outdated_npm),
                "pypiOutdated": len(outdated_pypi),
                "cargoOutdated": len(outdated_cargo),
                "majorLagOutdated": len(major_lag_outdated),
            },
            "outdatedNpm": outdated_npm[: policy.get("report", {}).get("maxOutdatedPerEcosystem", 200)],
            "outdatedPypi": outdated_pypi[: policy.get("report", {}).get("maxOutdatedPerEcosystem", 200)],
            "outdatedCargo": outdated_cargo[: policy.get("report", {}).get("maxOutdatedPerEcosystem", 200)],
            "majorLagItems": major_lag_outdated[: policy.get("report", {}).get("maxOutdatedPerEcosystem", 200)],
        },
        "security": {
            "summary": {
                "advisoryMatches": len(advisories),
            },
            "advisories": advisories[: policy.get("report", {}).get("maxVulnerabilities", 200)],
        },
        "codeAudit": code_audit_result,
        "semanticPhase": semantic_result,
        "automation": {
            "applyMode": "write" if args.apply_write else ("dry-run" if args.apply else "off"),
            "issuesMode": bool(args.create_issues),
            "semanticOnly": bool(args.semantic_only),
        },
        "enforcement": enforcement,
    }

    save_text(json_out, json.dumps(report, indent=2, ensure_ascii=True) + "\n")
    save_text(md_out, build_markdown(report) + "\n")

    print(f"[stack-control] report written: {json_out}")
    print(f"[stack-control] summary written: {md_out}")
    print(f"[stack-control] resolved ECMAScript: {resolved_standards.get('ecmascript')}")
    print(f"[stack-control] resolved Node LTS min: {resolved_standards.get('node', {}).get('minimumLtsMajor')}")

    # --- apply mode ---
    if args.apply or args.apply_write:
        do_write = bool(args.apply_write)
        mode_label = "WRITE" if do_write else "DRY-RUN"
        print(f"\n[stack-control] apply mode [{mode_label}]")
        patch_dir = json_out.parent / "patches"
        groups, patch_files = apply_upgrades(
            workspace,
            outdated_npm,
            outdated_pypi,
            outdated_cargo,
            write=do_write,
            patch_output_dir=patch_dir,
        )
        if groups:
            for app_dir, patches in sorted(groups.items()):
                print(f"  {app_dir}:")
                for p in patches:
                    print(f"    {'PATCHED' if do_write else 'WOULD PATCH'}: {p}")
                group_patch_files = patch_files.get(app_dir, [])
                for pf in group_patch_files:
                    print(f"    PATCH BUNDLE: {pf}")
        else:
            print("  no pinned outdated deps found to patch")

    # --- auto-issue creation ---
    if args.create_issues:
        gh_repo = os.environ.get("GITHUB_REPO", "")
        gh_token = os.environ.get("GITHUB_TOKEN", "")
        if not gh_repo or not gh_token:
            print(
                "[stack-control] --create-issues requires GITHUB_REPO and GITHUB_TOKEN env vars",
                file=sys.stderr,
            )
        else:
            create_github_issues(
                repo=gh_repo,
                token=gh_token,
                advisories=advisories,
                outdated_npm=outdated_npm,
                outdated_pypi=outdated_pypi,
                outdated_cargo=outdated_cargo,
            )

    # --- fail-on-critical: exit 1 when security advisories exist and flag is set ---
    fail_on_critical = bool(enforcement.get("failOnCritical", False))
    if fail_on_critical and advisories:
        print(
            f"[stack-control] FAIL: {len(advisories)} security advisory match(es) — failOnCritical=true",
            file=sys.stderr,
        )
        return 1

    fail_on_outdated = bool(enforcement.get("failOnOutdated", False)) or bool(args.fail_on_outdated)
    total_outdated = len(outdated_npm) + len(outdated_pypi) + len(outdated_cargo)
    if fail_on_outdated and total_outdated > 0:
        print(
            f"[stack-control] FAIL: {total_outdated} outdated pinned dependency(ies) — failOnOutdated=true",
            file=sys.stderr,
        )
        return 1

    fail_on_major_lag = bool(enforcement.get("failOnMajorLag", False))
    major_lag_threshold = int(enforcement.get("majorLagThreshold", 1))
    major_lag_count = sum(1 for item in major_lag_outdated if int(item.get("majorLag", 0)) >= major_lag_threshold)
    if fail_on_major_lag and major_lag_count > 0:
        print(
            f"[stack-control] FAIL: {major_lag_count} dependency(ies) lagging by >= {major_lag_threshold} major version(s)",
            file=sys.stderr,
        )
        return 1

    max_total_outdated = int(enforcement.get("maxTotalOutdated", 0))
    if max_total_outdated >= 0 and total_outdated > max_total_outdated:
        print(
            f"[stack-control] FAIL: outdated total {total_outdated} exceeds maxTotalOutdated={max_total_outdated}",
            file=sys.stderr,
        )
        return 1

    fail_on_deprecated_usage = bool(policy.get("report", {}).get("failOnDeprecatedUsage", False))
    deprecated_findings = int(code_audit_result.get("summary", {}).get("findings", 0))
    if fail_on_deprecated_usage and deprecated_findings > 0:
        print(
            f"[stack-control] FAIL: code audit found {deprecated_findings} deprecated/unsafe usage match(es)",
            file=sys.stderr,
        )
        return 1

    fail_on_semantic_unsupported = bool(policy.get("report", {}).get("failOnSemanticUnsupported", False)) or bool(
        args.fail_on_semantic_unsupported
    )
    semantic_unsupported = int(semantic_result.get("summary", {}).get("findings", 0))
    if fail_on_semantic_unsupported and semantic_unsupported > 0:
        print(
            f"[stack-control] FAIL: semantic phase found {semantic_unsupported} unsupported API usage match(es)",
            file=sys.stderr,
        )
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
