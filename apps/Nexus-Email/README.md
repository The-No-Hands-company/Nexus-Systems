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

Step 7 (IMAP/JMAP) is not started.

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
docker exec -i nexus-systems-postgres-1 psql -U nexus -d nexus_email_test \
  < crates/nexus-mailstore/migrations/20260815000001_initial_schema.sql

# Build the URL from the running container rather than pasting a password
# anywhere: the repo must never contain a credential-shaped string.
PGPW=$(docker exec nexus-systems-postgres-1 printenv POSTGRES_PASSWORD)
export NEXUS_EMAIL_TEST_DATABASE_URL="postgres://nexus:${PGPW}@127.0.0.1:5432/nexus_email_test"

cargo test -p nexus-mailstore
```
