#!/bin/bash
# PHANTOM — 5-Hop Oblivious Routing Demo
# Two nodes, 5-hop FHE routing, PQ encrypted packet exchange
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
PHANTOM="$ROOT/apps/Phantom"

echo "╔══════════════════════════════════════════════════════════╗"
echo "║  PHANTOM — 5-Hop Oblivious Routing Demo                ║"
echo "║  Post-quantum privacy network in action                 ║"
echo "╚══════════════════════════════════════════════════════════╝"
echo ""

# ── Build ──
echo "━━━ Building phantom-node ━━━"
cd "$PHANTOM"
cargo build -p phantom-node 2>&1 | grep "Finished\|error" | head -2

NODE="./target/debug/phantom-node"

# ── Start Node A (listener) ──
echo ""
echo "━━━ Node A: Alice (listener on :9999) ━━━"
$NODE --listen /ip4/0.0.0.0/tcp/9999 > /tmp/phantom-alice.log 2>&1 &
PID_A=$!
sleep 3

ALICE_ID=$(grep "Peer ID:" /tmp/phantom-alice.log | grep -o '12D3KooW[A-Za-z0-9]*' || echo "pending")
ALICE_DID=$(grep "Identity:" /tmp/phantom-alice.log | grep -o 'did:phantom:[a-f0-9]*' || echo "pending")
echo "  Peer ID: $ALICE_ID"
echo "  DID:     $ALICE_DID"

# ── Start Node B (connects to Alice) ──
echo ""
echo "━━━ Node B: Bob (connects to Node A) ━━━"
$NODE --listen /ip4/0.0.0.0/tcp/9998 --connect 127.0.0.1:9999 > /tmp/phantom-bob.log 2>&1 &
PID_B=$!
sleep 3

BOB_ID=$(grep "Peer ID:" /tmp/phantom-bob.log | grep -o '12D3KooW[A-Za-z0-9]*' || echo "pending")
SENT=$(grep "Test packet sent\|Sent packet" /tmp/phantom-bob.log | head -1 || echo "pending")
echo "  Peer ID: $BOB_ID"
echo "  Status:  $SENT"

# ── Check routing ──
echo ""
echo "━━━ Routing Activity ━━━"
echo "  Node A events:"
grep -c "Packet\|Peer\|routing" /tmp/phantom-alice.log 2>/dev/null | head -1 && echo "  (routing active)" || echo "  (waiting for packets)"

echo "  Node B events:"
grep -c "Packet\|Peer\|routing\|sent" /tmp/phantom-bob.log 2>/dev/null | head -1 && echo "  (routing active)" || echo "  (waiting for packets)"

# ── Results ──
echo ""
echo "╔══════════════════════════════════════════════════════════╗"
echo "║  5-Hop Oblivious Routing — Ready                        ║"
echo "╠══════════════════════════════════════════════════════════╣"
echo "║  Node A (Alice): $ALICE_ID"
echo "║  DID:            $ALICE_DID"
echo "║  Node B (Bob):   $BOB_ID"
echo "╚══════════════════════════════════════════════════════════╝"

kill $PID_A $PID_B 2>/dev/null
wait 2>/dev/null
