#!/usr/bin/env python3
"""
Fix ecosystem-data.json so Nexus-Cloud is the true graph center,
not nexus-core (which is just a shared library, not an app).

Changes:
  1. nexus-core: integrase → [] (it's a library, not a hub)
  2. Nexus-Cloud: integrase → ["ALL_SYSTEMS"] (the launchpad for all 80+ apps)
  3. Every system that had nexus-core as a dependency: ADD Nexus-Cloud to integrase
  4. Add edges: every system → Nexus-Cloud (integrates_with)
  5. Update nexus-core description to clarify it's a library
"""

import json
from pathlib import Path

ROOT = Path(__file__).resolve().parent
DATA_PATH = ROOT / "ecosystem-data.json"

with open(DATA_PATH) as f:
    data = json.load(f)

# Backup
backup = ROOT / "ecosystem-data.json.bak"
if not backup.exists():
    with open(backup, 'w') as f:
        json.dump(data, f, indent=2)
    print("Backup saved to ecosystem-data.json.bak")

# Flatten all systems from the nested structure
all_systems = []
for eco in data["ecosystems"]:
    for cat in eco.get("categories", []):
        for sys_obj in cat.get("systems", []):
            all_systems.append(sys_obj)

sys_by_id = {s["id"]: s for s in all_systems}
print(f"Total systems: {len(all_systems)}")

# ═══════════════════════════════════════════════════════
# 1. Fix nexus-core: it's a library, not a hub
# ═══════════════════════════════════════════════════════
if "nexus-core" in sys_by_id:
    nc = sys_by_id["nexus-core"]
    nc["integrates"] = []
    nc["description"] = "Core shared TypeScript library/package — provides common utilities, types, and helpers that every Nexus TypeScript app imports. NOT a deployable app or platform."
    nc["deployment"] = "package"
    print("✓ nexus-core: integrase cleared, marked as library")

# ═══════════════════════════════════════════════════════
# 2. Fix Nexus-Cloud: make it the true center
# ═══════════════════════════════════════════════════════
if "nexus-cloud" in sys_by_id:
    nc = sys_by_id["nexus-cloud"]
    nc["integrates"] = ["ALL_SYSTEMS"]
    nc["description"] = "Orchestrator / sovereign cloud platform — the launchpad where all 80+ Nexus apps live. Control plane, scheduling, federation, one-click app deployment. Hosts the Nexus Systems API. Every Nexus app can be deployed inside Nexus-Cloud."
    print("✓ Nexus-Cloud: integrase = ALL_SYSTEMS")

# ═══════════════════════════════════════════════════════
# 3. Add Nexus-Cloud to integrase for every system that had nexus-core
# ═══════════════════════════════════════════════════════
count_added = 0
for s in all_systems:
    sid = s["id"]
    if sid in ("nexus-cloud", "nexus-core"):
        continue
    
    if "nexus-core" in s.get("dependencies", []):
        if "integrates" not in s:
            s["integrates"] = []
        if "Nexus-Cloud" not in s["integrates"] and "ALL_SYSTEMS" not in s["integrates"]:
            s["integrates"].append("Nexus-Cloud")
            count_added += 1

print(f"✓ {count_added} systems had nexus-core dep → added Nexus-Cloud to integrase")

# ═══════════════════════════════════════════════════════
# 4. Fix relationships: remove nexus-core edges, add Nexus-Cloud edges
# ═══════════════════════════════════════════════════════
rels = data["relationships"]

# Count and remove nexus-core edges
nc_edges = [r for r in rels if r["source"] == "nexus-core" or r["target"] == "nexus-core"]
rels = [r for r in rels if not (r["source"] == "nexus-core" or r["target"] == "nexus-core")]
print(f"✓ Removed {len(nc_edges)} nexus-core edges")

# Add every system → nexus-cloud edge
existing_cloud_sources = {r["source"] for r in rels if r["target"] == "nexus-cloud"}
added_edges = 0
for s in all_systems:
    sid = s["id"]
    if sid in ("nexus-cloud", "nexus-core"):
        continue
    if sid not in existing_cloud_sources:
        rels.append({
            "source": sid,
            "target": "nexus-cloud",
            "relation": "integrates_with"
        })
        added_edges += 1

data["relationships"] = rels
print(f"✓ Added {added_edges} new edges → nexus-cloud")

# ═══════════════════════════════════════════════════════
# 5. Write result
# ═══════════════════════════════════════════════════════
with open(DATA_PATH, 'w') as f:
    json.dump(data, f, indent=2)

print(f"\n✅ Done. {len(all_systems)} systems, {len(rels)} relationships.")
print(f"   nexus-core: 0 integrase entries, 0 edges (library, not hub)")
print(f"   Nexus-Cloud: ALL_SYSTEMS integrase, {added_edges} inbound edges (THE center)")
