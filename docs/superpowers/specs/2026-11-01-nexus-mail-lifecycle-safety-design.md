# Nexus Mail Lifecycle Safety Foundation — Design

**Date:** 2026-11-01  
**Status:** Approved in conversation; awaiting written-spec review  
**Scope:** Prerequisite safety foundation for the Mail P1 lifecycle release

## Goal

Make permanent erasure, outbound delivery, and external-body storage safe under
concurrency, retries, crashes, and deployment. This work blocks the P1
release; it preserves P1's existing Trash, search, spam, API, and Dashboard
work but does not release it until all safety acceptance criteria pass.

## Non-negotiable invariants

1. A message is deleted only after its final mailbox membership and every
   non-terminal recipient intent are gone.
2. Concurrent membership erasure, submission, delivery completion, and message
   cleanup serialize in one lock order. No interleaving may leak an unreachable
   message, cancel a delivery, or deadlock reciprocal sends.
3. Empty Trash permanently erases every current Trash membership, including an
   IMAP `\Deleted` membership that is hidden from display listings.
4. An external object key is store-issued, unique, and never reused. Cleanup
   deletes a recorded immutable object version, never a mutable key alone.
5. External-body writes fail closed unless the configured storage supports
   bucket versioning, conditional object creation, object version IDs, and
   version-targeted deletion.
6. Cleanup never writes a message body, address, subject, object key, version
   ID, or account identifier to logs, metrics, or analytics.
7. A legacy external body without a provable immutable object-version mapping
   is never automatically deleted. Migration blocks release until the operator
   resolves it or confirms that no such rows exist.
8. A DMARC-quarantined message preserves and exposes its sender-policy
   disposition distinctly from user-initiated Junk placement.

## Message reference model

`messages` remains the shared immutable message row. A new `mail_submissions`
row is created atomically with a message, and one
`mail_submission_recipients` intent is created for every envelope recipient.
The intent records its resolved route and one state: `local_pending`,
`outbound_pending`, `delivered`, or `failed`. It is the authoritative
lifetime record; mailbox memberships and outbound queue rows are materialized
work derived from it, not independent holds.

Submission first parses and resolves every recipient. It then locks the
message and all local destination mailboxes in ascending UUID order, writes
the message, submission, and recipient intents, creates local memberships and
outbound queue rows, and commits once. A crash before commit creates nothing;
a crash after commit leaves complete durable recipient intents. A submission
is never exposed in a half-routed state.

Outbound work uses a fenced lease. Claiming selects `outbound_pending` intents
whose queue row is pending or whose delivery lease expired, atomically assigns
a new lease token and expiry, and marks the queue row delivering. Completion
or terminal failure requires that lease token, updates both the queue row and
recipient intent in one transaction, then performs checked orphan cleanup.
Restart reclaims expired leases; it never leaves a delivery permanently
`delivering`.

Every lifecycle operation acquires locks in this order: message row, object
generation row when present, then mailbox rows in ascending UUID order.
Explicit erase, Empty Trash, expiry purge, queue completion, terminal queue
retention purge, and external-body finalization all use this order. Orphan
cleanup runs while holding the message lock and deletes only when there is no
mailbox membership and no recipient intent in `local_pending` or
`outbound_pending`. It treats inconsistent queue/intent state as a database
error, never as permission to erase.

Queue foreign keys must not cascade-delete active work when a message is
erased. Terminal queue retention purge performs the checked orphan transition
under the message lock, so queue-only peer relays cannot retain content after
their last terminal record expires.

## External-body protocol

The object adapter is part of `nexus-mailsmtpd`; callers never receive static
S3 credentials. Before uploading an oversized body, the store creates a
durable reservation with a generation ID, opaque key, expiry, and cleanup
eligibility time:

```text
reserved -> uploading -> uploaded -> referenced -> cleanup_queued -> deleted
```

