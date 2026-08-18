# Nexus Email

A sovereign mail system: `info@tnhc.dev` on a mail server we wrote, receiving
from anyone and sending to anyone, with no third-party mail service in the path.

Design: `docs/superpowers/specs/2026-08-15-nexus-email-design.md` (repo root).

## Status

Build-order steps 1-5 of 7 are implemented:

1. **Store and identity** — mailboxes, addresses, messages, threads, folders, flags.
2. **Message format** — RFC 5322 parsing, MIME, transfer encodings, attachments,
   and generation.
3. **Internal and federated delivery** — routing, local delivery, the outbound
   queue with retry and bounce boundaries. A working mail product with no SMTP
   anywhere in it.
4. **Webmail** — `nexus-mailapi` plus the shell views at `app.<domain>/mail`.
5. **SMTP inbound** — the session state machine, the listener, anti-abuse and
   the relay policy. SPF/DKIM/DMARC verification is not written yet.

Steps 6-7 (SMTP outbound, IMAP/JMAP) are not started.

## Layout

- `crates/nexus-mailstore` — mailboxes, addresses, messages, threads, folders,
  flags. No network code.
- `crates/nexus-mailmsg` — RFC 5322 and MIME. Parsing is total and lenient
  because it faces bytes chosen by strangers; generation is strict because what
  we emit must survive strict receivers and later carry a DKIM signature over
  exactly those bytes.
- `crates/nexus-maildelivery` — routing, delivery and the outbound queue.
- `crates/nexus-mailapi` — the HTTP API the webmail consumes.
- `crates/nexus-mailsmtp` — SMTP: the session state machine and the listener.

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
