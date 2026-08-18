//! SMTP for Nexus Email.
//!
//! The session is a pure state machine over lines of text — no sockets, no
//! database, no clock — so every rule governing whether a stranger may send
//! mail through this server can be tested exhaustively. That matters more here
//! than anywhere else in the system: an open relay is found by the internet
//! within hours, and the reputational damage is not recoverable.

pub mod command;
pub mod inbound;
pub mod policy;
pub mod server;
pub mod session;

pub use command::{parse, Command, MAX_COMMAND_LINE};
pub use inbound::{AuthenticatingSink, PolicyMode};
pub use server::{Inbound, Sink, SmtpServer};
pub use session::{Action, Limits, RelayPolicy, Role, Session};
