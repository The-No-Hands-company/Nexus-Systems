#!/usr/bin/env python3
"""
Ecosystem Editor Debugger — diagnose editor/UI development problems and generate
precise AI prompts for fixing them.

This tool bridges the gap between "the editor feels rough" and "here's exactly what
to tell the AI to fix it." It scans your codebase for known editor anti-patterns,
rates each subsystem against professional editor standards (Blender/Maya/Houdini),
and produces structured diagnostic reports with concrete next steps.

Usage:
    python3 editor_debugger.py scan --root <path> [--format json|md]
    python3 editor_debugger.py prompt <issue-area> [--detailed]
    python3 editor_debugger.py rate  [--root <path>]
    python3 editor_debugger.py track --add <description>
    python3 editor_debugger.py track --list
    python3 editor_debugger.py --help
"""

from __future__ import annotations

import argparse
import json
import re
import sys
import textwrap
from dataclasses import dataclass, field
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

__version__ = "1.0.0"

BASE = Path(__file__).resolve().parent
REPORTS_DIR = BASE / "reports"
FIX_TRACKER_PATH = BASE / "fix-tracker.json"

# ═══════════════════════════════════════════════════════════════════
# Editor subsystems — what makes a professional 3D/modeling editor
# ═══════════════════════════════════════════════════════════════════

