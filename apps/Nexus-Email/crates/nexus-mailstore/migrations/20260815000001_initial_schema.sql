-- Nexus Email — store and identity.
--
-- Shaped by two decisions in the design doc that a generic mail schema would
-- get wrong:
--
--  1. Addresses are not accounts. A mailbox belongs to an ecosystem identity
--     that already exists in Auth, or to the node itself for role addresses
--     like info@. An address is a routing rule pointing at a mailbox, so
--     aliases and role addresses are the ordinary case rather than a bolt-on.
--
--  2. A message is stored once. Delivering the same message to five mailboxes
--     must not copy the body five times, so mailbox membership, folder
--     placement and per-mailbox flags live in a join table.

CREATE TABLE mailboxes (
    id              UUID PRIMARY KEY,
    -- Which ecosystem identity owns this. NULL means the node owns it, which
    -- is how role addresses (info@, postmaster@) exist without inventing a
    -- fake user to hold them. owner_kind keeps that explicit rather than
    -- leaving a NULL to be interpreted.
    owner_subject   TEXT,
    owner_kind      TEXT NOT NULL CHECK (owner_kind IN ('identity', 'node')),
    display_name    TEXT NOT NULL,
    created_at      TIMESTAMPTZ NOT NULL DEFAULT now(),

    CONSTRAINT mailbox_owner_consistent CHECK (
        (owner_kind = 'identity' AND owner_subject IS NOT NULL) OR
        (owner_kind = 'node'     AND owner_subject IS NULL)
    )
);
CREATE INDEX mailboxes_owner_idx ON mailboxes (owner_subject) WHERE owner_subject IS NOT NULL;

-- Routing: address -> mailbox. Many addresses may point at one mailbox.
CREATE TABLE addresses (
    id              UUID PRIMARY KEY,
    -- Stored already normalised (lowercased). The domain is case-insensitive
    -- per RFC 5321; the localpart technically is not, but treating it as
    -- case-sensitive means info@ and Info@ are different mailboxes, which
    -- surprises every human who has ever used email.
    localpart       TEXT NOT NULL,
    domain          TEXT NOT NULL,
    mailbox_id      UUID NOT NULL REFERENCES mailboxes(id) ON DELETE CASCADE,
    -- The address this mailbox sends as by default.
    is_primary      BOOLEAN NOT NULL DEFAULT FALSE,
    created_at      TIMESTAMPTZ NOT NULL DEFAULT now(),

    CONSTRAINT address_unique UNIQUE (localpart, domain),
    CONSTRAINT address_lowercase CHECK (
        localpart = lower(localpart) AND domain = lower(domain)
    )
);
CREATE INDEX addresses_mailbox_idx ON addresses (mailbox_id);
-- At most one primary address per mailbox.
CREATE UNIQUE INDEX addresses_one_primary_per_mailbox
    ON addresses (mailbox_id) WHERE is_primary;

CREATE TABLE threads (
    id                  UUID PRIMARY KEY,
    -- Normalised subject, kept for grouping messages whose headers do not
    -- reference each other. Threading is a stored relation, not something
    -- recomputed by scanning subjects on every read.
    subject_normalised  TEXT NOT NULL,
    last_activity_at    TIMESTAMPTZ NOT NULL,
    created_at          TIMESTAMPTZ NOT NULL DEFAULT now()
);
CREATE INDEX threads_activity_idx ON threads (last_activity_at DESC);

