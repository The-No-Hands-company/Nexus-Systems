#!/usr/bin/env python3
"""Auto-scan ecosystem and rebuild visualizer data."""
import json
from pathlib import Path
from datetime import datetime, timezone

ROOT = Path("/run/media/zajferx/Data/dev/The-No-hands-Company/projects/Nexus-Systems")
APPS = ROOT / "apps"
VIS_DIR = ROOT / "dhts" / "ecosystem-visualizer"

with open(VIS_DIR / "ecosystem-data.json") as f:
    old = json.load(f)

old_status = {}
old_desc = {}
old_deps = {}
old_integrates = {}
for eco in old.get("ecosystems", []):
    for cat in eco.get("categories", []):
        for sys in cat.get("systems", []):
            sid = sys.get("id", "").lower()
            old_status[sid] = sys.get("status", "shell")
            old_desc[sid] = sys.get("description", "")
            old_deps[sid] = sys.get("dependencies", [])
            old_integrates[sid] = sys.get("integrates", [])

systems = []
for app_dir in sorted(APPS.iterdir()):
    if not app_dir.is_dir() or app_dir.name.startswith("."):
        continue
    if app_dir.name in ("data", "docs", "tests"):
        continue

    name = app_dir.name
    app_id = name.lower()

    pkg = app_dir / "package.json"
    has_pkg = pkg.exists()
    has_cargo = (app_dir / "Cargo.toml").exists()
    has_python = (app_dir / "pyproject.toml").exists()
    has_cmake = (app_dir / "CMakeLists.txt").exists()
    has_engine = (app_dir / "engine" / "Cargo.toml").exists()
    if not (has_pkg or has_cargo or has_python or has_cmake or has_engine):
        continue

    ts_files = list(app_dir.glob("src/**/*.ts")) + list(app_dir.glob("src/**/*.tsx"))
    rs_files = list(app_dir.glob("engine/**/*.rs"))
    py_files = list(app_dir.glob("backend/**/*.py"))
    is_submodule = (app_dir / ".git").is_dir() or (app_dir / ".git").is_file()
    file_count = len(ts_files) + len(rs_files) + len(py_files)
    if file_count == 0:
        # Try broader search for submodules
        all_rs = list(app_dir.glob("**/*.rs"))
        all_py = list(app_dir.glob("**/*.py"))
        all_cpp = list(app_dir.glob("**/*.cpp"))
        file_count = len(all_rs) + len(all_py) + len(all_cpp)

    # Status from old data, fallback to auto
    status = old_status.get(app_id, "scaffolded")
    if app_id not in old_status:
        if has_cmake:
            status = "native"
        elif has_cargo or has_python:
            status = "built"
        elif file_count >= 4:
            status = "scaffolded"
        else:
            status = "shell"

    # Tech
    if has_cmake:
        tech = "C++23/Vulkan"
    elif has_cargo and has_engine:
        tech = "Rust + WASM"
    elif has_cargo:
        tech = "Rust"
    elif has_engine:
        tech = "Rust + TypeScript"
    elif has_python:
        tech = "Python"
    else:
        tech = "TypeScript"

    # Phantom bound
    has_phantom = False
    for ts_file in ts_files:
        try:
            if "PhantomApp" in ts_file.read_text()[:500]:
                has_phantom = True
                break
        except:
            pass

    port = None
    for srv in list(app_dir.glob("src/server.ts")) + list(app_dir.glob("src/server.js")):
        try:
            for line in srv.read_text().split("\n"):
                if ("PORT" in line or "port" in line) and ("||" in line or "??" in line):
                    parts = line.strip().replace("||", "&&").split("&&") if "&&" in line else line.split("||")
                    for part in parts:
                        part = part.strip().strip('"').strip("'").strip(";").strip()
                        try:
                            port = int(part)
                            break
                        except ValueError:
                            pass
                    if port:
                        break
                    # Try looking for quoted port
                    import re
                    m = re.search(r'"(\d+)"', line)
                    if m:
                        port = int(m.group(1))
                        break
        except:
            pass

    systems.append({
        "id": app_id,
        "name": name,
        "status": status,
        "tech": tech,
        "port": port,
        "files": file_count,
        "phantom": has_phantom,
        "submodule": is_submodule,
        "description": old_desc.get(app_id, f"{name} — {tech}"),
        "dependencies": old_deps.get(app_id, []),
        "integrates": old_integrates.get(app_id, []),
    })

