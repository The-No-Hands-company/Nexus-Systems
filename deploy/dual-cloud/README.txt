Nexus Systems — Dual-Cloud Federation Bootstrap
================================================

This directory contains everything needed to run two Nexus-Cloud control plane
instances that gossip with each other, demonstrating the federation layer.

Files
-----

  dual-cloud-compose.yml   — Docker Compose file for cloud-A (port 8787) and cloud-B (port 8788)
  federation-test.sh       — Automated test script that validates federation
  README.txt               — This file

Quick Start
-----------

  1. Build and start both clouds:

       docker compose -f dual-cloud-compose.yml up -d --build

  2. Wait for health checks (both clouds take ~10-15s to start):

       docker compose -f dual-cloud-compose.yml ps

  3. Run the federation test suite:

       bash federation-test.sh

  4. When finished, tear down:

       docker compose -f dual-cloud-compose.yml down -v

What It Does
------------

  cloud-A (port 8787, DID: did:nexus:cloud-a-001)
      │
      │  BOOTSTRAP_PEERS=http://cloud-b:8788
      │  ──────────────────────────────────────
      │  On startup, cloud-A contacts cloud-B and exchanges
      │  peer lists. Bootstrap peers are auto-trusted ("verified").
      │
      ▼
  cloud-B (port 8788, DID: did:nexus:cloud-b-002)

  After gossip, each cloud knows about the other. Tools registered on one
  cloud can be federated to the other, enabling cross-cloud visibility.

Key Endpoints (try from host)
-----------------------------

  # Identity
  curl http://localhost:8787/v1/federation/identity | jq
  curl http://localhost:8788/v1/federation/identity | jq

  # Peer list (should show the other cloud after gossip)
  curl -H "X-Api-Key: federation-dev-key" http://localhost:8787/v1/federation/peers | jq
  curl -H "X-Api-Key: federation-dev-key" http://localhost:8788/v1/federation/peers | jq

  # Discovery document
  curl http://localhost:8787/.well-known/nexus-cloud | jq
  curl http://localhost:8788/.well-known/nexus-cloud | jq

  # Register a tool
  curl -X POST http://localhost:8787/api/v1/tools \
    -H "X-Api-Key: federation-dev-key" \
    -H "Content-Type: application/json" \
    -d '{"id":"my-tool","name":"My Tool","mode":"standalone","capabilities":["demo"]}'

  # List tools (across both clouds)
  curl -H "X-Api-Key: federation-dev-key" http://localhost:8787/api/v1/tools | jq
  curl -H "X-Api-Key: federation-dev-key" http://localhost:8788/api/v1/tools | jq

Environment Variables
---------------------

  Each cloud is configured with:

  PORT                             — Listening port (8787 / 8788)
  NEXUS_CLOUD_URL                  — Publicly reachable URL (used in announcements)
  NEXUS_CLOUD_DOMAIN               — Cloud domain (cloud-a.nexus.local / cloud-b.nexus.local)
  NEXUS_CLOUD_NODE_DID             — Explicit DID identity
  BOOTSTRAP_PEERS                  — Comma-separated URLs of known peers
  NEXUS_CLOUD_FEDERATION_MIN_TRUST — Minimum trust level (verified = permissive)

Security Note
-------------

  This setup uses NEXUS_CLOUD_FEDERATION_MIN_TRUST=verified for easy bootstrapping.
  In production, set this to "trusted" and promote peers explicitly after identity
  verification. Bootstrap auto-trust only applies to peers explicitly listed in
  BOOTSTRAP_PEERS.

Troubleshooting
---------------

  - If peers don't appear: check that both containers are healthy and on the
    same Docker network (dual-cloud-net). The gossip runs 2 seconds after
    server start.

  - If tool registration fails: ensure X-Api-Key matches federation-dev-key.

  - View logs:
      docker compose -f dual-cloud-compose.yml logs cloud-a
      docker compose -f dual-cloud-compose.yml logs cloud-b

  - Rebuild after source changes:
      docker compose -f dual-cloud-compose.yml up -d --build
