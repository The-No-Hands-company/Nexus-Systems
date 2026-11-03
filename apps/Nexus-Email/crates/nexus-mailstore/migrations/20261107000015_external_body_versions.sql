ALTER TABLE object_generations
    ADD COLUMN uploaded_version_id TEXT,
    ADD COLUMN uploaded_generation_tag TEXT,
    ADD COLUMN upload_recorded_at TIMESTAMPTZ,
    ADD COLUMN cleanup_eligible_at TIMESTAMPTZ;

UPDATE object_generations
SET cleanup_eligible_at = reservation_expires_at + interval '15 minutes'
WHERE reservation_expires_at IS NOT NULL;

ALTER TABLE object_deletion_queue ADD COLUMN version_id TEXT;