EDITOR_SUBSYSTEMS = {
    "viewport": {
        "label": "Viewport / Camera",
        "weight": 25,
        "ideal": "Instant 60fps orbit/pan/zoom, no lag, no jitter. Framing always correct.",
        "anti_patterns": [
            (r"setTimeout|setInterval", "Timer-based updates instead of requestAnimationFrame"),
            (r"\.style\.(left|top|transform)\s*=", "Direct DOM style manipulation instead of Canvas/WebGL"),
            (r"onScroll|onWheel.*preventDefault", "Custom scroll handling may fight browser"),
            (r"camera\.(?!position|rotation|lookAt)", "Unusual camera API usage pattern"),
            (r"(orbit|pan|zoom|dolly).*=.*false", "Camera controls may be disabled"),
            (r"euler|quaternion.*\(0,\s*0,\s*0", "Camera rotation initialized to zero"),
            (r"perspectiveCamera\(\d+,\s*\d+", "Check aspect ratio handling"),
            (r"Math\.(PI|random).*\*\s*[2-9]", "Unusual rotation math for camera"),
        ],
    },
    "selection": {
        "label": "Selection / Picking",
        "weight": 20,
        "ideal": "Click selects instantly. Shift extends, Ctrl toggles. Marquee/rubber-band smooth. Raycasting is pixel-perfect.",
        "anti_patterns": [
            (r"raycaster.*intersect", "Check raycaster efficiency (use BVH, limit objects)"),
            (r"forEach.*intersect|for.*intersect", "Iterating all objects in raycaster path"),
            (r"\.children\.(forEach|map|filter)", "Traversing scene graph without spatial index"),
            (r"mouse\.(clientX|clientY|pageX|pageY)", "Check coordinate space conversion"),
            (r"getBoundingClientRect", "DOM rect may be stale on resize"),
            (r"addEventListener.*click.*\(", "Click handler may not account for 3D space"),
            (r"THREE\.Raycaster\(\s*\)", "New Raycaster per frame (should reuse)"),
            (r"intersectObjects\(\s*\w+\.children", "Intersecting entire scene without filter"),
        ],
    },
    "gizmos": {
        "label": "Transform Gizmos",
        "weight": 15,
        "ideal": "Translate/rotate/scale gizmos snap cleanly, axis-highlight on hover, no accidental multi-axis drags. Grid/angle snapping works.",
        "anti_patterns": [
            (r"TransformControls|Gizmo", "Check if using transform controls"),
            (r"snap.*false|snapping.*false", "Snapping may be disabled"),
            (r"translate|rotate|scale.*=.*\[0,\s*0,\s*0\]", "Gizmo position reset"),
            (r"attach\(\w+\)|detach\(\w*\)", "Gizmo attach/detach pattern"),
            (r"worldPosition|worldQuaternion", "World-space transform issues"),
            (r"onDrag|onPointerDown|onPointerMove", "Custom drag handling"),
            (r"gridSize|gridStep|snapGrid", "Grid snapping configuration"),
            (r"axesHelper|GridHelper", "Debug helper still present"),
        ],
    },
    "layout": {
        "label": "Layout / Panels / Docking",
        "weight": 15,
        "ideal": "Panels dock/drag/resize smoothly. No layout jumps. State persists across sessions.",
        "anti_patterns": [
            (r"resizeObserver|ResizeObserver", "Check if resize handling is efficient"),
            (r"flexbox|grid|display:\s*flex", "CSS layout may fight Canvas sizing"),
            (r"offsetWidth|offsetHeight|clientWidth|clientHeight", "Manual size calculation"),
            (r"position:\s*absolute|position:\s*fixed", "Absolute positioning may overlap 3D viewport"),
            (r"z-index\s*:\s*\d{3,}", "High z-index stacking conflicts"),
            (r"overflow:\s*hidden", "Overflow hidden may clip UI elements"),
            (r"splitter|Split|Resizable|Draggable", "Panel resizing implementation"),
            (r"localStorage|sessionStorage.*layout", "Layout persistence"),
        ],
    },
    "undo": {
        "label": "Undo / Redo System",
        "weight": 10,
        "ideal": "Undo/redo works predictably with Cmd+Z/Cmd+Shift+Z. Command history preserves state accurately. No history corruption.",
        "anti_patterns": [
            (r"undo|redo|history|command.*stack", "Check undo implementation pattern"),
            (r"\.push\(|\.pop\(|\.unshift\(|\.shift\(", "Array mutation in state management"),
            (r"JSON\.(parse|stringify).*state", "Serialization-based undo (slow, lossy)"),
            (r"deepClone|deepCopy|cloneDeep|structuredClone", "Cloning may miss references/non-serializable"),
            (r"useReducer|useState|createStore|zustand", "State management pattern"),
            (r"middleware|immer|produce", "Immer/proxy patterns for undo"),
        ],
    },
    "input": {
        "label": "Input / Keyboard Shortcuts",
        "weight": 10,
        "ideal": "Modifier keys work in all contexts. Shortcuts match Blender/Maya conventions. No key conflicts.",
        "anti_patterns": [
            (r"keydown|keyup|keypress", "Check keyboard event handling"),
            (r"preventDefault|stopPropagation.*key", "May block browser shortcuts"),
            (r"event\.(ctrlKey|metaKey|shiftKey|altKey)", "Modifier key handling"),
            (r"hotkey|shortcut|keybinding", "Shortcut registration system"),
            (r"addEventListener.*key.*(?!once)", "Keyboard listeners not cleaned up"),
            (r"KeyboardEvent.*repeat", "Key repeat handling"),
            (r"input.*capture|pointer.*capture", "Input capture may block other handlers"),
        ],
    },
    "rendering": {
        "label": "Rendering / Materials",
        "weight": 5,
        "ideal": "Wireframe/shaded/rendered modes all render correctly. No z-fighting. Transparency order correct. Grid clear.",
        "anti_patterns": [
            (r"renderer\.render\(\s*\w+,\s*\w+\s*\)", "Render call pattern — check per-frame"),
            (r"requestAnimationFrame|rAF", "Render loop implementation"),
            (r"WebGLRenderer|renderer\.setSize", "Renderer setup"),
            (r"material\.(transparent|opacity|alpha)", "Material transparency"),
            (r"sRGBEncoding|outputColorSpace|toneMapping", "Color space management"),
            (r"shadowMap|shadow\.|castShadow|receiveShadow", "Shadows enabled/disabled"),
            (r"frustumCulled|frustumCulling", "Frustum culling state"),
            (r"doubleSided|side.*DoubleSide", "Double-sided rendering"),
        ],
    },
}

