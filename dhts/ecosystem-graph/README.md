Nexus Systems Ecosystem Graph (MVP)

Purpose
- Keep docs as source of truth
- Derive a machine-readable relationship graph from metadata blocks
- Visualize interconnections in a local web graph view with document click-through

Metadata pattern (place in markdown docs)
- system: <system-name>
- owns: <item-1>, <item-2>
- depends_on: <item-1>, <item-2>
- exposes: <item-1>, <item-2>
- integrates_with: <item-1>, <item-2>

Graph contract (strict v1)
- Schema file: graph.schema.v1.json
- Output graph.json now includes schemaVersion = 1.0.0
- Required top-level keys: schemaVersion, generatedAt, nodeCount, edgeCount, nodes, edges
- Allowed node types: system, capability, surface, document
- Allowed edge relations: depends_on, integrates_with, owns, exposes, documented_by
- Extractor validates graph output before writing and exits non-zero if contract validation fails

How to generate graph.json
1. From workspace root:
   python3 ecosystem-graph/extract_graph.py --root . --out ecosystem-graph/graph.json

Extractor validation and diagnostics
- Warnings are emitted for malformed metadata lines (for example missing ":"), unknown keys inside a metadata block, duplicate keys in a block, and missing required system field.
- Validation errors fail the run with exit code 1.
- To treat warnings as failures, add --fail-on-warning (exit code 3 when warnings are found).

How to view graph locally
1. From workspace root:
   python3 -m http.server 8922
2. Open in browser:
   http://localhost:8922/ecosystem-graph/index.html

Tests
1. From workspace root:
   python3 -m unittest discover -s ecosystem-graph/tests -p "test_*.py" -v
2. Coverage:
   - Unit tests for metadata parsing, graph build stability, and contract validation
   - End-to-end smoke test that builds graph artifacts and serves/loads index.html + graph.json + app.js

One-command developer workflow
1. From ecosystem-graph directory:
   make dev
2. What it does:
   - generate: builds graph.json
   - verify: runs unit + smoke tests
   - serve: starts local server on port 8922

Notes
- The extractor scans all markdown files under the workspace root.
- Generated/vendor folders are skipped (.git, .venv, node_modules, target, dist, etc).
- Document nodes link back to source markdown paths.

Future Nexus-Cloud hosting path
- Keep this extractor output as canonical graph snapshot contract.
- Add Nexus-Cloud endpoint to serve graph snapshot and subgraph queries.
- Keep UI as a reusable frontend panel that can run standalone or embedded.
