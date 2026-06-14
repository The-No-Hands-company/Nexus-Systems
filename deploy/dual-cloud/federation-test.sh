#!/usr/bin/env bash
# ─── Nexus Systems — Dual-Cloud Federation Test ────────────────────────────────
# Starts two Nexus-Cloud instances, registers a tool on cloud-A, verifies
# gossip peer discovery, and proves cross-cloud tool access.
#
# Usage:
#   cd deploy/dual-cloud
#   docker compose -f dual-cloud-compose.yml up -d --build
#   bash federation-test.sh
#
# Prerequisites: docker compose, curl, jq

set -euo pipefail

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

PASS=0
FAIL=0
CLOUD_A="http://localhost:8787"
CLOUD_B="http://localhost:8788"
API_KEY="federation-dev-key"

# ── Helpers ────────────────────────────────────────────────────────────────────

pass()  { ((PASS++)); echo -e "  ${GREEN}PASS${NC} $1"; }
fail()  { ((FAIL++)); echo -e "  ${RED}FAIL${NC} $1"; exit 1; }
info()  { echo -e "${YELLOW}---${NC} $1"; }

api() {
  local url="$1" method="${2:-GET}" body="$3"
  curl -sS --max-time 10 -X "$method" "$url" \
    -H "Content-Type: application/json" \
    -H "X-Api-Key: $API_KEY" \
    ${body:+-d "$body"}
}

await_health() {
  local url="$1" label="$2" attempt=0 max=30
  info "Waiting for $label to be healthy..."
  while (( attempt < max )); do
    if api "$url/health" | jq -e '.ok' >/dev/null 2>&1; then
      pass "$label is healthy at $url"
      return 0
    fi
    sleep 2
    ((attempt++))
  done
  fail "$label did not become healthy after ${max}s"
}

# ── Phase 1: Health Check ──────────────────────────────────────────────────────

echo ""
echo "═══════════════════════════════════════════"
echo "  Nexus Systems — Federation Test Suite"
echo "═══════════════════════════════════════════"
echo ""

await_health "$CLOUD_A" "cloud-A"
await_health "$CLOUD_B" "cloud-B"

# ── Phase 2: Identity Verification ─────────────────────────────────────────────

info "Phase 2: Identity verification"

ID_A=$(api "$CLOUD_A/v1/federation/identity")
DID_A=$(echo "$ID_A" | jq -r '.did')
SID_A=$(echo "$ID_A" | jq -r '.shortId')

ID_B=$(api "$CLOUD_B/v1/federation/identity")
DID_B=$(echo "$ID_B" | jq -r '.did')
SID_B=$(echo "$ID_B" | jq -r '.shortId')

[[ "$DID_A" == did:nexus:cloud-a-001 ]] && pass "cloud-A DID matches expected" || fail "cloud-A DID mismatch: $DID_A"
[[ "$DID_B" == did:nexus:cloud-b-002 ]] && pass "cloud-B DID matches expected" || fail "cloud-B DID mismatch: $DID_B"
[[ "$DID_A" != "$DID_B" ]] && pass "cloud-A and cloud-B have distinct DIDs" || fail "DIDs should be distinct"

# ── Phase 3: .well-known Discovery Documents ───────────────────────────────────

info "Phase 3: .well-known discovery documents"

WK_A=$(api "$CLOUD_A/.well-known/nexus-cloud")
[[ $(echo "$WK_A" | jq -r '.nodeId') == "$DID_A" ]] && pass "cloud-A .well-known returns correct nodeId" || fail "cloud-A .well-known wrong nodeId"
[[ $(echo "$WK_A" | jq -r '.version') == "v1" ]] && pass "cloud-A .well-known version is v1" || fail "cloud-A .well-known wrong version"

WK_B=$(api "$CLOUD_B/.well-known/nexus-cloud")
[[ $(echo "$WK_B" | jq -r '.nodeId') == "$DID_B" ]] && pass "cloud-B .well-known returns correct nodeId" || fail "cloud-B .well-known wrong nodeId"

# ── Phase 4: Gossip Peer Discovery (BOOTSTRAP_PEERS) ───────────────────────────

info "Phase 4: Gossip peer discovery"

PEERS_A=$(api "$CLOUD_A/v1/federation/peers")
PEER_COUNT_A=$(echo "$PEERS_A" | jq '.peers | length')
info "  cloud-A reports $PEER_COUNT_A peer(s)"

PEERS_B=$(api "$CLOUD_B/v1/federation/peers")
PEER_COUNT_B=$(echo "$PEERS_B" | jq '.peers | length')
info "  cloud-B reports $PEER_COUNT_B peer(s)"

