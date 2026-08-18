//! Outbound SMTP: getting mail off this node and into the rest of the world.
//!
//! One asymmetry runs through the whole crate: **anything that is not an
//! explicit permanent refusal is deferred.** Deferring a message that was
//! genuinely undeliverable costs a few days of retries and one bounce.
//! Bouncing a message that would have gone through loses it, and the sender
//! has no copy. So a dropped connection, a timeout, an unparseable reply, a
//! filtered port — all of them mean "try again", not "give up".
//!
//! That matters concretely here: this node's outbound port 25 is filtered by
//! the ISP, so every direct delivery attempt fails at connect. It must read as
//! "cannot deliver from here yet", never as "that address is invalid", or the
//! queue would bounce perfectly good mail the moment an egress path appeared.

pub mod client;
pub mod mx;
pub mod reply;
pub mod worker;

pub use client::{deliver, Attempt};
pub use mx::{resolve, MailExchanger, MxError};
pub use reply::{classify, code_of, is_final_line, Disposition};
pub use worker::{DeliveryWorker, WorkerConfig};
