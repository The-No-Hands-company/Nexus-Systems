import json
import tempfile
import unittest
from pathlib import Path

import extract_graph as eg


class ExtractGraphUnitTests(unittest.TestCase):
    def test_parse_valid_metadata_block(self):
        with tempfile.TemporaryDirectory() as tmp_dir:
            root = Path(tmp_dir)
            doc = root / "sample.md"
            doc.write_text(
                "\n".join(
                    [
                        "system: Nexus-Cloud",
                        "owns: state cache, route discovery",
                        "depends_on: Nexus, Phantom",
                        "exposes: /api/cloud/discovery",
                        "integrates_with: Nexus-Hosting",
                        "",
                    ]
                ),
                encoding="utf-8",
            )

            blocks, diagnostics = eg.parse_metadata_blocks(doc, root)
            self.assertEqual(len(blocks), 1)
            self.assertEqual(len(diagnostics), 0)

            block = blocks[0]
            self.assertEqual(block.system, "Nexus-Cloud")
            self.assertEqual(block.owns, ["state cache", "route discovery"])
            self.assertEqual(block.depends_on, ["Nexus", "Phantom"])

    def test_parse_warns_on_missing_system(self):
        with tempfile.TemporaryDirectory() as tmp_dir:
            root = Path(tmp_dir)
            doc = root / "invalid.md"
            doc.write_text(
                "\n".join(
                    [
                        "owns: orphan value",
                        "depends_on Nexus-Cloud",
                        "",
                    ]
                ),
                encoding="utf-8",
            )

            blocks, diagnostics = eg.parse_metadata_blocks(doc, root)
            self.assertEqual(len(blocks), 0)
            self.assertGreaterEqual(len(diagnostics), 1)
            messages = [item.message for item in diagnostics]
            self.assertTrue(any("missing" in message for message in messages))
            self.assertTrue(any("malformed" in message for message in messages))

    def test_build_graph_has_deterministic_node_and_edge_order(self):
        blocks = [
            eg.MetadataBlock(
                source_path="b.md",
                system="Nexus-B",
                owns=["Capability B"],
                depends_on=["Nexus-A"],
                exposes=["/b"],
                integrates_with=[],
            ),
            eg.MetadataBlock(
                source_path="a.md",
                system="Nexus-A",
                owns=["Capability A"],
                depends_on=[],
                exposes=["/a"],
                integrates_with=["Nexus-B"],
            ),
        ]

        graph = eg.build_graph(blocks)
        self.assertEqual(graph["schemaVersion"], "1.0.0")

        node_ids = [n["id"] for n in graph["nodes"]]
        edge_ids = [e["id"] for e in graph["edges"]]

        self.assertEqual(node_ids, sorted(node_ids))
        self.assertEqual(edge_ids, sorted(edge_ids))

    def test_validate_graph_detects_invalid_relation(self):
        graph = {
            "schemaVersion": "1.0.0",
            "generatedAt": "2026-04-18T00:00:00+00:00",
            "nodeCount": 1,
            "edgeCount": 1,
            "nodes": [
                {
                    "id": "system:nexus",
                    "label": "Nexus",
                    "type": "system",
                    "sources": ["README.md"],
                }
            ],
            "edges": [
                {
                    "id": "edge:1",
                    "source": "system:nexus",
                    "target": "system:nexus",
                    "relation": "invalid_relation",
                }
            ],
        }

        errors = eg.validate_graph(graph)
        self.assertTrue(any("relation" in err for err in errors))
        self.assertTrue(any("self-loop" in err for err in errors))


if __name__ == "__main__":
    unittest.main()