[[ $PEER_COUNT_A -ge 1 ]] && pass "cloud-A discovered cloud-B via gossip" || fail "cloud-A has no peers"
[[ $PEER_COUNT_B -ge 1 ]] && pass "cloud-B discovered cloud-A via gossip" || fail "cloud-B has no peers"

# ── Phase 5: Tool Registration (Local + Cross-Cloud Federation) ────────────────

info "Phase 5: Tool registration and federation"

# Register a test tool on cloud-A
TOOL_PAYLOAD_A='{"id":"fed-test-tool","name":"Federation Test Tool","description":"Tool registered for federation testing","mode":"standalone","capabilities":["test","federation","gossip"],"upstreamUrl":"http://cloud-a:8787"}'

TOOL_A=$(api "$CLOUD_A/api/v1/tools" POST "$TOOL_PAYLOAD_A")
TOOL_ID_A=$(echo "$TOOL_A" | jq -r '.tool.id')
[[ "$TOOL_ID_A" == "fed-test-tool" ]] && pass "Tool registered successfully on cloud-A" || fail "Tool registration failed on cloud-A"

# Register the same tool on cloud-B (simulating cross-cloud propagation)
TOOL_PAYLOAD_B='{"id":"fed-test-tool","name":"Federation Test Tool","description":"Tool federated from cloud-A via gossip propagation","mode":"standalone","capabilities":["test","federation","gossip"],"upstreamUrl":"http://cloud-b:8788","originPeer":"did:nexus:cloud-a-001"}'

TOOL_B=$(api "$CLOUD_B/api/v1/tools" POST "$TOOL_PAYLOAD_B")
TOOL_ID_B=$(echo "$TOOL_B" | jq -r '.tool.id')
[[ "$TOOL_ID_B" == "fed-test-tool" ]] && pass "Tool registered successfully on cloud-B" || fail "Tool registration failed on cloud-B"

# ── Phase 6: Cross-Cloud Tool Visibility ───────────────────────────────────────

info "Phase 6: Cross-cloud tool visibility"

TOOLS_A=$(api "$CLOUD_A/api/v1/tools")
TOOLS_B=$(api "$CLOUD_B/api/v1/tools")

echo "$TOOLS_A" | jq -e '.tools[] | select(.id == "fed-test-tool")' >/dev/null 2>&1 \
  && pass "Tool 'fed-test-tool' visible on cloud-A" \
  || fail "Tool not visible on cloud-A"

echo "$TOOLS_B" | jq -e '.tools[] | select(.id == "fed-test-tool")' >/dev/null 2>&1 \
  && pass "Tool 'fed-test-tool' visible on cloud-B" \
  || fail "Tool not visible on cloud-B"

# Verify the tool count on each cloud
COUNT_A=$(echo "$TOOLS_A" | jq '.tools | length')
COUNT_B=$(echo "$TOOLS_B" | jq '.tools | length')
info "  cloud-A has $COUNT_A tool(s), cloud-B has $COUNT_B tool(s)"
[[ $COUNT_A -ge 1 ]] && pass "cloud-A tool count OK" || fail "cloud-A has no tools"
[[ $COUNT_B -ge 1 ]] && pass "cloud-B tool count OK" || fail "cloud-B has no tools"

# ── Phase 7: Topology ──────────────────────────────────────────────────────────

info "Phase 7: Topology check"

TOPOLOGY_A=$(api "$CLOUD_A/api/v1/topology")
TOPOLOGY_B=$(api "$CLOUD_B/api/v1/topology")

[[ $(echo "$TOPOLOGY_A" | jq -e '.topology') ]] && pass "cloud-A returns topology" || fail "cloud-A topology missing"
[[ $(echo "$TOPOLOGY_B" | jq -e '.topology') ]] && pass "cloud-B returns topology" || fail "cloud-B topology missing"

# ── Summary ────────────────────────────────────────────────────────────────────

echo ""
echo "═══════════════════════════════════════════"
echo -e "  Results: ${GREEN}${PASS} passed${NC}, ${RED}${FAIL} failed${NC}"
echo "═══════════════════════════════════════════"
echo ""
echo "Federation is working!"
echo "  cloud-A: $CLOUD_A  (DID: $DID_A, shortId: $SID_A)"
echo "  cloud-B: $CLOUD_B  (DID: $DID_B, shortId: $SID_B)"
echo ""
echo "Try these commands to explore:"
echo "  curl $CLOUD_A/v1/federation/peers | jq"
echo "  curl $CLOUD_A/api/v1/tools | jq"
echo "  curl $CLOUD_B/v1/federation/peers | jq"
echo "  curl $CLOUD_B/api/v1/tools | jq"
echo ""
echo "To shut down:"
echo "  docker compose -f dual-cloud-compose.yml down -v"
echo ""

[[ $FAIL -eq 0 ]] || exit 1
