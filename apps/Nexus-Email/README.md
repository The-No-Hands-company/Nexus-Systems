# Nexus Email

A sovereign mail system: `info@tnhc.dev` on a mail server we wrote, receiving
from anyone and sending to anyone, with no third-party mail service in the path.

Design: `docs/superpowers/specs/2026-08-15-nexus-email-design.md` (repo root).

## Status

Build-order steps 1-6 of 7 are implemented:

1. **Store and identity** — mailboxes, addresses, messages, threads, folders, flags.
2. **Message format** — RFC 5322 parsing, MIME, transfer encodings, attachments,
   and generation.
3. **Internal and federated delivery** — routing, local delivery, the outbound
   queue with retry and bounce boundaries. A working mail product with no SMTP
   anywhere in it.
4. **Webmail** — `nexus-mailapi` plus the shell views at `app.<domain>/mail`.
5. **SMTP inbound** — the session state machine, the listener, anti-abuse and
   the relay policy. SPF/DKIM/DMARC verification is not written yet.
6. **SMTP outbound** — MX resolution, the delivery client, and the worker that
   drains the queue.
6b. **Mail authentication** — DKIM, SPF and DMARC (`nexus-mailauth`), wired
   into the inbound SMTP path via `AuthenticatingSink`.

7. **IMAP4rev1** — `nexus-mailimap`, served by the same daemon on 2143, so
   ordinary mail clients can use a Nexus mailbox.

8. **Federated egress** — `nexus-mailfed`: signed node-to-node handoffs, pinned
   peers, and relay through a peer whose port 25 works. Design:
   `docs/superpowers/specs/2026-09-24-nexus-email-federated-egress.md`.

## The egress reality, measured

Outbound TCP 25 is filtered on this connection, over both IPv4 and IPv6. The
delivery worker was run against the real queue and behaved as designed: it
resolved Gmail's MX records, tried them in preference order, timed out on each,
and **deferred with a retry scheduled** rather than bouncing.

That asymmetry is the most important rule in `nexus-mailout`: anything that is
not an explicit permanent refusal is a deferral. Deferring undeliverable mail
costs a few days of retries and one bounce; bouncing deliverable mail loses it,
and the sender has no copy. So the queue holds that message until an egress
path exists — the ISP lifting the filter, or a peer node with clean egress.

## Layout

- `crates/nexus-mailstore` — mailboxes, addresses, messages, threads, folders,
  flags. No network code.
- `crates/nexus-mailmsg` — RFC 5322 and MIME. Parsing is total and lenient
  because it faces bytes chosen by strangers; generation is strict because what
  we emit must survive strict receivers and later carry a DKIM signature over
  exactly those bytes.
- `crates/nexus-maildelivery` — routing, delivery and the outbound queue.
- `crates/nexus-mailapi` — the HTTP API the webmail consumes.
- `crates/nexus-mailsmtp` — SMTP inbound: the session state machine and listener.
- `crates/nexus-mailout` — SMTP outbound: MX resolution, client, delivery worker.
- `crates/nexus-mailauth` — DKIM, SPF and DMARC.
- `crates/nexus-mailimap` — IMAP4rev1: session state machine and server.
- `crates/nexus-mailfed` — federation: node keys, signed handoffs, the ingest
  listener, the HTTP transport, and `nexus-mailctl`.

## Connecting a mail client

The daemon serves IMAP on `127.0.0.1:2143`, **plaintext**. There is no TLS in it,
so it is bound to loopback and anything beyond that needs a TLS terminator in
front. Credentials are your Nexus account, verified against Auth — the IMAP
server stores no password of its own.

UIDs are the part that has to be right. A UID is unique within a mailbox and
never reused, even after deletion, and they ascend in arrival order; if either
guarantee were ever broken the server would have to change UIDVALIDITY to tell
clients their cache is worthless. Those invariants are enforced in the schema
rather than in code, because breaking them does not fail loudly — it silently
shows people the wrong mail.

## Running it

`nexus-mailsmtpd` runs two listeners: an MX port for anonymous strangers, and a
submission port for our own users. `deploy.sh` starts it. The same process
runs the delivery worker, which drains the outbound queue, and the federation
listener on `127.0.0.1:2580`.

**Both default to unprivileged loopback ports (2525 / 2587).** Binding 25 needs
root or `CAP_NET_BIND_SERVICE`, and nothing can reach this node on 25 anyway —
the ISP filters it and the Cloudflare tunnel does not carry SMTP. Publishing an
MX needs both a route in and the capability; until then the daemon serves local
and federated mail.