CREATE TABLE messages (
    id              UUID PRIMARY KEY,
    -- Content address of the full RFC 5322 bytes. A message is immutable once
    -- stored, so identical content received twice is the same row.
    content_hash    TEXT NOT NULL UNIQUE,
    -- The Message-ID header. Not unique: a hostile or broken sender may reuse
    -- one, and refusing the message would lose real mail.
    rfc822_msg_id   TEXT,
    thread_id       UUID NOT NULL REFERENCES threads(id) ON DELETE RESTRICT,
    in_reply_to     TEXT,
    -- The References header, ordered, for threading against messages that may
    -- not have arrived yet.
    references_ids  TEXT[] NOT NULL DEFAULT '{}',

    subject         TEXT,
    from_address    TEXT NOT NULL,
    sent_at         TIMESTAMPTZ,
    received_at     TIMESTAMPTZ NOT NULL DEFAULT now(),

    -- Which path this arrived by. Provenance drives trust decisions later:
    -- a message that came over the authenticated node channel has been
    -- vouched for in a way an anonymous SMTP delivery has not.
    transport       TEXT NOT NULL CHECK (transport IN ('internal', 'federated', 'smtp')),

    size_bytes      BIGINT NOT NULL CHECK (size_bytes >= 0),
    headers         JSONB NOT NULL DEFAULT '{}'::jsonb,

    -- Exactly one of these carries the body. Small messages live inline so a
    -- read is one query; anything larger is in object storage keyed by
    -- content hash, because Postgres is a poor blob store and attachments are
    -- most of the bytes in real mail.
    body_inline     BYTEA,
    body_object_key TEXT,

    CONSTRAINT message_body_location CHECK (
        (body_inline IS NOT NULL AND body_object_key IS NULL) OR
        (body_inline IS NULL AND body_object_key IS NOT NULL)
    )
);
CREATE INDEX messages_thread_idx  ON messages (thread_id, received_at);
CREATE INDEX messages_msgid_idx   ON messages (rfc822_msg_id) WHERE rfc822_msg_id IS NOT NULL;

-- Envelope recipients, kept separately from the To/Cc headers because the
-- envelope is what actually determined delivery and the headers can lie.
CREATE TABLE message_recipients (
    message_id  UUID NOT NULL REFERENCES messages(id) ON DELETE CASCADE,
    address     TEXT NOT NULL,
    kind        TEXT NOT NULL CHECK (kind IN ('to', 'cc', 'bcc')),
    PRIMARY KEY (message_id, address, kind)
);

CREATE TABLE folders (
    id          UUID PRIMARY KEY,
    mailbox_id  UUID NOT NULL REFERENCES mailboxes(id) ON DELETE CASCADE,
    name        TEXT NOT NULL,
    -- The four IMAP special-use folders every client expects, plus 'custom'.
    kind        TEXT NOT NULL CHECK (kind IN ('inbox', 'sent', 'drafts', 'trash', 'archive', 'custom')),
    created_at  TIMESTAMPTZ NOT NULL DEFAULT now(),

    CONSTRAINT folder_name_unique UNIQUE (mailbox_id, name),
    -- Referenced by mailbox_messages so a message cannot be filed into a
    -- folder belonging to a different mailbox. Without this the folder_id
    -- FK alone would happily accept any folder in the table.
    CONSTRAINT folder_id_mailbox_unique UNIQUE (id, mailbox_id)
);
-- A mailbox has at most one of each special-use folder; 'custom' is unbounded.
CREATE UNIQUE INDEX folders_one_special_use_per_mailbox
    ON folders (mailbox_id, kind) WHERE kind <> 'custom';

-- Membership: which mailbox holds which message, where, and with what flags.
-- This is what lets one stored message appear in many mailboxes uncopied.
CREATE TABLE mailbox_messages (
    mailbox_id  UUID NOT NULL REFERENCES mailboxes(id) ON DELETE CASCADE,
    message_id  UUID NOT NULL REFERENCES messages(id) ON DELETE CASCADE,
    folder_id   UUID NOT NULL,

    -- The IMAP system flags, as columns because they are queried constantly
    -- (unread counts) and the set is fixed by RFC 3501.
    seen        BOOLEAN NOT NULL DEFAULT FALSE,
    answered    BOOLEAN NOT NULL DEFAULT FALSE,
    flagged     BOOLEAN NOT NULL DEFAULT FALSE,
    draft       BOOLEAN NOT NULL DEFAULT FALSE,
    deleted     BOOLEAN NOT NULL DEFAULT FALSE,
    -- User-defined keywords, which IMAP allows to be arbitrary.
    keywords    TEXT[] NOT NULL DEFAULT '{}',

    added_at    TIMESTAMPTZ NOT NULL DEFAULT now(),

    PRIMARY KEY (mailbox_id, message_id),
    -- Composite, not folder_id alone: the folder must belong to THIS
    -- mailbox. Enforced by the database because application code that
    -- forgets this check produces mail that is visible to the wrong person.
    FOREIGN KEY (folder_id, mailbox_id)
        REFERENCES folders (id, mailbox_id) ON DELETE RESTRICT
);
CREATE INDEX mailbox_messages_folder_idx ON mailbox_messages (folder_id, added_at DESC);
CREATE INDEX mailbox_messages_unseen_idx ON mailbox_messages (mailbox_id) WHERE NOT seen;
