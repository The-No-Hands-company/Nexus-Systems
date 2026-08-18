-- IMAP identity: UIDs and UIDVALIDITY.
--
-- IMAP clients sync by UID, not by row id. The contract is strict and clients
-- cache aggressively against it:
--
--  * a UID is unique within a mailbox and never reused, even after the message
--    is deleted — reusing one makes a client show the wrong message;
--  * UIDs ascend in the order messages were added, which is what lets a client
--    ask for "everything above N";
--  * if either of those guarantees is ever broken, the server must change
--    UIDVALIDITY, which tells clients to discard their cache and resync.
--
-- Getting this wrong does not fail loudly; it silently shows people the wrong
-- mail, which is why the invariants are enforced here rather than in code.

ALTER TABLE mailboxes
    -- Changing this is how we say "your cache is worthless". Seeded from the
    -- clock so a mailbox recreated with the same id can never look unchanged
    -- to a client that still remembers the old one.
    ADD COLUMN uid_validity BIGINT NOT NULL DEFAULT extract(epoch from now())::bigint,
    -- The next UID to hand out. Monotonic, and never decremented — deleting
    -- the highest-numbered message must not free its UID for reuse.
    ADD COLUMN uid_next BIGINT NOT NULL DEFAULT 1;

ALTER TABLE mailbox_messages ADD COLUMN uid BIGINT;

-- Existing rows predate UIDs; number them by arrival so the order matches what
-- a client would expect, then make the column mandatory.
WITH numbered AS (
    SELECT mailbox_id, message_id,
           row_number() OVER (PARTITION BY mailbox_id ORDER BY added_at, message_id) AS n
    FROM mailbox_messages
)
UPDATE mailbox_messages mm
SET uid = numbered.n
FROM numbered
WHERE mm.mailbox_id = numbered.mailbox_id AND mm.message_id = numbered.message_id;

UPDATE mailboxes m
SET uid_next = COALESCE(
    (SELECT max(uid) + 1 FROM mailbox_messages WHERE mailbox_id = m.id), 1);

ALTER TABLE mailbox_messages ALTER COLUMN uid SET NOT NULL;

-- The invariant a client depends on.
ALTER TABLE mailbox_messages ADD CONSTRAINT mailbox_uid_unique UNIQUE (mailbox_id, uid);

-- The hot IMAP query: "messages in this folder, by UID".
CREATE INDEX mailbox_messages_uid_idx ON mailbox_messages (mailbox_id, uid);
