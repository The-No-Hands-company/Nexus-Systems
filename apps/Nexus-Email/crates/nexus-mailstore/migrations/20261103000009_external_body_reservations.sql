ALTER TABLE object_generations
    ADD COLUMN reservation_expires_at TIMESTAMPTZ,
    ADD COLUMN finalized_message_id UUID REFERENCES messages(id) ON DELETE SET NULL,
    DROP CONSTRAINT object_generations_lifecycle_state_check,
    ADD CONSTRAINT object_generations_lifecycle_state_check
        CHECK (lifecycle_state IN ('reserved', 'referenced', 'cleanup_queued', 'deleted'));

CREATE INDEX object_generations_expired_reservations_idx
    ON object_generations (reservation_expires_at)
    WHERE lifecycle_state = 'reserved';
