use std::time::Duration;

use sqlx::PgPool;

/// A peer this node trusts, as pinned by an operator.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Peer {
    pub domain: String,
    /// Where its federation listener is reached, e.g. `https://mail.peer.example`.
    pub base_url: String,
    /// Ed25519 public key, base64.
    pub public_key: String,
    /// Whether it may hand us mail for the outside world to deliver.
    pub may_relay: bool,
}

/// Pinned peers and the replay guard.
#[derive(Clone)]
pub struct PeerDirectory {
    pool: PgPool,
}

impl PeerDirectory {
    pub fn new(pool: PgPool) -> Self {
        Self { pool }
    }

    pub fn pool(&self) -> &PgPool {
        &self.pool
    }

    /// Pin a peer, replacing any earlier pin for the same domain — which is
    /// how a key rotation or a revoked relay grant is applied.
    pub async fn add(&self, peer: &Peer) -> sqlx::Result<()> {
        sqlx::query(
            "INSERT INTO mail_peers (domain, base_url, public_key, may_relay)
             VALUES ($1, $2, $3, $4)
             ON CONFLICT (domain) DO UPDATE
               SET base_url = EXCLUDED.base_url, public_key = EXCLUDED.public_key,
                   may_relay = EXCLUDED.may_relay, updated_at = now()",
        )
        .bind(peer.domain.to_ascii_lowercase())
        .bind(peer.base_url.trim_end_matches('/'))
        .bind(&peer.public_key)
        .bind(peer.may_relay)
        .execute(&self.pool)
        .await?;
        Ok(())
    }

    pub async fn get(&self, domain: &str) -> sqlx::Result<Option<Peer>> {
        let row: Option<(String, String, String, bool)> = sqlx::query_as(
            "SELECT domain, base_url, public_key, may_relay FROM mail_peers WHERE domain = $1",
        )
        .bind(domain.to_ascii_lowercase())
        .fetch_optional(&self.pool)
        .await?;
        Ok(row.map(|(domain, base_url, public_key, may_relay)| Peer { domain, base_url, public_key, may_relay }))
    }

    pub async fn list(&self) -> sqlx::Result<Vec<Peer>> {
        let rows: Vec<(String, String, String, bool)> = sqlx::query_as(
            "SELECT domain, base_url, public_key, may_relay FROM mail_peers ORDER BY domain",
        )
        .fetch_all(&self.pool)
        .await?;
        Ok(rows
            .into_iter()
            .map(|(domain, base_url, public_key, may_relay)| Peer { domain, base_url, public_key, may_relay })
            .collect())
    }

    /// Returns whether a peer was pinned under that domain.
    pub async fn remove(&self, domain: &str) -> sqlx::Result<bool> {
        let done = sqlx::query("DELETE FROM mail_peers WHERE domain = $1")
            .bind(domain.to_ascii_lowercase())
            .execute(&self.pool)
            .await?;
        Ok(done.rows_affected() > 0)
    }

    /// Record a nonce. `false` means this node already saw it: a replay.
    ///
    /// One statement, so two concurrent copies of the same request cannot both
    /// be told they are first.
    pub async fn record_nonce(&self, node: &str, nonce: &str) -> sqlx::Result<bool> {
        let done = sqlx::query(
            "INSERT INTO federation_nonces (node, nonce) VALUES ($1, $2) ON CONFLICT DO NOTHING",
        )
        .bind(node.to_ascii_lowercase())
        .bind(nonce)
        .execute(&self.pool)
        .await?;
        Ok(done.rows_affected() == 1)
    }

    /// Forget nonces older than `window`. Safe because a request that old is
    /// refused on its timestamp before its nonce is ever consulted.
    pub async fn prune_nonces(&self, window: Duration) -> sqlx::Result<u64> {
        let done = sqlx::query(
            "DELETE FROM federation_nonces WHERE seen_at < now() - make_interval(secs => $1)",
        )
        .bind(window.as_secs_f64())
        .execute(&self.pool)
        .await?;
        Ok(done.rows_affected())
    }
}
