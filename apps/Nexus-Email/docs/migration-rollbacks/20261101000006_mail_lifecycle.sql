-- Roll back the lifecycle schema after the corresponding application release
-- has been withdrawn. This removes only the column and index introduced by
-- 20261101000006_mail_lifecycle.sql.

DROP INDEX IF EXISTS mailbox_messages_trash_expiry_idx;
ALTER TABLE mailbox_messages
    DROP COLUMN IF EXISTS trash_expires_at;
