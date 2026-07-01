#!/usr/bin/env python3
"""
Fix ecosystem-data.json to make Nexus-Cloud the graph center, then rebuild.
Nexus-Cloud (nexus.cloud) = the platform where all 80+ apps live
nexus-core = a shared TypeScript library, NOT an app in /apps

Run: python3 fix_and_rebuild.py
"""

import json
from pathlib import Path

ROOT = Path(__file__).resolve().parent
DATA_PATH = ROOT / "ecosystem-data.json"

def fix_data():
    """Transform ecosystem-data.json so Nexus-Cloud is the visual center."""
    with open(DATA_PATH) as f:
        data = json.load(f)

    # Backup
    bak_path = DATA_PATH.with_suffix('.json.bak2')
    with open(bak_path, 'w') as f:
        json.dump(data, f, indent=2)
    print(f"Backup: {bak_path}")

    changes = {"nexus_cloud_fixed": False, "nexus_core_fixed": False, "systems_got_cloud": 0}

    for eco in data.get("ecosystems", []):
        for cat in eco.get("categories", []):
            for sys in cat.get("systems", []):
                sid = sys["id"]

                # Fix 1: Nexus-Cloud → ALL_SYSTEMS
                if sid == "nexus-cloud":
                    sys["integrates"] = ["ALL_SYSTEMS"]
                    sys["description"] = (
                        "🌐 Sovereign cloud platform — the ONE launchpad for all 80+ Nexus apps. "
                        "Control plane, scheduling, federation, one-click deploy. "
                        "nexus.cloud is where everything lives. Hosts the Nexus Systems API."
                    )
                    changes["nexus_cloud_fixed"] = True
                    print(f"  ✓ nexus-cloud → integrates: ALL_SYSTEMS")

                # Fix 2: nexus-core → library, not a hub
                if sid == "nexus-core":
                    sys["integrates"] = []
                    sys["description"] = (
                        "📦 Core shared TypeScript library — auth helpers, DB connectors, logging, "
                        "config, common utilities. NOT a deployable app — just a package every "
                        "Nexus TS app imports. Found in /packages/nexus-core, not /apps/."
                    )
                    sys["deployment"] = "package"
                    changes["nexus_core_fixed"] = True
                    print(f"  ✓ nexus-core → integrates: [], deployment: package")

        # Fix 3: Any system depending on nexus-core should also integrate with Nexus-Cloud
        for cat in eco.get("categories", []):
            for sys in cat.get("systems", []):
                if sys["id"] in ("nexus-core", "nexus-cloud"):
                    continue
                deps = sys.get("dependencies", [])
                integrates = sys.get("integrates", [])
                if "nexus-core" in deps and "Nexus-Cloud" not in integrates:
                    integrates.append("Nexus-Cloud")
                    sys["integrates"] = integrates
                    changes["systems_got_cloud"] += 1

    print(f"  ✓ {changes['systems_got_cloud']} systems gained Nexus-Cloud integration")

    # Fix 4: Update relationships array — add Nexus-Cloud as the hub
    rels = data.get("relationships", [])
    existing_pairs = {(r["source"], r["target"]) for r in rels}

    # Add: Nexus-Cloud → owns → Nexus-Systems-API (already exists, check)
    # Add: All systems → integrate_with → Nexus-Cloud (reverse direction)
    all_ids = set()
    for eco in data.get("ecosystems", []):
        for cat in eco.get("categories", []):
            for sys in cat.get("systems", []):
                if sys["id"] not in ("nexus-cloud", "nexus-core"):
                    all_ids.add(sys["id"])

    added_rels = 0
    for sid in all_ids:
        pair = (sid, "nexus-cloud")
        if pair not in existing_pairs:
            rels.append({
                "source": sid,
                "target": "nexus-cloud",
                "relation": "available_on"
            })
            existing_pairs.add(pair)
            added_rels += 1

    data["relationships"] = rels
    print(f"  ✓ {added_rels} new relationships added → all apps point to Nexus-Cloud")

    # Write
    with open(DATA_PATH, 'w') as f:
        json.dump(data, f, indent=2)

    print(f"\n✅ Data fixed. {len(all_ids)} total apps, Nexus-Cloud is now the hub.")
    return data


if __name__ == "__main__":
    fix_data()
    print("\nNow run: python3 build.py")