# ═══════════════════════════════════════════════════════════════════
# AI prompt templates per subsystem
# ═══════════════════════════════════════════════════════════════════

PROMPT_TEMPLATES = {
    "viewport": textwrap.dedent("""\
        I need you to improve the viewport/camera system in the Nexus Modeling Editor.
        The current implementation has these specific issues:

        {issues}

        Professional editor reference (Blender/Maya/Houdini standard):
        - Orbit: MMB drag rotates around focus point instantly (no lag, no jitter)
        - Pan: Shift+MMB drag pans perpendicular to view direction
        - Zoom: Scroll wheel zooms toward cursor position, not view center
        - Frame selection: F key frames camera to fit selected objects
        - View presets: Numpad 1/3/7 for front/right/top ortho views
        - Camera respects near/far clip planes properly
        - No "tunneling" through objects when zooming

        Please fix these issues and make the viewport respond at 60fps without frame drops.
        Use requestAnimationFrame for the render loop and clamp delta time.
        """),
    "selection": textwrap.dedent("""\
        I need to fix the selection/picking system in the Nexus Modeling Editor.
        Current problems:

        {issues}

        Expected behavior (Blender standard):
        - Left click: select single object (deselect all others)
        - Shift+Left click: add to selection (extend)
        - Ctrl+Left click: toggle selection
        - Left-click drag: marquee/rubber-band selection
        - Right click: context menu
        - Selection highlight must be pixel-perfect (GPU picking or optimized raycaster)
        - Selection outline should be clearly visible (thick, high-contrast)
        - Raycasting must use a BVH/spatial index — never intersect the entire scene
        - Selection state must persist through camera changes

        Please implement these fixes with precise edge detection and spatial indexing.
        """),
    "gizmos": textwrap.dedent("""\
        The transform gizmos in the Nexus Modeling Editor need improvement.
        Issues found:

        {issues}

        Reference standard (Blender/Maya):
        - G: grab/move, R: rotate, S: scale
        - Axis-constraint: press X/Y/Z after G/R/S to lock to that axis
        - Arrow gizmo: colored axes (R=X, G=Y, B=Z) with cone tips
        - Hover highlight: axis turns yellow/white when mouse is near
        - Drag threshold: 3-5px before drag begins (prevents accidental moves)
        - Snapping: hold Ctrl to snap to grid increment
        - Gizmo size stays constant on screen (scale with distance)
        - World/local orientation toggle with axis indicator

        Please tighten the gizmo interaction to professional standards.
        """),
    "layout": textwrap.dedent("""\
        The editor layout/panel system needs work:

        {issues}

        Professional editor reference:
        - Panels dock to edges of the viewport area
        - Drag panel tab to re-dock or float as window
        - Panel split with draggable divider bar
        - Collapse/expand panels with click on header
        - Layout persists across sessions (localStorage or config file)
        - Resize viewport fills available space, not fixed dimensions
        - No layout jumps when panels open/close
        - Dark theme with consistent color palette

        Please make the layout system robust and production-ready.
        """),
    "undo": textwrap.dedent("""\
        The undo/redo system needs hardening:

        {issues}

        Requirements:
        - Cmd+Z / Ctrl+Z: undo
        - Cmd+Shift+Z / Ctrl+Shift+Z: redo
        - Command pattern: every action is a reversible command
        - History limit: 256 entries minimum, oldest popped when full
        - History is NOT based on JSON.stringify — uses structured command objects
        - Undo must restore: position, rotation, scale, material, name, parent
        - Grouped undo: drag operations commit as single undo step on mouse-up
        - No history corruption on rapid undo/redo
        - Visual feedback: brief toast showing "Undo: Move Object"

        Please implement a robust command-pattern undo system.
        """),
    "input": textwrap.dedent("""\
        The input/keyboard system needs fixing:

        {issues}

        Blender/Maya standard shortcuts to support:
        - G/R/S: grab/rotate/scale
        - Tab: toggle edit mode
        - Delete/X: delete selected
        - Shift+D: duplicate
        - Ctrl+Z / Cmd+Z: undo
        - F: frame selection
        - Numpad 1/3/7: front/right/top view
        - Numpad 5: toggle ortho/perspective
        - Numpad .: frame selection
        - Space: search/command palette
        - Modifier keys must work in all viewport states
        - No conflicts with browser shortcuts where possible
        - Shortcut overlay: hold a key to show radial/wheel menu

        Please implement a proper shortcut registration system.
        """),
    "rendering": textwrap.dedent("""\
        Rendering/material issues in the editor:

        {issues}

        Fix:
        - Wireframe mode: clean edges, no z-fighting at mesh boundaries
        - Shaded mode: consistent lighting, normals correct
        - Material preview: PBR with environment map
        - Grid: infinite grid with fading, major/minor lines
        - Selection outline: post-process or stencil outline (thick, visible through objects)
        - Alpha transparency: correct render order for transparent objects
        - No flickering or z-fighting artifacts
        - Anti-aliasing enabled (MSAA or post-process)
        - Background gradient (dark gray to black, not flat)

        Please fix rendering to professional editor quality.
        """),
}


