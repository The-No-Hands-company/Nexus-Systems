-- Migration: add phantom_did and did_metadata to users
-- Adds phantom_did TEXT, did_metadata JSON, and creates index idx_users_phantom_did

BEGIN TRANSACTION;

-- Add phantom_did column (text)
ALTER TABLE users ADD COLUMN phantom_did TEXT;

-- Add did_metadata column (JSON). For SQLite, JSON is an affinity; for Postgres, this is valid JSON type.
ALTER TABLE users ADD COLUMN did_metadata JSON;

-- Create index for quick lookups on phantom_did
CREATE INDEX IF NOT EXISTS idx_users_phantom_did ON users (phantom_did);

COMMIT;
