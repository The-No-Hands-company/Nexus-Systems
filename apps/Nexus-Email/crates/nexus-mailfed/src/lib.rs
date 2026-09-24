//! Federation for Nexus Email.

pub mod error;
pub mod key;
pub mod peers;
pub mod signature;

pub use error::{FedError, Result};
pub use key::NodeKey;
pub use peers::{Peer, PeerDirectory};
pub use signature::{sign, verify, SignedRequest};
