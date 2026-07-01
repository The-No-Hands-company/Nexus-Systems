#!/usr/bin/env python3
"""Extract a lightweight ecosystem graph from markdown metadata blocks.

Supported metadata keys per block:
- system
- owns
- depends_on
- exposes
- integrates_with
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
from concurrent.futures import ThreadPoolExecutor, as_completed
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Iterable

__version__ = "2.0.0"

SCHEMA_VERSION = "1.0.0"
META_KEYS = ["system", "owns", "depends_on", "exposes", "integrates_with"]
NODE_TYPES = {"system", "capability", "surface", "document"}
REL_TYPES = {"depends_on", "integrates_with", "owns", "exposes", "documented_by"}

META_PATTERN = re.compile(r"^(system|owns|depends_on|exposes|integrates_with):\s*(.*)$", re.IGNORECASE)
MISSING_COLON_PATTERN = re.compile(r"^(system|owns|depends_on|exposes|integrates_with)\b(?!\s*:)", re.IGNORECASE)
GENERIC_KEY_VALUE_PATTERN = re.compile(r"^([a-z_]+):\s*(.*)$", re.IGNORECASE)

# Skip large/generated/vendor folders during scan.
SKIP_DIRS = {
    ".git",
    ".venv",
    "node_modules",
    "target",
    "dist",
    "build",
    "out",
    ".next",
    ".nuxt",
    "coverage",
}


@dataclass
class MetadataBlock:
    source_path: str
    system: str
    owns: list[str]
    depends_on: list[str]
    exposes: list[str]
    integrates_with: list[str]


@dataclass
class Diagnostic:
    level: str
    source_path: str
    line: int | None
    message: str


def slugify(value: str) -> str:
    s = value.strip().lower()
    s = re.sub(r"[^a-z0-9]+", "-", s)
    return s.strip("-") or "item"


def split_csv(raw: str) -> list[str]:
    if not raw.strip():
        return []
    items = [part.strip() for part in raw.split(",") if part.strip()]
    # Keep insertion order while removing duplicates for stable output.
    return list(dict.fromkeys(items))


def should_skip(path: Path) -> bool:
    return any(part in SKIP_DIRS for part in path.parts)


def discover_markdown_files(root: Path) -> Iterable[Path]:
    for path in sorted(root.rglob("*.md")):
        rel = path.relative_to(root)
        if should_skip(rel):
            continue
        yield path


def parse_metadata_blocks(path: Path, root: Path) -> tuple[list[MetadataBlock], list[Diagnostic]]:
    lines = path.read_text(encoding="utf-8", errors="ignore").splitlines()
    blocks: list[MetadataBlock] = []
    diagnostics: list[Diagnostic] = []
    current: dict[str, str] = {}
    current_line: int | None = None

    def warn(message: str, line: int | None) -> None:
        diagnostics.append(
            Diagnostic(
                level="warning",
                source_path=str(path.relative_to(root)).replace("\\", "/"),
                line=line,
                message=message,
            )
        )

    def flush(end_line: int | None = None) -> None:
        nonlocal current, current_line
        if not current.get("system", "").strip():
            if current:
                warn("metadata block ignored because required 'system' field is missing", current_line or end_line)
            current = {}
            current_line = None
            return

        block = MetadataBlock(
            source_path=str(path.relative_to(root)).replace("\\", "/"),
            system=current.get("system", "").strip(),
            owns=split_csv(current.get("owns", "")),
            depends_on=split_csv(current.get("depends_on", "")),
            exposes=split_csv(current.get("exposes", "")),
            integrates_with=split_csv(current.get("integrates_with", "")),
        )
        blocks.append(block)
        current = {}
        current_line = None

    for line_no, line in enumerate(lines, start=1):
        match = META_PATTERN.match(line.strip())
        if match:
            key = match.group(1).lower()
            value = match.group(2).strip()
            # New system key starts a new block if one is already in progress.
            if key == "system" and current.get("system"):
                flush(line_no)

            if not current:
                current_line = line_no

            if key in current:
                warn(f"duplicate key '{key}' found in metadata block; last value wins", line_no)

            if key != "system" and not value:
                warn(f"metadata key '{key}' has an empty value", line_no)

            current[key] = value
        else:
            stripped = line.strip()
            if MISSING_COLON_PATTERN.match(stripped):
                warn("metadata key appears malformed (expected 'key: value')", line_no)
            else:
                generic = GENERIC_KEY_VALUE_PATTERN.match(stripped)
                if generic and generic.group(1).lower() not in META_KEYS and current:
                    warn(f"unknown metadata key '{generic.group(1)}' ignored", line_no)

            # End block on blank line for readability.
            if not stripped and current:
                flush(line_no)

    if current:
        flush(len(lines))

    return blocks, diagnostics


def upsert_node(nodes: dict[str, dict], node_id: str, label: str, node_type: str, source: str | None = None) -> None:
    existing = nodes.get(node_id)
    if existing:
        if source and source not in existing["sources"]:
            existing["sources"].append(source)
        return

    nodes[node_id] = {
        "id": node_id,
        "label": label,
        "type": node_type,
        "sources": [source] if source else [],
    }


def add_edge(edges: set[tuple[str, str, str]], source: str, target: str, relation: str) -> None:
    if source == target:
        return
    edges.add((source, target, relation))


def build_graph(blocks: list[MetadataBlock]) -> dict:
    nodes: dict[str, dict] = {}
    edges: set[tuple[str, str, str]] = set()

    for block in blocks:
        system_id = f"system:{slugify(block.system)}"
        doc_id = f"doc:{slugify(block.source_path)}"

        upsert_node(nodes, system_id, block.system, "system", block.source_path)
        upsert_node(nodes, doc_id, block.source_path, "document", block.source_path)
        add_edge(edges, system_id, doc_id, "documented_by")

        for item in block.owns:
            item_id = f"capability:{slugify(item)}"
            upsert_node(nodes, item_id, item, "capability", block.source_path)
            add_edge(edges, system_id, item_id, "owns")

        for dep in block.depends_on:
            dep_id = f"system:{slugify(dep)}"
            upsert_node(nodes, dep_id, dep, "system", block.source_path)
            add_edge(edges, system_id, dep_id, "depends_on")

        for exposed in block.exposes:
            endpoint_id = f"surface:{slugify(exposed)}"
            upsert_node(nodes, endpoint_id, exposed, "surface", block.source_path)
            add_edge(edges, system_id, endpoint_id, "exposes")

        for integration in block.integrates_with:
            integration_id = f"system:{slugify(integration)}"
            upsert_node(nodes, integration_id, integration, "system", block.source_path)
            add_edge(edges, system_id, integration_id, "integrates_with")

    edge_rows = [
        {
            "id": f"edge:{i}",
            "source": source,
            "target": target,
            "relation": relation,
        }
        for i, (source, target, relation) in enumerate(sorted(edges), start=1)
    ]

    return {
        "schemaVersion": SCHEMA_VERSION,
        "generatedAt": datetime.now(timezone.utc).isoformat(),
        "nodeCount": len(nodes),
        "edgeCount": len(edge_rows),
        "nodes": sorted(nodes.values(), key=lambda n: n["id"]),
        "edges": edge_rows,
    }


def validate_graph(graph: dict) -> list[str]:
    errors: list[str] = []
    required_top = ["schemaVersion", "generatedAt", "nodeCount", "edgeCount", "nodes", "edges"]

    for key in required_top:
        if key not in graph:
            errors.append(f"missing required top-level key: {key}")

    if errors:
        return errors

    if graph["schemaVersion"] != SCHEMA_VERSION:
        errors.append(f"schemaVersion must be '{SCHEMA_VERSION}'")

    if not isinstance(graph["generatedAt"], str):
        errors.append("generatedAt must be a string")

    if not isinstance(graph["nodeCount"], int) or graph["nodeCount"] < 0:
        errors.append("nodeCount must be a non-negative integer")

    if not isinstance(graph["edgeCount"], int) or graph["edgeCount"] < 0:
        errors.append("edgeCount must be a non-negative integer")

    nodes = graph.get("nodes")
    edges = graph.get("edges")

    if not isinstance(nodes, list):
        errors.append("nodes must be an array")
        nodes = []
    if not isinstance(edges, list):
        errors.append("edges must be an array")
        edges = []

    if graph["nodeCount"] != len(nodes):
        errors.append("nodeCount must match length of nodes array")

    if graph["edgeCount"] != len(edges):
        errors.append("edgeCount must match length of edges array")

    node_ids: set[str] = set()
    previous_node_id = ""
    for idx, node in enumerate(nodes):
        if not isinstance(node, dict):
            errors.append(f"nodes[{idx}] must be an object")
            continue

        for key in ("id", "label", "type", "sources"):
            if key not in node:
                errors.append(f"nodes[{idx}] missing key '{key}'")

        node_id = node.get("id")
        node_type = node.get("type")
        sources = node.get("sources")

        if isinstance(node_id, str):
            if node_id in node_ids:
                errors.append(f"duplicate node id '{node_id}'")
            node_ids.add(node_id)
            if node_id < previous_node_id:
                errors.append("nodes must be sorted by id for deterministic output")
            previous_node_id = node_id
        else:
            errors.append(f"nodes[{idx}].id must be a string")

        if not isinstance(node.get("label"), str) or not node.get("label", "").strip():
            errors.append(f"nodes[{idx}].label must be a non-empty string")

        if node_type not in NODE_TYPES:
            errors.append(f"nodes[{idx}].type must be one of {sorted(NODE_TYPES)}")

        if not isinstance(sources, list):
            errors.append(f"nodes[{idx}].sources must be an array")
        else:
            seen_sources: set[str] = set()
            for src in sources:
                if not isinstance(src, str) or not src:
                    errors.append(f"nodes[{idx}].sources entries must be non-empty strings")
                    continue
                if src.startswith("/"):
                    errors.append(f"nodes[{idx}] source '{src}' must be a relative path")
                if src in seen_sources:
                    errors.append(f"nodes[{idx}] has duplicate source '{src}'")
                seen_sources.add(src)

    edge_ids: set[str] = set()
    previous_edge_id = ""
    for idx, edge in enumerate(edges):
        if not isinstance(edge, dict):
            errors.append(f"edges[{idx}] must be an object")
            continue

        for key in ("id", "source", "target", "relation"):
            if key not in edge:
                errors.append(f"edges[{idx}] missing key '{key}'")

        edge_id = edge.get("id")
        source = edge.get("source")
        target = edge.get("target")
        relation = edge.get("relation")

        if isinstance(edge_id, str):
            if edge_id in edge_ids:
                errors.append(f"duplicate edge id '{edge_id}'")
            edge_ids.add(edge_id)
            if edge_id < previous_edge_id:
                errors.append("edges must be sorted by id for deterministic output")
            previous_edge_id = edge_id
        else:
            errors.append(f"edges[{idx}].id must be a string")

        if relation not in REL_TYPES:
            errors.append(f"edges[{idx}].relation must be one of {sorted(REL_TYPES)}")

        if source not in node_ids:
            errors.append(f"edges[{idx}].source '{source}' does not reference an existing node")

        if target not in node_ids:
            errors.append(f"edges[{idx}].target '{target}' does not reference an existing node")

        if source == target and isinstance(source, str):
            errors.append(f"edges[{idx}] cannot be a self-loop ('{source}')")

    return errors


def format_diagnostic(diag: Diagnostic) -> str:
    line_suffix = f":{diag.line}" if diag.line else ""
    return f"[{diag.level}] {diag.source_path}{line_suffix} {diag.message}"


def main() -> int:
    parser = argparse.ArgumentParser(description="Extract ecosystem graph JSON from markdown metadata.")
    parser.add_argument("--version", action="version", version=f"%(prog)s {__version__}")
    parser.add_argument("--root", default=".", help="Workspace root to scan")
    parser.add_argument("--out", default="ecosystem-graph/graph.json", help="Output JSON path")
    parser.add_argument(
        "--incremental",
        action="store_true",
        help="Skip scanning if markdown files haven't changed (uses hash cache)",
    )
    parser.add_argument(
        "--parallel",
        type=int,
        default=0,
        help="Number of parallel workers for scanning (default: sequential)",
    )
    parser.add_argument(
        "--fail-on-warning",
        action="store_true",
        help="Exit non-zero if metadata warnings are detected",
    )
    args = parser.parse_args()

    root = Path(args.root).resolve()
    out = Path(args.out).resolve()

    md_files = list(discover_markdown_files(root))

    # Incremental mode: skip if nothing changed
    if args.incremental:
        cache_path = out.parent / ".graph_hash_cache.json"
        chk = hashlib.sha256("".join(f"{f.stat().st_mtime}" for f in md_files).encode()).hexdigest()
        if cache_path.exists():
            try:
                cached = json.loads(cache_path.read_text())
                if cached.get("hash") == chk and out.exists():
                    print("No changes detected — skipping graph regeneration.")
                    return 0
            except (json.JSONDecodeError, OSError):
                pass

    all_blocks: list[MetadataBlock] = []
    all_diagnostics: list[Diagnostic] = []

    if args.parallel > 1 and len(md_files) > 1:
        with ThreadPoolExecutor(max_workers=args.parallel) as executor:
            futures = {executor.submit(parse_metadata_blocks, f, root): f for f in md_files}
            for future in as_completed(futures):
                blocks, diagnostics = future.result()
                all_blocks.extend(blocks)
                all_diagnostics.extend(diagnostics)
    else:
        for md_file in md_files:
            blocks, diagnostics = parse_metadata_blocks(md_file, root)
            all_blocks.extend(blocks)
            all_diagnostics.extend(diagnostics)

    # Write hash cache for incremental runs
    if args.incremental:
        cache_path.parent.mkdir(parents=True, exist_ok=True)
        cache_path.write_text(json.dumps({"hash": chk, "files": len(md_files)}))

    all_blocks.sort(key=lambda b: (b.source_path, b.system.lower()))

    graph = build_graph(all_blocks)
    validation_errors = validate_graph(graph)

    if validation_errors:
        for err in validation_errors:
            print(f"[error] {err}", file=sys.stderr)
        return 1

    warning_count = 0
    for diag in all_diagnostics:
        if diag.level == "warning":
            warning_count += 1
            print(format_diagnostic(diag), file=sys.stderr)

    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(graph, indent=2), encoding="utf-8")

    print(f"Wrote {graph['nodeCount']} nodes and {graph['edgeCount']} edges to {out}")
    if warning_count:
        print(f"Completed with {warning_count} warning(s)", file=sys.stderr)
        if args.fail_on_warning:
            return 3

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
