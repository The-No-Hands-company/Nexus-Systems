use nexus_mailstore::Address;

/// Where a recipient's mail has to go.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Route {
    /// A mailbox on this node. Delivered by writing rows, with no network
    /// involved at all.
    Local,
    /// Another Nexus node, reached over the authenticated node-to-node
    /// channel. Never SMTP: two systems that already trust each other have no
    /// reason to speak a protocol designed for strangers, and gain a spam
    /// problem and a reputation problem by doing so.
    Federated { node: String },
    /// The rest of the world. SMTP, and the only path that depends on an
    /// unfiltered port 25.
    External,
}

/// Decides the route for an address from the domains this node knows.
#[derive(Debug, Clone, Default)]
pub struct Router {
    local_domains: Vec<String>,
    peer_domains: Vec<String>,
}

impl Router {
    pub fn new() -> Self {
        Self::default()
    }

    /// A domain whose mail is delivered into mailboxes here.
    pub fn with_local_domain(mut self, domain: &str) -> Self {
        self.local_domains.push(domain.to_ascii_lowercase());
        self
    }

    /// A domain served by a Nexus node we federate with.
    pub fn with_peer_domain(mut self, domain: &str) -> Self {
        self.peer_domains.push(domain.to_ascii_lowercase());
        self
    }

    pub fn route(&self, addr: &Address) -> Route {
        // Local wins over federated. If a domain is somehow in both lists that
        // is a misconfiguration, and delivering locally is the safe reading:
        // mail lands in a mailbox here rather than being handed to a peer who
        // may hand it back, which is how loops start.
        if self.local_domains.iter().any(|d| d == &addr.domain) {
            return Route::Local;
        }
        if self.peer_domains.iter().any(|d| d == &addr.domain) {
            return Route::Federated { node: addr.domain.clone() };
        }
        Route::External
    }

    pub fn is_local_domain(&self, domain: &str) -> bool {
        let d = domain.to_ascii_lowercase();
        self.local_domains.iter().any(|x| x == &d)
    }
}
