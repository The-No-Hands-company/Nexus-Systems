//! Federation for Nexus Email.

pub mod admin;
pub mod client;
pub mod error;
pub mod ingest;
pub mod key;
pub mod peers;
pub mod signature;
pub mod wire;

pub use error::{FedError, Result};
pub use key::NodeKey;
pub use peers::{Peer, PeerDirectory};
pub use signature::{sign, verify, SignedRequest};
