BEGIN;

ALTER TABLE mail_submissions
    ADD COLUMN erasure_requested BOOLEAN NOT NULL DEFAULT false;

ALTER TABLE mail_submission_recipients ADD COLUMN retry_fingerprint BYTEA;
ALTER TABLE mail_submission_recipients DROP CONSTRAINT mail_submission_recipients_check;
ALTER TABLE mail_submission_recipients ADD CONSTRAINT mail_submission_recipients_work_check CHECK (
    (route = 'local' AND queue_id IS NULL AND retry_fingerprint IS NULL)
    OR
    (route IN ('smtp', 'federated') AND (
        (queue_id IS NOT NULL AND recipient IS NOT NULL AND retry_fingerprint IS NULL)
        OR
        (queue_id IS NULL AND recipient IS NULL AND retry_fingerprint IS NOT NULL
         AND state IN ('delivered', 'failed'))
    ))
);

COMMIT;
