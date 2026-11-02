# Nexus Mail P1 Lifecycle and Search — Design

**Date:** 2026-11-01
**Status:** Approved in conversation; awaiting written-spec review
**Scope:** Nexus Email P1 release

## Goal

Make Nexus Mail safely complete its core mailbox journey: a user can move mail
to Trash, restore it within seven days, erase it permanently on demand, find
their own mail through relevant search, and understand and control spam
disposition.

This release is privacy-first. A move to Trash is intentionally recoverable;
an explicit permanent erase removes the user's stored copy, not merely its UI
visibility. P2 labels, rules, signatures, expanded mobile/accessibility work,
and P3 optional AI assistance and templates are separate releases.

## Privacy and deletion invariants

1. Trash is recoverable for exactly seven days from the time of the move.
2. The UI states the automatic-purge deadline and identifies permanent erase
   as irreversible before the user confirms it.
3. Explicit permanent erase and scheduled expiry remove the same data in one
   transaction: mailbox membership, per-mailbox flags, folder state, and
   mailbox-search projection.
4. A message body, attachment metadata, attachment objects, and message search
   material are removed only when no other mailbox still references that
   message. A shared message remains available to other recipients.
5. Search indexes are content-bearing user data. They are deleted in the same
   transaction as the corresponding message/membership and are never copied to
   an external search system.
6. Destructive-operation logs and purge status contain aggregate technical
   counts only. They never contain an IP address, account identifier, sender,
   recipient, subject, body, attachment name, or full request URL.
7. Spam is never silently deleted. User action moves a message between Inbox,
   Junk, and Trash; inbound mail is placed in Junk automatically only when the
   sender's DMARC policy is `quarantine`.
8. External message objects have immutable generation keys. A pending cleanup
   job may delete only its recorded generation, and a new message must attach a
   new generation rather than reuse a queued key.

These rules implement the TNHC Charter's promise that deleting user data
deletes it everywhere TNHC controls, and the zero-retention design's
data-minimisation and storage-limitation requirements. Phantom is not treated
as an existing deletion or content-inspection mechanism.

## Storage and migration design

Nexus Mail keeps a message body once in `messages` and keeps each recipient's
placement and flags in `mailbox_messages`. P1 retains that ownership model.

The Mail store adds the minimal persistent state needed to identify a
Trash-placement deadline and search the content a mailbox is still permitted
to see. PostgreSQL full-text search, with a GIN index, covers subject, sender,
and parsed plain-text body. Search is always joined through
`mailbox_messages`, so a mailbox cannot search another mailbox's copy of a
shared message.

The migration:

- backfills the full-text projection for existing messages;
- records a seven-day expiry when a membership is moved into Trash;
- clears that expiry when it is restored or moved to Junk;
- creates the mailbox-safe search index;
- has an explicit down migration for all newly introduced database objects;
- is tested against a populated database and an interrupted/retried purge.

The implementation must use one store-level transactional erase operation for
both explicit deletion and automatic purge. It removes the target mailbox row
first, checks remaining references, and removes orphaned message content and
objects only when the reference count is zero. Retried operations must be
safe: an already-erased membership is a successful no-op from the caller's
perspective.

## Service lifecycle

`nexus-mailsmtpd` owns a bounded periodic purge pass. The pass finds only
expired Trash memberships, processes a limited batch per iteration, and yields
between batches so SMTP delivery and API traffic retain capacity. It exposes
aggregate health information only, such as the number of records purged in the
last pass and whether the pass completed.

No separate cron job, Meilisearch index, analytics event, or content-bearing
operational log is introduced. Recovery after restart reruns the idempotent
purge safely.

## HTTP contract

The existing folder-move route remains the sole route for recoverable actions:

```text
POST /api/v1/messages/:id/folder
{"folder":"trash" | "junk" | "inbox"}
```

P1 adds:

```text
POST /api/v1/folders/trash/empty
DELETE /api/v1/messages/:id
GET /api/v1/search?q=<query>&folder=<folder>&unread=<boolean>
```

`POST /folders/trash/empty` permanently erases every current Trash membership
owned by the authenticated mailbox. `DELETE /messages/:id` permanently erases
only the authenticated mailbox's membership; it does not erase another
recipient's copy. Both return `204 No Content` when the request has no
remaining applicable membership, avoiding a state-disclosure oracle and making
retries safe.

Search requires a non-empty bounded query, returns only the caller's mailbox
results in relevance order, and accepts folder and unread filters. Inbox and
Sent are the default scope; Junk and Trash are searched only when selected.
Malformed filters return a clear `400`; invalid or inaccessible entities remain
indistinguishable from absent entities.

Dashboard and Nexus API proxy allowlists must admit the new routes and preserve
the current trust boundary: browsers never control `X-Nexus-Subject`, and Mail
continues binding only to loopback.

## User experience

### Trash

- The Trash view shows the automatic seven-day purge policy.
- Its toolbar provides **Empty Trash**.
- Emptying Trash opens a confirmation that says the action permanently erases
  the selected mailbox copies and cannot be undone.
- Each Trash message provides **Restore** and **Delete permanently**.
- A successful action removes the message from the current view and provides
  an accessible status announcement. Failures retain the message and state a
  recoverable error.

### Search

- Mail has a search field and filters for folder and unread state.
- Results show a meaningful subject/sender/snippet match and an explicit
  empty state.
- Trash and Junk are not part of the default results but can be selected
  deliberately.
- Keyboard navigation, focus restoration after actions, labelled controls,
  visible focus, and narrow-screen layouts are tested for each core P1
  journey.

### Spam

- Junk is a visible mailbox folder.
- Inbox messages offer **Mark as Spam**; Junk messages offer **Not Spam**.
- A message delivered under a sender's DMARC `quarantine` policy visibly says
  that it was quarantined by sender policy.
- P1 adds neither opaque spam scoring nor automatic spam deletion.

## Tests and recovery coverage

The release requires focused automated coverage for:

- Inbox to Trash, Trash restore, explicit permanent erase, and empty Trash;
- seven-day expiry, a repeated purge, a restart between batches, and a failed
  migration rollback;
- shared-message references, attachment/object cleanup only after the final
  membership is erased, and per-mailbox flag removal;
- search relevance, folder/unread filters, mailbox isolation, and search
  exclusion of Trash/Junk by default;
- user spam disposition, DMARC-quarantine placement, and absence of automatic
  deletion;
- API/proxy authorization and idempotent destructive responses;
- Dashboard mobile and accessibility journeys for read, Trash, restore,
  permanent erase, search, and Junk.

The Mail quality gate must pass with the PostgreSQL migration test database.
Dashboard API/proxy and frontend tests must also pass.

## Release acceptance

Before release:

1. Run the Mail quality gate, including migration and recovery coverage.
2. Run affected Dashboard and Nexus API tests and checks.
3. Deploy to staging with a fresh Mail API artifact.
4. Verify send, receive, read, move to Trash, restore, permanent delete,
   automatic purge, search, and spam disposition through the real public path.
5. Verify the privacy canary and inspect only aggregate purge health.
6. Obtain founder approval.
7. Record the approved release and verification evidence in the changelog.

## Explicitly deferred

- **P2:** labels, user rules, signatures, and broader mobile/accessibility
  improvements.
- **P3:** optional AI assistance and reusable message templates.

Neither priority may add a new content-retaining external service to the P1
release.
