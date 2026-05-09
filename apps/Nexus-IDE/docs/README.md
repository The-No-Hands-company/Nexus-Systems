# Nexus IDE Docs

## MVP Architecture Note

Nexus IDE MVP starts as a small session and command bridge service.

- Session boundary: manages minimal IDE session metadata.
- Command boundary: accepts minimal command bridge requests and returns execution intent metadata.
- Health boundary: exposes liveness/readiness for local orchestration.

## Contract-First Gating

This app follows item 6 gating tracked in:

- docs/NEXT_BATCH_ITEM6_NEXUS_IDE_GATING.md

Initial v1 endpoint contract draft:

- Nexus-IDE/docs/MVP_CONTRACT_V1_DRAFT.md

Implemented runtime shell:

- Nexus-IDE/src/server.js
- Nexus-IDE/src/index.js
- Nexus-IDE/tests/ide.smoke.test.js

Validation command:

- npm test