## Administration: `nexus-mailctl`

Mailboxes, addresses and peers are managed on the node, not over HTTP. Shell
access already means operator, and an address-binding endpoint without a role
check lets any signed-in user take `info@` for themselves. It reads the same
`NEXUS_EMAIL_DATABASE_URL` and `NEXUS_EMAIL_DOMAIN` as the daemons.

```bash
cargo build --release -p nexus-mailfed
M=target/release/nexus-mailctl
$M mailbox create --node "Info"                 # prints the mailbox id
$M mailbox create --identity <auth-subject> "Eric"
$M address add <mailbox-id> info@tnhc.dev --primary
$M key show                                      # this node's public key
$M peer add <domain> <https://base-url> <public-key> [--may-relay]
$M peer list
$M peer remove <domain>
```

Only addresses in this node's domain can be bound. Restart `nexus-mailsmtpd`
and `nexus-mailapi` after pinning a peer: routing loads the peer list at start.

## Federated egress: sending without port 25

This node cannot reach any MX. Another Nexus node whose port 25 *is* open can
deliver for it: our worker DKIM-signs each message as `tnhc.dev`, hands it to
that peer over a signed HTTPS request, and the peer relays it from its own
address. Our software on both ends; no third-party mail service.

**It needs a peer.** Until someone runs one with working egress and a clean IP,
nothing changes for outside mail — it waits in the queue, deferred, exactly as
before. The peer's IP reputation is what Gmail and Outlook judge; no code on
either side changes that.

To set it up, on **this node** (the origin):

1. `nexus-mailctl key show`, and give the peer our domain, our federation URL,
   and that key.
2. `nexus-mailctl peer add <peer-domain> https://<peer-federation-host> <peer-key>`
   — no `--may-relay`: we do not relay for them unless we choose to.
3. Set `NEXUS_EMAIL_EGRESS=peer:<peer-domain>`.
4. Generate a DKIM key (`cargo run -p nexus-mailauth --example keygen`),
   publish its TXT record, and set `NEXUS_EMAIL_DKIM_KEY_PATH` and
   `NEXUS_EMAIL_DKIM_SELECTOR`.
5. Add the peer's sending IP to `tnhc.dev`'s SPF record.
6. Restart `nexus-mailsmtpd`.

On **the peer**: `nexus-mailctl peer add tnhc.dev https://<our-federation-host>
<our-key> --may-relay`, and restart. `--may-relay` is the grant; without it the
peer accepts our mail for its own users only.

**Exposing the listener.** Peers reach `POST /federation/v1/mail` over HTTPS.
The listener binds loopback (`NEXUS_EMAIL_FEDERATION_BIND`); publishing it means
a proxy route to `127.0.0.1:2580` for the host set in
`NEXUS_EMAIL_FEDERATION_HOST` (default `mail.<domain>`). That host is part of
every signature, so it must be the one peers pinned. Never route it to
`nexus-mailapi`, which trusts a caller-supplied identity header.

What a peer can and cannot do is enforced by the receiver, not trusted from the
sender: unpinned nodes, bad signatures, requests older than five minutes and
replayed nonces are refused; a peer may only send as its own domain; outside
recipients are relayed only under `--may-relay`; mail is never forwarded from
one peer to another.

| Setting | Default | Meaning |
|---|---|---|
| `NEXUS_EMAIL_EGRESS` | `direct` | `direct`, or `peer:<domain>`; anything else stops the daemon at start |
| `NEXUS_EMAIL_FEDERATION_BIND` | `127.0.0.1:2580` | the federation listener |
| `NEXUS_EMAIL_FEDERATION_HOST` | `mail.<domain>` | the host peers pin and sign for |
| `NEXUS_EMAIL_NODE_KEY_PATH` | `~/.config/nexus-email/node.key` | this node's Ed25519 key; created once, never replaced |
| `NEXUS_EMAIL_DKIM_KEY_PATH` / `_SELECTOR` | unset | both or neither |

The node key lives under `$HOME` for the same reason as the DKIM key: this
volume is NTFS, where file permissions cannot protect it. Losing it changes this
node's identity for every peer that pinned it.

## Inbound policy: Observe before Enforce

`AuthenticatingSink` takes a `PolicyMode`.

