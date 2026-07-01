"""Tests for ecosystem-graph / extract_graph.py"""

import json
from pathlib import Path

import pytest

import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from extract_graph import (
    MetadataBlock,
    build_graph,
    parse_metadata_blocks,
    slugify,
    split_csv,
    validate_graph,
)


def test_slugify():
    assert slugify("Hello World") == "hello-world"
    assert slugify("Nexus-Cloud v2.0!") == "nexus-cloud-v2-0"
    assert slugify("  ") == "item"


def test_split_csv():
    assert split_csv("a,b,c") == ["a", "b", "c"]
    assert split_csv("  hello , world  ") == ["hello", "world"]
    assert split_csv("x,y,x") == ["x", "y"]  # deduped
    assert split_csv("") == []


def test_parse_metadata_blocks_basic(tmp_path):
    md = tmp_path / "README.md"
    md.write_text("""\
# My System

system: my-system
owns: auth, billing
depends_on: database
exposes: rest-api
integrates_with: payment-gateway
""")
    blocks, diags = parse_metadata_blocks(md, tmp_path)
    assert len(blocks) == 1
    b = blocks[0]
    assert b.system == "my-system"
    assert b.owns == ["auth", "billing"]
    assert b.depends_on == ["database"]
    assert b.exposes == ["rest-api"]
    assert b.integrates_with == ["payment-gateway"]


def test_parse_metadata_blocks_multiple(tmp_path):
    md = tmp_path / "README.md"
    md.write_text("""\
system: frontend
owns: ui-kit

system: backend
depends_on: database
owns: api
""")
    blocks, _ = parse_metadata_blocks(md, tmp_path)
    assert len(blocks) == 2
    assert blocks[0].system == "frontend"
    assert blocks[1].system == "backend"


def test_parse_metadata_blocks_no_system(tmp_path):
    md = tmp_path / "README.md"
    md.write_text("owns: something\n")
    blocks, diags = parse_metadata_blocks(md, tmp_path)
    assert len(blocks) == 0
    assert len(diags) >= 1
    assert "missing" in diags[0].message.lower()


def test_parse_metadata_blocks_duplicate_warning(tmp_path):
    md = tmp_path / "README.md"
    md.write_text("system: test\nowns: a\nowns: b\n")
    blocks, diags = parse_metadata_blocks(md, tmp_path)
    assert len(blocks) == 1
    dup_warnings = [d for d in diags if "duplicate" in d.message.lower()]
    assert len(dup_warnings) >= 1


def test_build_graph():
    blocks = [
        MetadataBlock(
            source_path="README.md",
            system="api",
            owns=["auth"],
            depends_on=["db"],
            exposes=["rest"],
            integrates_with=["payments"],
        )
    ]
    graph = build_graph(blocks)
    assert graph["schemaVersion"] == "1.0.0"
    assert graph["nodeCount"] > 0
    assert graph["edgeCount"] > 0

    node_ids = {n["id"] for n in graph["nodes"]}
    assert "system:api" in node_ids
    assert "system:db" in node_ids
    assert "capability:auth" in node_ids


def test_validate_graph_valid():
    blocks = [MetadataBlock("README.md", "api", ["auth"], ["db"], ["rest"], [])]
    graph = build_graph(blocks)
    errors = validate_graph(graph)
    assert len(errors) == 0


def test_validate_graph_missing_key():
    graph = {"schemaVersion": "1.0.0"}  # missing many keys
    errors = validate_graph(graph)
    assert len(errors) > 0
    assert any("missing" in e.lower() for e in errors)


def test_build_graph_self_loop_skipped():
    blocks = [
        MetadataBlock(source_path="x.md", system="self-ref", owns=[], depends_on=["self-ref"], exposes=[], integrates_with=[])
    ]
    graph = build_graph(blocks)
    edges = graph["edges"]
    # self-loops should not exist
    for e in edges:
        assert e["source"] != e["target"], f"Self-loop found: {e}"
