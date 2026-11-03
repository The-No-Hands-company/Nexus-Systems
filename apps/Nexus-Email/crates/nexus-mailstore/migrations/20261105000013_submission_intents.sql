BEGIN;

DO $$
BEGIN
    IF EXISTS (
        SELECT 1 FROM message_submission_dispositions d
        WHERE d.disposition = 'outbound'
          AND NOT EXISTS (
              SELECT 1 FROM outbound_queue q
              WHERE q.message_id = d.message_id AND q.recipient = d.recipient
          )
    ) OR EXISTS (
        SELECT 1 FROM message_references r
        WHERE r.kind = 'outbound' AND r.released_at IS NULL
          AND NOT EXISTS (
              SELECT 1 FROM outbound_queue q
              WHERE q.id = r.protected_id AND q.message_id = r.message_id
                AND q.state IN ('pending', 'delivering')
          )
    ) THEN
        RAISE EXCEPTION 'inconsistent legacy outbound work requires reconciliation';
    END IF;
END $$;

CREATE TABLE mail_submissions (
    id UUID PRIMARY KEY,
    message_id UUID NOT NULL UNIQUE REFERENCES messages(id) ON DELETE RESTRICT,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE TABLE mail_submission_recipients (
    id UUID PRIMARY KEY,
    submission_id UUID NOT NULL REFERENCES mail_submissions(id) ON DELETE RESTRICT,
    recipient TEXT,
    route TEXT NOT NULL CHECK (route IN ('local', 'smtp', 'federated')),
    state TEXT NOT NULL CHECK (state IN ('local_pending', 'outbound_pending', 'delivered', 'failed')),
    queue_id UUID UNIQUE REFERENCES outbound_queue(id) ON DELETE RESTRICT,
    membership_id UUID REFERENCES mailbox_messages(id) ON DELETE CASCADE,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    CHECK ((route = 'local' AND queue_id IS NULL) OR
           (route IN ('smtp', 'federated') AND queue_id IS NOT NULL))
);

CREATE INDEX mail_submission_recipients_submission_idx
    ON mail_submission_recipients (submission_id);
CREATE INDEX mail_submission_recipients_pending_idx
    ON mail_submission_recipients (submission_id) WHERE state IN ('local_pending', 'outbound_pending');
CREATE INDEX mail_submission_recipients_membership_idx
    ON mail_submission_recipients (membership_id) WHERE membership_id IS NOT NULL;

ALTER TABLE outbound_queue ADD COLUMN lease_token UUID, ADD COLUMN lease_expires_at TIMESTAMPTZ;
ALTER TABLE outbound_queue DROP CONSTRAINT queue_no_duplicate_recipient;
CREATE INDEX outbound_queue_message_recipient_idx ON outbound_queue (message_id, recipient);
CREATE INDEX outbound_queue_expired_lease_idx
    ON outbound_queue (lease_expires_at) WHERE state = 'delivering';

-- Writers must be stopped during this migration. In-flight legacy deliveries
-- have no owner token, so return them to the pending queue for reclamation.
UPDATE outbound_queue SET state = 'pending' WHERE state = 'delivering';
ALTER TABLE outbound_queue ADD CONSTRAINT outbound_queue_lease_consistent CHECK (
    (state = 'delivering' AND lease_token IS NOT NULL AND lease_expires_at IS NOT NULL) OR
    (state <> 'delivering' AND lease_token IS NULL AND lease_expires_at IS NULL)
);

INSERT INTO mail_submissions (id, message_id)
SELECT gen_random_uuid(), m.id FROM messages m
WHERE EXISTS (SELECT 1 FROM outbound_queue q WHERE q.message_id = m.id)
   OR EXISTS (SELECT 1 FROM message_submission_dispositions d WHERE d.message_id = m.id);

INSERT INTO mail_submission_recipients (id, submission_id, recipient, route, state, queue_id)
SELECT gen_random_uuid(), s.id, q.recipient, q.route,
       CASE q.state WHEN 'pending' THEN 'outbound_pending'
                    WHEN 'delivered' THEN 'delivered' ELSE 'failed' END,
       q.id
FROM outbound_queue q JOIN mail_submissions s ON s.message_id = q.message_id;

INSERT INTO mail_submission_recipients (id, submission_id, recipient, route, state, membership_id)
SELECT d.id, s.id, d.recipient, 'local',
       CASE WHEN d.disposition = 'mailbox' THEN 'delivered' ELSE 'failed' END,
       d.membership_id
FROM message_submission_dispositions d
JOIN mail_submissions s ON s.message_id = d.message_id
WHERE d.disposition IN ('mailbox', 'rejected');

-- A legacy unfinished submission contains no recoverable recipient address.
-- Keep it pending for explicit operator reconciliation rather than lose mail.
INSERT INTO mail_submission_recipients (id, submission_id, route, state)
SELECT r.id, s.id, 'local', 'local_pending'
FROM message_references r
JOIN mail_submissions s ON s.message_id = r.message_id
WHERE r.kind = 'submission' AND r.released_at IS NULL;

DELETE FROM message_references r
USING mail_submissions s WHERE r.message_id = s.message_id;
DELETE FROM message_submission_dispositions d
USING mail_submissions s WHERE d.message_id = s.message_id;

COMMIT;
