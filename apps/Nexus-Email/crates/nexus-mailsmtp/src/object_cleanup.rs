use std::future::Future;

use chrono::{DateTime, Utc};
use nexus_mailstore::{MailStore, RecordedExternalBody, Result};

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct ObjectDeletionPass {
    pub deleted: u64,
    pub failed: u64,
}

/// Delete one bounded batch of externally stored mail content.
///
/// The object-store client remains outside the store crate. A failed operation
/// is put back on its durable retry schedule without retaining an error that
/// might contain message content or an object key.
pub async fn run_object_deletion_pass<F, Fut>(
    store: &MailStore,
    now: DateTime<Utc>,
    limit: i64,
    mut delete_object: F,
) -> Result<ObjectDeletionPass>
where
    F: FnMut(RecordedExternalBody) -> Fut,
    Fut: Future<Output = std::result::Result<(), ()>>,
{
    store
        .queue_expired_external_body_reservations(now, limit)
        .await?;

    let jobs = store.claim_object_deletion_jobs(now, limit).await?;
    let mut stats = ObjectDeletionPass {
        deleted: 0,
        failed: 0,
    };

    for job in jobs {
        let body = RecordedExternalBody {
            generation_id: job.generation_id,
            object_key: job.object_key,
            version_id: job.version_id,
        };
        if delete_object(body).await.is_ok() {
            store
                .complete_object_deletion(job.id, job.generation_id)
                .await?;
            stats.deleted += 1;
        } else {
            store.retry_object_deletion(job.id, now).await?;
            stats.failed += 1;
        }
    }

    Ok(stats)
}
