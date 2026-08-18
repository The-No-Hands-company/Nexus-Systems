//! Routing and delivery.
//!
//! The shape worth knowing: **SMTP is a gateway, not the substrate.** Mail
//! between Nexus users is a database write with no network involved, and mail
//! between Nexus nodes crosses the authenticated node-to-node channel. Neither
//! touches SMTP, so neither has a spam problem, a reputation problem, or any
//! dependence on an unfiltered port 25. SMTP exists in this system only because
//! the rest of the world speaks it.

pub mod deliver;
pub mod error;
pub mod queue;
pub mod route;
pub mod transport;

pub use deliver::{Deliverer, Disposition, Outcome};
pub use error::{DeliveryError, Result};
pub use queue::{backoff_for, max_attempts, Queue, QueuedDelivery};
pub use route::{Route, Router};
pub use transport::{FederatedHandoff, FederatedTransport};