The adapter receives only a reservation-scoped, short-lived upload authority.
It conditionally creates the store-issued key, tags the version with the
generation ID, records the storage-returned version ID, and then finalizes the
reservation transactionally with the message row. A finalization retry returns
the original message only when content hash and immutable message metadata
match; otherwise it fails without changing references.

The adapter stops accepting new uploads at reservation expiry. An unfinalized
reservation is not eligible for cleanup until **15 minutes after** that
authority expires. This quiescence interval fences in-flight uploads. Recovery
reconciles an unfinalized generation by listing only versions for that unique
key and generation tag, records them durably, and queues version-targeted
deletion only after the quiescence interval. It never treats a successful
delete of a nonexistent mutable key as proof that a later upload cannot occur.

Explicit abandonment transitions a live reservation to cleanup scheduling.
Cleanup deletes each recorded/reconciled version ID for that generation. A
generation is marked deleted only after every version-targeted deletion
succeeds; failures retain durable retry state. New messages cannot attach a
reserved, cleanup-queued, or deleted generation.

At daemon startup and deployment, the adapter verifies that the configured
bucket has versioning enabled and supports conditional create and
version-targeted delete. Inline messages continue to work if it is absent;
messages requiring external storage receive a transient delivery failure.

## Migration and rollout

The lifecycle migration uses a controlled maintenance window:

1. Stop all Mail API, SMTP, IMAP, and delivery writers before schema changes.
2. Apply additive reference, reservation, version, Trash-expiry, and
   quarantine-disposition migrations.
3. Backfill Trash expiry for existing Trash memberships.
4. Inventory every existing `body_object_key`. For each one, either record a
   verified immutable version mapping or mark it legacy-unmanaged. Do not
   delete legacy-unmanaged bodies automatically.
5. Verify the object bucket's versioned capability and run reconciliation in
   dry-run mode with aggregate-only output.
6. Start only new binaries, verify writer version/health, then resume ingress.

The release is blocked if a legacy external object cannot be mapped safely.
There is no rolling deployment that mixes old folder writers with the new
Trash lifecycle.

## API and user experience

The API's Empty Trash operation selects authoritative Trash memberships rather
than calling a display-list query, so hidden IMAP-deleted rows are included.
Its `204` response remains idempotent.

Message summaries/read DTOs include a non-content disposition field. The
Dashboard renders “Quarantined by sender policy” only for DMARC `quarantine`;
ordinary Junk remains user-controlled spam disposition. No sender-policy
details or message content appear in operational logs.

## Tests and release acceptance

Automated coverage must prove:

- final mailbox erase does not remove a pending or leased outbound recipient
  intent;
- concurrent final membership erasures result in exactly one cleanup and no
  unreachable message;
- reciprocal sends do not deadlock, and an expired delivery lease is reclaimed
  after restart;
- terminal queue retention performs the final orphan check for queue-only
  messages;
- Empty Trash erases visible and IMAP-deleted Trash memberships;
- finalized and deduplicated reservation retries are idempotent only for
  matching immutable metadata;
- abandoned, crashed, and expired uploads are reconciled before
  version-targeted cleanup, including an upload completing near expiry;
- legacy external objects cannot enter an unsafe automatic-delete path;
- a stopped old writer cannot create a Trash membership during migration;
- bucket capability failure rejects only external-body delivery;
- quarantine provenance reaches the Dashboard.

Before release, run the Mail quality gate with migration and PostgreSQL
interrupted-recovery coverage; run Dashboard and Nexus API checks; execute
staging send, receive, read, Trash, restore, erase, automatic purge, search,
Junk, and quarantine journeys through the public path; run the privacy canary;
obtain founder approval; and record the evidence in the changelog.

## Explicitly deferred

Labels, rules, signatures, broader mobile/accessibility work, AI assistance,
and templates remain P2/P3 work. This foundation adds no external search,
analytics, or content-retaining observability service.
