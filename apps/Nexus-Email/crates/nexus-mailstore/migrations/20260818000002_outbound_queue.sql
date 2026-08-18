-- The outbound queue.
--
-- Everything leaving this node passes through here: federated handoffs to
-- other Nexus nodes now, SMTP deliveries later. One row per recipient, not per
-- message, because delivery succeeds and fails per recipient — a message to
-- five people where one address bounces must deliver to the other four and
-- report only the one.

CREATE TABLE outbound_queue (
    id              UUID PRIMARY KEY,
    message_id      UUID NOT NULL REFERENCES messages(id) ON DELETE CASCADE,

    -- The envelope sender and recipient. Kept here rather than read from the
    -- message headers because the envelope is what governs delivery and the
    -- headers can disagree with it (mailing lists, forwards, bounces).
    envelope_from   TEXT NOT NULL,
    recipient       TEXT NOT NULL,
    destination     TEXT NOT NULL,

    route           TEXT NOT NULL CHECK (route IN ('federated', 'smtp')),
    state           TEXT NOT NULL DEFAULT 'pending'
                    CHECK (state IN ('pending', 'delivering', 'delivered', 'failed')),

    attempts        INTEGER NOT NULL DEFAULT 0,
    next_attempt_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    -- The reason the last attempt failed, kept for the DSN and for an operator
    -- trying to understand why mail is not moving.
    last_error      TEXT,

    created_at      TIMESTAMPTZ NOT NULL DEFAULT now(),
    completed_at    TIMESTAMPTZ,

    -- A terminal row must say when it finished, and a live one must not claim
    -- to have. Without this, "delivered with no completion time" is
    -- indistinguishable from a bug.
    CONSTRAINT queue_completion_consistent CHECK (
        (state IN ('delivered', 'failed') AND completed_at IS NOT NULL) OR
        (state IN ('pending', 'delivering') AND completed_at IS NULL)
    ),

    -- The same message is never queued twice for the same recipient. A retried
    -- submission must not become two deliveries.
    CONSTRAINT queue_no_duplicate_recipient UNIQUE (message_id, recipient)
);

-- The hot query: what is due to be delivered right now.
CREATE INDEX outbound_queue_due_idx
    ON outbound_queue (next_attempt_at)
    WHERE state = 'pending';

CREATE INDEX outbound_queue_message_idx ON outbound_queue (message_id);
