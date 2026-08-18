use nexus_mailimap::Authenticator;

/// Verifies IMAP credentials against Nexus-Auth.
///
/// The IMAP server deliberately holds no password of its own. Accounts live in
/// Auth, and a second copy of a credential check is a second place for it to be
/// subtly wrong — and a second place to update when policy changes.
pub struct AuthService {
    client: reqwest::Client,
    base_url: String,
}

impl AuthService {
    pub fn new(base_url: impl Into<String>) -> Self {
        Self {
            client: reqwest::Client::builder()
                .timeout(std::time::Duration::from_secs(10))
                .build()
                .unwrap_or_default(),
            base_url: base_url.into().trim_end_matches('/').to_string(),
        }
    }
}

impl Authenticator for AuthService {
    async fn verify(&self, user: &str, password: &str) -> Option<String> {
        let res = self
            .client
            .post(format!("{}/api/v1/auth/login", self.base_url))
            .json(&serde_json::json!({ "username": user, "password": password }))
            .send()
            .await
            .ok()?;

        if !res.status().is_success() {
            return None;
        }

        // The subject is what the mailbox is keyed on. A login that succeeds
        // but yields no user id is not a usable authentication, so it fails
        // rather than being treated as a pass.
        let body: serde_json::Value = res.json().await.ok()?;
        body.get("user")
            .and_then(|u| u.get("id"))
            .and_then(|id| id.as_str())
            .map(str::to_string)
    }
}
