#!/usr/bin/env python3
"""
Transform ecosystem-data.json so Nexus-Cloud is the central hub.
Problem: nexus-core (a shared library, NOT an app) had 30+ dependency edges,
making it look like the center of the ecosystem. Nexus-Cloud (the actual platform
where all 80+ apps live) had only 3 connections.

Fix:
  1. Every system that depends on nexus-core ALSO integrates with Nexus-Cloud
     (because it runs on/through the cloud platform)
  2. Nexus-Cloud's integrates becomes ALL_SYSTEMS
  3. nexus-core's integrates changes from ALL_TS_SYSTEMS to a descriptive marker
     (it's a library, not a hub)
  4. nexus-core gets a deployment type of "package" and stays as a dependency
     for technical accuracy, but no longer dominates the graph visually
"""

import json
import sys
from pathlib import Path

DATA_PATH = Path(__file__).parent / "ecosystem-data.json"
BACKUP_PATH = Path(__file__).parent / "ecosystem-data.json.bak"


def transform(data: dict) -> dict:
    ecosystems = data.get("ecosystems", [])
    all_system_ids: set[str] = set()
    all_ts_system_ids: set[str] = set()

    # Gather all system IDs
    for eco in ecosystems:
        for cat in eco.get("categories", []):
            for sys_ in cat.get("systems", []):
                sid = sys_["id"]
                all_system_ids.add(sid)
                if sys_.get("tech") == "TypeScript":
                    all_ts_system_ids.add(sid)

    changes = {
        "nexus_core_deps_added_cloud": 0,
        "nexus_cloud_integrates_updated": False,
        "nexus_core_integrates_changed": False,
    }

    # Pass 1: For every system that lists nexus-core as a dependency,
    # add Nexus-Cloud to its integrates (if not already there).
    for eco in ecosystems:
        for cat in eco.get("categories", []):
            for sys_ in cat.get("systems", []):
                if sys_["id"] == "nexus-core":
                    continue  # skip nexus-core itself
                if sys_["id"] == "nexus-cloud":
                    continue  # skip nexus-cloud itself

                deps = sys_.get("dependencies", [])
                if "nexus-core" in deps:
                    integrates = sys_.setdefault("integrates", [])
                    if "Nexus-Cloud" not in integrates:
                        integrates.append("Nexus-Cloud")
                        changes["nexus_core_deps_added_cloud"] += 1

    # Pass 2: Update Nexus-Cloud's integrates to cover all systems
    for eco in ecosystems:
        for cat in eco.get("categories", []):
            for sys_ in cat.get("systems", []):
                if sys_["id"] == "nexus-cloud":
                    sys_["integrates"] = ["ALL_SYSTEMS"]
                    sys_["description"] = (
                        "Orchestrator / sovereign cloud platform — the launchpad for all 80+ Nexus apps. "
                        "Control plane, scheduling, federation, one-click deploy. Hosts the Nexus Systems API. "
                        "For users: nexus.cloud is where you find everything."
                    )
                    changes["nexus_cloud_integrates_updated"] = True

    # Pass 3: Change nexus-core's integrates to not claim it integrates with everything
    # (it's a shared library, not a platform integration hub)
    for eco in ecosystems:
        for cat in eco.get("categories", []):
            for sys_ in cat.get("systems", []):
                if sys_["id"] == "nexus-core":
                    sys_["integrates"] = []  # it's a library, doesn't "integrate" with apps
                    sys_["description"] = (
                        "Core shared TypeScript library/package — the foundation every Nexus "
                        "TypeScript app builds on. Provides auth helpers, DB connectors, logging, "
                        "config, and common utilities. NOTE: nexus-core is a library, not a deployable app."
                    )
                    sys_["status"] = "built"
                    sys_["deployment"] = "package"
                    # Move to a more appropriate category name hint via description only
                    changes["nexus_core_integrates_changed"] = True

    data["_transformNote"] = (
        "nexus-core had 30+ dependency edges making it look like the central hub, "
        "but it is a shared library (not an app in /apps). Edges rerouted so Nexus-Cloud "
        "is the visual center — it's the platform where all 80+ apps live."
    )
    data["_transformStats"] = changes

    return data


def main():
    print(f"Reading {DATA_PATH}...")
    with open(DATA_PATH) as f:
        data = json.load(f)

    # Backup
    print(f"Backing up to {BACKUP_PATH}...")
    with open(BACKUP_PATH, "w") as f:
        json.dump(data, f, indent=2)

    # Transform
    data = transform(data)

    # Write
    print(f"Writing {DATA_PATH}...")
    with open(DATA_PATH, "w") as f:
        json.dump(data, f, indent=2)

    stats = data["_transformStats"]
    print(f"\nDone. Changes:")
    print(f"  Systems that got Nexus-Cloud added to integrates: {stats['nexus_core_deps_added_cloud']}")
    print(f"  Nexus-Cloud integrates updated to ALL_SYSTEMS: {stats['nexus_cloud_integrates_updated']}")
    print(f"  nexus-core integrates cleared (it's a library): {stats['nexus_core_integrates_changed']}")
    print(f"\nBackup saved to {BACKUP_PATH}")
    print("Run 'python3 build.py' to rebuild the visualizer.")


if __name__ == "__main__":
    main()
