//! IMAP4rev1, enough of it that ordinary mail clients work.
//!
//! Same shape as the SMTP crate: the session is a pure state machine that owns
//! no database handle. When it needs data it returns a `Request` and the server
//! answers it. That keeps every access-control decision — what an
//! unauthenticated client may ask, what may be read before a mailbox is
//! selected — testable without a database or a socket.

pub mod command;
pub mod server;
pub mod session;

pub use command::{expand_set, parse, Command, Tagged};
pub use server::{Authenticator, ImapServer};
pub use session::{flag_list, folder_attributes, select_response, Action, Request, Session, CAPABILITIES};