# Categories
cats = {
    "Communication": ["nexus", "nexus-meet", "nexus-team-chat", "nexus-social"],
    "AI & Intelligence": ["nexus-ai", "nexus-computer", "nexus-inference", "nexus-ai-hub", "nexus-agents", "nexusclaw"],
    "Creative & Media": ["nexus-graphic", "nexus-design", "nexus-draw", "nexus-photos", "nexus-video", "nexus-engine", "nexus-arcade", "nexus-game", "nexus-music", "nexus-media", "nexus-broadcast", "nexus-radio-live"],
    "Developer Tools": ["nexus-forge", "nexus-ide", "nexus-deploy", "nexus-code", "nexus-code-review", "nexus-testing", "nexus-terminal", "nexus-search", "nexus-gpu-test", "nexus-modeling"],
    "Productivity": ["nexus-notes", "nexus-tasks", "nexus-calendar", "nexus-planner", "nexus-tutor", "nexus-learn", "nexus-academy", "nexus-book", "nexus-wiki", "nexus-pdf", "nexus-office", "nexus-forms", "nexus-email", "nexus-contracts", "nexus-market", "nexus-sales", "nexus-crm", "nexus-store", "nexus-billing", "nexus-finance", "nexus-spend", "nexus-converter", "nexus-automate", "nexus-data", "nexus-dashboard", "nexus-insights", "nexus-analytics", "nexus-seo", "nexus-mind", "nexus-content", "nexus-survey", "nexus-home", "nexus-health", "nexus-fitness", "nexus-nutrition", "nexus-hr", "nexus-account", "nexus-accounting", "nexus-browsing", "nexus-provenance", "nexus-presence", "nexus-agenda", "nexus-confidential", "nexus-play"],
    "Platform & Security": ["nexus-cloud", "nexus-auth", "nexus-guardian", "nexus-tunnel", "nexus-edge", "nexus-monitor", "nexus-files", "nexus-vault", "nexus-network", "nexus-hosting", "nexus-database", "nexus-security", "nexus-compliance", "nexus-api", "nexus-systems-api", "nexus-warehouse", "nexuslang", "phantom", "nexus-vertical", "nexus-files", "nexus-chronicle", "nexus-context"],
}
cat_colors = {"Communication": "#6366f1", "AI & Intelligence": "#14b8a6", "Creative & Media": "#f59e0b", "Developer Tools": "#ec4899", "Productivity": "#10b981", "Platform & Security": "#8b5cf6", "Packages & SDKs": "#06b6d4"}

categories = []
for cat_name, cat_ids in cats.items():
    cat_systems = [s for s in systems if s["id"] in cat_ids]
    if cat_systems:
        categories.append({"id": cat_name.lower().replace(" & ", "-").replace(" ", "-"), "name": cat_name, "color": cat_colors.get(cat_name, "#06b6d4"), "systems": cat_systems})

# Packages
packages = [
    {"id": "phantom-sdk", "name": "Phantom SDK", "status": "built", "tech": "Rust + TypeScript", "port": None, "files": 10, "phantom": False, "submodule": False, "description": "Post-quantum identity layer — PQ keys, signing, encryption for all apps. 10 tests. 🔐 Bound to Nexus-Graphic, Photos, Design.", "dependencies": ["Phantom (protocol)"], "integrates": ["nexus-graphic", "nexus-photos", "nexus-design"]},
    {"id": "nexus-discovery", "name": "Nexus Discovery", "status": "built", "tech": "TypeScript", "port": None, "files": 3, "phantom": False, "submodule": False, "description": "Service mesh via Cloud topology — resolve(), list(), call(). Replaces all hardcoded cross-app URLs. 6 tests.", "dependencies": ["nexus-cloud"], "integrates": ["nexus-photos", "nexus-design", "nexus-gpu-test"]},
    {"id": "ghost-framework", "name": "Ghost Framework", "status": "built", "tech": "TypeScript", "port": None, "files": 3, "phantom": False, "submodule": False, "description": "CLI code generator for scaffolding new Nexus apps.", "dependencies": [], "integrates": []},
]
categories.append({"id": "packages-sdks", "name": "Packages & SDKs", "color": "#06b6d4", "systems": packages})

data = {
    "schemaVersion": "2.0.0",
    "generatedAt": datetime.now(timezone.utc).isoformat(),
    "organization": old["organization"],
    "ecosystems": [{
        "id": "nexus-systems",
        "name": "Nexus Systems",
        "description": f"{len(systems) + len(packages)} entries — {sum(1 for s in systems if s['status'] == 'native')} native, {sum(1 for s in systems if s['status'] == 'built')} built, {sum(1 for s in systems if s['status'] == 'scaffolded')} scaffolded, {sum(1 for s in systems if s['phantom'])} phantom-bound. Auto-generated + manually curated.",
        "categories": categories,
    }],
}

with open(VIS_DIR / "ecosystem-data.json", "w") as f:
    json.dump(data, f, ensure_ascii=False, indent=2)

print(f"Generated: {len(systems)} apps + {len(packages)} packages")
for s in sorted(systems, key=lambda x: x["status"]):
    if s["status"] in ("native", "built"):
        phantom = " 🔐" if s["phantom"] else ""
        print(f"  {s['status']:12s} {s['name']:30s} {s['tech']}{phantom}")
print(f"  {'---':12s} {'---':30s}")
print(f"  {'scaffolded:':12s} {sum(1 for s in systems if s['status'] == 'scaffolded')}")
print(f"  {'shell:':12s} {sum(1 for s in systems if s['status'] == 'shell')}")
