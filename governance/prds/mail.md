# Nexus Mail PRD

**Owner:** Founder
**Maturity baseline:** L3
**Release target:** L4

## Product purpose

Nexus Mail provides sovereign mailbox access, message composition, delivery,
and webmail in the Dashboard shell.

## Verified current scope

- Inbox and message reading through the Dashboard mail surface.
- Message composition and self-hosted SMTP/IMAP service components.
- Federated-egress capability for delivery where direct port-25 egress is not
  available.

## P1 requirements

- Trash, delete, and restore lifecycle.
- Search that returns relevant mailbox results.
- Spam handling with an explicit user-visible disposition.

## P2 requirements

- Labels, rules, and signatures.
- Better mobile and accessibility support for core mailbox journeys.

## P3 requirements

- Optional AI assistance and reusable message templates.

## Release acceptance

- Mail quality gate passes with migration and recovery coverage where storage
  changes.
- Staging verifies send, receive, read, delete, restore, and search journeys.
- Founder approves the release and records it in the changelog.