# ═══════════════════════════════════════════════════════════════════
# Scanner
# ═══════════════════════════════════════════════════════════════════

@dataclass
class Finding:
    subsystem: str
    file: str
    line: int
    pattern: str
    description: str
    severity: str = "warning"  # warning | critical | info


@dataclass
class SubsystemReport:
    name: str
    label: str
    score: int  # 0-100
    findings: list[Finding] = field(default_factory=list)
    missing_patterns: list[str] = field(default_factory=list)
    recommendations: list[str] = field(default_factory=list)


def scan_codebase(root: Path, exclude_dirs: set[str] | None = None, max_files: int = 500) -> dict[str, SubsystemReport]:
    """Scan a codebase for editor anti-patterns and return per-subsystem reports.

    Args:
        root: Root directory to scan.
        exclude_dirs: Directories to skip (default adds dhts, node_modules, etc).
        max_files: Maximum files to scan (prevents runaway scans).
    """
    if exclude_dirs is None:
        exclude_dirs = {"node_modules", ".git", "dist", "build", "__pycache__", ".venv", "target", "coverage", "Backups", "dhts", ".deepcode"}

    # Collect source files (breadth-first for early cutoff)
    source_files: list[Path] = []
    for ext in (".ts", ".tsx", ".js", ".jsx", ".py", ".rs"):
        for f in root.rglob(f"*{ext}"):
            if not any(part in exclude_dirs for part in f.parts):
                source_files.append(f)
                if len(source_files) >= max_files:
                    break
        if len(source_files) >= max_files:
            break

    if len(source_files) >= max_files:
        print(f"[editor-debugger] Warning: hit {max_files} file limit — results may be partial. Narrow --root.", file=sys.stderr)

    reports: dict[str, SubsystemReport] = {}
    for key, meta in EDITOR_SUBSYSTEMS.items():
        reports[key] = SubsystemReport(name=key, label=meta["label"], score=100)

    # Scan each file against each subsystem's anti-patterns
    for f in source_files:
        try:
            content = f.read_text(encoding="utf-8", errors="replace")
        except Exception:
            continue
        rel = str(f.relative_to(root))

        for key, meta in EDITOR_SUBSYSTEMS.items():
            for pattern, description in meta["anti_patterns"]:
                try:
                    cre = re.compile(pattern, re.IGNORECASE)
                except re.error:
                    continue
                for match in cre.finditer(content):
                    line_no = content[:match.start()].count("\n") + 1
                    reports[key].findings.append(Finding(
                        subsystem=key,
                        file=rel,
                        line=line_no,
                        pattern=pattern,
                        description=description,
                    ))

    # Compute scores: start at 100, subtract for findings
    for key, meta in EDITOR_SUBSYSTEMS.items():
        report = reports[key]
        finding_count = len(report.findings)

        # Calculate missing "ideal" patterns
        ideal_checks = {
            "viewport": ["requestAnimationFrame", "camera.position", "camera.lookAt"],
            "selection": ["Raycaster", "intersectObjects", "setFromCamera"],
            "gizmos": ["TransformControls", "Gizmo", "snap"],
            "layout": ["ResizeObserver", "flex", "splitter"],
            "undo": ["undo", "redo", "command", "history"],
            "input": ["keydown", "KeyboardEvent", "addEventListener"],
            "rendering": ["WebGLRenderer", "requestAnimationFrame", "render"],
        }

        for ideal in ideal_checks.get(key, []):
            found = any(ideal.lower() in f.description.lower() for f in report.findings)
            if not found and not any(ideal.lower() in f.pattern.lower() for f in report.findings):
                report.missing_patterns.append(ideal)

        # Score deductions
        deductions = finding_count * 3 + len(report.missing_patterns) * 5
        report.score = max(0, min(100, 100 - deductions))

        # Generate recommendations
        recs: list[str] = []
        if finding_count > 20:
            recs.append(f"CRITICAL: {finding_count} potential issues found — consider a focused refactor sprint")
        elif finding_count > 10:
            recs.append(f"WARNING: {finding_count} patterns found — address incrementally")
        elif finding_count > 0:
            recs.append(f"INFO: {finding_count} patterns to review")

        if report.missing_patterns:
            recs.append(f"MISSING: No sign of: {', '.join(report.missing_patterns[:5])}")

        if report.score < 30:
            recs.append("This subsystem needs a complete overhaul — see AI prompt template.")
        elif report.score < 60:
            recs.append("Several gaps identified — use the AI prompt generator for targeted fixes.")
        elif report.score < 80:
            recs.append("Minor issues only — incremental polish recommended.")

        report.recommendations = recs

    return reports