- **`Observe`** evaluates SPF, DKIM and DMARC, writes the `Authentication-
  Results` header, and delivers everything regardless. This is the correct
  setting when bringing a server up: it produces evidence to check the
  implementation against real mail *before* it is able to lose any.
- **`Enforce`** honours the sending domain's own published policy — `p=reject`
  is refused at SMTP time with a 550, `p=quarantine` is delivered to a Junk
  folder.

A rejection is a refusal during the SMTP conversation, not an accept-then-
discard. A silent discard leaves the sender — sometimes a legitimate,
misconfigured sender — believing the mail arrived.

The `Authentication-Results` header is written even when everything passes. A
later dispute about whether a message was authentic is unanswerable if the
evidence was thrown away at delivery time.

## Two approximations recorded rather than hidden

**The organizational domain is approximated.** Correct DMARC relaxed alignment
needs the Public Suffix List, a downloaded and frequently-changing dataset.
Naive "last two labels" would treat `a.co.uk` and `b.co.uk` as one
organization — which under DMARC means accepting forged mail as aligned. A
small set of common multi-label suffixes is handled explicitly and everything
else falls back to two labels. Adopting a real PSL is its own piece of work.

**SPF `ptr` is treated as no-match**, which is what RFC 7208 §5.5 recommends
for a mechanism it deprecates as slow and unreliable.

## DKIM keys

Generate one with:

```bash
cargo run -p nexus-mailauth --example keygen -- <selector> <domain>
```

It writes the private key under `$HOME/.config/nexus-email/` and prints the TXT
record to publish. **Not into the repository directory**: this project lives on
an NTFS/fuseblk volume where `chmod` silently does nothing, so a key stored
there has no permissions at all — and a DKIM private key is the authority to
send as the domain. The generator checks the resulting mode and warns loudly if
0600 did not take.

## Why the SMTP session has no sockets in it

`session.rs` is a pure state machine over lines of text — no sockets, no
database, no clock. Every rule deciding whether a stranger may send mail
through this server is therefore testable exhaustively, which matters more here
than anywhere else in the system: an open relay is found by the internet within
hours and the reputational damage is not recoverable.

The listener is deliberately thin, and there is one over-the-wire test that
relaying is still refused through it — because a listener that bypassed the
state machine would be an open relay no matter how well that machine is
tested.

## A security control that looks like a config value

`nexus-mailapi` binds loopback and trusts the `X-Nexus-Subject` header, which
the Dashboard sets after asking Auth who the caller is. That is only sound
because nothing off this machine can reach the port. Exposing it publicly would
turn that header into a way for anyone to claim to be anyone, so the bind
address is part of the security model, not a deployment preference.

## Two shapes worth knowing before changing anything

**An address is a routing rule, not an account.** A mailbox belongs to an
ecosystem identity that already exists in Auth, or to the node itself for role
addresses like `info@`. Aliases are therefore ordinary rather than a special
case, and no placeholder user has to exist to hold `postmaster@`.

**A message is stored once.** Mailbox membership, folder placement and
per-mailbox flags live in `mailbox_messages`, so delivering to five recipients
writes one message row and five membership rows — with independent read state.

## Running the tests

The database tests assert that *PostgreSQL* enforces the schema's constraints,
so they need a real one and there is no mock. Without the variable they panic
rather than skip, because a silently skipped integration test reads as a
passing one.

```bash
createdb nexus_email_test   # or: docker exec nexus-systems-postgres-1 createdb -U nexus nexus_email_test
for f in crates/nexus-mailstore/migrations/*.sql; do   # every migration, in order
  docker exec -i nexus-systems-postgres-1 psql -v ON_ERROR_STOP=1 -U nexus -d nexus_email_test < "$f"
done

# Build the URL from the running container rather than pasting a password
# anywhere: the repo must never contain a credential-shaped string.
PGPW=$(docker exec nexus-systems-postgres-1 printenv POSTGRES_PASSWORD)
export NEXUS_EMAIL_TEST_DATABASE_URL="postgres://nexus:${PGPW}@127.0.0.1:5432/nexus_email_test"

cargo test -p nexus-mailstore
```

`check.sh` builds the same URL for `nexus_email_test`. The two-node egress test
(`nexus-mailfed/tests/two_nodes.rs`) also needs the role to be allowed to
`CREATE DATABASE`: each node gets its own freshly migrated database, dropped
afterwards.
