"""Tests for ecosystem-editor-debugger / editor_debugger.py"""

import json
from pathlib import Path

import pytest

import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from editor_debugger import (
    EDITOR_SUBSYSTEMS,
    Finding,
    PROMPT_TEMPLATES,
    SubsystemReport,
    build_markdown_report,
    generate_prompt,
    load_fix_tracker,
    rate_editor,
    save_fix_tracker,
    scan_codebase,
)


# ── scan_codebase ────────────────────────────────────────────────────


def test_scan_empty_dir(tmp_path):
    reports = scan_codebase(tmp_path)
    assert len(reports) == len(EDITOR_SUBSYSTEMS)
    # Empty dir = missing all ideal patterns → scores are below 100
    for key, meta in EDITOR_SUBSYSTEMS.items():
        assert reports[key].score < 100
        assert len(reports[key].missing_patterns) > 0


def test_scan_with_editor_code(tmp_path):
    """Create a fake editor file with known anti-patterns."""
    src_file = tmp_path / "src" / "viewport.ts"
    src_file.parent.mkdir(parents=True)
    src_file.write_text("""\
import * as THREE from 'three';

// Bad: setTimeout for render loop
setTimeout(() => render(), 16);

// Bad: direct DOM style manipulation
canvas.style.left = '0px';
canvas.style.top = '0px';

// Good: uses camera
const camera = new THREE.PerspectiveCamera(45, aspect, 0.1, 1000);
camera.position.set(0, 5, 10);
camera.lookAt(0, 0, 0);

// Bad: new raycaster every frame
const raycaster = new THREE.Raycaster();
raycaster.intersectObjects(scene.children);
""")

    reports = scan_codebase(tmp_path)
    viewport = reports["viewport"]
    selection = reports["selection"]

    # viewport should have findings (setTimeout, style manipulation)
    assert viewport.score < 100
    assert len(viewport.findings) > 0

    # selection should have findings (new Raycaster, intersectObjects)
    assert selection.score < 100
    assert len(selection.findings) > 0


# ── rate_editor ──────────────────────────────────────────────────────


def test_rate_perfect():
    reports = {key: SubsystemReport(name=key, label=meta["label"], score=100) for key, meta in EDITOR_SUBSYSTEMS.items()}
    rating = rate_editor(reports)
    assert rating["overall"] == 100
    assert "A" in rating["grade"]


def test_rate_terrible():
    reports = {key: SubsystemReport(name=key, label=meta["label"], score=0) for key, meta in EDITOR_SUBSYSTEMS.items()}
    rating = rate_editor(reports)
    assert rating["overall"] == 0
    assert "F" in rating["grade"]


# ── generate_prompt ─────────────────────────────────────────────────


def test_generate_prompt_viewport():
    reports = {}
    prompt = generate_prompt(reports, "viewport")
    assert "viewport" in prompt.lower()
    assert "Blender" in prompt
    assert "orbit" in prompt.lower()


def test_generate_prompt_unknown_area():
    prompt = generate_prompt({}, "physics_engine")
    assert "Unknown area" in prompt


# ── build_markdown_report ────────────────────────────────────────────


def test_build_markdown_report():
    reports = {
        key: SubsystemReport(name=key, label=meta["label"], score=80)
        for key, meta in EDITOR_SUBSYSTEMS.items()
    }
    rating = rate_editor(reports)
    md = build_markdown_report(reports, rating)
    assert "# Nexus Editor Diagnostic Report" in md
    assert "80/100" in md


# ── fix tracker ──────────────────────────────────────────────────────


def test_fix_tracker_basic(tmp_path, monkeypatch):
    tracker_path = tmp_path / "fix-tracker.json"
    monkeypatch.setattr("editor_debugger.FIX_TRACKER_PATH", tracker_path)

    # Should start empty
    items = load_fix_tracker()
    assert items == []

    # Add an item
    items.append({"id": 1, "description": "Test issue", "status": "open", "created": "2025-01-01", "resolved": None})
    save_fix_tracker(items)

    # Load should return it
    loaded = load_fix_tracker()
    assert len(loaded) == 1
    assert loaded[0]["description"] == "Test issue"


# ── Finding dataclass ────────────────────────────────────────────────


def test_finding_fields():
    f = Finding(subsystem="viewport", file="test.ts", line=42, pattern="setTimeout", description="Timer-based update")
    assert f.subsystem == "viewport"
    assert f.line == 42
    assert f.severity == "warning"