# ═══════════════════════════════════════════════════════════════════
# AI Prompt Generator
# ═══════════════════════════════════════════════════════════════════

def generate_prompt(reports: dict[str, SubsystemReport], area: str, detailed: bool = False) -> str:
    """Generate a precise AI prompt for fixing issues in a given subsystem area."""
    if area not in PROMPT_TEMPLATES:
        valid = ", ".join(PROMPT_TEMPLATES.keys())
        return f"Unknown area '{area}'. Valid areas: {valid}"

    report = reports.get(area)
    issues = ""

    if report and report.findings:
        top_findings = report.findings[:15] if not detailed else report.findings
        issues = "\n".join(
            f"- [{f.file}:{f.line}] {f.description}"
            for f in top_findings
        )
        if report.missing_patterns:
            issues += f"\n- MISSING IMPLEMENTATIONS: {', '.join(report.missing_patterns)}"
    else:
        issues = "- No specific code patterns detected — describe your symptoms below:\n  [Your symptoms here]"

    template = PROMPT_TEMPLATES[area]
    return template.format(issues=issues)


# ═══════════════════════════════════════════════════════════════════
# Rating system
# ═══════════════════════════════════════════════════════════════════

def rate_editor(reports: dict[str, SubsystemReport]) -> dict[str, Any]:
    """Rate the editor against professional standards and return a summary."""
    scores = {}
    total_weight = 0
    weighted_sum = 0

    for key, report in reports.items():
        meta = EDITOR_SUBSYSTEMS[key]
        scores[key] = {
            "score": report.score,
            "weight": meta["weight"],
            "rating": "🟢 Excellent" if report.score >= 80 else "🟡 Needs Work" if report.score >= 50 else "🔴 Critical",
            "findings": len(report.findings),
            "missing": report.missing_patterns,
        }
        weighted_sum += report.score * meta["weight"]
        total_weight += meta["weight"]

    overall = round(weighted_sum / total_weight) if total_weight > 0 else 0

    return {
        "overall": overall,
        "grade": (
            "A — Production Ready" if overall >= 85 else
            "B — Mostly Solid" if overall >= 70 else
            "C — Rough Edges" if overall >= 50 else
            "D — Needs Major Work" if overall >= 30 else
            "F — Critical Issues"
        ),
        "subsystems": scores,
        "comparison": {
            "Blender": "A (95+)",
            "Maya": "A (95+)",
            "Houdini": "A (95+)",
            "Plasticity": "A- (90+)",
            "Your Editor": f"{overall}/100",
        },
    }


