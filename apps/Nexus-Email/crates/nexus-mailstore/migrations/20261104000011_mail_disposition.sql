ALTER TABLE mailbox_messages
    ADD COLUMN disposition TEXT NOT NULL DEFAULT 'none'
    CHECK (disposition IN ('none', 'dmarc_quarantine'));
