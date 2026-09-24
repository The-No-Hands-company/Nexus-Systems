# Nexus Email — federated egress

**Date:** 2026-09-24
**Status:** approved design, addendum to `2026-08-15-nexus-email-design.md`

## Why this exists

This node cannot reach anyone's MX: outbound TCP 25 is filtered by the ISP,
and asking the ISP is not an option the operator will take. The parent spec
names the answer — **federated egress**: another Nexus node with unfiltered
egress accepts our outbound mail over the node-to-node channel and delivers
it. Our software on both ends, no third-party mail service anywhere.

State of the tree when this was written, measured rather than assumed:

- `Route::Federated` mail is **enqueued and never sent**: the delivery worker
  skips every row whose route is not `smtp` (`nexus-mailout/src/worker.rs`).
- `FederatedTransport` is a trait with **no implementation**, and no node
  accepts a handoff.
- **No binary runs the delivery worker** — only `examples/drain.rs` does — so
  even `smtp` rows are never drained in production.
- **Nothing DKIM-signs outbound mail**, although `nexus-mailauth` can sign.
- Nexus-Cloud's federation advertises `signedRequests: true`, but gossip signs
  and verifies nothing, and the node DID is a hash with no keypair behind it.
  The "authenticated node-to-node channel" the parent spec relies on does
  **not exist**, so this work builds one for mail.

## What it does not do

It does not make mail reach Gmail today. Federated egress needs a peer whose
own port 25 is open and whose IP has a clean reputation; there is none yet
(`peerCount: 0`). This builds the machinery so that the day such a peer is
pinned, outbound mail flows with a config change and nothing else. Receiving
mail from the outside world (inbound MX on 25) is also out of scope.

## Design

### Node mail keys, pinned like SSH

Each node has an **Ed25519 mail key**, generated on first start and stored in
the data directory (`NEXUS_EMAIL_NODE_KEY_PATH`). Its public half is served at
`GET /federation/v1/key`.

Peers are **pinned by the operator**, never discovered: a `mail_peers` row
holds the peer's domain, base URL, public key, and whether it may use us as
an egress relay. An unpinned node is refused outright. This is the
`known_hosts` model: trust is an explicit act, and there is no network path
by which a stranger becomes trusted. It does not depend on Cloud federation
becoming real, and can move there when it does.

### The handoff: `POST /federation/v1/mail`

The body is the message **byte for byte** — content addresses and the origin's
DKIM signature must survive the trip. Metadata travels in headers:

| Header | Meaning |
|---|---|
| `X-Nexus-Node` | the sending node's domain (the peer key to check against) |
| `X-Nexus-Envelope-From` | RFC 5321 MAIL FROM |
| `X-Nexus-Recipients` | comma-separated RCPT TO list |
| `X-Nexus-Timestamp` | Unix seconds |
| `X-Nexus-Nonce` | 128 random bits, hex |
| `X-Nexus-Signature` | Ed25519 over the canonical string, base64 |

The canonical string is newline-joined:
`nexus-mail-v1`, method, path, host, timestamp, nonce, envelope-from,
recipients, lowercase-hex SHA-256 of the body. Every field an attacker could
usefully change is inside the signature.

The receiver rejects: an unpinned node; a bad signature; a timestamp more
than 300 s from its clock; a nonce it has seen from that node (nonces are
kept for the window, in `federation_nonces`).

### What the receiver does with each recipient

- **Recipient on one of our domains** → `Deliverer::accept_federated`, i.e.
  ordinary federated mail into a local mailbox.
- **Recipient elsewhere** → relay only if the peer is pinned with
  `may_relay = true` **and** the envelope-from domain is that peer's own
  domain. The message is stored with `Transport::Federated` and enqueued as an
  `smtp` delivery; our own worker delivers it.
- Anything else is refused per recipient. This node is never an open relay,
  and one peer cannot send as another peer's domain.

### What the sender does

The worker stops skipping non-`smtp` rows:

- `federated` rows go to `HttpTransport`, which signs and POSTs to the pinned
  peer for that domain.
- `smtp` rows follow `NEXUS_EMAIL_EGRESS`:
  - `direct` (default, today's behaviour) — MX lookup and SMTP on port 25;
  - `peer:<domain>` — hand the message to that pinned peer as a relay.

Retries, backoff and bounces are the existing queue's. A peer that answers
4xx/5xx, times out, or is unreachable defers; only an explicit per-recipient
refusal is permanent.

### DKIM at the origin

When `NEXUS_EMAIL_DKIM_KEY_PATH` and `NEXUS_EMAIL_DKIM_SELECTOR` are set, the
worker signs each message with the origin domain's key **before** it leaves —
directly or via a peer — so mail delivered by the peer is still
DKIM-aligned for `tnhc.dev`. The operator adds the peer's IP to the domain's
SPF record. The peer's IP reputation is what receiving providers judge; that
is a property of the peer, and no code here changes it.

### Where it runs

Federation ingest is **its own listener in `nexus-mailsmtpd`**
(`NEXUS_EMAIL_FEDERATION_BIND`, default `127.0.0.1:2580`), which also runs the
delivery worker. It is never mounted on `nexus-mailapi`: that service trusts a
caller-supplied `X-Nexus-Subject` header and is only safe on loopback. Public
exposure of the federation port through the ecosystem proxy is an operator
step, documented, not automated here.

### Administration: `nexus-mailctl`

Mailbox, address and peer management is a CLI on the node, not an HTTP API:

```
nexus-mailctl mailbox create --node "Info"            # prints the mailbox id
nexus-mailctl address add <mailbox-id> info@tnhc.dev --primary
nexus-mailctl peer add <domain> <base-url> <public-key> [--may-relay]
nexus-mailctl peer list | peer remove <domain>
nexus-mailctl key show
```

Shell access to the node already means operator; an HTTP endpoint would need
a role check that the Dashboard does not currently forward. This replaces the
uncommitted `POST /api/v1/mailboxes` and `/api/v1/addresses` endpoints, which
let any signed-in user bind any address — including `info@tnhc.dev` — to any
mailbox.

## Verification

- Signature: every canonical field, tampered alone, fails verification.
- Ingest refuses: unpinned node, bad signature, stale timestamp, replayed
  nonce, external recipient from a non-relay peer, envelope-from outside the
  peer's domain, recipient on a domain nobody serves.
- Worker: `federated` rows reach the transport; `smtp` rows reach the egress
  peer under `peer:`; a peer failure defers rather than bounces.
- DKIM: bytes leaving the worker carry a signature that `nexus-mailauth`
  verifies against the published key.
- **End to end, two nodes in one test**, each with its own database: node A
  submits to an external address, hands off to node B, and B's worker
  delivers to a fake MX, which receives A's exact bytes with A's DKIM
  signature intact.
