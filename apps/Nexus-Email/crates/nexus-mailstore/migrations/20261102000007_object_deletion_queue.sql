-- Durable outbox for bodies and attachments held outside PostgreSQL.
--
-- A message row and its final mailbox membership are removed in the same
-- transaction that queues this work. Object storage is intentionally owned by
-- the caller, so failed network deletion remains retryable here rather than
-- being silently lost with the message row.

CREATE TABLE object_deletion_queue (
    id                  UUID PRIMARY KEY,
    object_key          TEXT NOT NULL UNIQUE,
    attempts            INTEGER NOT NULL DEFAULT 0 CHECK (attempts >= 0),
    next_attempt_at     TIMESTAMPTZ NOT NULL,
    lease_expires_at    TIMESTAMPTZ,
    created_at          TIMESTAMPTZ NOT NULL
);

CREATE INDEX object_deletion_queue_ready_idx
    ON object_deletion_queue (next_attempt_at, created_at)
    WHERE lease_expires_at IS NULL;
