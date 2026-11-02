use std::future::Future;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct PurgePass {
    pub batches: u32,
    pub purged: u64,
    pub capped: bool,
}

/// Run one bounded purge pass.
///
/// The callback must erase at most `limit` eligible memberships and return the
/// number erased. This loop runs at most `max_batches` batches and yields
/// between batches to avoid monopolising the runtime.
pub async fn run_bounded_purge_pass<F, Fut, E>(
    batch_size: i64,
    max_batches: u32,
    mut purge_batch: F,
) -> Result<PurgePass, E>
where
    F: FnMut(i64) -> Fut,
    Fut: Future<Output = Result<u64, E>>,
{
    let batch_size = batch_size.clamp(1, 10_000);
    let max_batches = max_batches.max(1);

    let mut stats = PurgePass {
        batches: 0,
        purged: 0,
        capped: false,
    };
    let mut last_batch = 0_u64;

    for index in 0..max_batches {
        let purged = purge_batch(batch_size).await?;
        stats.batches += 1;
        stats.purged += purged;
        last_batch = purged;

        if purged == 0 || purged < batch_size as u64 {
            break;
        }

        if index + 1 < max_batches {
            tokio::task::yield_now().await;
        }
    }

    stats.capped = stats.batches == max_batches && last_batch == batch_size as u64;
    Ok(stats)
}
