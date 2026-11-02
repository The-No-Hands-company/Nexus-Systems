ALTER TABLE mailbox_messages
    ADD COLUMN id UUID NOT NULL DEFAULT gen_random_uuid(),
    ADD CONSTRAINT mailbox_messages_id_unique UNIQUE (id);

ALTER TABLE outbound_queue DROP CONSTRAINT outbound_queue_message_id_fkey;
ALTER TABLE outbound_queue
    ADD CONSTRAINT outbound_queue_message_id_fkey
    FOREIGN KEY (message_id) REFERENCES messages(id) ON DELETE RESTRICT;

CREATE INDEX outbound_queue_active_message_idx
    ON outbound_queue (message_id) WHERE state IN ('pending', 'delivering');

CREATE TABLE message_references (
    id UUID PRIMARY KEY,
    message_id UUID NOT NULL REFERENCES messages(id) ON DELETE RESTRICT,
    kind TEXT NOT NULL CHECK (kind IN ('submission', 'mailbox', 'outbound')),
    protected_id UUID NOT NULL,
    released_at TIMESTAMPTZ,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    UNIQUE (kind, protected_id)
);

CREATE INDEX message_references_active_message_idx
    ON message_references (message_id) WHERE released_at IS NULL;

CREATE TABLE message_submission_dispositions (
    id UUID PRIMARY KEY,
    submission_id UUID NOT NULL,
    message_id UUID NOT NULL REFERENCES messages(id) ON DELETE CASCADE,
    recipient TEXT NOT NULL,
    disposition TEXT NOT NULL CHECK (disposition IN ('mailbox', 'outbound', 'rejected')),
    membership_id UUID REFERENCES mailbox_messages(id) ON DELETE CASCADE,
    reason TEXT,
    CHECK ((disposition = 'mailbox') = (membership_id IS NOT NULL))
);

INSERT INTO message_references (id, message_id, kind, protected_id)
SELECT gen_random_uuid(), message_id, 'mailbox', id FROM mailbox_messages;

INSERT INTO message_references (id, message_id, kind, protected_id)
SELECT gen_random_uuid(), message_id, 'outbound', id
FROM outbound_queue WHERE state IN ('pending', 'delivering');

-- Existing unreferenced messages may be interrupted submissions. Retain them
-- until an operator or recovery worker can determine their disposition.
INSERT INTO message_references (id, message_id, kind, protected_id)
SELECT gen_random_uuid(), m.id, 'submission', m.id
FROM messages m
WHERE NOT EXISTS (SELECT 1 FROM message_references r WHERE r.message_id = m.id);
