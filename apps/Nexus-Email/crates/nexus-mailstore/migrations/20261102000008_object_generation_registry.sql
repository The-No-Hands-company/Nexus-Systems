-- Fence external object cleanup to an immutable generation.
--
-- Existing jobs have only a reusable key and cannot be proven safe after a
-- later write. They are retained but are deliberately not claimable until a
-- generation is associated by an operator; new jobs always carry one.

CREATE TABLE IF NOT EXISTS object_generations (
    id              UUID PRIMARY KEY,
    -- The upload contract supplies this key after bytes are already present.
    -- Reserve it forever: reuse could race cleanup for its old generation.
    object_key      TEXT NOT NULL UNIQUE,
    lifecycle_state TEXT NOT NULL CHECK (lifecycle_state IN ('referenced', 'cleanup_queued', 'deleted')),
    created_at      TIMESTAMPTZ NOT NULL
);

ALTER TABLE messages
    ADD COLUMN IF NOT EXISTS body_object_generation_id UUID
    REFERENCES object_generations(id) ON DELETE RESTRICT;

ALTER TABLE object_deletion_queue
    ADD COLUMN IF NOT EXISTS generation_id UUID
    REFERENCES object_generations(id) ON DELETE RESTRICT;

CREATE UNIQUE INDEX IF NOT EXISTS object_deletion_queue_generation_idx
    ON object_deletion_queue (generation_id)
    WHERE generation_id IS NOT NULL;
