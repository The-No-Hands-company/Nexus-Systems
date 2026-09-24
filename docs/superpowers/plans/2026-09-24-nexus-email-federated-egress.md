# Nexus Email federated egress — implementation plan

Spec: `docs/superpowers/specs/2026-09-24-nexus-email-federated-egress.md`.
Branch `feat/email-federated-egress`, worktree `.worktrees/email-egress`.
Every task is test-first: write the test, watch it fail for the right reason,
implement, watch it pass, run the crate's suite, commit.

Tests needing Postgres read `NEXUS_EMAIL_TEST_DATABASE_URL`, as the existing
suites do. Build in the worktree's own `target/` — never in the main
checkout's `apps/Nexus-Email/target`, which `deploy.sh` runs in production.

## Task 1 — `nexus-mailfed` crate: keys and the canonical signature

New crate `crates/nexus-mailfed` (deps: `ed25519-dalek` with `rand_core`,
`sha2`, `hex`, `base64`, `rand`).

- `NodeKey::generate()`, `NodeKey::load_or_create(path)` (32-byte seed file,
  created with 0600 intent), `public_b64()`.
- `SignedRequest { method, path, host, timestamp, nonce, envelope_from,
  recipients, body_sha256 }` with `canonical() -> String`.
- `sign(&NodeKey, &SignedRequest) -> String` and
  `verify(public_b64, &SignedRequest, sig_b64) -> Result<()>`.

Tests (`tests/signature.rs`): round trip verifies; each of the eight fields
altered alone fails; wrong key fails; malformed base64 fails;
`load_or_create` returns the same key twice from one path.

## Task 2 — peers and nonces in the store

Migration `20260924000005_federation.sql`:
`mail_peers(domain text pk, base_url text, public_key text, may_relay bool,
created_at)`; `federation_nonces(node text, nonce text, seen_at timestamptz,
pk(node, nonce))`.

`nexus-mailfed::peers::PeerDirectory` — `add`, `get`, `list`, `remove`,
`record_nonce(node, nonce) -> bool` (false if seen), `prune_nonces(older_than)`.

Tests (`tests/peers.rs`, DB): add/get/list/remove; re-adding updates;
a nonce is accepted once then refused; prune removes only old nonces.

## Task 3 — ingest

`nexus-mailfed::ingest` — an axum router with `GET /federation/v1/key` and
`POST /federation/v1/mail`. Verification order: headers present → peer pinned
→ timestamp window (300 s, injectable clock) → signature → nonce unused.
Then per recipient: local domain → `Deliverer::accept_federated`; external →
relay rules → new `Deliverer::relay_for_peer` (stores with
`Transport::Federated`, enqueues `Route::External`). Response `200` with a JSON
list of `{recipient, accepted, reason}`; request-level failures are `401`/`400`.

Tests (`tests/ingest.rs`, DB, in-process router via `tower::ServiceExt`):
local delivery lands in the inbox; relay from a `may_relay` peer is queued as
`smtp`; refused: unpinned node, bad signature, stale timestamp, replayed nonce,
external recipient from a non-relay peer, envelope-from not the peer's domain,
unserved domain.

## Task 4 — the HTTP transport

Make `FederatedTransport::send` async (boxed future, so it stays
object-safe). `nexus-mailfed::client::HttpTransport` — resolves the peer from
`PeerDirectory`, signs, POSTs with `reqwest` (rustls), maps: all recipients
accepted → `Delivered`; explicit per-recipient refusal → `Rejected`;
anything else (connect error, timeout, non-200) → `Deferred`.

Tests (`tests/client.rs`): against the Task 3 router bound on a loopback port —
accepted, refused, and a dead port deferring.

## Task 5 — the worker handles both routes

`DeliveryWorker` gains `Egress { Direct, Peer(String) }` and an
`Option<Arc<dyn FederatedTransport>>`. `federated` rows → transport to
`destination`; `smtp` rows → direct SMTP or the egress peer. No transport
configured for a federated row → deferred with a clear reason, never dropped.

Tests (`nexus-mailout/tests/worker_routes.rs`, DB): a fake transport records
calls; a federated row reaches it and is marked delivered; an smtp row under
`Peer` reaches it with the external recipient; a transport `Deferred` leaves
the row pending with `attempts = 1`.

## Task 6 — DKIM at the origin

Optional `DkimConfig { domain, selector, key }` on the worker; when set and the
envelope-from domain matches, the outgoing bytes are
`signed_message(sign(..), raw)`. The stored message is not modified.

Test: the bytes handed to the fake transport carry a `DKIM-Signature` that
`nexus-mailauth` verifies against the matching public key record.

## Task 7 — wire it into `nexus-mailsmtpd`

Env: `NEXUS_EMAIL_FEDERATION_BIND` (default `127.0.0.1:2580`),
`NEXUS_EMAIL_NODE_KEY_PATH`, `NEXUS_EMAIL_EGRESS`, `NEXUS_EMAIL_DKIM_KEY_PATH`,
`NEXUS_EMAIL_DKIM_SELECTOR`. The daemon runs the federation listener and the
delivery worker alongside SMTP/IMAP, all in the one `select!`. `deploy.sh`
passes the new variables with safe defaults.

Verification: the daemon starts against the dev database and
`curl 127.0.0.1:2580/federation/v1/key` returns the key.

## Task 8 — `nexus-mailctl`

Binary in `nexus-mailfed` (or its own crate): `mailbox create`, `address add`,
`peer add|list|remove`, `key show`. Tests drive the command functions against
the DB, not the binary's argv parsing.

## Task 9 — end to end, two nodes

`tests/two_nodes.rs`: two databases (created by the test), two ingest
routers on loopback ports, a fake MX (a minimal SMTP responder on a loopback
port). Node A pins B as egress, B pins A with `may_relay`. A submits to
`someone@example.test`; A's worker hands off; B's worker (MX override → fake
MX, port override) delivers. Assert the fake MX received A's exact bytes with
A's DKIM signature.

## Task 10 — docs and gate

Update `apps/Nexus-Email/README.md` (operator steps: generate key, exchange
public keys, `peer add`, set `NEXUS_EMAIL_EGRESS`, SPF include, expose 2580
through the proxy) and the ecosystem bible's Email row. Run `check.sh`.
