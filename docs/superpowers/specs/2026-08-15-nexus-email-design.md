# Nexus Email — a sovereign mail system

**Date:** 2026-08-15
**Status:** design, ready for an implementation plan

## Goal

`info@tnhc.dev` — a real address on a mail server we wrote, that can receive
mail from anyone and send mail to anyone, with no third-party mail service
anywhere in the path.

## The one thing code cannot do, stated plainly

Measured on this machine, 2026-08-15:

| Path | Result |
|---|---|
| TCP 25 outbound, IPv4 (`gmail-smtp-in.l.google.com`, `aspmx.l.google.com`) | **blocked** |
| TCP 25 outbound, IPv6 | **blocked** (and no working IPv6 route) |
| TCP 587 / 465 outbound | open |
| TCP 443 outbound | open |

Delivering mail to someone else's server means opening a TCP connection to
their MX on port 25. That port is filtered upstream of this machine. No
software fixes a filtered port.

This is worth separating from the sovereignty principle, because they are
different things. Depending on a third-party **mail service** — someone else's
MTA sending on our behalf, holding our messages, and owning our reputation —
is what the principle forbids. Depending on network **transit** is not
optional for anyone; we already depend on an ISP to carry packets.

So the design assumes **we run the MTA**, and treats egress as a deployment
property with two fully-Nexus options:

1. **Direct delivery.** The ISP lifts the port 25 filter (a request, often
   granted, sometimes tied to a business tariff). Our MTA then talks to the
   world's MX servers itself. Nothing else changes.
2. **Federated egress.** Another Nexus node with unfiltered egress accepts our
   outbound queue over the authenticated node-to-node channel and delivers it.
   This is our own software on both ends, and it is what the federation
   architecture already exists to do.

Both are the same build. Nothing below depends on which arrives first.

## Architecture

### Two transports, deliberately

Mail between Nexus users never touches SMTP.

```
nexus user -> nexus user        internal delivery, one database
nexus node -> nexus node        federated delivery over the node channel
nexus     <-> outside world     SMTP, the interop gateway
```

SMTP exists because the rest of the world speaks it, not because it is a good
way to move a message between two systems that already trust each other. The
node-to-node path is authenticated, encrypted, has no spam problem, and no
reputation problem. Treating SMTP as a *gateway* rather than the substrate is
the central design decision, and it is why internal mail keeps working
regardless of what any ISP filters.

### Components

| Component | Responsibility |
|---|---|
| `nexus-mta` | SMTP: inbound on 25 (MX), submission on 587, and the outbound delivery engine — MX resolution, connection, queue, retry, bounce/DSN generation |
| `nexus-mailstore` | Message storage, threading, folders/labels, flags, full-text search |
| `nexus-mailauth` | DKIM signing and verification, SPF evaluation, DMARC policy, ARC on forward, MTA-STS and TLS-RPT |
| `nexus-mailaccess` | IMAP and JMAP for third-party clients; the API the webmail uses |
| Webmail | Shell-native views at `app.tnhc.dev/mail`, per the ecosystem shell |

**Language: Rust.** The MTA parses hostile input from anonymous strangers on a
public port, and must not fall over or be memory-unsafe when it does. The
ecosystem already runs Rust daemons (`nexus-api`, `nexus-gateway`,
`nexus-federation`, `nexus-proxy`). The existing `apps/Nexus-Email` Python
scaffold is empty — `src/main.py` and nothing else — so nothing is being
thrown away.

**What we write ourselves:** the SMTP and IMAP state machines, message
parsing and generation, the queue and retry policy, DKIM signing, SPF and
DMARC evaluation, threading, search. **What we do not write:** TLS, DNS
resolution, cryptographic primitives, base64. Reinventing a TLS stack is not
sovereignty, it is a vulnerability. The line is: we own every protocol
decision; we use vetted implementations of the maths.

### Identity, and why this is not a Gmail clone

Addresses derive from ecosystem identity, not a separate account system. A
Nexus user already has one identity through Auth; the mailbox is an attribute
of it, not a new silo. `info@tnhc.dev` is an address owned by the node, routed
to a mailbox by policy.

Consequences worth stating:

- **Federated addressing.** `user@theirnode.example` reaches another Nexus node
  directly over the node channel, with no SMTP hop and no spam surface.
- **Mail is a Nexus object.** A message can reference a Draw board or a Chat
  thread as first-class links, because it lives in the same ecosystem rather
  than a separate mail world.
- **No advertising, no scanning, no retention games.** The reason the product
  exists.

### Inbound, without waiting for anything

Receiving needs an MX record pointing at a host that accepts TCP 25 —
the same egress question in reverse. Until direct inbound exists, mail for
`tnhc.dev` can arrive over the federated channel from any Nexus node that can
receive, and the store, threading and webmail are exercised fully by internal
and federated mail from day one.

The build order below deliberately front-loads everything that does not
depend on a port being open.

## Anti-abuse, which we own

A public MTA is an open target. Ours must, from the first day it listens:

- rate-limit by IP, sender and recipient
- refuse relaying for anyone not authenticated (open relays are found within
  hours and are unrecoverable reputationally)
- evaluate SPF and DMARC on inbound and act on the result
- cap message size, recipient count and connection concurrency
- require TLS on submission, and authenticate every submission

## Out of scope

- Calendar and contacts, though the scaffold's README mentions them. Mail
  first; CalDAV/CardDAV are their own project.
- AI filtering, also mentioned in the scaffold. A spam classifier is worth
  having and is not what makes this work.
- Migration tooling for importing mail from elsewhere.

## Verification

- **SMTP conformance** against the RFC 5321 command grammar, including the
  error paths: malformed commands, oversized input, pipelining, unexpected
  sequences. Written as tests, not observed by hand.
- **We are not an open relay.** Asserted directly, as a test that fails loudly.
- **DKIM signatures verify** against an independent verifier, not only our own.
- **A round trip through the federated path** — compose on one node, read on
  another — with no SMTP involved.
- **Queue durability**: messages survive a restart mid-delivery, retry with
  backoff, and produce a DSN when they finally fail.
- **Egress readiness**: a delivery attempt to a real MX reports the exact
  failure (filtered port, refused, greylisted), never a silent drop.

## Build order

Each step is independently useful and independently verifiable.

1. **Store and identity.** Mailboxes, addresses, messages, threads, folders,
   flags. No network. Proves the data model.
2. **Message format.** RFC 5322 parsing and generation, MIME, attachments,
   headers. The thing everything else moves.
3. **Internal + federated delivery.** Nexus user to Nexus user, and node to
   node. A complete, usable mail product with no SMTP at all.
4. **Webmail in the shell.** `app.tnhc.dev/mail` — compose, read, thread,
   search. The point at which it becomes real to a person.
5. **SMTP inbound.** Listener, anti-abuse, SPF/DKIM/DMARC verification.
   Accepts mail from the world the moment an MX can point at us.
6. **SMTP outbound.** MX resolution, delivery engine, queue, retry, DSN, DKIM
   signing. Delivers directly when egress allows, and hands off to a
   federated node when it does not.
7. **IMAP/JMAP.** Third-party clients.
