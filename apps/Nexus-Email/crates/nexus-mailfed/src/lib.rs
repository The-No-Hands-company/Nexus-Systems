//! Federation for Nexus Email.

pub mod error;
pub mod key;
pub mod signature;

pub use error::{FedError, Result};
pub use key::NodeKey;
pub use signature::{sign, verify, SignedRequest};