# ═══════════════════════════════════════════════════════════════════
# Fix tracker
# ═══════════════════════════════════════════════════════════════════

def load_fix_tracker() -> list[dict[str, Any]]:
    if FIX_TRACKER_PATH.exists():
        try:
            return json.loads(FIX_TRACKER_PATH.read_text())
        except (json.JSONDecodeError, OSError):
            pass
    return []


def save_fix_tracker(items: list[dict[str, Any]]) -> None:
    FIX_TRACKER_PATH.parent.mkdir(parents=True, exist_ok=True)
    FIX_TRACKER_PATH.write_text(json.dumps(items, indent=2, ensure_ascii=False) + "\n")


# ═══════════════════════════════════════════════════════════════════
# CLI
# ═══════════════════════════════════════════════════════════════════

def _build_argparser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="editor-debugger",
        description="Nexus Editor Debugger — diagnose and fix editor/UI development issues.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=textwrap.dedent("""\
            Examples:
              editor-debugger scan --root ../../apps/nexus-modeling
              editor-debugger rate --root ../../apps/nexus-modeling
              editor-debugger prompt viewport --detailed
              editor-debugger prompt selection
              editor-debugger track --add "Viewport jitter on zoom"
              editor-debugger track --list
              editor-debugger track --done 1
        """),
    )
    parser.add_argument("--version", action="version", version=f"%(prog)s {__version__}")

    sub = parser.add_subparsers(dest="command", help="Commands")

    # scan
    p_scan = sub.add_parser("scan", help="Scan codebase for editor anti-patterns")
    p_scan.add_argument("--root", required=True, help="Root directory to scan")
    p_scan.add_argument("--format", choices=["json", "md"], default="md", help="Output format")
    p_scan.add_argument("--out", default=None, help="Output file path (default: stdout)")

    # prompt
    p_prompt = sub.add_parser("prompt", help="Generate an AI-ready prompt for fixing a subsystem")
    p_prompt.add_argument("area", help="Subsystem: viewport, selection, gizmos, layout, undo, input, rendering")
    p_prompt.add_argument("--detailed", action="store_true", help="Include all findings, not just top 15")
    p_prompt.add_argument("--root", default=".", help="Root to scan first (default: cwd)")

    # rate
    p_rate = sub.add_parser("rate", help="Rate the editor against professional standards")
    p_rate.add_argument("--root", default=".", help="Root directory to scan")

    # track
    p_track = sub.add_parser("track", help="Track editor fixes over time")
    p_track.add_argument("--add", default=None, help="Add a new issue to track")
    p_track.add_argument("--list", action="store_true", help="List all tracked issues")
    p_track.add_argument("--done", type=int, default=None, help="Mark issue #N as resolved")

    return parser


def build_markdown_report(reports: dict[str, SubsystemReport], rating: dict[str, Any]) -> str:
    lines = [
        "# Nexus Editor Diagnostic Report",
        f"Generated: {datetime.now(timezone.utc).isoformat()}",
        "",
        f"## Overall Rating: {rating['overall']}/100 — {rating['grade']}",
        "",
        "| Subsystem | Score | Rating | Findings |",
        "|-----------|-------|--------|----------|",
    ]
    for key, meta in EDITOR_SUBSYSTEMS.items():
        r = reports[key]
        s = rating["subsystems"][key]
        lines.append(f"| {meta['label']} | {s['score']}/100 | {s['rating']} | {s['findings']} |")

    lines.append("")
    lines.append("## Comparison")
    for editor, grade in rating["comparison"].items():
        lines.append(f"- {editor}: {grade}")

    for key, report in reports.items():
        meta = EDITOR_SUBSYSTEMS[key]
        lines.append("")
        lines.append(f"## {meta['label']} ({report.score}/100)")
        if report.recommendations:
            for rec in report.recommendations:
                lines.append(f"- {rec}")
        if report.findings:
            lines.append(f"\n### Top Issues ({len(report.findings)} total)")
            for f in report.findings[:10]:
                lines.append(f"- `{f.file}:{f.line}` — {f.description}")
            if len(report.findings) > 10:
                lines.append(f"- ... and {len(report.findings) - 10} more")
        if report.missing_patterns:
            lines.append(f"\n### Missing Implementations")
            for m in report.missing_patterns:
                lines.append(f"- Expected but not found: `{m}`")

    lines.append("")
    lines.append("## Next Steps")
    lines.append("Run `editor-debugger prompt <subsystem>` to generate an AI-ready fix prompt.")
    lines.append("Run `editor-debugger track --add <issue>` to track progress.")

    return "\n".join(lines)


def main(argv: list[str] | None = None) -> int:
    parser = _build_argparser()
    ns = parser.parse_args(argv)

    if not ns.command:
        parser.print_help()
        return 0

    if ns.command == "scan":
        root = Path(ns.root).resolve()
        if not root.exists():
            print(f"Error: root path does not exist: {root}", file=sys.stderr)
            return 1

        reports = scan_codebase(root)
        rating = rate_editor(reports)

        if ns.format == "json":
            output = json.dumps({key: {
                "score": r.score,
                "findings": [{"file": f.file, "line": f.line, "description": f.description} for f in r.findings],
                "missing": r.missing_patterns,
                "recommendations": r.recommendations,
            } for key, r in reports.items()}, indent=2)
            # Add rating
            full = {"reports": json.loads(output), "rating": rating}
            output = json.dumps(full, indent=2)
        else:
            output = build_markdown_report(reports, rating)

        if ns.out:
            Path(ns.out).parent.mkdir(parents=True, exist_ok=True)
            Path(ns.out).write_text(output)
            print(f"Report written to {ns.out}")
        else:
            print(output)

    elif ns.command == "prompt":
        root = Path(ns.root).resolve()
        reports = scan_codebase(root) if root.exists() else {}
        prompt = generate_prompt(reports, ns.area, detailed=ns.detailed)
        print(prompt)

    elif ns.command == "rate":
        root = Path(ns.root).resolve()
        reports = scan_codebase(root) if root.exists() else {}
        rating = rate_editor(reports)
        print(f"\nEditor Readiness: {rating['overall']}/100 — {rating['grade']}\n")
        for key, s in rating["subsystems"].items():
            label = EDITOR_SUBSYSTEMS[key]["label"]
            bar = "█" * (s["score"] // 5) + "░" * (20 - s["score"] // 5)
            print(f"  {label:25s} {bar} {s['score']}/100 {s['rating']}")
        print()

    elif ns.command == "track":
        items = load_fix_tracker()
        if ns.add:
            items.append({
                "id": len(items) + 1,
                "description": ns.add,
                "status": "open",
                "created": datetime.now(timezone.utc).isoformat(),
                "resolved": None,
            })
            save_fix_tracker(items)
            print(f"Tracked issue #{len(items)}: {ns.add}")
        elif ns.list:
            if not items:
                print("No tracked issues.")
            else:
                print("\nTracked Issues:")
                for item in items:
                    icon = "✅" if item["status"] == "resolved" else "🔴"
                    print(f"  {icon} #{item['id']} [{item['status']}] {item['description']}")
                print()
        elif ns.done is not None:
            for item in items:
                if item["id"] == ns.done:
                    item["status"] = "resolved"
                    item["resolved"] = datetime.now(timezone.utc).isoformat()
                    save_fix_tracker(items)
                    print(f"Marked #{ns.done} as resolved: {item['description']}")
                    break
            else:
                print(f"Issue #{ns.done} not found.", file=sys.stderr)
                return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
