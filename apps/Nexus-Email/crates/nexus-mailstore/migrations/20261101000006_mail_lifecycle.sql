-- Mail lifecycle storage: Trash expiry.
--
-- Trash is recoverable for seven days. We persist the per-membership expiry so
-- purge is deterministic and idempotent.

ALTER TABLE mailbox_messages
    ADD COLUMN trash_expires_at TIMESTAMPTZ;

-- Existing Trash memberships should start expiring from when they were filed.
UPDATE mailbox_messages mm
SET trash_expires_at = GREATEST(now(), mm.added_at) + interval '7 days'
FROM folders f
WHERE mm.folder_id = f.id
  AND mm.mailbox_id = f.mailbox_id
  AND f.kind = 'trash'
  AND mm.trash_expires_at IS NULL;

CREATE INDEX mailbox_messages_trash_expiry_idx
    ON mailbox_messages (trash_expires_at)
    WHERE trash_expires_at IS NOT NULL;
