-- Full-text search.
--
-- The searchable text is stored explicitly rather than derived from the body
-- at query time: the body is raw RFC 5322 bytes, possibly base64, possibly in
-- a legacy charset, possibly a multipart tree. Decoding that per query would
-- be both slow and wrong. The delivery path already parses the message, so it
-- extracts the displayable text once and records it here.

ALTER TABLE messages ADD COLUMN search_text TEXT;

-- Generated, so it can never drift from its inputs — there is no code path
-- that updates one without the other.
ALTER TABLE messages ADD COLUMN search_vector tsvector
    GENERATED ALWAYS AS (
        setweight(to_tsvector('simple', coalesce(subject, '')), 'A') ||
        setweight(to_tsvector('simple', coalesce(from_address, '')), 'B') ||
        setweight(to_tsvector('simple', coalesce(search_text, '')), 'C')
    ) STORED;

CREATE INDEX messages_search_idx ON messages USING GIN (search_vector);
