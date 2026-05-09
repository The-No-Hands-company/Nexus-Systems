# Nexus Files Docs

## MVP Architecture Note

Nexus Files MVP starts as a small file write and list service.

- Write boundary: accepts minimal file metadata and content reference payloads.
- List boundary: exposes basic file listing for local development.
- Health boundary: exposes liveness/readiness for local orchestration.

## Contract-First Gating

This app follows item 4 gating tracked in:

- docs/NEXT_BATCH_ITEM4_NEXUS_FILES_GATING.md

Initial v1 endpoint contract draft:

- Nexus-Files/docs/MVP_CONTRACT_V1_DRAFT.md

Implemented runtime shell:

- Nexus-Files/src/server.js
- Nexus-Files/src/index.js
- Nexus-Files/tests/files.smoke.test.js

Validation command:

- npm test
