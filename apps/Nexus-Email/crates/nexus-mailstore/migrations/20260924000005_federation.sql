-- Federated egress: the peers this node trusts, and the nonces it has seen.
--
-- Peers are pinned by an operator, never discovered. A row here is the whole
-- of a peer's trust: its key, where to reach it, and whether it may use this
-- node as a relay to the outside world.

CREATE TABLE mail_peers (
    domain      TEXT PRIMARY KEY CHECK (domain = lower(domain) AND domain <> ''),
    base_url    TEXT NOT NULL CHECK (base_url ~ '^https?://'),
    -- Ed25519 public key, base64 of the 32 raw bytes.
    public_key  TEXT NOT NULL CHECK (length(public_key) = 44),
    may_relay   BOOLEAN NOT NULL DEFAULT FALSE,
    created_at  TIMESTAMPTZ NOT NULL DEFAULT now(),
    updated_at  TIMESTAMPTZ NOT NULL DEFAULT now()
);

-- A signed request is valid for a few minutes; within that window the nonce is
-- what stops a captured request from being replayed. Rows older than the
-- window carry no information and are pruned.
CREATE TABLE federation_nonces (
    node     TEXT NOT NULL,
    nonce    TEXT NOT NULL,
    seen_at  TIMESTAMPTZ NOT NULL DEFAULT now(),
    PRIMARY KEY (node, nonce)
);

CREATE INDEX federation_nonces_seen_at ON federation_nonces (seen_at);
